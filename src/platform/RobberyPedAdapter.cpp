#include "RobberyPedAdapter.h"

#include <main.h>
#include <natives.h>

#include <cmath>

namespace gco::platform {
namespace {

bool validEntity(const EntityHandle entity) {
    return entity != 0 && ENTITY::DOES_ENTITY_EXIST(entity) != FALSE;
}

bool validPed(const PedHandle ped) {
    return validEntity(ped) && ENTITY::IS_ENTITY_A_PED(ped) != FALSE;
}

} // namespace

bool NativeRobberyPedAdapter::playerFreeAimingAt(const EntityHandle target) const {
    if (!validEntity(target)) return false;
    Entity aimed = 0;
    const BOOL hit = PLAYER::GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(PLAYER::PLAYER_ID(), &aimed);
    return hit != FALSE && aimed == target;
}

bool NativeRobberyPedAdapter::missionEntity(const EntityHandle entity) const {
    return validEntity(entity) && ENTITY::IS_ENTITY_A_MISSION_ENTITY(entity) != FALSE;
}

bool NativeRobberyPedAdapter::pedArmed(const PedHandle ped) const {
    // Native reference: 1=melee, 2=explosive, 4=other weapons; 7 checks any weapon except fists.
    return validPed(ped) && WEAPON::IS_PED_ARMED(ped, 7) != FALSE;
}

bool NativeRobberyPedAdapter::handsUp(
    const PedHandle ped,
    const PedHandle facingPed,
    const int durationMs) const {

    if (!validPed(ped) || !validPed(facingPed)) return false;
    TASK::TASK_HANDS_UP(ped, durationMs, facingPed, 500, 0);
    return true;
}

bool NativeRobberyPedAdapter::cower(const PedHandle ped, const int durationMs) const {
    if (!validPed(ped)) return false;
    TASK::TASK_COWER(ped, durationMs);
    return true;
}

bool NativeRobberyPedAdapter::fleeFrom(
    const PedHandle ped,
    const PedHandle target,
    const float safeDistance,
    const int durationMs) const {

    if (!validPed(ped) || !validPed(target) || !std::isfinite(safeDistance) || safeDistance <= 0.0f) {
        return false;
    }
    TASK::TASK_SMART_FLEE_PED(ped, target, safeDistance, durationMs, TRUE, FALSE);
    return true;
}

bool NativeRobberyPedAdapter::combatPed(const PedHandle ped, const PedHandle target) const {
    if (!validPed(ped) || !validPed(target)) return false;
    // Native reference documents combatFlags=0 and threatResponseFlags=16 for standard attack behavior.
    TASK::TASK_COMBAT_PED(ped, target, 0, 16);
    return true;
}

bool NativeRobberyPedAdapter::goStraightTo(
    const PedHandle ped,
    const Vec3& target,
    const float speed,
    const float heading) const {

    if (!validPed(ped)
        || !std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z)
        || !std::isfinite(speed) || speed <= 0.0f || !std::isfinite(heading)) {
        return false;
    }
    TASK::TASK_GO_STRAIGHT_TO_COORD(
        ped,
        target.x, target.y, target.z,
        speed,
        -1,
        heading,
        0.5f);
    return true;
}

bool NativeRobberyPedAdapter::turnToCoord(
    const PedHandle ped,
    const Vec3& target,
    const int durationMs) const {

    if (!validPed(ped)
        || !std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z)) {
        return false;
    }
    TASK::TASK_TURN_PED_TO_FACE_COORD(ped, target.x, target.y, target.z, durationMs);
    return true;
}

bool NativeRobberyPedAdapter::clearTasks(const PedHandle ped, const bool immediate) const {
    if (!validPed(ped)) return false;
    if (immediate) TASK::CLEAR_PED_TASKS_IMMEDIATELY(ped);
    else TASK::CLEAR_PED_TASKS(ped);
    return true;
}

} // namespace gco::platform
