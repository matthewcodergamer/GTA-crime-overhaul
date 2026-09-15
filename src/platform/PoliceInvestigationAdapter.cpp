#include "PoliceInvestigationAdapter.h"

#include <main.h>
#include <natives.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace gco::platform {
namespace {

constexpr const char* kPolicePedModel = "s_m_y_cop_01";
constexpr float kArrivalRadius = 7.5f;
constexpr float kPresentationRadiusSquared = 9.0f;

bool validPed(const PedHandle ped) {
    return ped != 0 && ENTITY::DOES_ENTITY_EXIST(ped) != FALSE && ENTITY::IS_ENTITY_A_PED(ped) != FALSE;
}

crime::CrimeLocation spawnPoint(const crime::CrimeLocation& center, const std::size_t index) {
    static constexpr std::array<std::array<float, 2>, 8> offsets{{
        {{20.0f, 0.0f}}, {{-20.0f, 0.0f}}, {{0.0f, 20.0f}}, {{0.0f, -20.0f}},
        {{15.0f, 15.0f}}, {{-15.0f, 15.0f}}, {{15.0f, -15.0f}}, {{-15.0f, -15.0f}}
    }};
    crime::CrimeLocation point = center;
    const auto& offset = offsets[index % offsets.size()];
    point.x += offset[0];
    point.y += offset[1];
    point.zoneTag = "police_response_spawn";
    return point;
}

const char* scenarioFor(const investigation::PresentationStyle style) noexcept {
    switch (style) {
    case investigation::PresentationStyle::Clipboard: return "WORLD_HUMAN_CLIPBOARD";
    case investigation::PresentationStyle::Phone: return "WORLD_HUMAN_STAND_MOBILE";
    case investigation::PresentationStyle::Radio: return "WORLD_HUMAN_COP_IDLES";
    case investigation::PresentationStyle::Idle:
    case investigation::PresentationStyle::FaceWitness:
        return nullptr;
    }
    return nullptr;
}

} // namespace

NativePoliceInvestigationAdapter::~NativePoliceInvestigationAdapter() {
    cleanupAll();
}

void NativePoliceInvestigationAdapter::requestPoliceModel() const {
    const Hash model = static_cast<Hash>(MISC::GET_HASH_KEY(kPolicePedModel));
    STREAMING::REQUEST_MODEL(model);
}

bool NativePoliceInvestigationAdapter::policeModelReady() const {
    const Hash model = static_cast<Hash>(MISC::GET_HASH_KEY(kPolicePedModel));
    if (STREAMING::IS_MODEL_IN_CDIMAGE(model) == FALSE || STREAMING::IS_MODEL_VALID(model) == FALSE) return false;
    if (STREAMING::HAS_MODEL_LOADED(model) == FALSE) {
        STREAMING::REQUEST_MODEL(model);
        return false;
    }
    return true;
}

PedHandle NativePoliceInvestigationAdapter::createOfficer(
    const crime::CrimeLocation& position,
    const float heading) const {

    if (!policeModelReady()) return 0;
    const Hash model = static_cast<Hash>(MISC::GET_HASH_KEY(kPolicePedModel));
    const PedHandle ped = PED::CREATE_PED(
        6,
        model,
        position.x,
        position.y,
        position.z,
        heading,
        FALSE,
        TRUE);
    if (!validPed(ped)) return 0;
    ENTITY::SET_ENTITY_AS_MISSION_ENTITY(ped, TRUE, TRUE);
    PED::SET_BLOCKING_OF_NON_TEMPORARY_EVENTS(ped, TRUE);
    return ped;
}

