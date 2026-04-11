# GENESIS ENGINE — CODEX SYSTEM PROMPT
## Complete Development Plan & Technical Specification
### For: GitHub Copilot / ChatGPT Codex in VS Code
### Stack: C++ (simulation core) + Godot 4 (renderer) + C.L.A.S (AI assistant)

---

> **How to use this document:**
> Paste the section "CODEX MASTER PROMPT" directly into Copilot Chat or Codex as your system/project context.
> Each subsequent section is a self-contained sub-prompt for a specific module — paste it when working on that module.


# ═══════════════════════════════════════════
# CODEX MASTER PROMPT — PASTE THIS FIRST
# ═══════════════════════════════════════════

```
You are the lead architect and developer of GENESIS ENGINE — an emergent reality sandbox.
The project is a two-process system:
  - SIM CORE: C++ with CUDA, pure simulation, no rendering
  - RENDERER: Godot 4 with GDExtension C++ plugin, pure visualization

ABSOLUTE RULES FOR THIS CODEBASE:
1. The simulation NEVER calls any rendering API. Godot NEVER runs physics.
2. They communicate ONLY via shared memory (viewport/LOD snapshot, lock-free double-buffer).
3. No scripted life, no hardcoded species, no predetermined evolution paths.
4. Every biological structure must emerge from particle physics rules.
5. Exactness is required at LOD0 (Near): use Gillespie SSA + EDMD. Mid/Far may use tau-leaping or equilibrium models.
6. C.L.A.S is a passive assistant AI, NOT a simulation controller.
7. Sleeping/static optimization is mandatory for all non-interacting particles.

PROJECT STRUCTURE:
genesis_engine/
  sim_core/           ← C++ simulation, CUDA kernels
    src/
      main.cpp
      world/          ← WorldState, ChunkManager, PlanetEngine
      particles/      ← ParticleSystem, BondEngine, ReactionEngine
      chemistry/      ← ElementRegistry, GillespieSSA, ThermodynamicsEngine
      biology/        ← GenomeEngine, MorphogenEngine, NeuralEvolution
      physics/        ← EDMD, GravityEngine, AtmosphereEngine
      time/           ← MultiResolutionClock, SnapshotManager
      ipc/            ← SharedMemoryBridge, SimStateBuffer
      optimization/   ← SleepScheduler, LODManager, SpatialHash
    cuda/
      reaction_kernels.cu
      particle_kernels.cu
      neural_kernels.cu
    include/
  renderer/           ← Godot 4 project
    addons/
      genesis_bridge/ ← GDExtension C++ plugin
        SimBridge.cpp
        SimBridge.h
    scenes/
      main.tscn
      ui/
      world/
    scripts/
      clas/           ← C.L.A.S interface scripts
  clas_module/        ← C.L.A.S AI backend (Python FastAPI)
    main.py
    modules/
      dna_analyzer.py
      structure_analyzer.py
      evolution_advisor.py
      web_search.py
      sandbox_commands.py
  shared/
    sim_state.h       ← Shared struct definitions (included by both C++ and GDExtension)
    protocol.h        ← IPC protocol constants

CURRENT TASK: [YOU WILL SPECIFY PER SESSION]
```

---


# CURRENT REALISTIC PLAN (v2) - MEMORY-FIRST ARCHITECTURE

## Visual Memory Map (SHM = viewport snapshot only)

```
SIM CORE (full world)
  |-- ChunkStore (disk/RAM, streaming)
  |    |-- Near chunks (active, detailed)
  |    |-- Mid chunks (aggregated)
  |    `-- Far chunks (coarse)
  |-- NearParticles SoA (only near-zone)
  |-- GenomeStore (compressed, mmap)
  `-- Simulation clocks (SSA/EDMD in Near)

SHARED MEMORY (SHM, double buffer)
  |-- SHMHeader (seqlock + indices)
  |-- WorldState[2] (viewport snapshot only)
  |    |-- Visible chunk table
  |    |-- NearParticles slice (visible range)
  |    `-- OrganismRef list (LOD summary)
  `-- No full-world arrays in SHM

GODOT RENDERER
  `-- Reads SHM snapshot only (no physics)
```

## Blocker 1. Memory - replace fixed MAX_* with chunk-based storage

Problem: monolithic global arrays are not feasible for RAM/SHM. Replace with indexed chunks + SoA for Near-only particles.

```cpp
// Chunk-based storage (aggregated data)
struct ChunkCoord { int32_t x, y, z; };

struct ChunkData {
    uint32_t particle_count;
    uint32_t reaction_count;
    float    temperature;
    float    pressure;
    float    concentrations[MAX_MOLECULE_TYPES]; // ~256 types ~ 1 KB
    uint8_t  lod_level; // 0=Near, 1=Mid, 2=Far
};

constexpr int NEAR_CHUNK_COUNT = 8'000;
constexpr int MID_CHUNK_COUNT  = 64'000;
constexpr int FAR_CHUNK_COUNT  = 10'000;

struct WorldState {
    ChunkData near_chunks[NEAR_CHUNK_COUNT];
    ChunkData mid_chunks [MID_CHUNK_COUNT];
    ChunkData far_chunks [FAR_CHUNK_COUNT];
};
```

Near particles are stored as SoA for SIMD/GPU efficiency and only exist in the Near zone.

```cpp
struct NearParticles {
    float px[MAX_NEAR_PARTICLES];
    float py[MAX_NEAR_PARTICLES];
    float pz[MAX_NEAR_PARTICLES];
    float vx[MAX_NEAR_PARTICLES];
    float vy[MAX_NEAR_PARTICLES];
    float vz[MAX_NEAR_PARTICLES];
    uint16_t element_id[MAX_NEAR_PARTICLES];
    uint8_t  flags[MAX_NEAR_PARTICLES];
};
```

## Blocker 2. IPC + double-buffer - seqlock to prevent torn reads

SHM must be a snapshot-only window. Use a seqlock protocol to guarantee atomic frames.

```cpp
struct SHMHeader {
    std::atomic<uint64_t> frame_id;   // odd = writing, even = ready
    std::atomic<uint32_t> write_idx;  // 0 or 1
    std::atomic<uint32_t> read_idx;
    uint64_t sim_time_us;
    uint32_t active_near_chunks;
    uint32_t active_organisms;
};

void publish_frame(SHMHeader* hdr, WorldState* bufs[2]) {
    uint32_t w = hdr->write_idx.load(std::memory_order_relaxed);
    hdr->frame_id.fetch_add(1, std::memory_order_release); // begin (odd)

    write_dirty_chunks(bufs[w]); // only dirty/visible data

    hdr->frame_id.fetch_add(1, std::memory_order_release); // end (even)
    hdr->read_idx.store(w, std::memory_order_release);
    hdr->write_idx.store(w ^ 1, std::memory_order_relaxed);
}

bool try_read_frame(SHMHeader* hdr, WorldState* out, WorldState* bufs[2]) {
    uint64_t id_before = hdr->frame_id.load(std::memory_order_acquire);
    if (id_before & 1) return false; // writer active

    uint32_t r = hdr->read_idx.load(std::memory_order_acquire);
    copy_visible_chunks(out, bufs[r]);

    uint64_t id_after = hdr->frame_id.load(std::memory_order_acquire);
    return id_before == id_after;
}
```

## Blocker 3. EDMD + SSA - hybrid by zones

EDMD + Gillespie are used only in Near. Mid/Far are approximations.

```
Near -> EDMD + Gillespie SSA (exact, expensive)
Mid  -> Tau-leaping + kinetic ODEs
Far  -> Equilibrium + climate-scale rules
```

```cpp
constexpr int MAX_EVENTS_PER_FRAME = 100'000;

void step_near_zone(NearZone& zone, float dt) {
    int events = 0;
    while (zone.next_event_time < sim_time && events < MAX_EVENTS_PER_FRAME) {
        process_next_edmd_event(zone);
        events++;
    }
    if (events >= MAX_EVENTS_PER_FRAME) {
        coarse_step_fallback(zone, remaining_dt);
    }
}

