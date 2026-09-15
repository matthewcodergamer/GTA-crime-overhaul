#pragma once

#include "SpeakerPresentation.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gco::dialogue {

enum class InterviewSpeaker : std::uint8_t {
    Officer,
    Witness
};

enum class WitnessKind : std::uint8_t {
    Clerk,
    Civilian,
    SecurityGuard
};

enum class SuspectKnowledge : std::uint8_t {
    Unknown,
    DescriptionOnly,
    Identified
};

enum class PlateKnowledge : std::uint8_t {
    None,
    Partial,
    Full
};

enum class InterviewTopic : std::uint8_t {
    Opening,
    Face,
    Clothing,
    Vehicle,
    Plate,
    Direction,
    Summary,
    Closing
};

enum class EvidenceClaim : std::uint32_t {
    None = 0,
    FaceSeen = 1u << 0,
    FaceNotSeen = 1u << 1,
    MaskSeen = 1u << 2,
    ClothingSeen = 1u << 3,
    VehicleSeen = 1u << 4,
    VehicleNotSeen = 1u << 5,
    PlateSeen = 1u << 6,
    PlateNotSeen = 1u << 7,
    DirectionSeen = 1u << 8,
    SuspectUnknown = 1u << 9,
    SuspectDescriptionOnly = 1u << 10,
    SuspectIdentified = 1u << 11
};

constexpr EvidenceClaim operator|(const EvidenceClaim left, const EvidenceClaim right) noexcept {
    return static_cast<EvidenceClaim>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
}

constexpr EvidenceClaim operator&(const EvidenceClaim left, const EvidenceClaim right) noexcept {
    return static_cast<EvidenceClaim>(
        static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right));
}

constexpr EvidenceClaim& operator|=(EvidenceClaim& left, const EvidenceClaim right) noexcept {
    left = left | right;
    return left;
}

[[nodiscard]] constexpr bool hasClaim(const EvidenceClaim value, const EvidenceClaim claim) noexcept {
    return (value & claim) != EvidenceClaim::None;
}

struct WitnessStatementFacts final {
    WitnessKind witnessKind = WitnessKind::Civilian;
    SuspectKnowledge suspectKnowledge = SuspectKnowledge::Unknown;

    bool faceObserved = false;
    bool faceCovered = false;
    float faceConfidence = 0.0f;

    bool clothingObserved = false;
    std::string clothingDescription;

    bool vehicleObserved = false;
    std::string vehicleDescription;
    std::string vehicleColor;

    PlateKnowledge plateKnowledge = PlateKnowledge::None;
    std::string plateText;

    bool directionObserved = false;
    std::string directionDescription;

    bool shotsFired = false;
    bool witnessPanicked = false;
};

struct InterviewOptions final {
    std::size_t maxQuestionPairs = 4;
    std::uint32_t variationSeed = 0;
    bool includeOpening = true;
    bool includeSummary = true;
    bool includeClosing = true;

    // Presentation-only. These traits may influence data-driven wording and voice selection,
    // but the composer must never use them to create or strengthen evidence.
    SpeakerPresentationProfile officerProfile{};
    SpeakerPresentationProfile witnessProfile{};
};

struct ConversationTurn final {
    InterviewSpeaker speaker = InterviewSpeaker::Officer;
    InterviewTopic topic = InterviewTopic::Opening;
    std::string semanticEvent;
    std::string text;
    EvidenceClaim claims = EvidenceClaim::None;
    bool evidenceBound = false;
};

struct InterviewPlan final {
    std::vector<ConversationTurn> turns;
    SpeakerPresentationProfile officerProfile{};
    SpeakerPresentationProfile witnessProfile{};

    [[nodiscard]] bool empty() const noexcept { return turns.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return turns.size(); }
};

struct OverhearPolicy final {
    float maxDistance = 18.0f;
    bool requireLineOfSight = true;
};

struct OverhearSample final {
    float distance = 0.0f;
    bool clearLineOfSight = true;
};

class InvestigationDialogueComposer final {
public:
    [[nodiscard]] static InterviewPlan compose(
        const WitnessStatementFacts& facts,
        const InterviewOptions& options = {});

    [[nodiscard]] static EvidenceClaim allowedClaims(const WitnessStatementFacts& facts) noexcept;
    [[nodiscard]] static bool claimsAreTruthful(
        const WitnessStatementFacts& facts,
        EvidenceClaim claims) noexcept;
};

[[nodiscard]] bool canOverhear(
    const OverhearSample& sample,
    const OverhearPolicy& policy = {}) noexcept;

[[nodiscard]] std::string_view interviewSpeakerName(InterviewSpeaker speaker) noexcept;
[[nodiscard]] std::string_view interviewTopicName(InterviewTopic topic) noexcept;

} // namespace gco::dialogue