bool NativePoliceInvestigationAdapter::activateScene(const investigation::DispatchPlan& plan) {
    if (plan.caseId == 0 || plan.response == investigation::PoliceResponseType::None || plan.officerCount == 0) {
        return false;
    }
    if (auto* existing = findScene(plan.caseId); existing != nullptr) {
        existing->plan = plan;
        existing->officers.erase(
            std::remove_if(existing->officers.begin(), existing->officers.end(), [](const PedHandle ped) {
                return !validPed(ped);
            }),
            existing->officers.end());
        if (existing->officers.size() >= plan.officerCount) return true;
    }

    requestPoliceModel();
    if (!policeModelReady()) return false;

    auto& scene = scenes_[plan.caseId];
    scene.plan = plan;
    scene.tasksIssued = false;
    scene.officers.erase(
        std::remove_if(scene.officers.begin(), scene.officers.end(), [](const PedHandle ped) {
            return !validPed(ped);
        }),
        scene.officers.end());

    while (scene.officers.size() < plan.officerCount) {
        const std::size_t index = scene.officers.size();
        const auto spawn = spawnPoint(plan.geometry.center, index);
        const float heading = static_cast<float>((index % 4) * 90);
        const auto ped = createOfficer(spawn, heading);
        if (ped == 0) break;
        scene.officers.push_back(ped);
        taskToPoint(ped, plan.geometry.center, 1.6f);
    }

    const Hash model = static_cast<Hash>(MISC::GET_HASH_KEY(kPolicePedModel));
    STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
    return !scene.officers.empty();
}

void NativePoliceInvestigationAdapter::taskInvestigation(const investigation::DispatchPlan& plan) {
    auto* scene = findScene(plan.caseId);
    if (scene == nullptr || scene->officers.empty()) return;
    scene->plan = plan;

    std::vector<const investigation::SceneTask*> meaningful;
    meaningful.reserve(plan.tasks.size());
    for (const auto& task : plan.tasks) {
        if (task.kind == investigation::SceneTaskKind::Approach
            || task.kind == investigation::SceneTaskKind::InterviewWitness) {
            continue;
        }
        meaningful.push_back(&task);
    }
    if (meaningful.empty()) return;

    for (std::size_t i = 0; i < scene->officers.size(); ++i) {
        const PedHandle officer = scene->officers[i];
        if (!validPed(officer)) continue;
        const auto* task = meaningful[i % meaningful.size()];
        if (!scene->tasksIssued) {
            taskToPoint(officer, task->target, 1.05f);
            continue;
        }
        const auto position = services_.world.entityPosition(officer);
        if (position && distanceSquared(*position, task->target) <= kPresentationRadiusSquared) {
            applyPresentation(officer, task->presentation);
        }
    }
    scene->tasksIssued = true;
}

PoliceSceneSnapshot NativePoliceInvestigationAdapter::sceneSnapshot(
    const LogicalId caseId,
    const crime::CrimeLocation& center) const {

    PoliceSceneSnapshot result{};
    const auto* scene = findScene(caseId);
    if (scene == nullptr) return result;
    result.active = true;
    float nearest = 1.0e12f;
    for (const auto ped : scene->officers) {
        if (!validPed(ped)) continue;
        ++result.liveOfficerCount;
        if (result.leadOfficer == 0) result.leadOfficer = ped;
        const auto position = services_.world.entityPosition(ped);
        if (position) nearest = std::min(nearest, distanceSquared(*position, center));
    }
    result.arrived = result.liveOfficerCount > 0 && nearest <= kArrivalRadius * kArrivalRadius;
    return result;
}

bool NativePoliceInvestigationAdapter::beginInterview(
    const LogicalId caseId,
    const PedHandle witness,
    const investigation::PresentationStyle style) {

    auto* scene = findScene(caseId);
    if (scene == nullptr || scene->officers.empty() || !validPed(witness)) return false;
    const PedHandle officer = scene->officers.front();
    if (!validPed(officer)) return false;
    TASK::CLEAR_PED_TASKS(officer);
    TASK::TASK_TURN_PED_TO_FACE_ENTITY(officer, witness, 1200);
    TASK::TASK_TURN_PED_TO_FACE_ENTITY(witness, officer, 1200);
    TASK::TASK_STAND_STILL(officer, 1800);
    TASK::TASK_STAND_STILL(witness, 1800);
    applyPresentation(officer, style);
    return true;
}