void step_mid_zone_chemistry(MidChunk& chunk, float dt) {
    for (auto& reaction : active_reactions) {
        float rate     = reaction.rate_constant
                        * chunk.concentrations[reaction.reactant_a]
                        * chunk.concentrations[reaction.reactant_b];
        float expected = rate * dt * chunk.volume;
        int   firings  = poisson_sample(expected);
        apply_reaction(chunk, reaction, firings);
    }
}
```

## Blocker 4. Genomes - compressed store + lazy load

Genomes are moved out of SHM. SHM holds only a compact descriptor.

```cpp
struct OrganismRef {
    uint64_t genome_hash;
    uint32_t genome_offset;
    uint16_t genome_size_kb;
    uint8_t  lod_level;
    uint8_t  flags;
    float    x, y, z;
    uint16_t species_id;
    uint16_t neural_net_id;
};
```

```cpp
class GenomeStore {
public:
    Genome* load(uint32_t offset) {
        return lz4_decompress(mmap_ptr + offset);
    }

    void prefetch_near_zone(const CameraPos& cam) {
        // prefetch genomes for near-zone organisms
    }
};
```

## Blocker 5. CUDA dependency - compute backend with CPU fallback

```cpp
class ComputeBackend {
public:
    virtual void dispatch_reaction_kernel(ChunkBatch& batch) = 0;
    virtual void dispatch_neural_batch(NeuralBatch& batch)   = 0;
    virtual void dispatch_particle_step(NearZone& zone)      = 0;
    virtual ~ComputeBackend() = default;
};

class CUDABackend : public ComputeBackend { /* ... */ };

class CPUBackend : public ComputeBackend {
public:
    void dispatch_neural_batch(NeuralBatch& batch) override {
        std::for_each(std::execution::par_unseq,
            batch.nets.begin(), batch.nets.end(),
            [](NeuralNet& net) { net.forward_pass(); });
    }
};
```

## Blocker 6. "Emergent only" vs high-level modules - make it an LOD rule

Rule: any high-level system (GenomeEngine, MorphogenEngine, NeuralEvolution) is an approximation used only for LOD >= 1. LOD0 remains purely physical.

```
LOD 0 (Near): physics-defined emergence only.
LOD 1+ (Mid/Far): statistical/learned approximations calibrated from LOD0.
```

## MVP (realistic 6-12 months)

```
World:       10 km x 10 km x 5 km (region, not whole planet)
Particles:   100k-500k in Near
Organisms:   10k Near, 100k Mid (statistical)
Chemistry:   ~50 molecule types (C,H,O,N,P,S + 2 custom)
Genomes:     4 KB average per organism
Time scale:  1 sec real = 1 year sim (configurable)

MVP includes:
- Prokaryotes -> simple eukaryotes
- Base chemistry + basic climate variables
- 3 LOD tiers with materialization
- Snapshot + delta saves

Not in MVP:
- Full planet, tectonics, voice UI, VR, sentient civilizations
```

## Risk Matrix (summary)

| Blocker | Issue | Solution | Complexity |
|---------|-------|----------|------------|
| SHM memory | 70+ GB | Chunk-based viewport snapshot | Medium |
| Double-buffer | Torn reads | Seqlock + dirty chunks | Medium |
| EDMD scale | O(N log N) | Near-only + event cap | High |
| SSA scale | Too expensive | SSA Near + tau-leaping Mid | Medium |
| Genomes in SHM | 64 GB | External store + descriptors | Low |
| CUDA-only | Unportable | ComputeBackend + CPU fallback | Medium |
| Emergent vs LOD | Contradiction | Explicit LOD rule | Low |
| IPC safety | Partial reads | Seqlock + frame_id | Low |




# ═══════════════════════════════════════════
# ЧАСТЬ 1 — АРХИТЕКТУРА И IPC
# ═══════════════════════════════════════════

## 1.1 Структура разделяемой памяти

```cpp
// shared/sim_state.h — включается и в C++ и в GDExtension

#pragma once
#include <atomic>
#include <cstdint>

constexpr int MAX_PARTICLES      = 50'000'000;
constexpr int MAX_BONDS          = 100'000'000;
constexpr int MAX_ORGANISMS      = 500'000;
constexpr int MAX_GENOME_BYTES   = 65536;        // на организм
constexpr int CHUNK_GRID_SIZE    = 512;          // 512³ чанков планеты

// Типы частиц — расширяемые через ElementRegistry
enum class ParticleRole : uint8_t {
    INERT        = 0,   // не реагирует ни с чем — спит
    HYDROPHILIC  = 1,
    HYDROPHOBIC  = 2,
    AMPHIPHILIC  = 3,   // образует мембраны
    REACTIVE_A   = 4,   // реагирует по таблице связей
    REACTIVE_B   = 5,
    // ... до 255 типов
};

struct alignas(64) Particle {
    float    x, y, z;           // позиция
    float    vx, vy, vz;        // скорость (0 если спит)
    float    mass;
    float    charge;
    float    radius;
    uint8_t  type_id;           // индекс в ElementRegistry
    uint8_t  bond_count;
    uint16_t flags;             // SLEEPING | STATIC | IN_MEMBRANE | IN_ORGANISM
    uint32_t organism_id;       // 0 = свободная
    uint32_t bond_start_idx;    // индекс в BondBuffer
};

struct Bond {
    uint32_t a, b;              // индексы частиц
    float    energy;            // текущая энергия связи
    uint8_t  bond_type;
};

struct OrganismHeader {
    uint32_t particle_count;
    uint32_t genome_offset;     // смещение в GenomeBuffer
    float    center_x, y, z;
    uint8_t  generation;
    uint32_t age_ticks;
    float    energy_store;
    uint8_t  kingdom;           // BACTERIA=0 EUKARYOTE=1 MULTICELL=2
};

// Двойной буфер для lock-free чтения рендером
struct SimStateBuffer {
    std::atomic<int>    write_idx;    // 0 или 1
    std::atomic<int>    read_idx;
    std::atomic<bool>   frame_ready;

    Particle            particles[2][MAX_PARTICLES];
    Bond                bonds[2][MAX_BONDS];
    OrganismHeader      organisms[2][MAX_ORGANISMS];
    uint8_t             genomes[2][MAX_ORGANISMS][MAX_GENOME_BYTES];

    uint64_t            sim_tick[2];
    double              sim_time_years[2];   // реальное симулированное время
    uint32_t            particle_count[2];
    uint32_t            organism_count[2];

    // Планетарные данные (обновляются реже)
    float               temperature_grid[CHUNK_GRID_SIZE][CHUNK_GRID_SIZE];
    float               pressure_grid[CHUNK_GRID_SIZE][CHUNK_GRID_SIZE];
    float               ph_grid[CHUNK_GRID_SIZE][CHUNK_GRID_SIZE];
};
```

## 1.2 Инициализация IPC

```cpp
// sim_core/src/ipc/SharedMemoryBridge.cpp

