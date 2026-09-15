#include "dialogue/InvestigationDialogue.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool hasEvent(const gco::dialogue::InterviewPlan& plan, const std::string_view event) {
    for (const auto& turn : plan.turns) {
        if (turn.semanticEvent == event) {
            return true;
        }
    }
    return false;
}

void expectAllClaimsTruthful(
    const gco::dialogue::InterviewPlan& plan,
    const gco::dialogue::WitnessStatementFacts& facts) {

    for (const auto& turn : plan.turns) {
        expect(
            gco::dialogue::InvestigationDialogueComposer::claimsAreTruthful(facts, turn.claims),
            "conversation turn must never claim evidence the witness/case does not know");
    }
}

void testMaskedUnknownSuspect() {
    using namespace gco::dialogue;

    WitnessStatementFacts facts{};
    facts.witnessKind = WitnessKind::Clerk;
    facts.suspectKnowledge = SuspectKnowledge::Unknown;
    facts.faceObserved = false;
    facts.faceCovered = true;
    facts.clothingObserved = true;
    facts.clothingDescription = "a dark jacket and gray pants";
    facts.vehicleObserved = true;
    facts.vehicleColor = "black";
    facts.vehicleDescription = "two-door coupe";
    facts.plateKnowledge = PlateKnowledge::Partial;
    facts.plateText = "46E";
    facts.directionObserved = true;
    facts.directionDescription = "east toward the freeway";

    const auto plan = InvestigationDialogueComposer::compose(
        facts,
        InterviewOptions{5, 1337, true, true, true});

    expect(!plan.empty(), "masked unknown suspect should produce an interview plan");
    expect(hasEvent(plan, "witness.face.not_seen"), "masked witness should report face not seen");
    expect(hasEvent(plan, "witness.clothing.seen"), "known clothing should be interviewable");
    expect(hasEvent(plan, "witness.vehicle.seen"), "known vehicle should be interviewable");
    expect(hasEvent(plan, "witness.plate.seen"), "partial plate should still be a seen-plate statement");
    expect(hasEvent(plan, "witness.direction.seen"), "known direction should be interviewable");
    expect(hasEvent(plan, "officer.summary.suspect_unknown"), "officer summary should preserve unknown suspect state");
    expectAllClaimsTruthful(plan, facts);
}

void testNoVehicleCannotCreatePlateEvidence() {
    using namespace gco::dialogue;

    WitnessStatementFacts facts{};
    facts.suspectKnowledge = SuspectKnowledge::Unknown;
    facts.faceObserved = false;
    facts.vehicleObserved = false;
    facts.plateKnowledge = PlateKnowledge::Full;
    facts.plateText = "SHOULDNOTUSE";

    const auto plan = InvestigationDialogueComposer::compose(facts, InterviewOptions{5, 8, true, true, true});
    expect(hasEvent(plan, "witness.vehicle.not_seen"), "no vehicle observation should be stated explicitly");
    expect(!hasEvent(plan, "officer.ask.plate"), "plate should not be asked when no vehicle was observed");
    expect(!hasEvent(plan, "witness.plate.seen"), "inconsistent plate field must not create plate evidence");
    expectAllClaimsTruthful(plan, facts);
}

void testFaceSeenDoesNotBecomeIdentity() {
    using namespace gco::dialogue;

    WitnessStatementFacts facts{};
    facts.faceObserved = true;
    facts.faceConfidence = 0.9f;
    facts.suspectKnowledge = SuspectKnowledge::DescriptionOnly;
    facts.vehicleObserved = false;

    const auto plan = InvestigationDialogueComposer::compose(facts, InterviewOptions{2, 42, true, true, true});
    expect(hasEvent(plan, "witness.face.seen"), "face observation should produce a seen-face answer");
    expect(hasEvent(plan, "officer.summary.description_only"), "face observation must not magically become confirmed identity");
    expect(!hasEvent(plan, "officer.summary.suspect_identified"), "description-only case must not claim identified suspect");
    expectAllClaimsTruthful(plan, facts);
}

void testQuestionPairCap() {
    using namespace gco::dialogue;

    WitnessStatementFacts facts{};
    facts.faceObserved = true;
    facts.clothingObserved = true;
    facts.clothingDescription = "red hoodie";
    facts.vehicleObserved = true;
    facts.vehicleDescription = "sedan";
    facts.plateKnowledge = PlateKnowledge::Full;
    facts.plateText = "ABC123";
    facts.directionObserved = true;
    facts.directionDescription = "south";

    const auto plan = InvestigationDialogueComposer::compose(facts, InterviewOptions{2, 1, false, false, false});
    expect(plan.size() == 4, "two question pairs should produce exactly four turns when framing turns are disabled");
}

void testOverhearPolicy() {
    using namespace gco::dialogue;

    expect(canOverhear(OverhearSample{8.0f, true}), "near listener with LOS should hear conversation");
    expect(!canOverhear(OverhearSample{30.0f, true}), "listener outside hearing radius should not hear conversation");
    expect(!canOverhear(OverhearSample{8.0f, false}), "default overhear policy should require LOS");
    expect(canOverhear(OverhearSample{8.0f, false}, OverhearPolicy{18.0f, false}), "policy may explicitly allow hearing without LOS");
    expect(!canOverhear(OverhearSample{-1.0f, true}), "negative distance should fail closed");
}

} // namespace

int main() {
    testMaskedUnknownSuspect();
    testNoVehicleCannotCreatePlateEvidence();
    testFaceSeenDoesNotBecomeIdentity();
    testQuestionPairCap();
    testOverhearPolicy();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "InvestigationDialogueTests passed.\n";
    return EXIT_SUCCESS;
}
