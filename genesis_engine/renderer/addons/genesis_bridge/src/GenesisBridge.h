#pragma once

#include "BridgeShm.h"

#include <godot_cpp/classes/node.hpp>
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
  double get_sim_time_seconds() const;
  bool is_connected() const;
  void set_name(const godot::String& name);
  godot::String get_name() const;

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