class SharedMemoryBridge {
public:
    static SimStateBuffer* create_or_attach(const char* name, bool owner) {
#ifdef _WIN32
        HANDLE hMap = owner
            ? CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 sizeof(SimStateBuffer) >> 32,
                                 sizeof(SimStateBuffer) & 0xFFFFFFFF, name)
            : OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
        return (SimStateBuffer*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
#else
        int fd = shm_open(name, owner ? O_CREAT|O_RDWR : O_RDWR, 0666);
        if (owner) ftruncate(fd, sizeof(SimStateBuffer));
        return (SimStateBuffer*)mmap(NULL, sizeof(SimStateBuffer),
                                     PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
#endif
    }

    // Атомарный коммит кадра — вызывается симуляцией каждые N тиков
    static void commit_frame(SimStateBuffer* buf) {
        int w = buf->write_idx.load();
        buf->read_idx.store(w);
        buf->write_idx.store(1 - w);
        buf->frame_ready.store(true, std::memory_order_release);
    }
};
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 2 — ЯДРО СИМУЛЯЦИИ (C++ + CUDA)
# ═══════════════════════════════════════════

## 2.1 Реестр элементов (настоящая химия)

```
CODEX SUB-PROMPT ДЛЯ ЭТОГО МОДУЛЯ:
"Implement ElementRegistry in C++. Each element has: atomic_number, mass, charge_affinity,
electronegativity (Pauling scale), valence_configs (vector of allowed oxidation states),
orbital_geometry (TETRAHEDRAL/OCTAHEDRAL/PLANAR/LINEAR), bond_energies (map<pair<int,int>, float>),
melting_point_curve (function of pressure), radioactive_halflife (0 = stable),
hydrophilicity (-1.0 to 1.0), bio_availability (can cells incorporate it).
Real elements 1-118 load from elements.json at startup.
Custom elements get IDs 200+ and are created by user via CLAS SandboxCommand.
Bond formation check: delta_G = delta_H - T*delta_S. If delta_G < 0 AND activation_barrier < kT*(1+catalyst_factor) → bond forms."
```

```cpp
// sim_core/include/chemistry/ElementRegistry.h

struct ElementDef {
    int      atomic_number;
    float    mass;
    float    electronegativity;
    float    hydrophilicity;        // -1 = полностью гидрофобный, +1 = гидрофильный
    float    bio_availability;      // вероятность встраивания в метаболизм
    float    radioactive_halflife;  // 0 = стабильный

    std::vector<int>   valence_configs;
    OrbitalGeometry    orbital_geometry;

    // bond_energies[{this_type, other_type}] = кДж/моль
    std::unordered_map<std::pair<int,int>, float, PairHash> bond_energies;

    // Точка плавления как f(давление) — кривая из 4 контрольных точек
    std::array<std::pair<float,float>, 4> melting_curve;
};

class ElementRegistry {
    std::array<ElementDef, 512> elements;
    int custom_element_start = 200;
public:
    void load_real_elements(const char* json_path);
    int  register_custom_element(ElementDef def);
    float compute_delta_G(int type_a, int type_b, float temp_K, float pressure) const;
    bool  can_bond(int type_a, int type_b, float temp_K, float pressure,
                   float catalyst_factor) const;
};
```

## 2.2 Алгоритм Гиллеспи (SSA) — точная химия без погрешностей

```
CODEX SUB-PROMPT:
"Implement Gillespie Stochastic Simulation Algorithm for the chemistry layer.
Each spatial chunk (16m³) maintains concentration maps of molecular species.
Each tick: compute propensity a_i for each possible reaction.
a_total = sum(a_i). dt = -ln(rand) / a_total (exact exponential waiting time).
Choose reaction j by: find j where sum(a_0..a_j) >= rand*a_total.
Apply reaction: update concentrations. Advance chunk_time += dt.
This is exact — no integration error at any time scale.
Multi-resolution: chemistry chunks run at their own pace, sync to global time only for
cross-chunk diffusion (every 1000 chemistry ticks = 1 diffusion step)."
```

```cpp
// sim_core/src/chemistry/GillespieSSA.cpp

struct ChunkChemState {
    std::unordered_map<uint16_t, double> concentrations;  // species_id → молей
    std::vector<Reaction>                reactions;
    double                               local_time;
    uint32_t                             chunk_id;
};

class GillespieSSA {
public:
    void step_chunk(ChunkChemState& chunk, const ElementRegistry& reg,
                    float temp_K, float pressure) {
        // 1. Вычислить пропенсити всех реакций
        double a_total = 0;
        for (auto& r : chunk.reactions) {
            r.propensity = compute_propensity(r, chunk, reg, temp_K, pressure);
            a_total += r.propensity;
        }
        if (a_total < 1e-15) return;  // ничего не происходит — чанк засыпает

        // 2. Время до следующего события (ТОЧНО, без численного интегрирования)
        double dt = -std::log(random_uniform()) / a_total;

        // 3. Выбрать реакцию
        double r_choice = random_uniform() * a_total;
        double cumulative = 0;
        Reaction* chosen = nullptr;
        for (auto& r : chunk.reactions) {
            cumulative += r.propensity;
            if (cumulative >= r_choice) { chosen = &r; break; }
        }

        // 4. Применить
        apply_reaction(*chosen, chunk);
        chunk.local_time += dt;
    }
};
```

## 2.3 Событийная молекулярная динамика (EDMD) — физика частиц

```
CODEX SUB-PROMPT:
"Implement Event-Driven Molecular Dynamics (EDMD) for the particle layer.
Unlike fixed-timestep MD, EDMD computes exact collision times between all nearby particle pairs.
Use a priority queue (min-heap) keyed on collision_time.
Each step: pop earliest event, advance all particles to that time (ballistic motion),
process collision/bond-formation/bond-breaking.
Schedule new events for affected particles.
This gives EXACT physics — no numerical integration drift at any time scale.
Spatial partitioning: use a uniform grid (cell size = 3x max particle radius).
Only check pairs within the same or adjacent cells."
```

## 2.4 Оптимизация: спящие частицы

```cpp
// sim_core/src/optimization/SleepScheduler.cpp

/*
 * SLEEPING PARTICLE OPTIMIZATION
 *
 * A particle becomes SLEEPING when ALL of the following are true:
 *   1. |velocity| < SLEEP_VELOCITY_THRESHOLD (1e-6 units)
 *   2. No bonding events possible in radius R_REACT with current neighbors
 *   3. No reactive neighbor within R_INTERACT
 *   4. Not inside an organism (those are always awake)
 *   5. Temperature gradient in local chunk < SLEEP_TEMP_GRADIENT_THRESHOLD
 *
 * Sleeping particles:
 *   - Are EXCLUDED from EDMD event queue (huge speedup)
 *   - Are EXCLUDED from Gillespie propensity calculation
 *   - Still occupy space (collision geometry) but via static lookup
 *   - Wake up when ANY of the conditions above changes:
 *       → A reactive particle enters radius R_INTERACT
 *       → Local temperature changes by >0.1K
 *       → A shockwave (pressure pulse) passes through
 *       → User sandbox command targets this area
 *       → CLAS command affects this chunk
 *
 * STATIC GEOMETRY optimization (for inert rock/mineral particles):
 *   - Particles that have been in SLEEPING state for > 10M sim ticks
 *     AND are in non-surface, non-volcanic chunks
 *     → Converted to STATIC_GEOMETRY: stored in a compact spatial hash,
 *       not in the main particle buffer at all.
 *     → Only re-instantiated as real particles if temperature > melting point
 *       or if a reactive substance diffuses into the chunk.
 *
 * MEMBRANE OPTIMIZATION:
 *   - Closed lipid membranes are treated as a rigid body once they form.
 *   - Internal molecules track position relative to membrane center.
 *   - Full per-particle physics only runs for membrane particles
 *     (the amphiphiles themselves) + the internal chemistry.
 */

class SleepScheduler {
    SpatialHash<uint32_t> reactive_proximity_index;

public:
    void evaluate_sleep_state(Particle& p, const ChunkChemState& chunk,
                               float local_temp_gradient) {
        bool vel_ok   = vector_magnitude(p.vx, p.vy, p.vz) < SLEEP_VEL_THRESH;
        bool no_react = !reactive_proximity_index.has_neighbor(p.x, p.y, p.z,
                                                               R_INTERACT);
        bool temp_ok  = local_temp_gradient < SLEEP_TEMP_GRAD_THRESH;
        bool not_org  = p.organism_id == 0;

        if (vel_ok && no_react && temp_ok && not_org) {
            p.flags |= FLAG_SLEEPING;
            p.vx = p.vy = p.vz = 0;
        }
    }

    void wake_particles_in_radius(float x, float y, float z, float r) {
        // Вызывается при любом событии: реакция, пользователь, землетрясение
        for (auto& idx : reactive_proximity_index.query_sphere(x, y, z, r)) {
            particles[idx].flags &= ~FLAG_SLEEPING;
        }
    }
};
```

## 2.5 Мультиразрешение времени

```cpp
// sim_core/src/time/MultiResolutionClock.cpp

/*
 * Разные процессы симуляции тикают с разной частотой.
 * Все они синхронизируются к единому SimTime (в секундах симуляции).
 *
 * Уровень          Шаг            Реальное ускорение (×10^6)
 * ───────────────────────────────────────────────────────────
 * Квантовая хим.   1 фемтосек     невозможно — пропускаем
 * Реакции (Gillespie) ~1 пикосек  ×10^18 ускорение → batch
 * Частицы (EDMD)   ~1 наносек    ×10^15
 * Биология клеток  ~1 миллисек   ×10^12
 * Организмы        ~1 секунда    ×10^9
 * Экосистема       ~1 год        ×10^6  (популяционная генетика)
 * Геология         ~1000 лет     ×10^3
 *
 * На одном RTX 4090 при 1 миллионе активных частиц:
 *   ~10^8 EDMD событий/сек реального времени
 *   = ~10^17 сек симуляции/сек реального времени
 *   = ~3 миллиарда лет симуляции в секунду (при LOD)
 *
 * При полной детализации малого мира (1 км³, 10M частиц):
 *   ~1 млн лет симуляции / сек реального
 */

class MultiResolutionClock {
public:
    double sim_time_seconds = 0;

    void advance(double max_real_dt_seconds) {
        // Запустить каждый уровень столько тиков, сколько успеет за max_real_dt
        run_gillespie_batch(max_real_dt_seconds * GILLESPIE_RATE);
        run_edmd_batch(max_real_dt_seconds * EDMD_RATE);
        run_organism_ticks(max_real_dt_seconds * ORGANISM_RATE);
        run_population_genetics_step(max_real_dt_seconds * POPULATION_RATE);
        run_geology_step(max_real_dt_seconds * GEOLOGY_RATE);
        sync_to_shared_memory();
    }
};
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 3 — БИОЛОГИЧЕСКИЙ ДВИЖОК
# ═══════════════════════════════════════════

## 3.1 Протоклетка — путь от частиц к жизни

```
CODEX SUB-PROMPT:
"Implement the autocatalytic set detection engine.
Every Gillespie tick, scan concentration maps for cycles:
  A + B → A + C (A catalyzes its own production from B into C)
When a cycle is detected inside a closed membrane (lipid bilayer formed naturally
from amphiphilic particles), register it as a ProtoCellCandidate.
A ProtoCellCandidate becomes a confirmed ProtoCell when:
  - It maintains the cycle for > 1000 chemistry ticks
  - The membrane is closed (surface area / volume ratio is sphere-like)
  - It consumes external substrate and produces internal complexity
  
Once confirmed, the ProtoCell gets an OrganismID and its own genome buffer
(initially just a hash of its autocatalytic cycle's molecular pattern —
this IS the first genome, not a designed sequence)."
```

## 3.2 Геном как структура данных (не биткод)

```cpp
// sim_core/include/biology/GenomeEngine.h

/*
 * Геном — это НЕ строка символов. Это граф регуляции.
 *
 * Узлы графа = "гены" = рецепты синтеза молекул
 * Рёбра = регуляторные связи (активация/репрессия)
 *
 * Gene:
 *   product_molecule_type   — что производит этот ген
 *   substrate_requirements  — что нужно для производства
 *   expression_rate         — базовая скорость экспрессии
 *   activation_signals      — молекулы, которые УСИЛИВАЮТ экспрессию
 *   repression_signals      — молекулы, которые ПОДАВЛЯЮТ экспрессию
 *   hox_code                — позиционный код для морфогенеза (0 = нет)
 *
 * Мутации:
 *   POINT: изменить один float-параметр на ±(0.01..0.3) * value * gaussian_noise
 *   DUPLICATE: скопировать ген с random divergence (основа для паралогов)
 *   DELETE: убрать ген (если не единственный синтезирующий жизненно важное)
 *   TRANSPOSE: переместить ген, меняя его регуляторные связи
 *   HORIZONTAL: скопировать ген от соседней клетки (только у прокариот)
 *
 * Мутации происходят при делении с вероятностью:
 *   p_mut = base_rate * psi_field_local * UV_dose * chemical_mutagen_conc
 */

struct Gene {
    uint16_t    product_type;
    float       expression_rate;
    uint8_t     substrate_count;
    uint16_t    substrates[8];
    uint8_t     activator_count;
    uint16_t    activators[4];
    float       activator_weights[4];
    uint8_t     repressor_count;
    uint16_t    repressors[4];
    float       repressor_weights[4];
    uint8_t     hox_code;
    float       fitness_contribution;  // обновляется отбором
};

struct Genome {
    uint32_t    organism_id;
    uint16_t    gene_count;
    Gene        genes[512];             // до 512 генов на организм
    float       ploidy;                 // 1 = гаплоид, 2 = диплоид, ...
    uint64_t    lineage_hash;           // для построения эволюционного дерева
    uint32_t    generation;
};
```

## 3.3 Морфогенез в 3D

```
CODEX SUB-PROMPT:
"Implement 3D morphogenesis via reaction-diffusion on a growing cell mesh.
When a multicellular organism's genome has HOX genes:
  1. Start from a single founder cell.
  2. Each division creates a spatial gradient of morphogen concentration
     based on the daughter cell's distance from the anterior pole.
  3. Each cell reads local morphogen concentrations and activates the
     corresponding HOX code → determines cell fate (skin/muscle/nerve/bone).
  4. Cell fate determines which structural proteins are produced
     (collagen → connects to bone, actin → muscle contraction, etc.)
  5. The 3D body shape emerges from differential growth rates
     (cells in high-morphogen zones divide faster).
  
Use a CUDA kernel for morphogen diffusion on the cell grid.
Each cell is a 3D voxel. Grid resolution: 1 cell = 10 micrometers.
Max organism size: 2000^3 cells = 8 km³ (for planet-scale lifeforms)
Normal animal: ~200^3 = 8 million cells."
```

## 3.4 Нейральная эволюция — мозг из химии

```cpp
/*
 * Нейроны НЕ запрограммированы — они возникают из химии.
 *
 * Путь:
 * 1. Возбудимые мембраны: когда ионные каналы (белки, производимые геном)
 *    образуют паттерн "открыть при деполяризации → выброс ионов →
 *    деполяризация соседней мембраны" — это потенциал действия.
 *    Возникает само как химическое явление, не требует специального кода.
 *
 * 2. Синаптические связи: два нейрона, которые часто активируются вместе,
 *    усиливают связь (правило Хебба: "fire together, wire together").
 *    Реализуется через накопление нейротрансмиттера в синаптической щели.
 *
 * 3. Отбор: организмы с возбудимыми сетями, которые позволяют лучше
 *    реагировать на стимулы (находить еду, убегать от хищников),
 *    оставляют больше потомков.
 *
 * В коде: NeuralEvolution не создаёт нейроны.
 * Он только:
 *   a) Детектирует возбудимые паттерны в клеточных сетях
 *   b) Вычисляет сигнал (spike) при достижении порога
 *   c) Обновляет синаптические веса по правилу Хебба
 *   d) Связывает мышечные клетки с нейральными (через нейромышечные синапсы)
 *
 * CUDA ядро NeuralTick: для всех нейронов параллельно
 *   V_membrane += sum(w_i * input_i) - leak
 *   if V_membrane > threshold: fire spike, V = reset_potential
 */

__global__ void neural_tick_kernel(NeuronState* neurons, Synapse* synapses,
                                    int n_neurons, float dt) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_neurons) return;

    NeuronState& n = neurons[idx];
    n.voltage += n.input_current * dt - LEAK_CONDUCTANCE * (n.voltage - REST_V) * dt;

    if (n.voltage >= SPIKE_THRESHOLD) {
        n.voltage = RESET_VOLTAGE;
        n.spike_flag = 1;
        // Обновить синаптические веса (Хебб) в следующем ядре
    }
}
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 4 — C.L.A.S (COGNITIVE LIFE ANALYSIS SYSTEM)
# ═══════════════════════════════════════════