void NativePoliceInvestigationAdapter::presentInterviewTurn(
    const LogicalId caseId,
    const PedHandle witness,
    const dialogue::InterviewSpeaker speaker,
    const investigation::PresentationStyle style,
    const std::string_view subtitle) {

    const auto* scene = findScene(caseId);
    if (scene == nullptr || scene->officers.empty()) return;
    const PedHandle officer = scene->officers.front();
    if (validPed(officer) && validPed(witness)) {
        TASK::TASK_TURN_PED_TO_FACE_ENTITY(officer, witness, 900);
        TASK::TASK_TURN_PED_TO_FACE_ENTITY(witness, officer, 900);
        applyPresentation(speaker == dialogue::InterviewSpeaker::Officer ? officer : witness, style);
    }
    // Subtitle is the authoritative fallback. Ambient speech is deliberately optional and is not
    // guessed here; validated voice/model-specific speech can be layered on later without changing facts.
    services_.ui.subtitle(subtitle, 2800, true);
}

void NativePoliceInvestigationAdapter::endInterview(const LogicalId caseId, const PedHandle witness) {
    const auto* scene = findScene(caseId);
    if (scene == nullptr || scene->officers.empty()) return;
    const PedHandle officer = scene->officers.front();
    if (validPed(officer)) TASK::TASK_STAND_STILL(officer, 1200);
    if (validPed(witness)) TASK::TASK_STAND_STILL(witness, 900);
}

void NativePoliceInvestigationAdapter::cleanupScene(const LogicalId caseId) {
    auto it = scenes_.find(caseId);
    if (it == scenes_.end()) return;
    for (auto& ped : it->second.officers) deleteOfficer(ped);
    scenes_.erase(it);
}

void NativePoliceInvestigationAdapter::cleanupAll() {
    for (auto& [caseId, scene] : scenes_) {
        (void)caseId;
        for (auto& ped : scene.officers) deleteOfficer(ped);
    }
    scenes_.clear();
}

float NativePoliceInvestigationAdapter::distanceSquared(
    const platform::Vec3& a,
    const crime::CrimeLocation& b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

void NativePoliceInvestigationAdapter::taskToPoint(
    const PedHandle ped,
    const crime::CrimeLocation& point,
    const float speed) const {
    if (!validPed(ped)) return;
    TASK::TASK_GO_STRAIGHT_TO_COORD(ped, point.x, point.y, point.z, speed, -1, 0.0f, 0.45f);
}

void NativePoliceInvestigationAdapter::applyPresentation(
    const PedHandle ped,
    const investigation::PresentationStyle style) const {
    if (!validPed(ped)) return;
    if (style == investigation::PresentationStyle::FaceWitness) {
        TASK::TASK_STAND_STILL(ped, 1800);
        return;
    }
    const char* scenario = scenarioFor(style);
    if (scenario == nullptr) {
        TASK::TASK_STAND_STILL(ped, 1800);
        return;
    }
    // These scenarios are presentation-only. Core investigation state never waits on them and
    // remains correct if a build/model refuses the scenario or streaming removes the ped.
    TASK::TASK_START_SCENARIO_IN_PLACE(ped, scenario, 0, TRUE);
}

void NativePoliceInvestigationAdapter::deleteOfficer(PedHandle& ped) const {
    if (!validPed(ped)) {
        ped = 0;
        return;
    }
    TASK::CLEAR_PED_TASKS_IMMEDIATELY(ped);
    PED::DELETE_PED(&ped);
    ped = 0;
}

NativePoliceInvestigationAdapter::LiveScene* NativePoliceInvestigationAdapter::findScene(
    const LogicalId caseId) noexcept {
    const auto it = scenes_.find(caseId);
    return it == scenes_.end() ? nullptr : &it->second;
}

const NativePoliceInvestigationAdapter::LiveScene* NativePoliceInvestigationAdapter::findScene(
    const LogicalId caseId) const noexcept {
    const auto it = scenes_.find(caseId);
    return it == scenes_.end() ? nullptr : &it->second;
}

} // namespace gco::platform
