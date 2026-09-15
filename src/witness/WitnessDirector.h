#pragma once

#include "WitnessDomain.h"
#include "crime/CrimeDirector.h"
#include "crime/CrimeRegistry.h"
#include "platform/PedPresentationAdapter.h"
#include "platform/PlatformAdapters.h"
#include "platform/RobberyPedAdapter.h"
#include "platform/WitnessPerceptionAdapter.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gco::witness {

struct WitnessDirectorTuning final {
    std::size_t maxCandidates = 16;
    std::size_t samplesPerFiveHzTick = 3;
    std::uint64_t candidateRefreshMs = 600;
    std::uint64_t initialNoiseWindowMs = 5000;
    std::uint64_t violenceNoiseWindowMs = 3500;
    std::uint64_t aftermathMs = 20000;
    WitnessPerceptionPolicy perception{};
};

class WitnessDirector final {
public:
    WitnessDirector(
        platform::PlatformServices& platform,
        platform::NativePedPresentationAdapter& pedPresentation,
        crime::CrimeDirector& crimeDirector,
        crime::CrimeRegistry& crimeRegistry,
        EventBus& events,
        WitnessDirectorTuning tuning = {});
    ~WitnessDirector();

    void initialize();
    void shutdown();
    void tickFiveHz(std::uint64_t persistentNowMs, bool gameplayAllowed, platform::PedHandle excludedPed = 0);
    void renderDebug() const;
    void setDebugEnabled(bool value) noexcept { debugEnabled_ = value; }
    void toggleDebug() noexcept { debugEnabled_ = !debugEnabled_; }
    void clearCasePersistenceDirty() noexcept { casePersistenceDirty_ = false; }
    [[nodiscard]] bool debugEnabled() const noexcept { return debugEnabled_; }
    [[nodiscard]] bool casePersistenceDirty() const noexcept { return casePersistenceDirty_; }
    [[nodiscard]] std::size_t candidateCount() const noexcept { return candidates_.size(); }
    [[nodiscard]] bool incidentActive() const noexcept { return incident_.caseId != 0; }
    [[nodiscard]] LogicalId activeCaseId() const noexcept { return incident_.caseId; }
    [[nodiscard]] std::string debugSummary() const;

private:
    struct Incident final {
        LogicalId businessId = 0;
        LogicalId caseId = 0;
        LogicalId crimeId = 0;
        platform::Vec3 origin{};
        std::uint64_t startedAtMs = 0;
        std::uint64_t endedAtMs = 0;
        std::uint64_t noiseUntilMs = 0;
        std::uint64_t violenceNoiseUntilMs = 0;
        platform::Vec3 violenceOrigin{};
    };

    struct Candidate final {
        platform::PedHandle ped = 0;
        std::string sourceKey;
        WitnessEmotion emotion{};
        WitnessObservation observation{};
        ReportingPlan report{};
        bool reactionPlanned = false;
        bool wasAlive = true;
        std::uint64_t firstTrackedAtMs = 0;
        std::uint64_t lastSampleAtMs = 0;
        std::uint64_t lastPresentAtMs = 0;
        platform::Vec3 lastPosition{};
        float lastHeading = 0.0f;
        float lastDistance = 0.0f;
        float lastFovQuality = 0.0f;
        float lastVisualConfidence = 0.0f;
        bool lastLos = false;
        bool lastHeard = false;
    };

    void onRobberyStarted(const RuntimeEvent& event);
    void onRobberyFinished(const RuntimeEvent& event);
    void onViolenceEvent(const RuntimeEvent& event);
    void refreshCandidates(std::uint64_t nowMs, platform::PedHandle excludedPed);
    void sampleCandidate(Candidate& candidate, std::uint64_t nowMs);
    void planReaction(Candidate& candidate, std::uint64_t nowMs);
    void applyReaction(Candidate& candidate);
    void advanceReport(Candidate& candidate, std::uint64_t nowMs);
    void persistBasicEvidence(Candidate& candidate, std::uint64_t nowMs);
    void persistDetailedEvidence(Candidate& candidate, std::uint64_t nowMs);
    void persistEvidence(
        const Candidate& candidate,
        crime::EvidenceKind kind,
        float confidence,
        std::uint64_t observedAtMs,
        std::string descriptor,
        const platform::Vec3& location);
    void markViolence(const platform::Vec3& origin, std::uint64_t nowMs);
    void prune(std::uint64_t nowMs);
    void clearIncident();

    [[nodiscard]] static std::optional<LogicalId> payloadId(std::string_view payload, std::string_view key);
    [[nodiscard]] static WeaponClass mapWeaponClass(platform::PerceivedWeaponClass value) noexcept;
    [[nodiscard]] static FaceCoverKnowledge mapFaceCover(platform::FaceCoverState value) noexcept;

    platform::PlatformServices& platform_;
    platform::NativePedPresentationAdapter& pedPresentation_;
    platform::NativeRobberyPedAdapter behaviorPed_;
    platform::NativeWitnessPerceptionAdapter perception_;
    crime::CrimeDirector& crimeDirector_;
    crime::CrimeRegistry& crimeRegistry_;
    EventBus& events_;
    WitnessDirectorTuning tuning_;
    StaggeredScanCursor scanCursor_;

    EventBus::SubscriptionId robberyStartedSub_ = 0;
    EventBus::SubscriptionId robberyFinishedSub_ = 0;
    EventBus::SubscriptionId clerkKilledSub_ = 0;
    Incident incident_{};
    std::vector<Candidate> candidates_;
    std::uint64_t nextCandidateRefreshMs_ = 0;
    std::uint64_t lastTickNowMs_ = 0;
    std::uint64_t sourceOrdinal_ = 1;
    bool initialized_ = false;
    bool debugEnabled_ = false;
    bool phoneFallbackLogged_ = false;
    bool casePersistenceDirty_ = false;
};

} // namespace gco::witness