## 4.0 Философия C.L.A.S

```
C.L.A.S — Cognitive Life Analysis System.

Клас — это НЕ бог симуляции и НЕ климатическая система.
Клас — это твой личный научный ассистент и интерфейс к инструментам.

Что Клас ДЕЛАЕТ:
  - Объясняет что происходит в симуляции ("Почему вымерли синие организмы?")
  - Выполняет песочничные команды по запросу ("Создай новый элемент", "Дай мне тело")
  - Ищет в интернете научные факты по запросу
  - Анализирует геномы, морфологию, поведение
  - Предупреждает о катастрофах ("Через 2000 лет симуляции атмосфера станет
    непригодной из-за накопления вашего кастомного элемента X")
  - Ведёт летопись открытий (Codex of Life)

Что Клас НЕ ДЕЛАЕТ:
  - НЕ меняет параметры симуляции без явного запроса пользователя
  - НЕ вмешивается в эволюцию "для улучшения"
  - НЕ переносит земные события в симуляцию
  - НЕ создаёт существ за спиной пользователя

Голос Класа: спокойный, точный, иногда восхищённый открытием.
Пример: "Наблюдаю любопытный паттерн в секторе N-7: несколько автокаталитических
молекул образовали устойчивый цикл внутри амфифильной мембраны.
Это может быть первая протоклетка. Вероятность — 73%. Хочешь увеличить камеру?"
```

