#include "PlatformTypes.h"

#include <cmath>

namespace gco::platform {

GridCell gridCellFor(const Vec3& position, const float cellSize) noexcept {
    if (!(cellSize > 0.0f) || !std::isfinite(cellSize)) {
        return {};
    }

    return {
        static_cast<int>(std::floor(position.x / cellSize)),
        static_cast<int>(std::floor(position.y / cellSize)),
        static_cast<int>(std::floor(position.z / cellSize))
    };
}

std::string_view pedGenderName(const PedGender gender) noexcept {
    switch (gender) {
    case PedGender::Unknown: return "Unknown";
    case PedGender::Masculine: return "Masculine";
    case PedGender::Feminine: return "Feminine";
    }
    return "Unknown";
}

std::string_view lineOfSightProfileName(const LineOfSightProfile profile) noexcept {
    switch (profile) {
    case LineOfSightProfile::DefaultVisibility: return "DefaultVisibility";
    case LineOfSightProfile::Count: break;
    }
    return "Unknown";
}

std::string_view inputActionName(const InputAction action) noexcept {
    switch (action) {
    case InputAction::Interact: return "Interact";
    case InputAction::Cancel: return "Cancel";
    case InputAction::Sprint: return "Sprint";
    case InputAction::Aim: return "Aim";
    case InputAction::Attack: return "Attack";
    case InputAction::EnterVehicle: return "EnterVehicle";
    case InputAction::Count: break;
    }
    return "Unknown";
}

bool OwnedObjectTracker::track(const ObjectHandle object) {
    if (object == 0 || contains(object)) {
        return false;
    }
    objects_.push_back(object);
    return true;
}

bool OwnedObjectTracker::untrack(const ObjectHandle object) {
    const auto found = std::find(objects_.begin(), objects_.end(), object);
    if (found == objects_.end()) {
        return false;
    }
    objects_.erase(found);
    return true;
}

bool OwnedObjectTracker::contains(const ObjectHandle object) const noexcept {
    return object != 0 && std::find(objects_.begin(), objects_.end(), object) != objects_.end();
}

std::vector<ObjectHandle> OwnedObjectTracker::takeAll() {
    std::vector<ObjectHandle> result;
    result.swap(objects_);
    return result;
}

} // namespace gco::platform
