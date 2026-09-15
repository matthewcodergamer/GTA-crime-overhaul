#include "WitnessPerceptionAdapter.h"

#include <main.h>
#include <natives.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>

namespace gco::platform {
namespace {

bool validEntity(const EntityHandle entity) {
    return entity != 0 && ENTITY::DOES_ENTITY_EXIST(entity) != FALSE;
}

bool validPed(const PedHandle ped) {
    return validEntity(ped) && ENTITY::IS_ENTITY_A_PED(ped) != FALSE;
}

constexpr std::uint32_t joaat(const std::string_view text) noexcept {
    std::uint32_t hash = 0;
    for (const char raw : text) {
        const unsigned char ch = static_cast<unsigned char>(raw >= 'A' && raw <= 'Z' ? raw - 'A' + 'a' : raw);
        hash += ch;
        hash += hash << 10U;
        hash ^= hash >> 6U;
    }
    hash += hash << 3U;
    hash ^= hash >> 11U;
    hash += hash << 15U;
    return hash;
}

PerceivedWeaponClass classifyGroup(const std::uint32_t group) noexcept {
    if (group == joaat("GROUP_UNARMED")) return PerceivedWeaponClass::Unarmed;
    if (group == joaat("GROUP_MELEE")) return PerceivedWeaponClass::Melee;
    if (group == joaat("GROUP_PISTOL")) return PerceivedWeaponClass::Handgun;
    if (group == joaat("GROUP_SMG")) return PerceivedWeaponClass::Smg;
    if (group == joaat("GROUP_SHOTGUN")) return PerceivedWeaponClass::Shotgun;
    if (group == joaat("GROUP_RIFLE")) return PerceivedWeaponClass::Rifle;
    if (group == joaat("GROUP_MG")) return PerceivedWeaponClass::MachineGun;
    if (group == joaat("GROUP_SNIPER")) return PerceivedWeaponClass::Sniper;
    if (group == joaat("GROUP_HEAVY")) return PerceivedWeaponClass::Heavy;
    if (group == joaat("GROUP_THROWN")) return PerceivedWeaponClass::Thrown;
    return PerceivedWeaponClass::Unknown;
}

} // namespace

bool NativeWitnessPerceptionAdapter::playerShooting() const {
    const PedHandle player = PLAYER::PLAYER_PED_ID();
    return validPed(player) && PED::IS_PED_SHOOTING(player) != FALSE;
}

bool NativeWitnessPerceptionAdapter::entityDamagedBy(
    const EntityHandle entity,
    const EntityHandle attacker) const {

    return validEntity(entity) && validEntity(attacker)
        && ENTITY::HAS_ENTITY_BEEN_DAMAGED_BY_ENTITY(entity, attacker, TRUE) != FALSE;
}

std::optional<VehicleHandle> NativeWitnessPerceptionAdapter::vehiclePedIsUsing(const PedHandle ped) const {
    if (!validPed(ped) || PED::IS_PED_IN_ANY_VEHICLE(ped, FALSE) == FALSE) return std::nullopt;
    const VehicleHandle vehicle = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
    if (!validEntity(vehicle) || ENTITY::IS_ENTITY_A_VEHICLE(vehicle) == FALSE) return std::nullopt;
    return vehicle;
}

PerceivedWeaponClass NativeWitnessPerceptionAdapter::selectedWeaponClass(const PedHandle ped) const {
    if (!validPed(ped)) return PerceivedWeaponClass::Unknown;
    const std::uint32_t weapon = static_cast<std::uint32_t>(WEAPON::GET_SELECTED_PED_WEAPON(ped));
    const std::uint32_t group = static_cast<std::uint32_t>(WEAPON::GET_WEAPONTYPE_GROUP(weapon));
    return classifyGroup(group);
}

FaceCoverState NativeWitnessPerceptionAdapter::faceCoverState(const PedHandle ped) const {
    if (!validPed(ped)) return FaceCoverState::Unknown;

    // Stage 4 uses a deliberately conservative visual proxy: component slot 1 is GTA's
    // freemode-mask slot and is also used by several story-mode face-cover variants.
    // Stage 5 replaces this with project-approved identity/mask rules. Until then this is
    // evidence that a witness saw a visible face covering, not an identity-system verdict.
    const int drawable = PED::GET_PED_DRAWABLE_VARIATION(ped, 1);
    if (drawable > 0) return FaceCoverState::Covered;
    if (drawable == 0) return FaceCoverState::Uncovered;
    return FaceCoverState::Unknown;
}

float NativeWitnessPerceptionAdapter::ambientVisibilityFactor() const {
    const int hour = CLOCK::GET_CLOCK_HOURS();
    if (hour >= 7 && hour <= 18) return 1.0f;
    if (hour == 6 || hour == 19) return 0.78f;
    if (hour == 5 || hour == 20) return 0.66f;
    return 0.54f;
}

bool NativeWitnessPerceptionAdapter::standStill(const PedHandle ped, const int durationMs) const {
    if (!validPed(ped)) return false;
    TASK::TASK_STAND_STILL(ped, std::max(0, durationMs));
    return true;
}

} // namespace gco::platform