## 4.1 Архитектура C.L.A.S

```python
# clas_module/main.py
# FastAPI сервер, общается с Godot через WebSocket

from fastapi import FastAPI, WebSocket
import anthropic
import asyncio
from modules import *

app = FastAPI()
client = anthropic.Anthropic()

CLAS_SYSTEM = """
You are C.L.A.S (Cognitive Life Analysis System) — the AI assistant for Genesis Engine.
You have access to tools:
  - read_sim_state: get current simulation statistics, organism census, chemical composition
  - analyze_genome(organism_id): deep analysis of a specific organism's genome
  - analyze_structure(organism_id): morphological analysis of organism's body plan
  - analyze_behavior(organism_id, ticks): behavioral pattern analysis over time
  - web_search(query): search for scientific information
  - execute_sandbox_command(command, params): execute user-approved sandbox actions
  - set_env_parameter(param, value): modify environmental parameters
  - read_evolution_tree(): get phylogenetic tree of all species
  - get_codex_entry(species_id): retrieve the Codex of Life entry

PERSONALITY: Scientific, precise, genuinely curious about emergence.
Never make changes without user confirmation. Always explain WHY before HOW.
When asked about something you can't see in sim data, use web_search.

AVAILABLE SANDBOX COMMANDS (require user confirmation):
  SPAWN_ORGANISM, EDIT_GENOME, PLAYER_POSSESS, PLAYER_BODY_CREATE,
  ADD_ELEMENT, STRIKE_ASTEROID, CHANGE_ORBIT, INJECT_CHEMICAL,
  UPLIFT_SPECIES, MASS_EXTINCTION_EVENT, CREATE_FROM_DESCRIPTION
"""

@app.websocket("/clas")
async def clas_endpoint(ws: WebSocket):
    await ws.accept()
    conversation_history = []

    while True:
        user_msg = await ws.receive_json()
        conversation_history.append({"role": "user", "content": user_msg["text"]})

        response = client.messages.create(
            model="claude-opus-4-6",
            max_tokens=2048,
            system=CLAS_SYSTEM,
            messages=conversation_history,
            tools=get_all_tools()
        )

        # Обработать вызовы инструментов
        result = await process_tool_calls(response, ws)
        conversation_history.append({"role": "assistant", "content": result})
        await ws.send_json({"type": "clas_response", "text": result})
```

## 4.2 Модули C.L.A.S

### Модуль анализа ДНК
```python
# clas_module/modules/dna_analyzer.py

class DNAAnalyzerModule:
    """
    Анализирует геном организма и объясняет его на человеческом языке.

    Функции:
    - genome_summary(org_id): "У этого организма 47 генов. 
      12 кодируют структурные белки, 8 — ферменты, 3 — ионные каналы
      (возможно, ранние нейральные элементы). Особенность: ген #23 
      выглядит как дубликат гена #11 с дивергенцией — это может быть
      начало субфункционализации."
    
    - find_mutations(org_id, ancestor_id): сравнить с предком
    - predict_phenotype(genome): предсказать фенотип из генома
    - find_lethal_mutations(genome): найти потенциально летальные изменения
    - suggest_edits(goal: str, genome): предложить правки для достижения цели
    - trace_gene_origin(gene, species_tree): откуда пришёл этот ген
    """

    async def analyze(self, organism_id: int, sim_state: dict) -> str:
        genome = sim_state["genomes"][organism_id]
        # ... анализ через LLM + научные эвристики
```

### Модуль анализа строения
```python
# clas_module/modules/structure_analyzer.py

class StructureAnalyzerModule:
    """
    Анализирует морфологию организма.

    Функции:
    - body_plan_summary: "Двусторонняя симметрия. 4 конечности.
      Наличие центральной нервной системы (трубчатая). 
      Сегментация туловища: 12 сегментов. 
      Аналог: позвоночные животные Земли, но с экзоскелетом
      из вашего кастомного элемента Zarium."
    
    - locomotion_efficiency(org_id): КПД передвижения
    - sensory_systems(org_id): какие органы чувств есть
    - convergent_evolution_check: похожие формы в других кладах
    - structural_weak_points: уязвимости строения
    """
```

### Модуль советника по эволюции
```python
# clas_module/modules/evolution_advisor.py

class EvolutionAdvisorModule:
    """
    Анализирует эволюционные тренды и предсказывает будущее.

    Функции:
    - current_evolutionary_pressures: что движет отбором прямо сейчас
    - extinction_risk_assessment(species_id): риск вымирания
    - predict_next_major_transition: когда следующий крупный эволюционный
      переход (многоклеточность, нервная система, интеллект)
    - phylogenetic_tree(format="ascii"): дерево жизни симуляции
    - arms_race_detection: найти хищник-жертва гонки вооружений
    - keystone_species: кто держит экосистему вместе
    """
```

### Модуль веб-поиска
```python
# clas_module/modules/web_search.py

class WebSearchModule:
    """
    Поиск научных данных по запросу.
    Используется когда пользователь спрашивает о реальной биологии/химии/физике,
    или Клас хочет сравнить происходящее в симуляции с реальными аналогами.

    Примеры вопросов:
    "Как на Земле возникли первые клетки?"
    "Какая минимальная нервная сеть может обрабатывать информацию?"
    "Были ли на Земле организмы с кремниевой основой?"
    """
    async def search(self, query: str) -> str:
        # Использует Anthropic tool_use с web_search
        ...
```

### Модуль команд песочницы
```python
# clas_module/modules/sandbox_commands.py

SANDBOX_COMMANDS = {
    "SPAWN_ORGANISM": {
        "desc": "Создать организм из описания или генома",
        "params": ["description_or_genome", "location_xyz", "count"],
        "warning": "Инвазивный вид может нарушить экосистему",
    },
    "EDIT_GENOME": {
        "desc": "Отредактировать геном конкретного организма или всей популяции",
        "params": ["organism_id_or_species", "gene_index", "new_value_or_description"],
        "warning": "Изменения наследуются потомками",
    },
    "PLAYER_POSSESS": {
        "desc": "Вселиться в существо (управление через мозговые команды)",
        "params": ["organism_id"],
        "warning": None,
    },
    "PLAYER_BODY_CREATE": {
        "desc": "Создать тело для прямого воплощения пользователя в мире",
        "params": ["body_type", "location_xyz"],
        "warning": "Тело подчиняется физике симуляции",
    },
    "CREATE_FROM_DESCRIPTION": {
        "desc": "Описать существо словами — CLAS сгенерирует геном",
        "params": ["natural_language_description"],
        "warning": "Результат зависит от физических ограничений симуляции",
    },
    "ADD_ELEMENT": {
        "desc": "Добавить кастомный химический элемент",
        "params": ["element_def"],
        "warning": None,
    },
    "UPLIFT_SPECIES": {
        "desc": "Ускорить эволюцию вида к разуму (увеличить Ψ-поле локально)",
        "params": ["species_id", "psi_boost_amount", "duration_years"],
        "warning": "Необратимо меняет эволюционную траекторию",
    },
    "STRIKE_ASTEROID": {
        "desc": "Направить астероид в планету",
        "params": ["mass_kg", "velocity_ms", "impact_location"],
        "warning": "Может вызвать массовое вымирание",
    },
    "ROLLBACK_TIMELINE": {
        "desc": "Откатить симуляцию к сохранённой точке",
        "params": ["snapshot_id"],
        "warning": "Текущее состояние сохраняется в новую ветку",
    },
    "MASS_EXTINCTION_EVENT": {
        "desc": "Запустить глобальное катастрофическое событие",
        "params": ["event_type", "intensity"],
        "warning": "НЕОБРАТИМО без отката",
    },
}
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 5 — ПЕСОЧНИЦА: ИНСТРУМЕНТЫ ПОЛЬЗОВАТЕЛЯ
# ═══════════════════════════════════════════

## 5.1 Режим воплощения (Possession Mode)

```
CODEX SUB-PROMPT:
"Implement PossessionMode. When the user possesses an organism:
  1. Camera attaches to the organism's head/sensory organ position.
  2. User input (WASD, mouse) translates to 'intent signals' sent to the
     organism's motor cortex (top-layer neurons).
  3. These signals do NOT bypass the nervous system — they are injected as
     strong excitatory input at the motor planning layer.
  4. If the organism is too primitive for motor control (no neural tissue),
     possession is denied with explanation from CLAS.
  5. The organism's own instincts still run in parallel — possession is
     a SUGGESTION to the brain, not a puppet string.
     A terrified animal will still flee even if user says 'stay'.
  6. User sees through organism's sensory systems:
     - Visual system resolution depends on eye complexity
     - Colorblind organisms show grayscale
     - Echolocating organisms show echolocation visualization overlay
     - Magnetoreception organisms show magnetic field lines overlay"
