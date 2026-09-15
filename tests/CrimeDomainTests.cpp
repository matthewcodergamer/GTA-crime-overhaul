#include "crime/CrimeDirector.h"
#include "crime/CrimeDebugInspector.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(const float a, const float b, const float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

gco::crime::CrimeOccurrence occurrence(
    const gco::crime::CrimeType type,
    const std::uint64_t at,
    std::string incident) {
    gco::crime::CrimeOccurrence out{};
    out.type = type;
    out.occurredAtMs = at;
    out.incidentKey = std::move(incident);
    out.location = {10.0f, 20.0f, 30.0f, "debug_zone"};
    return out;
}

gco::crime::EvidenceRecord evidence(
    const gco::crime::EvidenceKind kind,
    const float confidence,
    const std::uint64_t at,
    std::string sourceFamily,
    std::string dedup) {
    gco::crime::EvidenceRecord out{};
    out.source = gco::crime::EvidenceSource::SyntheticDebug;
    out.kind = kind;
    out.confidence = confidence;
    out.observedAtMs = at;
    out.independenceKey = std::move(sourceFamily);
    out.dedupKey = std::move(dedup);
    out.snapshot.descriptor = "synthetic";
    out.snapshot.location = {10.0f, 20.0f, 30.0f, "debug_zone"};
    return out;
}

void testCaseMergeAndStableIds() {
    using namespace gco;
    using namespace gco::crime;

    LogicalIdGenerator ids;
    EventBus bus;
    CrimeRegistry registry;
    CrimeDirector director(registry, ids, bus);

    const auto first = director.recordCrime(occurrence(CrimeType::ArmedRobbery, 100, "incident-A"));
    const auto second = director.recordCrime(occurrence(CrimeType::PropertyDamage, 120, "incident-A"));
    const auto third = director.recordCrime(occurrence(CrimeType::Assault, 130, "incident-B"));

    expect(first.caseId != 0 && first.crimeId != 0, "first crime must allocate IDs");
    expect(first.caseId == second.caseId, "same incident key must merge into one case");
    expect(first.crimeId != second.crimeId, "merged crimes still need unique crime IDs");
    expect(second.mergedIntoExistingCase, "merge result must report existing-case reuse");
    expect(third.caseId != first.caseId, "different incident key must create a different case");
    expect(logicalIdDomain(first.caseId) == LogicalIdDomain::Case, "case ID must use Case domain");
    expect(logicalIdDomain(first.crimeId) == LogicalIdDomain::Crime, "crime ID must use Crime domain");

    const CaseFile* file = registry.findCase(first.caseId);
    expect(file != nullptr && file->crimeIds.size() == 2, "merged case must own both crime IDs");
    expect(file != nullptr && file->severity == CrimeSeverity::Serious, "armed robbery must dominate property-damage severity");
}

void testSeverityEscalation() {
    using namespace gco;
    using namespace gco::crime;

    LogicalIdGenerator ids;
    EventBus bus;
    CrimeRegistry registry;
    CrimeDirector director(registry, ids, bus);
    const auto recorded = director.recordCrime(occurrence(CrimeType::PropertyDamage, 10, "severity"));

    expect(director.escalateCrime(recorded.crimeId, CrimeSeverity::Major, 20), "crime severity must be able to escalate");
    const CaseFile* file = registry.findCase(recorded.caseId);
    expect(file != nullptr && file->severity == CrimeSeverity::Major, "case severity must follow highest related crime");
    expect(!director.escalateCrime(recorded.crimeId, CrimeSeverity::Minor, 30), "crime severity must never de-escalate through escalation API");
}

void testCanonicalTransitionsAndImmediateSeparation() {
    using namespace gco;
    using namespace gco::crime;

    LogicalIdGenerator ids;
    EventBus bus;
    CrimeRegistry registry;
    CrimeDirector director(registry, ids, bus);
    const auto recorded = director.recordCrime(occurrence(CrimeType::Robbery, 100, "flow"));

    expect(!director.markReported(recorded.caseId, 101), "unobserved case must not skip directly to reported");
    expect(director.addEvidence(recorded.caseId, evidence(EvidenceKind::CrimeObserved, 0.6f, 102, "witness-A", "obs-A"))
        == EvidenceAddResult::Added, "first observation must be accepted");
    expect(registry.findCase(recorded.caseId)->state == CaseState::Observed, "first evidence must advance unobserved case to observed");

    ImmediateResponseState immediate{};
    immediate.active = true;
    immediate.reportPending = true;
    immediate.tacticalLevel = 2;
    expect(director.setImmediateResponse(recorded.caseId, immediate, 103), "immediate response must update independently");
    expect(registry.findCase(recorded.caseId)->state == CaseState::Observed, "immediate tactical state must not rewrite long-term case state");

    expect(director.beginReporting(recorded.caseId, 104), "observed -> reporting must be valid");
    expect(director.markReported(recorded.caseId, 105), "reporting -> reported must be valid");
    expect(director.beginInvestigation(recorded.caseId, 106), "reported -> investigating must be valid");
    expect(director.markUnknownSuspect(recorded.caseId, 0.25f, 107), "investigating -> unknown suspect must be valid");
    expect(director.issueBoloOrWarrant(recorded.caseId, false, true, 108), "unknown suspect may receive vehicle BOLO");
    expect(director.beginPursuitOrSearch(recorded.caseId, 109), "BOLO -> pursuit/search must be valid");
    expect(director.markDormant(recorded.caseId, 110), "pursuit/search -> dormant must be valid");
    expect(registry.findCase(recorded.caseId)->state == CaseState::Dormant, "canonical flow must finish dormant when unresolved");
}

