#include "witness/WitnessDomain.h"

#include <cstdlib>
#include <iostream>
#include <set>

namespace {
int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testHearingDoesNotCreateVisualIdentity() {
    using namespace gco::witness;
    WitnessObservation observation{};
    observation.caseId = 1;
    observation.crimeId = 2;

    PerceptionSample sample{};
    sample.distance = 20.0f;
    sample.inFov = false;
    sample.clearLos = false;
    sample.heardThreat = true;
    sample.heardGunshot = true;
    sample.hearingStrength = 0.7f;
    sample.faceCover = FaceCoverKnowledge::FaceVisible;
    sample.outfitSignature = "should_not_leak";
    sample.weaponVisible = true;
    sample.weaponClass = WeaponClass::Handgun;
    sample.vehicleVisible = true;
    sample.vehicle = VehicleVisual{0x1234u, 1, 2};
    sample.plate = "OMNI123";
    sample.plateViewQuality = 1.0f;

    applyPerceptionSample(observation, sample, 1000);
    expect(observation.heardThreat && observation.heardGunshot, "hearing can create awareness");
    expect(!observation.sawCrime, "hearing alone does not become visual observation");
    expect(!observation.faceCover.observed, "hearing alone cannot reveal face/mask");
    expect(!observation.outfit.observed, "hearing alone cannot reveal outfit");
    expect(!observation.weapon.observed, "hearing alone cannot reveal weapon class");
    expect(!observation.vehicle.observed, "hearing alone cannot reveal vehicle");
    expect(!observation.plate.observed, "hearing alone cannot reveal plate");
}

void testTwoWitnessesProduceDifferentReports() {
    using namespace gco::witness;
    WitnessObservation frontWitness{};
    frontWitness.caseId = 42;
    frontWitness.crimeId = 77;
    WitnessObservation backRoomWitness = frontWitness;

    PerceptionSample front{};
    front.distance = 5.0f;
    front.inFov = true;
    front.clearLos = true;
    front.fovQuality = 0.96f;
    front.lightingFactor = 1.0f;
    front.visualDeltaMs = 900;
    front.heardThreat = true;
    front.hearingStrength = 0.9f;
    front.faceViewQuality = 0.95f;
    front.faceCover = FaceCoverKnowledge::FaceVisible;
    front.outfitSignature = "MODEL:shirt.jacket.pants";
    front.weaponVisible = true;
    front.weaponClass = WeaponClass::Handgun;
    front.directionVisible = true;
    front.direction = DirectionVisual{{10.0f, 4.0f, 1.0f}, 90.0f};
    applyPerceptionSample(frontWitness, front, 1000);
    applyPerceptionSample(frontWitness, front, 1600);

    PerceptionSample heard{};
    heard.distance = 12.0f;
    heard.inFov = false;
    heard.clearLos = false;
    heard.heardThreat = true;
    heard.hearingStrength = 0.72f;
    applyPerceptionSample(backRoomWitness, heard, 1000);
    applyPerceptionSample(backRoomWitness, heard, 1600);

    expect(frontWitness.sawCrime, "front witness visually observes the crime");
    expect(frontWitness.faceCover.observed && frontWitness.faceCover.value == FaceCoverKnowledge::FaceVisible,
        "front witness gets face observation after enough quality/time");
    expect(frontWitness.outfit.observed, "front witness records outfit");
    expect(frontWitness.weapon.observed && frontWitness.weapon.value == WeaponClass::Handgun,
        "front witness records visible weapon class");
    expect(frontWitness.lastKnownDirection.observed, "front witness records last-known direction");

    expect(backRoomWitness.heardThreat, "second witness hears same crime");
    expect(!backRoomWitness.sawCrime, "second witness does not visually observe same crime");
    expect(!backRoomWitness.faceCover.observed, "second witness cannot report face");
    expect(!backRoomWitness.outfit.observed, "second witness cannot report outfit");
    expect(!backRoomWitness.weapon.observed, "second witness cannot report weapon class");
    expect(frontWitness.meaningful() && backRoomWitness.meaningful(), "both reports are meaningful but materially different");
}

void testPlateRequiresGeometryAndViewTime() {
    using namespace gco::witness;
    WitnessObservation observation{};
    PerceptionSample sample{};
    sample.distance = 4.0f;
    sample.inFov = true;
    sample.clearLos = true;
    sample.fovQuality = 1.0f;
    sample.lightingFactor = 1.0f;
    sample.visualDeltaMs = 250;
    sample.vehicleVisible = true;
    sample.vehicle = VehicleVisual{0xBEEFu, 12, 12};
    sample.plate = "ABC123";
    sample.plateViewQuality = 0.95f;

    applyPerceptionSample(observation, sample, 1000);
    expect(!observation.plate.observed, "brief glimpse does not read plate");
    applyPerceptionSample(observation, sample, 1250);
    expect(!observation.plate.observed, "plate still gated before minimum view time");
    applyPerceptionSample(observation, sample, 1500);
    expect(observation.plate.observed && observation.plate.value == "ABC123", "sustained good geometry can produce plate evidence");
}

void testStaggeredScanBudget() {
    using namespace gco::witness;
    StaggeredScanCursor cursor(3);
    std::set<std::size_t> visited;
    for (int tick = 0; tick < 4; ++tick) {
        const auto indices = cursor.next(10);
        expect(indices.size() == 3, "scan never exceeds configured per-tick budget");
        visited.insert(indices.begin(), indices.end());
    }
    expect(visited.size() == 10, "round-robin staggering eventually samples every candidate");

    const auto tiny = cursor.next(2);
    expect(tiny.size() == 2, "scan budget clamps to actual candidate count");
}

void testReportingDelayPartialAndInterruption() {
    using namespace gco::witness;
    WitnessEmotion calm{};
    calm.fear = 0.2f;
    calm.panic = 0.1f;
    calm.defiance = 0.1f;
    auto plan = makeReportingPlan(calm, WitnessReaction::CallPolice, 1000, 123);
    expect(plan.state == ReportingState::Waiting && plan.eligibleAtMs > 1000, "reporting is delayed rather than instant");
    expect(advanceReporting(plan, plan.eligibleAtMs - 1, false) == ReportingAdvance::None, "report cannot start early");
    expect(advanceReporting(plan, plan.eligibleAtMs, false) == ReportingAdvance::Begin, "report starts when delay elapses");
    expect(advanceReporting(plan, plan.partialAtMs, false) == ReportingAdvance::CommitBasic, "basic facts can be committed partway through call");
    expect(plan.basicFactsCommitted, "partial report marks basic facts committed");
    expect(advanceReporting(plan, plan.partialAtMs + 1, true) == ReportingAdvance::Interrupted, "normal gameplay can interrupt remaining report");
    expect(plan.basicFactsCommitted && !plan.detailedFactsCommitted, "interruption preserves already-committed basic evidence without inventing details");
}

void testConfidenceNeedsFovLosDistanceAndTime() {
    using namespace gco::witness;
    expect(visualConfidence(5.0f, 30.0f, 1.0f, 1.0f, false, 2000) == 0.0f, "blocked LOS yields zero visual confidence");
    expect(visualConfidence(35.0f, 30.0f, 1.0f, 1.0f, true, 2000) == 0.0f, "outside visual radius yields zero visual confidence");
    const float brief = visualConfidence(5.0f, 30.0f, 0.9f, 1.0f, true, 100);
    const float sustained = visualConfidence(5.0f, 30.0f, 0.9f, 1.0f, true, 2000);
    expect(sustained > brief, "sustained view increases confidence");
    const float dark = visualConfidence(5.0f, 30.0f, 0.9f, 0.5f, true, 2000);
    expect(dark < sustained, "lighting approximation reduces confidence rather than changing awareness radius into omniscience");
}

} // namespace

int main() {
    testHearingDoesNotCreateVisualIdentity();
    testTwoWitnessesProduceDifferentReports();
    testPlateRequiresGeometryAndViewTime();
    testStaggeredScanBudget();
    testReportingDelayPartialAndInterruption();
    testConfidenceNeedsFovLosDistanceAndTime();

    if (failures != 0) {
        std::cerr << failures << " WitnessDomain assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "WitnessDomainTests passed.\n";
    return EXIT_SUCCESS;
}