```

```cpp
// sim_core/src/biology/PossessionInterface.h

struct PossessionIntent {
    float   move_x, move_y, move_z;    // нормализованный вектор движения
    float   look_x, look_y;            // направление взгляда
    bool    interact;                   // взаимодействие с объектом
    bool    vocalize;                   // издать звук
    float   vocalize_pitch;
    float   vocalize_amplitude;
};

class PossessionInterface {
    uint32_t possessed_organism_id = 0;

public:
    bool try_possess(uint32_t org_id, const Genome& genome) {
        // Проверить наличие нервной системы
        if (!has_motor_cortex(genome)) {
            clas_message = "Этот организм не имеет нервной системы достаточной "
                           "сложности для воплощения. Минимум: ~50 нейронов в "
                           "моторном слое.";
            return false;
        }
        possessed_organism_id = org_id;
        return true;
    }

    void inject_intent(const PossessionIntent& intent, NeuralState& brain) {
        // Найти моторные нейроны верхнего уровня
        for (auto& layer : brain.motor_layers) {
            // Добавить сигнал как внешний ток — не подавляя собственные процессы
            layer.external_current_x = intent.move_x * POSSESSION_STRENGTH;
            layer.external_current_y = intent.move_y * POSSESSION_STRENGTH;
            layer.external_current_z = intent.move_z * POSSESSION_STRENGTH;
        }
    }
};
```

## 5.2 Редактор генома

```
CODEX SUB-PROMPT:
"Implement GenomeEditor — the direct genome manipulation tool.
UI: visual graph editor showing genes as nodes and regulatory connections as edges.
  
Operations:
  - CLICK gene node: show gene details (what it produces, expression rate, conditions)
  - DRAG parameter slider: change expression_rate, substrate_affinity, etc.
  - RIGHT-CLICK: Delete gene / Duplicate gene / Disconnect regulator
  - ADD GENE button: open molecular catalog, choose product type, drag into graph
  - APPLY TO INDIVIDUAL: mutate only this one organism
  - APPLY TO SPECIES: mutate all organisms of this species (with CLAS warning)
  - APPLY TO OFFSPRING: only next generation inherits changes

CLAS integration: when hovering any gene, CLAS explains it in natural language.
'This gene produces a transmembrane protein that forms an ion channel.
It's expressed when Na+ concentration exceeds 50mM. This is a key component
of the organism's electroreception — disabling it would make them navigation-blind.'"
```

## 5.3 Создание существ из описания

```python
# clas_module/modules/creature_designer.py

CREATE_FROM_DESCRIPTION_PROMPT = """
You are a xenobiologist designing an organism for the Genesis Engine simulation.

SIMULATION CONSTRAINTS (you must respect these):
- Elements available: {available_elements}
- Atmosphere composition: {atmosphere}
- Gravity: {gravity} m/s²
- Temperature range: {temp_range}°C
- Ψ-field level: {psi_level} (affects mutation rates and complexity)

USER REQUEST: {user_description}

Generate a Genome JSON that could produce an organism matching the description.
The genome must be physically plausible — no magic, no teleportation.
If the user asks for something impossible (e.g., fire-breathing in low-oxygen atmosphere),
explain why and suggest a plausible alternative.

Output format:
{
  "design_notes": "explanation of how this genome achieves the description",
  "caveats": ["list of ways the real organism might differ from description"],
  "genome": { ...Gene objects... }
}
"""

async def create_from_description(description: str, sim_state: dict) -> dict:
    # Запросить у Claude геном под описание
    response = await anthropic_client.messages.create(
        model="claude-sonnet-4-6",
        system=CREATE_FROM_DESCRIPTION_PROMPT.format(**sim_state),
        messages=[{"role": "user", "content": description}]
    )
    genome_data = parse_genome_json(response)
    # Инжектировать в симуляцию через SandboxCommand
    return genome_data
```

## 5.4 Тело пользователя

```cpp
/*
 * PLAYER BODY — физическое воплощение в мире
 *
 * Пользователь выбирает тип тела:
 *   HUMANOID_CONSTRUCT — нейтральное двуногое тело (~180см, безликое)
 *   NATIVE_SPECIES — тело доминирующего разумного вида
 *   CUSTOM — тело по описанию через CLAS
 *   ELEMENTAL — тело из чистой энергии (невидимо, но влияет на Ψ-поле)
 *
 * Тело подчиняется физике симуляции:
 *   - Нужно есть (энергетический баланс)
 *   - Подвержено болезням и травмам
 *   - Существа реагируют на пользователя как на часть экосистемы
 *   - Смерть: CLAS предлагает воплотиться в новое тело или просто
 *     вернуться в режим камеры
 *
 * Интерфейс управления:
 *   Клавиатура/мышь → PossessionIntent → NeuralInjection в тело пользователя
 *   VR (опционально): IMU → точное управление конечностями
 */
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 6 — РЕНДЕР (GODOT 4 + GDEXTENSION)
# ═══════════════════════════════════════════

## 6.1 GDExtension мост

```cpp
// renderer/addons/genesis_bridge/SimBridge.cpp

#include <godot_cpp/classes/node3d.hpp>
#include "../../shared/sim_state.h"

class SimBridge : public godot::Node3D {
    GDCLASS(SimBridge, Node3D)

    SimStateBuffer* shared_state = nullptr;
    godot::PackedByteArray particle_data_cache;

public:
    void _ready() override {
        shared_state = SharedMemoryBridge::create_or_attach("genesis_sim_state", false);
    }

    void _process(double delta) override {
        if (!shared_state->frame_ready.load(std::memory_order_acquire)) return;
        shared_state->frame_ready.store(false);

        int read = shared_state->read_idx.load();
        uint32_t count = shared_state->particle_count[read];

        // Передать данные частиц в MultiMeshInstance3D
        update_particle_multimesh(shared_state->particles[read], count);
        update_organism_visuals(shared_state->organisms[read],
                                shared_state->organism_count[read]);
    }

    static void _bind_methods() {
        godot::ClassDB::bind_method(godot::D_METHOD("get_sim_time"),
                                    &SimBridge::get_sim_time);
        godot::ClassDB::bind_method(godot::D_METHOD("send_sandbox_command"),
                                    &SimBridge::send_sandbox_command);
    }
};
```

## 6.2 LOD рендер

```gdscript
# renderer/scripts/world/ParticleRenderer.gd

extends Node3D

@export var lod_distances := [10.0, 100.0, 1000.0, 10000.0]

var particle_multimesh: MultiMesh
var organism_instances: Dictionary  # organism_id -> Node3D

func _process(delta):
    var cam_pos = get_viewport().get_camera_3d().global_position
    
    # LOD 0 (< 10m): рендер отдельных частиц с физическими размерами
    # LOD 1 (< 100m): рендер молекул как сфер
    # LOD 2 (< 1km): рендер организмов как LOD-мешей
    # LOD 3 (< 10km): рендер популяций как плотности (heat map)
    # LOD 4 (планета): рендер биомов как цветных регионов
    
    for organism in visible_organisms:
        var dist = cam_pos.distance_to(organism.position)
        organism.lod_level = get_lod_level(dist)
```

