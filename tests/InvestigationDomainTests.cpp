#include "investigation/InvestigationDomain.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace gco;

namespace {

crime::EvidenceRecord witnessEvidence(
    const crime::EvidenceKind kind,
    const std::string& source,
    const std::string& descriptor,
    const float confidence,
    const crime::CrimeLocation& location) {
    crime::EvidenceRecord evidence{};
    evidence.source = crime::EvidenceSource::Witness;
    evidence.kind = kind;
    evidence.confidence = confidence;
    evidence.observedAtMs = 1100;
    evidence.independenceKey = source;
    evidence.dedupKey = source + ':' + std::string(crime::evidenceKindName(kind));
    evidence.snapshot.descriptor = descriptor;
    evidence.snapshot.location = location;
    return evidence;
}

bool hasSemantic(
    const investigation::InterviewSourceSummary& summary,
    const investigation::InterviewSemanticEvent semantic) {
    return std::any_of(summary.facts.begin(), summary.facts.end(), [semantic](const auto& fact) {
        return fact.semantic == semantic;
    });
}

} // namespace

int main() {
    const crime::CrimeLocation scene{100.0f, 200.0f, 30.0f, "prototype_store"};
    crime::CaseFile file{};
    file.id = makeLogicalId(LogicalIdDomain::Case, 1);
    file.state = crime::CaseState::Reported;
    file.severity = crime::CrimeSeverity::Major;
    file.createdAtMs = 1000;
    file.updatedAtMs = 1200;

    crime::CrimeEvent robbery{};
    robbery.id = makeLogicalId(LogicalIdDomain::Crime, 1);
    robbery.caseId = file.id;
    robbery.type = crime::CrimeType::ArmedRobbery;
    robbery.severity = crime::CrimeSeverity::Major;
    robbery.location = scene;
    robbery.businessId = makeLogicalId(LogicalIdDomain::Business, 1);
    robbery.occurredAtMs = 1000;

    crime::CrimeEvent assault = robbery;
    assault.id = makeLogicalId(LogicalIdDomain::Crime, 2);
    assault.type = crime::CrimeType::OfficerAssault;
    assault.severity = crime::CrimeSeverity::Critical;
    file.crimeIds = {robbery.id, assault.id};

    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::CrimeObserved,
        "witness:alpha",
        "witness_visually_observed_crime;heard_gunshot",
        0.80f,
        scene));
    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::Face,
        "witness:alpha",
        "witness_saw_uncovered_face",
        0.72f,
        scene));
    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::Vehicle,
        "witness:alpha",
        "model=0x1234ABCD;primaryColor=12;secondaryColor=14",
        0.70f,
        scene));
    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::Plate,
        "witness:alpha",
        "plate=ABC123",
        0.78f,
        scene));
    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::Direction,
        "witness:alpha",
        "heading=90;x=130;y=200;z=30",
        0.55f,
        scene));
    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::Body,
        "witness:alpha",
        "witness_saw_victim_after_violence",
        0.65f,
        crime::CrimeLocation{103.0f, 202.0f, 30.0f, "body"}));

    // A second witness reported the crime but never recorded face/vehicle/plate facts.
    file.evidence.push_back(witnessEvidence(
        crime::EvidenceKind::CrimeObserved,
        "witness:beta",
        "witness_heard_crime_only",
        0.35f,
        scene));

    const std::vector<const crime::CrimeEvent*> crimes{&robbery, &assault};
    investigation::SceneGeometry geometry{};
    geometry.center = scene;
    geometry.interiorPoints.push_back({101.0f, 200.0f, 30.0f, "interior"});
    geometry.exitPoints.push_back({95.0f, 198.0f, 30.0f, "exit"});

    const auto facts = investigation::deriveDispatchFacts(file, crimes);
    assert(facts.officerAttack);
    assert(facts.armed);
    assert(facts.shotsFired);
    assert(facts.death);

    const auto plan = investigation::makeDispatchPlan(file, crimes, geometry);
    assert(plan.response == investigation::PoliceResponseType::Critical);
    assert(plan.officerCount == 6);
    assert(plan.tacticalLevel == 4);
    assert(plan.geometry.center.x == scene.x);
    assert(plan.geometry.center.y == scene.y);

    // There is intentionally no player/GPS input in makeDispatchPlan. Every approach task must
    // remain anchored to the recorded crime scene rather than an arbitrary hidden-player point.
    const platform::Vec3 hiddenPlayer{9999.0f, -9999.0f, 900.0f};
    bool foundApproach = false;
    bool foundInterior = false;
    bool foundBody = false;
    bool foundExit = false;
    for (const auto& task : plan.tasks) {
        assert(!(task.target.x == hiddenPlayer.x && task.target.y == hiddenPlayer.y));
        foundApproach = foundApproach || task.kind == investigation::SceneTaskKind::Approach;
        foundInterior = foundInterior || task.kind == investigation::SceneTaskKind::CheckInterior;
        foundBody = foundBody || task.kind == investigation::SceneTaskKind::CheckBody;
        foundExit = foundExit || task.kind == investigation::SceneTaskKind::CheckExit;
    }
    assert(foundApproach && foundInterior && foundBody && foundExit);

    const auto alpha = investigation::deriveInterviewSummary(file, "witness:alpha");
    assert(alpha.hasReport);
    assert(hasSemantic(alpha, investigation::InterviewSemanticEvent::FaceSeen));
    assert(hasSemantic(alpha, investigation::InterviewSemanticEvent::VehicleSeen));
    assert(hasSemantic(alpha, investigation::InterviewSemanticEvent::PlateSeen));
    assert(hasSemantic(alpha, investigation::InterviewSemanticEvent::DirectionKnown));
    assert(hasSemantic(alpha, investigation::InterviewSemanticEvent::ShotsReported));
    assert(!hasSemantic(alpha, investigation::InterviewSemanticEvent::FaceNotSeen));
    assert(alpha.dialogueFacts.faceObserved);
    assert(alpha.dialogueFacts.vehicleObserved);
    assert(alpha.dialogueFacts.plateText == "ABC123");

    const auto beta = investigation::deriveInterviewSummary(file, "witness:beta");
    assert(beta.hasReport);
    assert(hasSemantic(beta, investigation::InterviewSemanticEvent::FaceNotSeen));
    assert(hasSemantic(beta, investigation::InterviewSemanticEvent::VehicleNotSeen));
    assert(hasSemantic(beta, investigation::InterviewSemanticEvent::PlateNotSeen));
    assert(!hasSemantic(beta, investigation::InterviewSemanticEvent::FaceSeen));
    assert(!hasSemantic(beta, investigation::InterviewSemanticEvent::VehicleSeen));
    assert(!hasSemantic(beta, investigation::InterviewSemanticEvent::PlateSeen));

    const auto sources = investigation::reportingWitnessSourceKeys(file);
    assert(sources.size() == 2);
    assert(std::find(sources.begin(), sources.end(), "witness:alpha") != sources.end());
    assert(std::find(sources.begin(), sources.end(), "witness:beta") != sources.end());

    std::cout << "InvestigationDomainTests passed\n";
    return 0;
}
