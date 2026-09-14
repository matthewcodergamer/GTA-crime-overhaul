#pragma once

#include "Foundation.h"

#include <cstdint>

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
    void shutdown();

    RuntimePaths paths_;
    RuntimeConfig config_;
    WorldStateStore worldState_;

    bool initialized_ = false;
    std::uint64_t frameCount_ = 0;
    std::uint64_t lastFiveHzMs_ = 0;
    std::uint64_t lastTwoHzMs_ = 0;
    std::uint64_t lastOneHzMs_ = 0;
    std::uint64_t lastHeartbeatMs_ = 0;
};

} // namespace gco
