#include "GenesisBridge.h"

#include "shared/sim_state.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <algorithm>
#include <cstring>

namespace genesis::bridge {

namespace {

godot::Color UnpackColor(uint32_t rgba) {
  const float r = static_cast<float>(rgba & 0xFF) / 255.0f;
  const float g = static_cast<float>((rgba >> 8) & 0xFF) / 255.0f;
  const float b = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
  const float a = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
  return godot::Color(r, g, b, a);
}

godot::Vector3 ChunkToWorld(const shared::ChunkData& chunk, int chunk_size) {
  const float size = static_cast<float>(chunk_size);
  return godot::Vector3((static_cast<float>(chunk.coord.x) + 0.5f) * size,
                        (static_cast<float>(chunk.coord.y) + 0.5f) * size,
                        (static_cast<float>(chunk.coord.z) + 0.5f) * size);
}

} // namespace

bool GenesisBridge::open(const godot::String& name) {
  if (name.is_empty()) {
    shm_name_ = shared::kDefaultShmName;
  } else {
    const godot::CharString utf8 = name.utf8();
    shm_name_ = utf8.get_data();
  }
  const std::string desired = shm_name_;
  shm_name_ = desired;
  connected_ = shm_.Open(desired);
  return connected_;
}

bool GenesisBridge::poll() {
  if (!connected_) {
    if (shm_name_.empty()) {
      shm_name_ = shared::kDefaultShmName;
    }
    connected_ = shm_.Open(shm_name_);
    if (!connected_) {
      return false;
    }
  }

  const shared::SimStateBuffer* buffer = shm_.Buffer();
  uint64_t frame_id = 0;
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (shared::TryReadSnapshot(buffer, &snapshot_, &frame_id)) {
      last_frame_id_ = frame_id;
      sim_time_us_ = buffer->header.sim_time_us;
      return true;
    }
  }
  // Fallback: copy last completed buffer without strict seqlock to keep UI smooth.
  const uint32_t r = buffer->header.read_idx.load(std::memory_order_acquire);
  std::memcpy(&snapshot_, &buffer->buffers[r], sizeof(shared::WorldState));
  frame_id = buffer->header.frame_id.load(std::memory_order_acquire);
  last_frame_id_ = frame_id;
  sim_time_us_ = buffer->header.sim_time_us;
  return true;
}

int GenesisBridge::get_particle_count() const {
  return static_cast<int>(snapshot_.near_particles.count);
}

godot::PackedVector3Array GenesisBridge::get_positions() const {
  const int count = get_particle_count();
  godot::PackedVector3Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = godot::Vector3(snapshot_.near_particles.px[i],
                            snapshot_.near_particles.py[i],
                            snapshot_.near_particles.pz[i]);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_particle_colors() const {
  const int count = get_particle_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = UnpackColor(snapshot_.near_particles.color_rgba[i]);
  }
  return arr;
}

double GenesisBridge::get_sim_time_seconds() const {
  return static_cast<double>(sim_time_us_) / 1'000'000.0;
}

bool GenesisBridge::is_connected() const {
  return connected_;
}

bool GenesisBridge::is_shm_connected() const {
  return connected_;
}

void GenesisBridge::set_name(const godot::String& name) {
  const godot::CharString utf8 = name.utf8();
  shm_name_ = utf8.get_data();
}

godot::String GenesisBridge::get_name() const {
  return godot::String(shm_name_.c_str());
}

int GenesisBridge::get_near_chunk_count() const {
  return static_cast<int>(snapshot_.near_chunk_count);
}

int GenesisBridge::get_mid_chunk_count() const {
  return static_cast<int>(snapshot_.mid_chunk_count);
}

int GenesisBridge::get_far_chunk_count() const {
  return static_cast<int>(snapshot_.far_chunk_count);
}

godot::PackedVector3Array GenesisBridge::get_near_chunk_positions() const {
  const int count = get_near_chunk_count();
  godot::PackedVector3Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = ChunkToWorld(snapshot_.near_chunks[i], 2);
  }
  return arr;
}

godot::PackedVector3Array GenesisBridge::get_mid_chunk_positions() const {
  const int count = get_mid_chunk_count();
  godot::PackedVector3Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = ChunkToWorld(snapshot_.mid_chunks[i], 32);
  }
  return arr;
}

