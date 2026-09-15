#pragma once

#include "IdentitySystem.h"
#include "CoreServices.h"
#include "crime/CrimeDirector.h"
#include "platform/PlatformAdapters.h"
#include "platform/RobberyPedAdapter.h"
#include "platform/WitnessPerceptionAdapter.h"
#include "robbery/StoreRuntime.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace gco::identity {

struct ClerkRecognitionTuning final {
    float observationRadius = 12.0f;
    float halfFovDegrees = 68.0f;
    float faceHalfAngleDegrees = 78.0f;
    std::uint64_t faceMinimumViewMs = 350;
    std::uint64_t recognitionCheckMs = 1200;
    std::uint64_t recognitionReactionCooldownMs = 30000;
};

class ClerkRecognitionDirector final {
public:
    ClerkRecognitionDirector(
        platform::PlatformServices& platform,
        IdentitySystem& identitySystem,
        robbery::PrototypeStoreRuntime& storeRuntime,
        crime::CrimeDirector& crimeDirector,
        EventBus& events,
        ClerkRecognitionTuning tuning = {});
    ~ClerkRecognitionDirector();

    void initialize();
    void shutdown();
    void tickFiveHz(std::uint64_t nowMs, bool gameplayAllowed);

    [[nodiscard]] bool casePersistenceDirty() const noexcept { return casePersistenceDirty_; }
    void clearCasePersistenceDirty() noexcept { casePersistenceDirty_ = false; }
    [[nodiscard]] RecognitionResult lastRecognition() const noexcept { return lastRecognition_; }
    [[nodiscard]] std::string debugSummary() const;

private:
    struct RobberyObservation final {
        LogicalId businessId = 0;
        LogicalId caseId = 0;
        LogicalId crimeId = 0;
        LogicalId clerkId = 0;
        bool active = false;
        bool faceObserved = false;
        CharacterIdentity faceIdentity = CharacterIdentity::Unknown;
        float faceConfidence = 0.0f;
        std::uint64_t faceObservedAtMs = 0;
        bool maskObserved = false;
        MaskCoverage maskCoverage = MaskCoverage::None;
        float maskConfidence = 0.0f;
        std::uint64_t maskObservedAtMs = 0;
        bool outfitObserved = false;
        std::string outfitKey;
        float outfitConfidence = 0.0f;
        std::uint64_t outfitObservedAtMs = 0;
    };

    void onRobberyStarted(const RuntimeEvent& event);
    void onRobberyFinished(const RuntimeEvent& event);
    void onReportCompleted(const RuntimeEvent& event);
    void onClerkReplacedOrKilled(const RuntimeEvent& event);
    void sampleRobberyObservation(std::uint64_t nowMs);
    void evaluateRepeatRecognition(std::uint64_t nowMs);
    void applyRecognitionBehavior(const RecognitionResult& result, std::uint64_t nowMs);
    void persistReportedClerkEvidence(std::uint64_t nowMs);
    void addCaseEvidence(crime::EvidenceKind kind, float confidence, std::uint64_t observedAtMs, std::string descriptor);

    [[nodiscard]] bool visibleSample(
        platform::PedHandle clerk,
        platform::PedHandle player,
        const platform::PedSnapshot& clerkSnapshot,
        const platform::PedSnapshot& playerSnapshot,
        std::uint64_t nowMs,
        float& confidence,
        float& faceQuality);
    [[nodiscard]] static std::optional<LogicalId> payloadId(std::string_view payload, std::string_view key);

    platform::PlatformServices& platform_;
    platform::NativeRobberyPedAdapter pedBehavior_;
    platform::NativeWitnessPerceptionAdapter perception_;
    IdentitySystem& identitySystem_;
    robbery::PrototypeStoreRuntime& storeRuntime_;
    crime::CrimeDirector& crimeDirector_;
    EventBus& events_;
    ClerkRecognitionTuning tuning_;

    EventBus::SubscriptionId robberyStartedSub_ = 0;
    EventBus::SubscriptionId robberyFinishedSub_ = 0;
    EventBus::SubscriptionId reportCompletedSub_ = 0;
    EventBus::SubscriptionId clerkReplacedSub_ = 0;
    EventBus::SubscriptionId clerkKilledSub_ = 0;

    RobberyObservation observation_{};
    RecognitionResult lastRecognition_{};
    std::uint64_t accumulatedViewMs_ = 0;
    std::uint64_t lastViewSampleMs_ = 0;
    std::uint64_t nextRecognitionCheckMs_ = 0;
    std::uint64_t nextRecognitionReactionMs_ = 0;
    bool initialized_ = false;
    bool casePersistenceDirty_ = false;
};

} // namespace gco::identity
