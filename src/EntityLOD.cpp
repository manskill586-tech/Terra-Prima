#include "terra/world/EntityLOD.h"

namespace terra::world {

void EntityLOD::PromoteToNear(uint64_t id) {
  auto it = mid_.find(id);
  if (it != mid_.end()) {
    near_[id] = it->second;
    mid_.erase(it);
    return;
  }
  it = far_.find(id);
  if (it != far_.end()) {
    near_[id] = it->second;
    far_.erase(it);
  }
}

void EntityLOD::DemoteToMid(uint64_t id) {
  auto it = near_.find(id);
  if (it != near_.end()) {
    mid_[id] = it->second;
    near_.erase(it);
  }
}

void EntityLOD::DemoteToFar(uint64_t id) {
  auto it = mid_.find(id);
  if (it != mid_.end()) {
    far_[id] = it->second;
    mid_.erase(it);
  }
}

void EntityLOD::Update(float deltaTime) {
  for (auto& [id, entity] : near_) {
    entity.age += deltaTime;
    entity.skill += deltaTime * 0.01f;
  }
  for (auto& [id, entity] : mid_) {
    entity.age += deltaTime;
  }
}

std::size_t EntityLOD::NearCount() const {
  return near_.size();
}

std::size_t EntityLOD::MidCount() const {
  return mid_.size();
}

std::size_t EntityLOD::FarCount() const {
  return far_.size();
}

EntityState& EntityLOD::GetOrCreate(uint64_t id) {
  auto it = near_.find(id);
  if (it != near_.end()) {
    return it->second;
  }
  it = mid_.find(id);
  if (it != mid_.end()) {
    return it->second;
  }
  it = far_.find(id);
  if (it != far_.end()) {
    return it->second;
  }
  return far_.emplace(id, EntityState{id}).first->second;
}

} // namespace terra::world