godot::PackedVector3Array GenesisBridge::get_far_chunk_positions() const {
  const int count = get_far_chunk_count();
  godot::PackedVector3Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = ChunkToWorld(snapshot_.far_chunks[i], 256);
  }
  return arr;
}

godot::PackedInt32Array GenesisBridge::get_near_chunk_phases() const {
  const int count = get_near_chunk_count();
  godot::PackedInt32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = static_cast<int>(snapshot_.near_chunks[i].phase);
  }
  return arr;
}

godot::PackedInt32Array GenesisBridge::get_mid_chunk_phases() const {
  const int count = get_mid_chunk_count();
  godot::PackedInt32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = static_cast<int>(snapshot_.mid_chunks[i].phase);
  }
  return arr;
}

godot::PackedInt32Array GenesisBridge::get_far_chunk_phases() const {
  const int count = get_far_chunk_count();
  godot::PackedInt32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = static_cast<int>(snapshot_.far_chunks[i].phase);
  }
  return arr;
}

godot::PackedInt32Array GenesisBridge::get_near_chunk_species() const {
  const int count = get_near_chunk_count();
  godot::PackedInt32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = static_cast<int>(snapshot_.near_chunks[i].dominant_species_id);
  }
  return arr;
}

godot::PackedInt32Array GenesisBridge::get_mid_chunk_species() const {
  const int count = get_mid_chunk_count();
  godot::PackedInt32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = static_cast<int>(snapshot_.mid_chunks[i].dominant_species_id);
  }
  return arr;
}

