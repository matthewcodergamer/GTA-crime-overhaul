#pragma once

#include "Foundation.h"
#include "platform/PlatformAdapters.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace gco {

class Runtime final {
public:
    Runtime();
    ~Runtime();

    void run();

private:
    bool initialize();
    void tick();
    void tickFiveHz();
    void tickTwoHz();
    void tickOneHz();
    void renderDebugOverlay();
    void shutdown();

    RuntimePaths paths_;
    RuntimeConfig config_;
    WorldStateStore worldState_;
    platform::PlatformServices platform_;

    bool initialized_ = false;
    bool missionSuspended_ = false;
    std::uint64_t frameCount_ = 0;
    std::uint64_t lastFiveHzMs_ = 0;
    std::uint64_t lastTwoHzMs_ = 0;
    std::uint64_t lastOneHzMs_ = 0;
    std::uint64_t lastHeartbeatMs_ = 0;

    platform::MissionState missionState_{};
    std::optional<platform::PedSnapshot> playerSnapshot_;
    std::size_t nearbyPedCount_ = 0;
    std::size_t nearbyVehicleCount_ = 0;
};

} // namespace gco
