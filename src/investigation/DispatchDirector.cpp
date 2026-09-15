#include "DispatchDirector.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gco::investigation {

DispatchDirector::DispatchDirector(
    crime::CrimeRegistry& registry,
    crime::CrimeDirector& crimeDirector,
    platform::IPoliceInvestigationAdapter& adapter,
    EventBus& events,
    GeometryResolver geometryResolver,
    DispatchTuning tuning)
    : registry_(registry),
      crimeDirector_(crimeDirector),
      adapter_(adapter),
      events_(events),
      geometryResolver_(std::move(geometryResolver)),
      tuning_(tuning) {}

DispatchDirector::~DispatchDirector() {
    shutdown();
}

void DispatchDirector::initialize(const std::uint64_t nowMs) {
    if (initialized_) return;
    initialized_ = true;
    ensureScenes(nowMs);
}

void DispatchDirector::shutdown() {
    if (!initialized_) return;
    adapter_.cleanupAll();
    scenes_.clear();
    plans_.clear();
    initialized_ = false;
}

void DispatchDirector::tickTwoHz(
    const std::uint64_t nowMs,
    const bool gameplayAllowed,
    const std::optional<platform::Vec3> playerPosition) {

    if (!initialized_) return;
    ensureScenes(nowMs);

    for (auto& [caseId, scene] : scenes_) {
        const auto planIt = plans_.find(caseId);
        if (planIt == plans_.end()) continue;
        const auto& plan = planIt->second;

        if (nowMs >= scene.expiresAtMs) {
            expireScene(scene, plan, nowMs);
            continue;
        }
        updateDetail(scene, plan, nowMs, gameplayAllowed, playerPosition);

        if (!scene.logicalArrivalRecorded && nowMs >= scene.expectedArrivalAtMs) {
            recordLogicalArrival(scene, plan, nowMs);
        }
    }
}

const SceneRecord* DispatchDirector::findScene(const LogicalId caseId) const noexcept {
    const auto it = scenes_.find(caseId);
    return it == scenes_.end() ? nullptr : &it->second;
}

const DispatchPlan* DispatchDirector::findPlan(const LogicalId caseId) const noexcept {
    const auto it = plans_.find(caseId);
    return it == plans_.end() ? nullptr : &it->second;
}

std::vector<const SceneRecord*> DispatchDirector::scenes() const {
    std::vector<const SceneRecord*> result;
    result.reserve(scenes_.size());
    for (const auto& [caseId, scene] : scenes_) {
        (void)caseId;
        result.push_back(&scene);
    }
    std::sort(result.begin(), result.end(), [](const SceneRecord* a, const SceneRecord* b) {
        return a->dispatchedAtMs < b->dispatchedAtMs;
    });
    return result;
}

bool DispatchDirector::highDetailArrived(const LogicalId caseId) const noexcept {
    const auto* scene = findScene(caseId);
    if (scene == nullptr || scene->detailLevel != SceneDetailLevel::HighDetail || !scene->logicalArrivalRecorded) {
        return false;
    }
    const auto snapshot = adapter_.sceneSnapshot(caseId, scene->geometry.center);
    return snapshot.active && snapshot.arrived;
}

platform::PedHandle DispatchDirector::leadOfficer(const LogicalId caseId) const {
    const auto* scene = findScene(caseId);
    if (scene == nullptr) return 0;
    return adapter_.sceneSnapshot(caseId, scene->geometry.center).leadOfficer;
}

std::string DispatchDirector::debugSummary() const {
    std::ostringstream out;
    out << "DispatchDirector: scenes=" << scenes_.size();
    for (const auto* scene : scenes()) {
        if (scene == nullptr) continue;
        out << "\n - case=" << scene->caseId
            << ";response=" << policeResponseTypeName(scene->response)
            << ";detail=" << sceneDetailLevelName(scene->detailLevel)
            << ";arrived=" << (scene->logicalArrivalRecorded ? "true" : "false")
            << ";expires=" << scene->expiresAtMs;
    }
    return out.str();
}

void DispatchDirector::ensureScenes(const std::uint64_t nowMs) {
    for (const auto* file : registry_.cases()) {
        if (file == nullptr || !caseEligible(*file)) continue;
        ensureScene(*file, nowMs);
    }
}

void DispatchDirector::ensureScene(const crime::CaseFile& file, const std::uint64_t nowMs) {
    if (scenes_.contains(file.id)) return;
    const auto crimes = registry_.crimesForCase(file.id);
    if (crimes.empty()) return;

    SceneGeometry geometry{};
    geometry.center = crimes.front()->location;
    if (geometryResolver_) {
        const auto resolved = geometryResolver_(file, crimes);
        if (!(resolved.center.x == 0.0f && resolved.center.y == 0.0f && resolved.center.z == 0.0f)) {
            geometry = resolved;
        } else {
            geometry.interiorPoints = resolved.interiorPoints;
            geometry.exitPoints = resolved.exitPoints;
        }
    }

    auto plan = makeDispatchPlan(file, crimes, std::move(geometry));
    if (plan.response == PoliceResponseType::None) return;

    const std::uint64_t dispatchAt = file.updatedAtMs != 0 ? file.updatedAtMs : nowMs;
    SceneRecord scene{};
    scene.caseId = file.id;
    scene.businessId = plan.businessId;
    scene.response = plan.response;
    scene.geometry = plan.geometry;
    scene.tasks = plan.tasks;
    scene.dispatchedAtMs = dispatchAt;
    scene.expectedArrivalAtMs = dispatchAt + plan.abstractArrivalDelayMs;
    scene.expiresAtMs = dispatchAt + tuning_.sceneLifetimeMs;
    scene.businessRecoveryAtMs = scene.expiresAtMs + tuning_.businessRecoveryDelayMs;

    // A case restored from disk may already have been investigating before this runtime started.
    if (file.state != crime::CaseState::Reported) {
        scene.logicalArrivalRecorded = true;
        scene.investigationStarted = true;
        scene.arrivedAtMs = file.updatedAtMs;
    }

    plans_.emplace(file.id, std::move(plan));
    scenes_.emplace(file.id, std::move(scene));
    events_.publish(RuntimeEvent{
        "dispatch.scene_created",
        file.id,
        "target=crime_scene;response=" + std::string(policeResponseTypeName(plans_.at(file.id).response))});
}