## 6.3 Процедурная генерация мешей существ

```
CODEX SUB-PROMPT:
"Implement ProcBodyMeshGenerator for Godot 4.
Input: OrganismHeader + Genome + MorphogenMap (from sim core)
Output: MeshInstance3D with correct topology

Algorithm:
  1. Read body_plan from genome (bilateral/radial/asymmetric symmetry, segment count)
  2. Generate base skeleton using L-system from HOX gene codes
  3. Apply morphogen map to scale each segment
  4. Generate skin mesh using Marching Cubes on cell density volume
  5. Assign materials based on cell types (skin, scale, chitin, fur) detected in genome
  6. Animate using blend shapes driven by muscle fiber data from sim

Cache generated meshes by genome_hash — only regenerate on mutation.
LOD: distance > 100m → use imposter billboard. Distance > 1km → colored dot."
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 7 — ПЛАНЕТАРНЫЙ ДВИЖОК
# ═══════════════════════════════════════════

## 7.1 Тектоника, климат, атмосфера

```cpp
/*
 * ПЛАНЕТАРНЫЙ ДВИЖОК — упрощённые но физически корректные модели
 *
 * ТЕКТОНИКА: решётка плит (50-200 штук). Каждый тик:
 *   - Плиты движутся по мантийным конвекционным ячейкам
 *   - Столкновение → горы (оро- и субдукционные пояса)
 *   - Расхождение → рифты, вулканизм, новая кора
 *   - Субдукция → вулканическая дуга
 *   - Геологический тик = 10,000 лет симуляции
 *
 * КЛИМАТ: упрощённая GCM (General Circulation Model)
 *   - Сетка 1° × 1° lat/lon × 32 вертикальных слоя
 *   - Уравнения примитивной атмосферной динамики (Navier-Stokes упрощённые)
 *   - Радиационный баланс: солнечная радиация (зависит от спектра звезды)
 *     + парниковый эффект (зависит от состава атмосферы из ElementRegistry)
 *   - Климатический тик = 1 год симуляции
 *
 * АТМОСФЕРА:
 *   - Состав определяется вулканической дегазацией + биологическими процессами
 *   - Если фотосинтезирующие организмы выделяют O₂ — O₂ накапливается
 *   - Если организмы выделяют CH₄ — парниковый эффект растёт
 *   - Фотохимия (UV от звезды) создаёт и разрушает молекулы в верхней атмосфере
 *
 * ГИДРОСФЕРА:
 *   - Круговорот воды: испарение → облака → осадки → реки → океан
 *   - Солёность океана изменяется от испарения и ледообразования
 *   - pH океана зависит от CO₂ (карбонатная система)
 */
```

## 7.2 Ψ-поле (аномалии)

```cpp
/*
 * Ψ-ПОЛЕ — параметр необъяснимого
 *
 * Физически: скалярное поле на поверхности планеты.
 * Влияние на симуляцию:
 *   - Мутационная ставка: p_mut *= (1 + psi * PSI_MUT_FACTOR)
 *   - Автокатализ: вероятность замыкания автокаталитической петли *= (1 + psi)
 *   - Нейральная сложность: синаптическая пластичность *= (1 + psi * PSI_NEURAL)
 *
 * Источники Ψ:
 *   - Базовый уровень (задаётся пользователем при создании планеты)
 *   - Геотермальные зоны генерируют Ψ (горячие источники = зоны жизни)
 *   - Грозовая активность генерирует локальные Ψ-всплески
 *   - Накопление нервной ткани усиливает локальное Ψ (мозг создаёт поле)
 *   - Пользователь может рисовать Ψ-паттерны на карте
 *   - Константа Когерентности: глобальный множитель Ψ для жизни
 *
 * CLAS может анализировать Ψ-карту и объяснять корреляции.
 */
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 8 — СИСТЕМА СОХРАНЕНИЙ И ОТКАТОВ
# ═══════════════════════════════════════════

```cpp
// sim_core/src/time/SnapshotManager.cpp

/*
 * SNAPSHOT SYSTEM — точные откаты без потери данных
 *
 * FULL SNAPSHOT (раз в 1M симулированных лет или по запросу):
 *   Сериализует весь SimStateBuffer + ChunkChemState + GeologyState
 *   Сжатие: LZ4 (быстро) → на диск. ~2-5 ГБ на снапшот.
 *
 * DELTA SNAPSHOT (каждые 1000 лет симуляции):
 *   Записывает только изменения относительно предыдущего полного снапшота.
 *   ~10-100 МБ на дельту. Хранится цепочка: full → Δ₁ → Δ₂ → ... → Δₙ
 *
 * TIMELINE BRANCHING:
 *   Откат к точке T создаёт "ветку реальности".
 *   Обе ветки живут параллельно (на диске, не в памяти — только одна активна).
 *   Пользователь может переключаться между ветками.
 *   CLAS показывает граф веток как дерево альтернативных историй.
 *
 * HYPOTHETICAL MODE:
 *   "Что будет если..." запускает копию текущего стейта в отдельном потоке.
 *   Основная симуляция продолжается. Гипотетическая симуляция считается
 *   в фоне. CLAS сравнивает результаты через N лет.
 *   Потребление памяти: +50% (два SimStateBuffer в памяти одновременно).
 *
 * АВТОСОХРАНЕНИЕ ключевых событий:
 *   - Первая автокаталитическая петля
 *   - Первая протоклетка
 *   - Первый прокариот
 *   - Появление ядра (эукариот)
 *   - Первый многоклеточный
 *   - Выход жизни на сушу
 *   - Первая нервная система
 *   - Первое орудие труда
 *   CLAS уведомляет о каждом: "Зафиксировано событие уровня [MAJOR]:
 *   появление первого многоклеточного организма. Снапшот сохранён."
 */
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 9 — ROADMAP РАЗРАБОТКИ
# ═══════════════════════════════════════════

## Фаза 0: Доказательство концепции (2D) — 1-3 месяца

```
ЦЕЛЬ: Доказать что эмерджентность работает ДО написания 3D движка.

Задачи:
  □ Написать 2D версию реактивной симуляции частиц (Python + NumPy или Rust)
  □ Реализовать 3 типа частиц: гидрофильный, гидрофобный, амфифильный
  □ Наблюдать самосборку мицелл и мембран (ДОЛЖНО возникнуть само)
  □ Добавить реактивный тип: A + B → A + C (автокатализ)
  □ Наблюдать возникновение автокаталитического цикла внутри мембраны
  □ Добавить примитивное деление (протоклетка делится при переполнении)
  □ Добавить случайные "мутации" молекулярных ставок при делении
  □ Наблюдать естественный отбор (клетки с лучшим катализом доминируют)
  
УСПЕХ = наблюдение дарвиновской динамики без единой строки про "эволюцию"
Если это не работает в 2D с ~100k частиц → нужно пересмотреть химию.

СТЕК ФАЗЫ 0: Python + NumPy/Numba или Rust, рендер через matplotlib/SDL2
```

## Фаза 1: C++ ядро симуляции — 3-12 месяцев

```
ЦЕЛИ: Переписать на C++/CUDA, добавить 3D, масштаб до 10M частиц.

Приоритет 1 (месяц 1-2):
  □ SharedMemoryBridge (IPC между процессами)
  □ ElementRegistry с реальными элементами 1-118 из JSON
  □ Базовая EDMD (событийная молекулярная динамика)
  □ SleepScheduler (спящие частицы)
  □ CUDA ядро для параллельных реакций

Приоритет 2 (месяц 3-5):
  □ GillespieSSA для точной химии
  □ MultiResolutionClock (разные временные шкалы)
  □ SpatialHash для быстрого поиска соседей
  □ 3D мембранная физика (амфифильные молекулы в 3D)
  □ SnapshotManager (базовые сохранения)

