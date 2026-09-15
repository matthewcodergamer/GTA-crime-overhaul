#pragma once

#include "PlatformTypes.h"

namespace gco::platform {

class NativeRobberyPedAdapter final {
public:
    [[nodiscard]] bool playerFreeAimingAt(EntityHandle target) const;
    [[nodiscard]] bool missionEntity(EntityHandle entity) const;
    [[nodiscard]] bool pedArmed(PedHandle ped) const;

    bool handsUp(PedHandle ped, PedHandle facingPed, int durationMs) const;
    bool cower(PedHandle ped, int durationMs) const;
    bool fleeFrom(PedHandle ped, PedHandle target, float safeDistance, int durationMs) const;
    bool combatPed(PedHandle ped, PedHandle target) const;
    bool goStraightTo(PedHandle ped, const Vec3& target, float speed, float heading) const;
    bool turnToCoord(PedHandle ped, const Vec3& target, int durationMs) const;
    bool clearTasks(PedHandle ped, bool immediate) const;
};

} // namespace gco::platform