void DispatchDirector::recordLogicalArrival(
    SceneRecord& scene,
    const DispatchPlan& plan,
    const std::uint64_t nowMs) {

    if (scene.logicalArrivalRecorded) return;
    scene.logicalArrivalRecorded = true;
    scene.arrivedAtMs = nowMs;

    const auto* file = registry_.findCase(scene.caseId);
    if (file != nullptr && file->state == crime::CaseState::Reported) {
        if (crimeDirector_.beginInvestigation(scene.caseId, nowMs)) {
            scene.investigationStarted = true;
            casePersistenceDirty_ = true;
        }
    }

    crime::ImmediateResponseState immediate{};
    immediate.active = true;
    immediate.reportPending = false;
    immediate.pursuitActive = false;
    immediate.tacticalLevel = plan.tacticalLevel;
    immediate.lastUpdatedAtMs = nowMs;
    if (crimeDirector_.setImmediateResponse(scene.caseId, immediate, nowMs)) {
        casePersistenceDirty_ = true;
    }

    events_.publish(RuntimeEvent{
        "dispatch.scene_arrived",
        scene.caseId,
        "target=crime_scene;tacticalLevel=" + std::to_string(plan.tacticalLevel)});
}

void DispatchDirector::updateDetail(
    SceneRecord& scene,
    const DispatchPlan& plan,
    const std::uint64_t nowMs,
    const bool gameplayAllowed,
    const std::optional<platform::Vec3>& playerPosition) {

    const float radiusSq = tuning_.highDetailRadius * tuning_.highDetailRadius;
    const bool nearScene = gameplayAllowed && playerPosition.has_value()
        && distanceSquared(*playerPosition, scene.geometry.center) <= radiusSq;

    if (!nearScene) {
        if (scene.detailLevel == SceneDetailLevel::HighDetail) {
            adapter_.cleanupScene(scene.caseId);
            scene.detailLevel = SceneDetailLevel::Abstract;
            events_.publish(RuntimeEvent{
                "dispatch.scene_abstracted",
                scene.caseId,
                "reason=player_far_or_runtime_restricted"});
        }
        return;
    }

    if (scene.detailLevel == SceneDetailLevel::Abstract) {
        if (adapter_.activateScene(plan)) {
            scene.detailLevel = SceneDetailLevel::HighDetail;
            scene.lastHighDetailAtMs = nowMs;
            events_.publish(RuntimeEvent{
                "dispatch.scene_reconstructed",
                scene.caseId,
                "target=crime_scene"});
        }
    }

    if (scene.detailLevel != SceneDetailLevel::HighDetail) return;
    scene.lastHighDetailAtMs = nowMs;
    const auto snapshot = adapter_.sceneSnapshot(scene.caseId, scene.geometry.center);
    if (snapshot.arrived && !scene.logicalArrivalRecorded) {
        recordLogicalArrival(scene, plan, nowMs);
    }
    if (snapshot.arrived) adapter_.taskInvestigation(plan);
}

void DispatchDirector::expireScene(
    SceneRecord& scene,
    const DispatchPlan& plan,
    const std::uint64_t nowMs) {

    (void)plan;
    if (scene.detailLevel == SceneDetailLevel::HighDetail) {
        adapter_.cleanupScene(scene.caseId);
        scene.detailLevel = SceneDetailLevel::Abstract;
    }

    const auto* file = registry_.findCase(scene.caseId);
    if (file != nullptr && file->immediate.active) {
        auto immediate = file->immediate;
        immediate.active = false;
        immediate.reportPending = false;
        immediate.pursuitActive = false;
        immediate.lastUpdatedAtMs = nowMs;
        if (crimeDirector_.setImmediateResponse(scene.caseId, immediate, nowMs)) casePersistenceDirty_ = true;
    }

    if (!scene.recoveryEventPublished && nowMs >= scene.businessRecoveryAtMs) {
        scene.recoveryEventPublished = true;
        events_.publish(RuntimeEvent{
            "business.investigation_recovered",
            scene.businessId.value_or(0),
            "caseId=" + std::to_string(scene.caseId)});
    }
}

float DispatchDirector::distanceSquared(
    const platform::Vec3& point,
    const crime::CrimeLocation& target) noexcept {
    const float dx = point.x - target.x;
    const float dy = point.y - target.y;
    const float dz = point.z - target.z;
    return dx * dx + dy * dy + dz * dz;
}

bool DispatchDirector::caseEligible(const crime::CaseFile& file) noexcept {
    if (file.resolution != crime::CaseResolution::None) return false;
    switch (file.state) {
    case crime::CaseState::Reported:
    case crime::CaseState::Investigating:
    case crime::CaseState::UnknownSuspect:
    case crime::CaseState::IdentifiedSuspect:
    case crime::CaseState::BoloOrWarrant:
    case crime::CaseState::PursuitOrSearch:
    case crime::CaseState::Dormant:
        return true;
    default:
        return false;
    }
}

} // namespace gco::investigation
