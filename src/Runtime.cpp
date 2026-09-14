#include "Runtime.h"

#include <Windows.h>
#include <main.h>

#include <exception>
#include <sstream>

namespace gco {
namespace {

std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(GetTickCount64());
}

} // namespace

Runtime::Runtime()
    : paths_(RuntimePaths::discover()),
      config_(RuntimeConfig::load(paths_.configFile)),
      worldState_(paths_) {}

Runtime::~Runtime() {
    shutdown();
}

bool Runtime::initialize() {
    if (initialized_) {
        return true;
    }

    paths_.ensureDirectories();

    if (!Logger::instance().open(paths_.logRoot / L"GTA_Crime_Overhaul.log")) {
        return false;
    }

    Logger::instance().info("GTA Crime Overhaul starting.");
#ifdef GCO_VERSION
    Logger::instance().info(std::string("Build version: ") + GCO_VERSION);
#endif

    // Reload after the directories exist. This keeps defaults safe on first boot.
    config_ = RuntimeConfig::load(paths_.configFile);

    if (!worldState_.ensureInitialized()) {
        Logger::instance().error("Unable to initialize world save state.");
        return false;
    }

    if (!config_.enabled) {
        Logger::instance().warn("Mod is disabled in configuration; runtime will remain idle.");
    }

    const auto now = nowMs();
    lastFiveHzMs_ = now;
    lastTwoHzMs_ = now;
    lastOneHzMs_ = now;
    lastHeartbeatMs_ = now;
    initialized_ = true;

    Logger::instance().info("Native ASI runtime initialized successfully.");
    return true;
}

void Runtime::run() {
    try {
        if (!initialize()) {
            // Stay yielded rather than repeatedly attempting initialization every frame.
            for (;;) {
                scriptWait(1000);
            }
        }

        for (;;) {
            if (config_.enabled) {
                tick();
            }
            scriptWait(0);
        }
    } catch (const std::exception& ex) {
        Logger::instance().error(std::string("Unhandled std::exception in ScriptMain: ") + ex.what());
        for (;;) {
            scriptWait(1000);
        }
    } catch (...) {
        Logger::instance().error("Unhandled unknown exception in ScriptMain.");
        for (;;) {
            scriptWait(1000);
        }
    }
}

void Runtime::tick() {
    ++frameCount_;
    const auto now = nowMs();

    if (now - lastFiveHzMs_ >= 200) {
        lastFiveHzMs_ = now;
        tickFiveHz();
    }

    if (now - lastTwoHzMs_ >= 500) {
        lastTwoHzMs_ = now;
        tickTwoHz();
    }

    if (now - lastOneHzMs_ >= 1000) {
        lastOneHzMs_ = now;
        tickOneHz();
    }

    if (config_.debugLogging && now - lastHeartbeatMs_ >= 30000) {
        lastHeartbeatMs_ = now;
        std::ostringstream out;
        out << "Runtime heartbeat; frames=" << frameCount_;
        Logger::instance().debug(out.str());
    }

    // Per-frame work is intentionally empty in Phase 0.
    // Future additions here are limited to critical input/UI/active interaction supervision.
}

void Runtime::tickFiveHz() {
    // Reserved for bounded nearby-world snapshots and active interaction availability.
}

void Runtime::tickTwoHz() {
    // Reserved for staggered witness perception, police search planning and nearby scene logic.
}

void Runtime::tickOneHz() {
    // Reserved for case housekeeping, business recovery and persistence reconciliation.
}

void Runtime::shutdown() {
    if (!initialized_) {
        return;
    }

    Logger::instance().info("GTA Crime Overhaul shutting down.");
    initialized_ = false;
    Logger::instance().close();
}

} // namespace gco