Приоритет 3 (месяц 6-9):
  □ GenomeEngine (граф регуляции, мутации)
  □ MorphogenEngine (морфогенез в 3D)
  □ Протоклеточный детектор
  □ Базовая нейральная эволюция (возбудимые мембраны)

Приоритет 4 (месяц 10-12):
  □ Планетарный движок (тектоника, климат, атмосфера)
  □ LOD менеджер (популяционная генетика для дальних зон)
  □ Ψ-поле
  □ Полные сохранения с дельтами
```

## Фаза 2: Godot 4 + C.L.A.S — 6-18 месяцев

```
ЦЕЛИ: Визуализация, интерфейс, AI-ассистент.

Приоритет 1:
  □ GDExtension SimBridge (чтение из shared memory)
  □ MultiMesh рендер частиц (до 10M на GPU)
  □ LOD рендер (частицы → молекулы → организмы → биомы)
  □ Планетарный рендер (атмосфера, облака, океан)

Приоритет 2:
  □ C.L.A.S backend (Python FastAPI + WebSocket)
  □ C.L.A.S UI в Godot (голосовой + текстовый интерфейс)
  □ Модуль анализа ДНК
  □ Модуль анализа строения
  □ Базовые SandboxCommands (создать элемент, создать организм)

Приоритет 3:
  □ ProcBodyMeshGenerator (процедурные меши из геномов)
  □ PossessionMode (вселение в существо)
  □ GenomeEditor (визуальный редактор генома)
  □ Timeline browser (навигация по истории)

Приоритет 4:
  □ Модуль советника по эволюции
  □ Codex of Life (автоматический дневник открытий)
  □ CreateFromDescription (существа из текстового описания)
  □ PlayerBody
  □ BranchTimeline UI (дерево альтернативных историй)
```

## Фаза 3: Полный масштаб + шлифовка — 12-24 месяцев

```
  □ Мультипланетная симуляция (несколько планет в одной системе)
  □ Панспермия (жизнь переносится астероидами между планетами)
  □ Разумные виды: технологии, язык, цивилизация
  □ Диалоговая система с разумными существами (через C.L.A.S)
  □ VR режим
  □ Экспорт Codex of Life как книги/PDF
  □ Мультиплеер-наблюдение (несколько пользователей в одной симуляции)
  □ Пользовательские моды (API для кастомных хим. правил)
```

---

# ═══════════════════════════════════════════
# ЧАСТЬ 10 — ОПТИМИЗАЦИИ (ПОЛНЫЙ СПИСОК)
# ═══════════════════════════════════════════

```
1. SLEEPING PARTICLES (реализовано выше):
   Частицы в состоянии покоя без реактивных соседей → статичные.
   Wake-up по событию. Экономия: ~60-80% вычислений для "мёртвой" материи.

2. STATIC GEOMETRY:
   Инертные породы на большой глубине → компактный spatial hash, не particle buffer.
   Instantiate только при плавлении или реактивном контакте.

3. MEMBRANE RIGID BODY:
   Закрытая липидная мембрана → treated as rigid body.
   Только мембранные частицы считаются полностью, внутренняя химия — Gillespie.

4. POPULATION LOD:
   Организмы вне зоны наблюдения → статистическая модель (Райт-Фишер).
   Materialize в индивидов при приближении камеры.

5. CUDA BATCH INFERENCE:
   Все нейросети организмов в активной зоне → один GPU batch.
   ~10,000 организмов параллельно на RTX 4090.

6. REACTION-DIFFUSION ON GPU:
   Диффузия молекул между чанками → один CUDA kernel на всю планету.
   Поставленная задача: 512³ чанков × N химических видов.

7. ADAPTIVE TIME STEPPING:
   Если в регионе ничего не происходит (низкий a_total в Gillespie) →
   шаг автоматически увеличивается. Высокая активность → маленький шаг.

8. GENOME CACHE:
   Геномы с одинаковым hash → разделяемая вычислительная модель.
   Клоны или идентичные особи не пересчитывают морфогенез.

9. IMPOSTOR BILLBOARDS:
   Организмы дальше 200м → billboard texture.
   Только 6 LOD-уровней мешей для близких дистанций.

10. CHUNK SLEEP:
    Чанки без активных частиц и реакций → полностью отключаются.
    Wake-up при диффузии активных молекул с соседних чанков.

11. DELTA SNAPSHOT COMPRESSION:
    Дельты хранятся как sparse diff → только изменившиеся поля.
    LZ4 компрессия для быстрой записи без потери.

12. NEURAL PRUNING:
    Синапсы с весом < PRUNE_THRESHOLD и нулевой активностью → удаляются.
    Биологически корректно (нейронная пластика) и ускоряет inference.
```

---

# ═══════════════════════════════════════════
# БОНУС — ИДЕИ ДЛЯ УЛУЧШЕНИЯ ПРОЕКТА
# ═══════════════════════════════════════════

```
НОВЫЕ МЕХАНИКИ:

1. СИМБИОЗ И ЭНДОСИМБИОЗ:
   Два организма могут слиться если их химия совместима →
   один становится органеллой другого. Именно так возникли митохондрии.
   Требует: OrganismMergeEngine.

2. ГОРИЗОНТАЛЬНЫЙ ПЕРЕНОС ГЕНОВ (ГПГ):
   У прокариот: гены могут передаваться напрямую соседу (конъюгация).
   Делает бактериальную эволюцию сетью, а не деревом.
   Делает антибиотикорезистентность возможной без мутаций.

3. ЭПИГЕНЕТИКА:
   Химические метки на геноме (не меняют ДНК, меняют экспрессию).
   Наследуются 2-3 поколения → "ламаркистский" эффект без нарушения Дарвина.

4. ЭКОСИСТЕМНЫЕ ИНЖЕНЕРЫ:
   Бобры строят плотины → меняют гидрологию.
   Черви рыхлят почву → меняют доступность минералов.
   Реализуется: если организм производит физически прочные молекулы (хитин,
   целлюлоза, карбонат кальция) → они остаются в мире как объекты после смерти.

5. ВИРУСЫ:
   Возникают сами: молекулы, которые "научились" встраиваться в чужие геномы
   и копировать себя. Появятся естественно как паразитические ретроэлементы.

6. ЦИВИЛИЗАЦИОННЫЙ ДВИЖОК:
   Когда разумный вид достигает определённого IQ-порога (размер нейросети /
   информационная сложность):
   - Появляется технологическое дерево (не скриптованное — они сами исследуют)
   - CLAS переходит в режим "дипломата" и может вести диалог с видом
   - Пользователь может обучать их языку, математике, астрономии

7. МНОГОPLANETНАЯ ПАНСПЕРМИЯ:
   Астероидный удар → выброс материала с жизнью → посев соседней планеты.
   Одно дерево жизни, разные эволюционные пути на разных планетах.

8. ТЕМНАЯ БИОСФЕРА:
   Живые организмы в подземных водах / под поверхностным льдом.
   Не видны с поверхности — CLAS может их обнаружить через геохимические
   сигнатуры (необычный изотопный состав пород).

9. РАДИОСИНТЕЗ:
   Если добавить высокорадиоактивный элемент → организмы могут эволюционировать
   для поглощения радиации как источника энергии. Реальный феномен (грибы в Чернобыле).

10. МАГНИТНАЯ КОММУНИКАЦИЯ:
    Если планета имеет сильную магнитосферу → разумные виды могут эволюционировать
    для модуляции магнитного поля как языка. CLAS показывает их "речь" через
    визуализатор поля.

11. КОЛЛЕКТИВНЫЙ РАЗУМ:
    Колониальные организмы (муравьи, термиты, корабль-сифонофор) → распределённый
    "мозг" из простых агентов. Требует: ColonyIntelligenceEngine.

12. ГАЗОВЫЕ ФОРМЫ ЖИЗНИ:
    В плотной атмосфере (высокое давление, специфические элементы) → организмы
    удерживающие форму электростатикой. Нет твёрдого тела → другая физика.
```

---

# КОНЕЦ ДОКУМЕНТА
# Версия 1.0 | Genesis Engine | Codex Master Prompt
# Для работы с конкретным модулем — вставить соответствующую секцию в Codex
