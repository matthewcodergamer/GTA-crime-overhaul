#include "InvestigationDialogue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace gco::dialogue {
namespace {

std::uint32_t mixSeed(std::uint32_t value) noexcept {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

template <std::size_t N>
std::string_view choose(
    const std::array<std::string_view, N>& choices,
    const std::uint32_t seed,
    const std::uint32_t salt) noexcept {

    static_assert(N > 0);
    return choices[mixSeed(seed ^ salt) % N];
}

std::string witnessNoun(const WitnessKind kind) {
    switch (kind) {
    case WitnessKind::Clerk: return "clerk";
    case WitnessKind::Civilian: return "witness";
    case WitnessKind::SecurityGuard: return "security guard";
    }
    return "witness";
}

std::string joinVehicleDescription(const WitnessStatementFacts& facts) {
    if (!facts.vehicleObserved) {
        return {};
    }

    if (!facts.vehicleColor.empty() && !facts.vehicleDescription.empty()) {
        return facts.vehicleColor + " " + facts.vehicleDescription;
    }
    if (!facts.vehicleColor.empty()) {
        return facts.vehicleColor + " vehicle";
    }
    if (!facts.vehicleDescription.empty()) {
        return facts.vehicleDescription;
    }
    return "vehicle";
}

ConversationTurn officerTurn(
    const InterviewTopic topic,
    std::string event,
    std::string text,
    const EvidenceClaim claims = EvidenceClaim::None,
    const bool evidenceBound = false) {

    return ConversationTurn{
        InterviewSpeaker::Officer,
        topic,
        std::move(event),
        std::move(text),
        claims,
        evidenceBound
    };
}

ConversationTurn witnessTurn(
    const InterviewTopic topic,
    std::string event,
    std::string text,
    const EvidenceClaim claims) {

    return ConversationTurn{
        InterviewSpeaker::Witness,
        topic,
        std::move(event),
        std::move(text),
        claims,
        true
    };
}

void appendFacePair(
    InterviewPlan& plan,
    const WitnessStatementFacts& facts,
    const std::uint32_t seed) {

    constexpr std::array questions{
        std::string_view{"Did you get a look at the robber?"},
        std::string_view{"Could you see the suspect's face?"},
        std::string_view{"Did you get a clear look at their face?"},
        std::string_view{"Was the suspect's face visible?"}
    };
    plan.turns.push_back(officerTurn(
        InterviewTopic::Face,
        "officer.ask.face",
        std::string(choose(questions, seed, 0x1001u))));

    if (facts.faceObserved) {
        EvidenceClaim claims = EvidenceClaim::FaceSeen;
        if (facts.faceCovered) {
            claims |= EvidenceClaim::MaskSeen;
        }

        std::string text;
        if (facts.faceCovered) {
            text = facts.faceConfidence >= 0.75f
                ? "I saw part of the face, but they had it covered."
                : "I caught part of the face, but there was a mask in the way.";
        } else if (facts.faceConfidence >= 0.75f) {
            constexpr std::array answers{
                std::string_view{"Yes. I got a clear look at the face."},
                std::string_view{"Yeah. I saw the face pretty clearly."},
                std::string_view{"I did. I got a good look at the face."}
            };
            text = choose(answers, seed, 0x1002u);
        } else {
            constexpr std::array answers{
                std::string_view{"I saw the face, but only for a moment."},
                std::string_view{"I got a look, but I wouldn't call it perfect."},
                std::string_view{"I saw the face, just not for very long."}
            };
            text = choose(answers, seed, 0x1003u);
        }

        plan.turns.push_back(witnessTurn(
            InterviewTopic::Face,
            "witness.face.seen",
            std::move(text),
            claims));
        return;
    }

    EvidenceClaim claims = EvidenceClaim::FaceNotSeen;
    std::string text;
    if (facts.faceCovered) {
        claims |= EvidenceClaim::MaskSeen;
        constexpr std::array answers{
            std::string_view{"No. Their face was covered. I couldn't tell you who it was."},
            std::string_view{"No, I never saw the face. They had it covered."},
            std::string_view{"I couldn't see the face. There was a mask on."}
        };
        text = choose(answers, seed, 0x1004u);
    } else {
        constexpr std::array answers{
            std::string_view{"No. I never got a clean look at the face."},
            std::string_view{"I couldn't make out the face."},
            std::string_view{"No, I didn't see enough to identify the face."}
        };
        text = choose(answers, seed, 0x1005u);
    }

    plan.turns.push_back(witnessTurn(
        InterviewTopic::Face,
        "witness.face.not_seen",
        std::move(text),
        claims));
}

void appendClothingPair(
    InterviewPlan& plan,
    const WitnessStatementFacts& facts,
    const std::uint32_t seed) {

    constexpr std::array questions{
        std::string_view{"What were they wearing?"},
        std::string_view{"Anything useful about the suspect's clothes?"},
        std::string_view{"Describe the clothing for me."},
        std::string_view{"Do you remember what the suspect had on?"}
    };
    plan.turns.push_back(officerTurn(
        InterviewTopic::Clothing,
        "officer.ask.clothing",
        std::string(choose(questions, seed, 0x2001u))));

    const std::string description = facts.clothingDescription.empty()
        ? std::string{"the clothing, but not enough detail to be specific"}
        : facts.clothingDescription;

    constexpr std::array prefixes{
        std::string_view{"I remember "},
        std::string_view{"Yeah. "},
        std::string_view{"What stood out was "}
    };

    std::string text;
    if (facts.clothingDescription.empty()) {
        text = "I remember the clothing, but not enough detail to be specific.";
    } else {
        text = std::string(choose(prefixes, seed, 0x2002u)) + description + ".";
    }

    plan.turns.push_back(witnessTurn(
        InterviewTopic::Clothing,
        "witness.clothing.seen",
        std::move(text),
        EvidenceClaim::ClothingSeen));
}

void appendVehiclePair(
    InterviewPlan& plan,
    const WitnessStatementFacts& facts,
    const std::uint32_t seed) {

    constexpr std::array questions{
        std::string_view{"Did you see a getaway vehicle?"},
        std::string_view{"What were they driving?"},
        std::string_view{"Did you get a look at the vehicle?"},
        std::string_view{"Anything on the getaway car?"}
    };
    plan.turns.push_back(officerTurn(
        InterviewTopic::Vehicle,
        "officer.ask.vehicle",
        std::string(choose(questions, seed, 0x3001u))));

    if (!facts.vehicleObserved) {
        constexpr std::array answers{
            std::string_view{"No. I didn't see a vehicle."},
            std::string_view{"I never saw what they left in."},
            std::string_view{"No, I couldn't tell you what they were driving."}
        };
        plan.turns.push_back(witnessTurn(
            InterviewTopic::Vehicle,
            "witness.vehicle.not_seen",
            std::string(choose(answers, seed, 0x3002u)),
            EvidenceClaim::VehicleNotSeen));
        return;
    }

    const std::string vehicle = joinVehicleDescription(facts);
    constexpr std::array prefixes{
        std::string_view{"Yeah. It was a "},
        std::string_view{"I saw a "},
        std::string_view{"They left in a "}
    };
    const std::string text = std::string(choose(prefixes, seed, 0x3003u)) + vehicle + ".";
    plan.turns.push_back(witnessTurn(
        InterviewTopic::Vehicle,
        "witness.vehicle.seen",
        text,
        EvidenceClaim::VehicleSeen));
}

void appendPlatePair(
    InterviewPlan& plan,
    const WitnessStatementFacts& facts,
    const std::uint32_t seed) {

    constexpr std::array questions{
        std::string_view{"Did you catch the plate?"},
        std::string_view{"Any part of the license plate?"},
        std::string_view{"Could you read the plate?"},
        std::string_view{"Anything from the plate number?"}
    };
    plan.turns.push_back(officerTurn(
        InterviewTopic::Plate,
        "officer.ask.plate",
        std::string(choose(questions, seed, 0x4001u))));

    if (facts.plateKnowledge == PlateKnowledge::Full && !facts.plateText.empty()) {
        plan.turns.push_back(witnessTurn(
            InterviewTopic::Plate,
            "witness.plate.seen",
            "Yeah. The plate was " + facts.plateText + ".",
            EvidenceClaim::PlateSeen));
        return;
    }

    if (facts.plateKnowledge == PlateKnowledge::Partial && !facts.plateText.empty()) {
        plan.turns.push_back(witnessTurn(
            InterviewTopic::Plate,
            "witness.plate.seen",
            "I only caught part of it: " + facts.plateText + ".",
            EvidenceClaim::PlateSeen));
        return;
    }

    constexpr std::array answers{
        std::string_view{"No. I didn't get the plate."},
        std::string_view{"I couldn't read it."},
        std::string_view{"No, the plate was gone before I could make it out."}
    };
    plan.turns.push_back(witnessTurn(
        InterviewTopic::Plate,
        "witness.plate.not_seen",
        std::string(choose(answers, seed, 0x4002u)),
        EvidenceClaim::PlateNotSeen));
}

void appendDirectionPair(
    InterviewPlan& plan,
    const WitnessStatementFacts& facts,
    const std::uint32_t seed) {

    constexpr std::array questions{
        std::string_view{"Which way did they go?"},
        std::string_view{"What direction did the suspect leave?"},
        std::string_view{"Where did they head after that?"},
        std::string_view{"Point me in the direction they went."}
    };
    plan.turns.push_back(officerTurn(
        InterviewTopic::Direction,
        "officer.ask.direction",
        std::string(choose(questions, seed, 0x5001u))));

    const std::string direction = facts.directionDescription.empty()
        ? std::string{"away from the scene"}
        : facts.directionDescription;
    constexpr std::array prefixes{
        std::string_view{"They went "},
        std::string_view{"I saw them heading "},
        std::string_view{"They took off "}
    };
    plan.turns.push_back(witnessTurn(
        InterviewTopic::Direction,
        "witness.direction.seen",
        std::string(choose(prefixes, seed, 0x5002u)) + direction + ".",
        EvidenceClaim::DirectionSeen));
}

ConversationTurn summaryTurn(const WitnessStatementFacts& facts, const std::uint32_t seed) {
    switch (facts.suspectKnowledge) {
    case SuspectKnowledge::Unknown: {
        constexpr std::array lines{
            std::string_view{"All right. For now we're looking for an unknown suspect."},
            std::string_view{"Okay. We don't have an identity yet."},
            std::string_view{"Understood. Suspect is still unknown right now."}
        };
        return officerTurn(
            InterviewTopic::Summary,
            "officer.summary.suspect_unknown",
            std::string(choose(lines, seed, 0x6001u)),
            EvidenceClaim::SuspectUnknown,
            true);
    }
    case SuspectKnowledge::DescriptionOnly: {
        constexpr std::array lines{
            std::string_view{"All right. We've got a description, but no confirmed identity."},
            std::string_view{"Okay. Description only for now; we don't have a name."},
            std::string_view{"Understood. We have something to look for, but no ID yet."}
        };
        return officerTurn(
            InterviewTopic::Summary,
            "officer.summary.description_only",
            std::string(choose(lines, seed, 0x6002u)),
            EvidenceClaim::SuspectDescriptionOnly,
            true);
    }
    case SuspectKnowledge::Identified: {
        constexpr std::array lines{
            std::string_view{"All right. We have an identified suspect."},
            std::string_view{"Okay. We've got a confirmed identity to work with."},
            std::string_view{"Understood. The suspect has been identified."}
        };
        return officerTurn(
            InterviewTopic::Summary,
            "officer.summary.suspect_identified",
            std::string(choose(lines, seed, 0x6003u)),
            EvidenceClaim::SuspectIdentified,
            true);
    }
    }

    return officerTurn(InterviewTopic::Summary, "officer.summary", "All right.");
}

} // namespace

InterviewPlan InvestigationDialogueComposer::compose(
    const WitnessStatementFacts& facts,
    const InterviewOptions& options) {

    InterviewPlan plan;
    const std::uint32_t seed = options.variationSeed;

    if (options.includeOpening) {
        constexpr std::array openings{
            std::string_view{"All right, tell me what happened."},
            std::string_view{"Start from the beginning. Tell me what you saw."},
            std::string_view{"Okay. Walk me through what happened here."},
            std::string_view{"I need your statement. Tell me what you remember."}
        };
        const std::string role = witnessNoun(facts.witnessKind);
        std::string text = std::string(choose(openings, seed, 0x0101u));
        if (facts.witnessPanicked) {
            text = "Take your time. " + role + ", tell me what you remember.";
        }
        plan.turns.push_back(officerTurn(
            InterviewTopic::Opening,
            "officer.interview.start",
            std::move(text)));
    }

    enum class Candidate : std::uint8_t { Face, Clothing, Vehicle, Plate, Direction };
    std::vector<Candidate> candidates;
    candidates.reserve(5);
    candidates.push_back(Candidate::Face);
    if (facts.clothingObserved) {
        candidates.push_back(Candidate::Clothing);
    }
    candidates.push_back(Candidate::Vehicle);
    if (facts.vehicleObserved) {
        candidates.push_back(Candidate::Plate);
    }
    if (facts.directionObserved) {
        candidates.push_back(Candidate::Direction);
    }

    const std::size_t maxPairs = std::max<std::size_t>(1, options.maxQuestionPairs);
    const std::size_t pairCount = std::min(maxPairs, candidates.size());
    for (std::size_t index = 0; index < pairCount; ++index) {
        switch (candidates[index]) {
        case Candidate::Face:
            appendFacePair(plan, facts, seed + static_cast<std::uint32_t>(index));
            break;
        case Candidate::Clothing:
            appendClothingPair(plan, facts, seed + static_cast<std::uint32_t>(index));
            break;
        case Candidate::Vehicle:
            appendVehiclePair(plan, facts, seed + static_cast<std::uint32_t>(index));
            break;
        case Candidate::Plate:
            appendPlatePair(plan, facts, seed + static_cast<std::uint32_t>(index));
            break;
        case Candidate::Direction:
            appendDirectionPair(plan, facts, seed + static_cast<std::uint32_t>(index));
            break;
        }
    }

    if (options.includeSummary) {
        plan.turns.push_back(summaryTurn(facts, seed));
    }

    if (options.includeClosing) {
        constexpr std::array closings{
            std::string_view{"All right. Stay close in case we need anything else."},
            std::string_view{"Okay. That's enough for now."},
            std::string_view{"Thanks. We'll take it from here."},
            std::string_view{"All right. Let us know if you remember anything else."}
        };
        plan.turns.push_back(officerTurn(
            InterviewTopic::Closing,
            "officer.interview.end",
            std::string(choose(closings, seed, 0x7001u))));
    }

    return plan;
}

EvidenceClaim InvestigationDialogueComposer::allowedClaims(const WitnessStatementFacts& facts) noexcept {
    EvidenceClaim claims = EvidenceClaim::None;

    if (facts.faceObserved) {
        claims |= EvidenceClaim::FaceSeen;
    } else {
        claims |= EvidenceClaim::FaceNotSeen;
    }
    if (facts.faceCovered) {
        claims |= EvidenceClaim::MaskSeen;
    }
    if (facts.clothingObserved) {
        claims |= EvidenceClaim::ClothingSeen;
    }
    if (facts.vehicleObserved) {
        claims |= EvidenceClaim::VehicleSeen;
        if ((facts.plateKnowledge == PlateKnowledge::Full || facts.plateKnowledge == PlateKnowledge::Partial)
            && !facts.plateText.empty()) {
            claims |= EvidenceClaim::PlateSeen;
        } else {
            claims |= EvidenceClaim::PlateNotSeen;
        }
    } else {
        claims |= EvidenceClaim::VehicleNotSeen;
    }
    if (facts.directionObserved) {
        claims |= EvidenceClaim::DirectionSeen;
    }

    switch (facts.suspectKnowledge) {
    case SuspectKnowledge::Unknown:
        claims |= EvidenceClaim::SuspectUnknown;
        break;
    case SuspectKnowledge::DescriptionOnly:
        claims |= EvidenceClaim::SuspectDescriptionOnly;
        break;
    case SuspectKnowledge::Identified:
        claims |= EvidenceClaim::SuspectIdentified;
        break;
    }

    return claims;
}

bool InvestigationDialogueComposer::claimsAreTruthful(
    const WitnessStatementFacts& facts,
    const EvidenceClaim claims) noexcept {

    const std::uint32_t allowed = static_cast<std::uint32_t>(allowedClaims(facts));
    const std::uint32_t requested = static_cast<std::uint32_t>(claims);
    return (requested & ~allowed) == 0;
}

bool canOverhear(const OverhearSample& sample, const OverhearPolicy& policy) noexcept {
    if (!(policy.maxDistance > 0.0f) || !std::isfinite(policy.maxDistance)) {
        return false;
    }
    if (!(sample.distance >= 0.0f) || !std::isfinite(sample.distance)) {
        return false;
    }
    if (sample.distance > policy.maxDistance) {
        return false;
    }
    return !policy.requireLineOfSight || sample.clearLineOfSight;
}

std::string_view interviewSpeakerName(const InterviewSpeaker speaker) noexcept {
    switch (speaker) {
    case InterviewSpeaker::Officer: return "Officer";
    case InterviewSpeaker::Witness: return "Witness";
    }
    return "Unknown";
}

std::string_view interviewTopicName(const InterviewTopic topic) noexcept {
    switch (topic) {
    case InterviewTopic::Opening: return "Opening";
    case InterviewTopic::Face: return "Face";
    case InterviewTopic::Clothing: return "Clothing";
    case InterviewTopic::Vehicle: return "Vehicle";
    case InterviewTopic::Plate: return "Plate";
    case InterviewTopic::Direction: return "Direction";
    case InterviewTopic::Summary: return "Summary";
    case InterviewTopic::Closing: return "Closing";
    }
    return "Unknown";
}

} // namespace gco::dialogue
