#pragma once

#include "PlatformTypes.h"

#include <cstdint>
#include <optional>

namespace gco::platform {

enum class PerceivedWeaponClass : std::uint8_t {
    Unknown,
    Unarmed,
    Melee,
    Handgun,
    Smg,
    Shotgun,
    Rifle,
    MachineGun,
    Sniper,
    Heavy,
    Thrown
};

enum class FaceCoverState : std::uint8_t {
    Unknown,
    Uncovered,
    Covered
};

class NativeWitnessPerceptionAdapter final {
public:
    [[nodiscard]] bool playerShooting() const;
    [[nodiscard]] bool entityDamagedBy(EntityHandle entity, EntityHandle attacker) const;
    [[nodiscard]] std::optional<VehicleHandle> vehiclePedIsUsing(PedHandle ped) const;
    [[nodiscard]] PerceivedWeaponClass selectedWeaponClass(PedHandle ped) const;
    [[nodiscard]] FaceCoverState faceCoverState(PedHandle ped) const;
    [[nodiscard]] float ambientVisibilityFactor() const;
    bool standStill(PedHandle ped, int durationMs) const;
};

} // namespace gco::platform
