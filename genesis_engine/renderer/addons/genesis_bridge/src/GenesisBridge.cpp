#include "GenesisBridge.h"

#include "shared/sim_state.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace genesis::bridge {

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
  if (!shared::TryReadSnapshot(buffer, &snapshot_, &frame_id)) {
    return false;
  }
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

double GenesisBridge::get_sim_time_seconds() const {
  return static_cast<double>(sim_time_us_) / 1'000'000.0;
}

bool GenesisBridge::is_connected() const {
  return connected_;
}

void GenesisBridge::set_name(const godot::String& name) {
  const godot::CharString utf8 = name.utf8();
  shm_name_ = utf8.get_data();
}

godot::String GenesisBridge::get_name() const {
  return godot::String(shm_name_.c_str());
}

void GenesisBridge::_bind_methods() {
  godot::ClassDB::bind_method(godot::D_METHOD("open", "name"), &GenesisBridge::open);
  godot::ClassDB::bind_method(godot::D_METHOD("poll"), &GenesisBridge::poll);
  godot::ClassDB::bind_method(godot::D_METHOD("get_particle_count"), &GenesisBridge::get_particle_count);
  godot::ClassDB::bind_method(godot::D_METHOD("get_positions"), &GenesisBridge::get_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("get_sim_time_seconds"), &GenesisBridge::get_sim_time_seconds);
  godot::ClassDB::bind_method(godot::D_METHOD("is_connected"), &GenesisBridge::is_connected);
  godot::ClassDB::bind_method(godot::D_METHOD("set_name", "name"), &GenesisBridge::set_name);
  godot::ClassDB::bind_method(godot::D_METHOD("get_name"), &GenesisBridge::get_name);
  godot::ClassDB::add_property("GenesisBridge",
                               godot::PropertyInfo(godot::Variant::STRING, "name"),
                               "set_name", "get_name");
}

} // namespace genesis::bridge