void testEvidenceDedupAndIndependence() {
    using namespace gco;
    using namespace gco::crime;

    LogicalIdGenerator ids;
    EventBus bus;
    CrimeRegistry registry;
    CrimeDirector director(registry, ids, bus);
    const auto recorded = director.recordCrime(occurrence(CrimeType::Robbery, 1, "evidence"));

    auto a1 = evidence(EvidenceKind::Vehicle, 0.40f, 2, "witness-A", "vehicle-A-1");
    auto exact = a1;
    auto a2 = evidence(EvidenceKind::Vehicle, 0.60f, 3, "witness-A", "vehicle-A-2");
    auto b = evidence(EvidenceKind::Vehicle, 0.50f, 4, "witness-B", "vehicle-B-1");

    expect(director.addEvidence(recorded.caseId, a1) == EvidenceAddResult::Added, "first evidence must add");
    expect(director.addEvidence(recorded.caseId, exact) == EvidenceAddResult::Duplicate, "exact evidence duplicate must be suppressed");
    expect(director.addEvidence(recorded.caseId, a2) == EvidenceAddResult::Added, "same source may provide a later stronger immutable record");
    expect(director.addEvidence(recorded.caseId, b) == EvidenceAddResult::Added, "independent source evidence must add");

    const float fused = registry.aggregateConfidence(recorded.caseId, EvidenceKind::Vehicle);
    expect(near(fused, 0.80f), "confidence fusion must use max per source family then combine independent sources");
    expect(registry.findCase(recorded.caseId)->evidence.size() == 3, "duplicate suppression must not mutate existing historical records");
}

void testDecayKeepsMajorCases() {
    using namespace gco;
    using namespace gco::crime;

    LogicalIdGenerator ids;
    EventBus bus;
    CrimeRegistry registry;
    CrimeDirector director(registry, ids, bus);

    const auto weak = director.recordCrime(occurrence(CrimeType::PropertyDamage, 10, "weak"));
    director.addEvidence(weak.caseId, evidence(EvidenceKind::PropertyDamage, 0.10f, 20, "observer", "weak-evidence"));

    const auto major = director.recordCrime(occurrence(CrimeType::Homicide, 10, "major"));
    director.addEvidence(major.caseId, evidence(EvidenceKind::CrimeObserved, 0.10f, 20, "observer", "major-evidence"));

    CaseDecayPolicy policy{};
    policy.minorWeakCaseExpiryMs = 100;
    policy.moderateWeakCaseExpiryMs = 100;
    policy.weakEvidenceThreshold = 0.35f;

    const std::size_t resolved = director.applyDecay(1000, policy);
    expect(resolved == 1, "only weak low-level case should auto-resolve");
    expect(registry.findCase(weak.caseId)->state == CaseState::Resolved, "weak low-level case must resolve after expiry");
    expect(registry.findCase(weak.caseId)->resolution == CaseResolution::ExpiredWeakEvidence, "decay resolution reason must be explicit");
    expect(registry.findCase(major.caseId)->state != CaseState::Resolved, "major case must never disappear through weak-evidence decay");
}

void testInspector() {
    using namespace gco;
    using namespace gco::crime;
    LogicalIdGenerator ids;
    EventBus bus;
    CrimeRegistry registry;
    CrimeDirector director(registry, ids, bus);
    const auto recorded = director.recordCrime(occurrence(CrimeType::VehicleTheft, 50, "inspect"));
    const std::string text = CrimeDebugInspector::formatRegistry(registry);
    expect(text.find(std::to_string(recorded.caseId)) != std::string::npos, "inspector must include case ID");
    expect(text.find("vehicle_theft") != std::string::npos, "inspector must include crime type");
}

} // namespace

int main() {
    testCaseMergeAndStableIds();
    testSeverityEscalation();
    testCanonicalTransitionsAndImmediateSeparation();
    testEvidenceDedupAndIndependence();
    testDecayKeepsMajorCases();
    testInspector();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "CrimeDomainTests passed.\n";
    return EXIT_SUCCESS;
}
