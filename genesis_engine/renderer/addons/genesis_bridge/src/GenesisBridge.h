#pragma once

#include "BridgeShm.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace genesis::bridge {

class GenesisBridge : public godot::Node {
  GDCLASS(GenesisBridge, godot::Node);

public:
  GenesisBridge() = default;
  ~GenesisBridge() override = default;

  bool open(const godot::String& name);
  bool poll();
  int get_particle_count() const;
  godot::PackedVector3Array get_positions() const;
  godot::PackedColorArray get_particle_colors() const;
  double get_sim_time_seconds() const;
  bool is_connected() const;
  bool is_shm_connected() const;
  void set_name(const godot::String& name);
  godot::String get_name() const;

  int get_near_chunk_count() const;
  int get_mid_chunk_count() const;
  int get_far_chunk_count() const;

  godot::PackedVector3Array get_near_chunk_positions() const;
  godot::PackedVector3Array get_mid_chunk_positions() const;
  godot::PackedVector3Array get_far_chunk_positions() const;

  godot::PackedInt32Array get_near_chunk_phases() const;
  godot::PackedInt32Array get_mid_chunk_phases() const;
  godot::PackedInt32Array get_far_chunk_phases() const;

  godot::PackedInt32Array get_near_chunk_species() const;
  godot::PackedInt32Array get_mid_chunk_species() const;
  godot::PackedInt32Array get_far_chunk_species() const;

  godot::PackedColorArray get_near_chunk_colors() const;
  godot::PackedColorArray get_mid_chunk_colors() const;
  godot::PackedColorArray get_far_chunk_colors() const;

  godot::PackedColorArray get_near_chunk_custom() const;
  godot::PackedColorArray get_mid_chunk_custom() const;
  godot::PackedColorArray get_far_chunk_custom() const;

  godot::PackedFloat32Array get_near_chunk_hardness() const;
  godot::PackedFloat32Array get_mid_chunk_hardness() const;
  godot::PackedFloat32Array get_far_chunk_hardness() const;

  godot::PackedFloat32Array get_near_chunk_scale() const;
  godot::PackedFloat32Array get_mid_chunk_scale() const;
  godot::PackedFloat32Array get_far_chunk_scale() const;

  godot::PackedFloat32Array get_near_chunk_tph() const;
  godot::PackedFloat32Array get_mid_chunk_tph() const;
  godot::PackedFloat32Array get_far_chunk_tph() const;

protected:
  static void _bind_methods();

private:
  std::string shm_name_;
  bool connected_{false};
  uint64_t last_frame_id_{0};
  uint64_t sim_time_us_{0};
  shared::WorldState snapshot_{};
  BridgeShm shm_;
};

} // namespace genesis::bridge
