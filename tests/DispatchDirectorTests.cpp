#include "investigation/DispatchDirector.h"

#include <cassert>
#include <iostream>
#include <unordered_map>

using namespace gco;

namespace {

class FakePoliceAdapter final : public platform::IPoliceInvestigationAdapter {
public:
    bool activateScene(const investigation::DispatchPlan& plan) override {
        ++activateCount;
        lastPlan = plan;
        active[plan.caseId] = true;
        arrived[plan.caseId] = true;
        return true;
    }

    void taskInvestigation(const investigation::DispatchPlan& plan) override {
        ++taskCount;
        lastPlan = plan;
    }

    platform::PoliceSceneSnapshot sceneSnapshot(
        const LogicalId caseId,
        const crime::CrimeLocation&) const override {
        platform::PoliceSceneSnapshot out{};
        const auto it = active.find(caseId);
        out.active = it != active.end() && it->second;
        const auto arrivedIt = arrived.find(caseId);
        out.arrived = arrivedIt != arrived.end() && arrivedIt->second;
        out.liveOfficerCount = out.active ? 2 : 0;
        out.leadOfficer = out.active ? 77 : 0;
        return out;
    }

    bool beginInterview(LogicalId, platform::PedHandle, investigation::PresentationStyle) override { return true; }
    void presentInterviewTurn(
        LogicalId,
        platform::PedHandle,
        dialogue::InterviewSpeaker,
        investigation::PresentationStyle,
        std::string_view) override {}
    void endInterview(LogicalId, platform::PedHandle) override {}

    void cleanupScene(const LogicalId caseId) override {
        ++cleanupCount;
        active[caseId] = false;
    }

    void cleanupAll() override {
        for (auto& [caseId, value] : active) {
            (void)caseId;
            value = false;
        }
    }

    mutable std::unordered_map<LogicalId, bool> active;
    mutable std::unordered_map<LogicalId, bool> arrived;
    investigation::DispatchPlan lastPlan{};
    int activateCount = 0;
    int taskCount = 0;
    int cleanupCount = 0;
};

crime::EvidenceRecord observedEvidence(const std::uint64_t nowMs) {
    crime::EvidenceRecord evidence{};
    evidence.source = crime::EvidenceSource::Witness;
    evidence.kind = crime::EvidenceKind::CrimeObserved;
    evidence.confidence = 0.8f;
    evidence.observedAtMs = nowMs;
    evidence.independenceKey = "witness:test";
    evidence.dedupKey = "witness:test:crime";
    evidence.snapshot.descriptor = "witness_visually_observed_crime";
    evidence.snapshot.location = {100.0f, 200.0f, 30.0f, "scene"};
    return evidence;
}

} // namespace

int main() {
    LogicalIdGenerator ids;
    EventBus events;
    crime::CrimeRegistry registry;
    crime::CrimeDirector crimeDirector(registry, ids, events);

    crime::CrimeOccurrence occurrence{};
    occurrence.type = crime::CrimeType::ArmedRobbery;
    occurrence.location = {100.0f, 200.0f, 30.0f, "reported_scene"};
    occurrence.occurredAtMs = 1000;
    occurrence.incidentKey = "dispatch-test";
    occurrence.businessId = makeLogicalId(LogicalIdDomain::Business, 1);
    const auto recorded = crimeDirector.recordCrime(occurrence);
    assert(recorded.caseId != 0);
    assert(crimeDirector.addEvidence(recorded.caseId, observedEvidence(1100)) == crime::EvidenceAddResult::Added);
    assert(crimeDirector.beginReporting(recorded.caseId, 1150));
    assert(crimeDirector.markReported(recorded.caseId, 1200));

    FakePoliceAdapter adapter;
    investigation::DispatchTuning tuning{};
    tuning.highDetailRadius = 150.0f;
    tuning.sceneLifetimeMs = 300000;
    tuning.businessRecoveryDelayMs = 30000;

    investigation::DispatchDirector dispatch(
        registry,
        crimeDirector,
        adapter,
        events,
        [](const crime::CaseFile&, const std::vector<const crime::CrimeEvent*>& crimes) {
            investigation::SceneGeometry geometry{};
            geometry.center = crimes.front()->location;
            geometry.interiorPoints.push_back({101.0f, 201.0f, 30.0f, "interior"});
            geometry.exitPoints.push_back({95.0f, 200.0f, 30.0f, "exit"});
            return geometry;
        },
        tuning);

    dispatch.initialize(1200);
    const auto* scene = dispatch.findScene(recorded.caseId);
    const auto* plan = dispatch.findPlan(recorded.caseId);
    assert(scene != nullptr && plan != nullptr);
    assert(plan->geometry.center.x == 100.0f && plan->geometry.center.y == 200.0f);

    // Hidden player position is only a detail-streaming input. Far away must not activate a live
    // scene or alter the response target.
    dispatch.tickTwoHz(1300, true, platform::Vec3{5000.0f, -5000.0f, 500.0f});
    assert(adapter.activateCount == 0);
    assert(dispatch.findPlan(recorded.caseId)->geometry.center.x == 100.0f);

    // Abstract police arrival still advances the case at the scene without spawning psychic cops.
    const std::uint64_t logicalArrival = scene->expectedArrivalAtMs + 1;
    dispatch.tickTwoHz(logicalArrival, true, platform::Vec3{5000.0f, -5000.0f, 500.0f});
    const auto* file = registry.findCase(recorded.caseId);
    assert(file != nullptr);
    assert(file->state == crime::CaseState::Investigating);
    assert(file->immediate.active);
    assert(!file->immediate.pursuitActive);
    assert(dispatch.casePersistenceDirty());

    // Returning near the recorded crime scene reconstructs high detail there.
    dispatch.tickTwoHz(logicalArrival + 500, true, platform::Vec3{105.0f, 201.0f, 30.0f});
    assert(adapter.activateCount == 1);
    assert(dispatch.findScene(recorded.caseId)->detailLevel == investigation::SceneDetailLevel::HighDetail);
    assert(adapter.taskCount >= 1);
    assert(dispatch.highDetailArrived(recorded.caseId));

    // Leaving abstracts the scene and deletes live police, but the record survives.
    dispatch.tickTwoHz(logicalArrival + 1000, true, platform::Vec3{1000.0f, 1000.0f, 100.0f});
    assert(adapter.cleanupCount == 1);
    assert(dispatch.findScene(recorded.caseId) != nullptr);
    assert(dispatch.findScene(recorded.caseId)->detailLevel == investigation::SceneDetailLevel::Abstract);

    // Coming back before expiry reconstructs the same scene again.
    dispatch.tickTwoHz(logicalArrival + 1500, true, platform::Vec3{99.0f, 198.0f, 30.0f});
    assert(adapter.activateCount == 2);
    assert(dispatch.findPlan(recorded.caseId)->geometry.center.x == 100.0f);
    assert(dispatch.findPlan(recorded.caseId)->geometry.center.y == 200.0f);

    dispatch.shutdown();
    std::cout << "DispatchDirectorTests passed\n";
    return 0;
}
