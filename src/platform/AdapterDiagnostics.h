#pragma once

#include "PlatformAdapters.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gco::platform {

enum class AdapterProbeStatus : std::uint8_t {
    Pass,
    Fail,
    Skip,
    ManualRequired,
    ResearchRequired
};

std::string_view adapterProbeStatusName(AdapterProbeStatus status) noexcept;

struct AdapterProbeResult final {
    std::string id;
    AdapterProbeStatus status = AdapterProbeStatus::Skip;
    std::string detail;
};

struct AdapterProbeReport final {
    std::vector<AdapterProbeResult> results;

    void add(std::string id, AdapterProbeStatus status, std::string detail);
    void append(AdapterProbeReport other);
    [[nodiscard]] std::size_t failures() const noexcept;
    [[nodiscard]] std::size_t passes() const noexcept;
    [[nodiscard]] bool safeChecksPassed() const noexcept { return failures() == 0; }
};

// Debug-only harness for Stage 1 native wrappers. It deliberately avoids guessing
// game assets: animation, prop/bone, door and speech positive-path tests remain
// RESEARCH_REQUIRED until exact identifiers have been validated in-game.
class AdapterDiagnostics final {
public:
    explicit AdapterDiagnostics(PlatformServices& services) noexcept : services_(services) {}

    AdapterProbeReport probeWorld();
    AdapterProbeReport probeAnimation();
    AdapterProbeReport probeProps();
    AdapterProbeReport probeInteriorsAndDoors();
    AdapterProbeReport probeUi();
    AdapterProbeReport probeAudio();
    AdapterProbeReport probeInput(std::uint64_t nowMs);
    AdapterProbeReport probeDebugDraw(std::uint64_t nowMs);
    AdapterProbeReport probeAll(std::uint64_t nowMs);

    void beginInputProbe(std::uint64_t nowMs, std::uint64_t durationMs = 10000) noexcept;
    [[nodiscard]] std::vector<InputAction> tickInputProbe(std::uint64_t nowMs);
    [[nodiscard]] bool inputProbeActive(std::uint64_t nowMs) const noexcept;

    void beginVisualProbe(std::uint64_t nowMs, std::uint64_t durationMs = 5000) noexcept;
    void renderVisualProbe(std::uint64_t nowMs);
    [[nodiscard]] bool visualProbeActive(std::uint64_t nowMs) const noexcept;

private:
    PlatformServices& services_;
    std::uint64_t inputProbeUntilMs_ = 0;
    std::uint64_t visualProbeUntilMs_ = 0;
};

} // namespace gco::platform