godot::PackedInt32Array GenesisBridge::get_far_chunk_species() const {
  const int count = get_far_chunk_count();
  godot::PackedInt32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = static_cast<int>(snapshot_.far_chunks[i].dominant_species_id);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_near_chunk_colors() const {
  const int count = get_near_chunk_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = UnpackColor(snapshot_.near_chunks[i].albedo_rgba);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_mid_chunk_colors() const {
  const int count = get_mid_chunk_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = UnpackColor(snapshot_.mid_chunks[i].albedo_rgba);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_far_chunk_colors() const {
  const int count = get_far_chunk_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = UnpackColor(snapshot_.far_chunks[i].albedo_rgba);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_near_chunk_custom() const {
  const int count = get_near_chunk_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    const auto& chunk = snapshot_.near_chunks[i];
    const float pressure_norm = std::clamp(chunk.pressure / 200000.0f, 0.0f, 1.0f);
    arr[i] = godot::Color(chunk.opacity, chunk.roughness, chunk.metallic, pressure_norm);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_mid_chunk_custom() const {
  const int count = get_mid_chunk_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    const auto& chunk = snapshot_.mid_chunks[i];
    const float pressure_norm = std::clamp(chunk.pressure / 200000.0f, 0.0f, 1.0f);
    arr[i] = godot::Color(chunk.opacity, chunk.roughness, chunk.metallic, pressure_norm);
  }
  return arr;
}

godot::PackedColorArray GenesisBridge::get_far_chunk_custom() const {
  const int count = get_far_chunk_count();
  godot::PackedColorArray arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    const auto& chunk = snapshot_.far_chunks[i];
    const float pressure_norm = std::clamp(chunk.pressure / 200000.0f, 0.0f, 1.0f);
    arr[i] = godot::Color(chunk.opacity, chunk.roughness, chunk.metallic, pressure_norm);
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_near_chunk_hardness() const {
  const int count = get_near_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = snapshot_.near_chunks[i].hardness;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_mid_chunk_hardness() const {
  const int count = get_mid_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = snapshot_.mid_chunks[i].hardness;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_far_chunk_hardness() const {
  const int count = get_far_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = snapshot_.far_chunks[i].hardness;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_near_chunk_scale() const {
  const int count = get_near_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = snapshot_.near_chunks[i].scale;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_mid_chunk_scale() const {
  const int count = get_mid_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = snapshot_.mid_chunks[i].scale;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_far_chunk_scale() const {
  const int count = get_far_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count);
  for (int i = 0; i < count; ++i) {
    arr[i] = snapshot_.far_chunks[i].scale;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_near_chunk_tph() const {
  const int count = get_near_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count * 3);
  for (int i = 0; i < count; ++i) {
    const auto& chunk = snapshot_.near_chunks[i];
    const int base = i * 3;
    arr[base + 0] = chunk.temperature;
    arr[base + 1] = chunk.pressure;
    arr[base + 2] = chunk.humidity;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_mid_chunk_tph() const {
  const int count = get_mid_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count * 3);
  for (int i = 0; i < count; ++i) {
    const auto& chunk = snapshot_.mid_chunks[i];
    const int base = i * 3;
    arr[base + 0] = chunk.temperature;
    arr[base + 1] = chunk.pressure;
    arr[base + 2] = chunk.humidity;
  }
  return arr;
}

godot::PackedFloat32Array GenesisBridge::get_far_chunk_tph() const {
  const int count = get_far_chunk_count();
  godot::PackedFloat32Array arr;
  arr.resize(count * 3);
  for (int i = 0; i < count; ++i) {
    const auto& chunk = snapshot_.far_chunks[i];
    const int base = i * 3;
    arr[base + 0] = chunk.temperature;
    arr[base + 1] = chunk.pressure;
    arr[base + 2] = chunk.humidity;
  }
  return arr;
}

void GenesisBridge::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("open", "name"), &GenesisBridge::open);
  godot::ClassDB::bind_method(godot::D_METHOD("poll"), &GenesisBridge::poll);
  godot::ClassDB::bind_method(godot::D_METHOD("get_particle_count"), &GenesisBridge::get_particle_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_positions"), &GenesisBridge::get_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("get_particle_colors"), &GenesisBridge::get_particle_colors);
  godot::ClassDB::bind_method(godot::D_METHOD("get_sim_time_seconds"), &GenesisBridge::get_sim_time_seconds);
  godot::ClassDB::bind_method(godot::D_METHOD("is_connected"), &GenesisBridge::is_connected);
  godot::ClassDB::bind_method(godot::D_METHOD("is_shm_connected"), &GenesisBridge::is_shm_connected);
  godot::ClassDB::bind_method(godot::D_METHOD("set_name", "name"), &GenesisBridge::set_name);
  godot::ClassDB::bind_method(godot::D_METHOD("get_name"), &GenesisBridge::get_name);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_count"), &GenesisBridge::get_near_chunk_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_count"), &GenesisBridge::get_mid_chunk_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_count"), &GenesisBridge::get_far_chunk_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_positions"), &GenesisBridge::get_near_chunk_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_positions"), &GenesisBridge::get_mid_chunk_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_positions"), &GenesisBridge::get_far_chunk_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_phases"), &GenesisBridge::get_near_chunk_phases);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_phases"), &GenesisBridge::get_mid_chunk_phases);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_phases"), &GenesisBridge::get_far_chunk_phases);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_species"), &GenesisBridge::get_near_chunk_species);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_species"), &GenesisBridge::get_mid_chunk_species);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_species"), &GenesisBridge::get_far_chunk_species);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_colors"), &GenesisBridge::get_near_chunk_colors);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_colors"), &GenesisBridge::get_mid_chunk_colors);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_colors"), &GenesisBridge::get_far_chunk_colors);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_custom"), &GenesisBridge::get_near_chunk_custom);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_custom"), &GenesisBridge::get_mid_chunk_custom);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_custom"), &GenesisBridge::get_far_chunk_custom);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_hardness"), &GenesisBridge::get_near_chunk_hardness);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_hardness"), &GenesisBridge::get_mid_chunk_hardness);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_hardness"), &GenesisBridge::get_far_chunk_hardness);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_scale"), &GenesisBridge::get_near_chunk_scale);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_scale"), &GenesisBridge::get_mid_chunk_scale);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_scale"), &GenesisBridge::get_far_chunk_scale);
  godot::ClassDB::bind_method(godot::D_METHOD("get_near_chunk_tph"), &GenesisBridge::get_near_chunk_tph);
  godot::ClassDB::bind_method(godot::D_METHOD("get_mid_chunk_tph"), &GenesisBridge::get_mid_chunk_tph);
  godot::ClassDB::bind_method(godot::D_METHOD("get_far_chunk_tph"), &GenesisBridge::get_far_chunk_tph);
  godot::ClassDB::add_property("GenesisBridge",
                               godot::PropertyInfo(godot::Variant::STRING, "name"),
                               "set_name", "get_name");
}

} // namespace genesis::bridge
