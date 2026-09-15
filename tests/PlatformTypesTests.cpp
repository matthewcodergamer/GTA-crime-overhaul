#include "platform/PlatformTypes.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testDistanceAndRadius() {
    using namespace gco::platform;
    const Vec3 a{0.0f, 0.0f, 0.0f};
    const Vec3 b{3.0f, 4.0f, 0.0f};

    expect(std::fabs(distanceSquared(a, b) - 25.0f) < 0.0001f, "distanceSquared should use xyz delta");
    expect(withinRadius(a, b, 5.0f), "point on radius boundary should be included");
    expect(!withinRadius(a, b, 4.99f), "point outside radius should be excluded");
    expect(!withinRadius(a, b, -1.0f), "negative radius should be rejected");
}

void testGridFiltering() {
    using namespace gco::platform;

    expect(gridCellFor(Vec3{19.9f, 0.0f, -0.1f}, 10.0f) == GridCell{1, 0, -1},
        "gridCellFor should floor positive and negative coordinates consistently");
    expect(gridCellFor(Vec3{-10.0f, -20.1f, 30.0f}, 10.0f) == GridCell{-1, -3, 3},
        "gridCellFor should use stable floor cells");
    expect(gridCellFor(Vec3{5.0f, 5.0f, 5.0f}, 0.0f) == GridCell{},
        "zero cell size should fail safely");
    expect(gridCellFor(Vec3{5.0f, 5.0f, 5.0f}, -1.0f) == GridCell{},
        "negative cell size should fail safely");
    expect(gridCellFor(Vec3{5.0f, 5.0f, 5.0f}, std::numeric_limits<float>::quiet_NaN()) == GridCell{},
        "non-finite cell size should fail safely");
}

void testMissionSuspension() {
    using namespace gco::platform;

    expect(!MissionState{}.shouldSuspendGameplay(), "normal world state should not suspend gameplay");

    MissionState mission{};
    mission.missionFlag = true;
    expect(mission.shouldSuspendGameplay(), "mission flag should suspend gameplay");

    MissionState cutscene{};
    cutscene.cutsceneActive = true;
    expect(cutscene.shouldSuspendGameplay(), "active cutscene should suspend gameplay");

    MissionState playing{};
    playing.cutscenePlaying = true;
    expect(playing.shouldSuspendGameplay(), "playing cutscene should suspend gameplay");

    MissionState noControl{};
    noControl.playerControlOn = false;
    expect(noControl.shouldSuspendGameplay(), "loss of player control should suspend gameplay-facing systems");
}

void testSnapshotDefaults() {
    using namespace gco::platform;

    PedSnapshot ped{};
    expect(!ped.alive && !ped.ragdoll && !ped.isPlayer, "ped snapshot defaults should be conservative");
    expect(ped.props.front().drawable == -1, "ped props should default to absent");

    VehicleSnapshot vehicle{};
    expect(!vehicle.projectVehicleId.has_value(), "ambient vehicles must not acquire fake persistent IDs");
    expect(vehicle.plate.empty(), "vehicle plate should default empty");
}

} // namespace

int main() {
    testDistanceAndRadius();
    testGridFiltering();
    testMissionSuspension();
    testSnapshotDefaults();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "PlatformTypesTests passed.\n";
    return EXIT_SUCCESS;
}
