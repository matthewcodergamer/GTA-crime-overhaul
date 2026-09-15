#include "PlatformAdapters.h"

#include <Windows.h>
#include <main.h>
#include <natives.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace gco::platform {
namespace {

constexpr std::size_t kWorldPoolCapacity = 2048;
constexpr float kPi = 3.14159265358979323846f;

Vec3 fromNative(const Vector3& value) noexcept {
    return {value.x, value.y, value.z};
}

template <typename HandleT, typename PoolFn, typename ExistsFn>
std::vector<HandleT> collectNearby(
    PoolFn pool,
    ExistsFn exists,
    const Vec3& center,
    const float radius,
    const std::size_t maxResults) {

    if (radius < 0.0f || maxResults == 0) {
        return {};
    }

    std::array<int, kWorldPoolCapacity> handles{};
    const int count = std::clamp(
        pool(handles.data(), static_cast<int>(handles.size())),
        0,
        static_cast<int>(handles.size()));

    std::vector<std::pair<float, HandleT>> candidates;
    candidates.reserve(std::min<std::size_t>(static_cast<std::size_t>(count), maxResults * 2U));

    const float radiusSq = radius * radius;
    for (int i = 0; i < count; ++i) {
        const HandleT handle = static_cast<HandleT>(handles[static_cast<std::size_t>(i)]);
        if (!exists(handle)) {
            continue;
        }

        const Vec3 position = fromNative(ENTITY::GET_ENTITY_COORDS(handle, TRUE));
        const float distanceSq = distanceSquared(center, position);
        if (distanceSq <= radiusSq) {
            candidates.emplace_back(distanceSq, handle);
        }
    }

    const std::size_t keep = std::min(maxResults, candidates.size());
    if (keep < candidates.size()) {
        std::partial_sort(
            candidates.begin(),
            candidates.begin() + static_cast<std::ptrdiff_t>(keep),
            candidates.end(),
            [](const auto& left, const auto& right) { return left.first < right.first; });
        candidates.resize(keep);
    } else {
        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const auto& left, const auto& right) { return left.first < right.first; });
    }

    std::vector<HandleT> result;
    result.reserve(candidates.size());
    for (const auto& [distanceSq, handle] : candidates) {
        (void)distanceSq;
        result.push_back(handle);
    }
    return result;
}

ControlBinding bindingFor(const InputAction action) noexcept {
    // Current GTA control IDs. Using PAD means keyboard/controller remapping remains GTA-owned.
    switch (action) {
    case InputAction::Interact: return {0, 51};       // INPUT_CONTEXT
    case InputAction::Cancel: return {0, 177};       // INPUT_FRONTEND_CANCEL
    case InputAction::Sprint: return {0, 21};        // INPUT_SPRINT
    case InputAction::Aim: return {0, 25};           // INPUT_AIM
    case InputAction::Attack: return {0, 24};        // INPUT_ATTACK
    case InputAction::EnterVehicle: return {0, 23};  // INPUT_VEH_ENTER
    case InputAction::Count: break;
    }
    return {0, 0};
}

} // namespace

PedHandle NativeWorldAdapter::playerPed() const {
    return PLAYER::PLAYER_PED_ID();
}

bool NativeWorldAdapter::entityExists(const EntityHandle entity) const {
    return entity != 0 && ENTITY::DOES_ENTITY_EXIST(entity) != FALSE;
}

bool NativeWorldAdapter::pedExists(const PedHandle ped) const {
    return entityExists(ped) && ENTITY::IS_ENTITY_A_PED(ped) != FALSE;
}

bool NativeWorldAdapter::vehicleExists(const VehicleHandle vehicle) const {
    return entityExists(vehicle) && ENTITY::IS_ENTITY_A_VEHICLE(vehicle) != FALSE;
}

std::optional<Vec3> NativeWorldAdapter::entityPosition(const EntityHandle entity) const {
    if (!entityExists(entity)) {
        return std::nullopt;
    }
    return fromNative(ENTITY::GET_ENTITY_COORDS(entity, TRUE));
}

bool NativeWorldAdapter::playerAlive() const {
    const PedHandle ped = playerPed();
    if (!pedExists(ped)) {
        return false;
    }
    return PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID()) == FALSE
        && PED::IS_PED_DEAD_OR_DYING(ped, TRUE) == FALSE;
}

int NativeWorldAdapter::wantedLevel() const {
    return PLAYER::GET_PLAYER_WANTED_LEVEL(PLAYER::PLAYER_ID());
}

MissionState NativeWorldAdapter::missionState() const {
    MissionState state{};
    state.missionFlag = MISC::GET_MISSION_FLAG() != FALSE;
    state.cutsceneActive = CUTSCENE::IS_CUTSCENE_ACTIVE() != FALSE;
    state.cutscenePlaying = CUTSCENE::IS_CUTSCENE_PLAYING() != FALSE;
    state.playerControlOn = PLAYER::IS_PLAYER_CONTROL_ON(PLAYER::PLAYER_ID()) != FALSE;
    return state;
}

std::vector<PedHandle> NativeWorldAdapter::nearbyPeds(
    const Vec3& center,
    const float radius,
    const std::size_t maxResults) const {

    return collectNearby<PedHandle>(
        worldGetAllPeds,
        [this](const PedHandle handle) { return pedExists(handle); },
        center,
        radius,
        maxResults);
}

std::vector<VehicleHandle> NativeWorldAdapter::nearbyVehicles(
    const Vec3& center,
    const float radius,
    const std::size_t maxResults) const {

    return collectNearby<VehicleHandle>(
        worldGetAllVehicles,
        [this](const VehicleHandle handle) { return vehicleExists(handle); },
        center,
        radius,
        maxResults);
}

bool NativeWorldAdapter::hasLineOfSight(
    const EntityHandle from,
    const EntityHandle to,
    const int traceType) const {

    if (!entityExists(from) || !entityExists(to)) {
        return false;
    }
    return ENTITY::HAS_ENTITY_CLEAR_LOS_TO_ENTITY(from, to, traceType) != FALSE;
}

std::optional<PedSnapshot> NativeWorldAdapter::snapshotPed(const PedHandle ped) const {
    if (!pedExists(ped)) {
        return std::nullopt;
    }

    PedSnapshot snapshot{};
    snapshot.modelHash = static_cast<std::uint32_t>(ENTITY::GET_ENTITY_MODEL(ped));
    snapshot.position = fromNative(ENTITY::GET_ENTITY_COORDS(ped, TRUE));
    snapshot.heading = ENTITY::GET_ENTITY_HEADING(ped);
    snapshot.alive = PED::IS_PED_DEAD_OR_DYING(ped, TRUE) == FALSE;
    snapshot.ragdoll = PED::IS_PED_RAGDOLL(ped) != FALSE;
    snapshot.isPlayer = PED::IS_PED_A_PLAYER(ped) != FALSE;

    for (int component = 0; component < static_cast<int>(snapshot.components.size()); ++component) {
        auto& variation = snapshot.components[static_cast<std::size_t>(component)];
        variation.drawable = PED::GET_PED_DRAWABLE_VARIATION(ped, component);
        variation.texture = PED::GET_PED_TEXTURE_VARIATION(ped, component);
        variation.palette = PED::GET_PED_PALETTE_VARIATION(ped, component);
    }

    for (int prop = 0; prop < static_cast<int>(snapshot.props.size()); ++prop) {
        auto& variation = snapshot.props[static_cast<std::size_t>(prop)];
        variation.drawable = PED::GET_PED_PROP_INDEX(ped, prop, 0);
        variation.texture = PED::GET_PED_PROP_TEXTURE_INDEX(ped, prop);
    }

    return snapshot;
}

std::optional<VehicleSnapshot> NativeWorldAdapter::snapshotVehicle(
    const VehicleHandle vehicle,
    const std::optional<std::uint64_t> projectVehicleId) const {

    if (!vehicleExists(vehicle)) {
        return std::nullopt;
    }

    VehicleSnapshot snapshot{};
    snapshot.modelHash = static_cast<std::uint32_t>(ENTITY::GET_ENTITY_MODEL(vehicle));
    snapshot.position = fromNative(ENTITY::GET_ENTITY_COORDS(vehicle, TRUE));
    snapshot.heading = ENTITY::GET_ENTITY_HEADING(vehicle);
    VEHICLE::GET_VEHICLE_COLOURS(vehicle, &snapshot.primaryColor, &snapshot.secondaryColor);

    if (const char* plate = VEHICLE::GET_VEHICLE_NUMBER_PLATE_TEXT(vehicle); plate != nullptr) {
        snapshot.plate = plate;
        const auto first = snapshot.plate.find_first_not_of(' ');
        const auto last = snapshot.plate.find_last_not_of(' ');
        snapshot.plate = first == std::string::npos
            ? std::string{}
            : snapshot.plate.substr(first, last - first + 1);
    }

    snapshot.plateStyle = VEHICLE::GET_VEHICLE_NUMBER_PLATE_TEXT_INDEX(vehicle);
    snapshot.projectVehicleId = projectVehicleId;
    return snapshot;
}

bool NativeAnimationAdapter::requestDictionary(
    const std::string_view dictionary,
    const std::uint32_t timeoutMs) {

    if (dictionary.empty()) {
        return false;
    }

    const std::string name(dictionary);
    if (STREAMING::DOES_ANIM_DICT_EXIST(name.c_str()) == FALSE) {
        return false;
    }

    STREAMING::REQUEST_ANIM_DICT(name.c_str());
    const ULONGLONG startedAt = GetTickCount64();
    do {
        if (STREAMING::HAS_ANIM_DICT_LOADED(name.c_str()) != FALSE) {
            return true;
        }
        scriptWait(0);
    } while (GetTickCount64() - startedAt < timeoutMs);

    return STREAMING::HAS_ANIM_DICT_LOADED(name.c_str()) != FALSE;
}

void NativeAnimationAdapter::releaseDictionary(const std::string_view dictionary) {
    if (dictionary.empty()) {
        return;
    }
    const std::string name(dictionary);
    STREAMING::REMOVE_ANIM_DICT(name.c_str());
}

bool NativeAnimationAdapter::stopAnimation(
    const PedHandle ped,
    const std::string_view dictionary,
    const std::string_view clip,
    const float blendOut) {

    if (ped == 0
        || ENTITY::DOES_ENTITY_EXIST(ped) == FALSE
        || ENTITY::IS_ENTITY_A_PED(ped) == FALSE
        || dictionary.empty()
        || clip.empty()) {
        return false;
    }

    const std::string dictionaryValue(dictionary);
    const std::string clipValue(clip);
    TASK::STOP_ANIM_TASK(ped, dictionaryValue.c_str(), clipValue.c_str(), blendOut);
    return true;
}

bool NativeAnimationAdapter::cancelPedTasks(const PedHandle ped, const bool immediate) {
    if (ped == 0 || ENTITY::DOES_ENTITY_EXIST(ped) == FALSE || ENTITY::IS_ENTITY_A_PED(ped) == FALSE) {
        return false;
    }

    if (immediate) {
        TASK::CLEAR_PED_TASKS_IMMEDIATELY(ped);
    } else {
        TASK::CLEAR_PED_TASKS(ped);
        TASK::CLEAR_PED_SECONDARY_TASK(ped);
    }
    return true;
}

bool NativePropAttachmentAdapter::attach(
    const ObjectHandle object,
    const EntityHandle parent,
    const int parentBoneIndex,
    const Vec3& offset,
    const Vec3& rotation,
    const bool collision,
    const bool fixedRotation) {

    if (object == 0 || parent == 0
        || ENTITY::DOES_ENTITY_EXIST(object) == FALSE
        || ENTITY::DOES_ENTITY_EXIST(parent) == FALSE) {
        return false;
    }

    ENTITY::ATTACH_ENTITY_TO_ENTITY(
        object,
        parent,
        parentBoneIndex,
        offset.x,
        offset.y,
        offset.z,
        rotation.x,
        rotation.y,
        rotation.z,
        FALSE,
        FALSE,
        collision ? TRUE : FALSE,
        FALSE,
        2,
        fixedRotation ? TRUE : FALSE,
        0);
    return ENTITY::IS_ENTITY_ATTACHED_TO_ENTITY(object, parent) != FALSE;
}

bool NativePropAttachmentAdapter::detach(
    const ObjectHandle object,
    const bool collision,
    const bool dynamic) {

    if (object == 0 || ENTITY::DOES_ENTITY_EXIST(object) == FALSE) {
        return false;
    }
    ENTITY::DETACH_ENTITY(object, collision ? TRUE : FALSE, dynamic ? TRUE : FALSE);
    return true;
}

void NativePropAttachmentAdapter::deleteOwned(ObjectHandle& object) {
    if (object == 0) {
        return;
    }

    if (ENTITY::DOES_ENTITY_EXIST(object) != FALSE) {
        if (ENTITY::IS_ENTITY_ATTACHED(object) != FALSE) {
            ENTITY::DETACH_ENTITY(object, TRUE, TRUE);
        }
        Object nativeObject = object;
        OBJECT::DELETE_OBJECT(&nativeObject);
    }
    object = 0;
}

InteriorId NativeInteriorDoorAdapter::interiorFromEntity(const EntityHandle entity) const {
    if (entity == 0 || ENTITY::DOES_ENTITY_EXIST(entity) == FALSE) {
        return 0;
    }
    return INTERIOR::GET_INTERIOR_FROM_ENTITY(entity);
}

bool NativeInteriorDoorAdapter::isInteriorReady(const InteriorId interior) const {
    return interior != 0
        && INTERIOR::IS_VALID_INTERIOR(interior) != FALSE
        && INTERIOR::IS_INTERIOR_READY(interior) != FALSE;
}

void NativeInteriorDoorAdapter::refreshInterior(const InteriorId interior) {
    if (interior != 0 && INTERIOR::IS_VALID_INTERIOR(interior) != FALSE) {
        INTERIOR::REFRESH_INTERIOR(interior);
    }
}

bool NativeInteriorDoorAdapter::doorExists(const std::uint32_t doorSystemHash) const {
    return doorSystemHash != 0
        && OBJECT::IS_DOOR_REGISTERED_WITH_SYSTEM(static_cast<Hash>(doorSystemHash)) != FALSE;
}

bool NativeInteriorDoorAdapter::addDoorToSystem(
    const std::uint32_t doorSystemHash,
    const std::uint32_t modelHash,
    const Vec3& position) {

    if (doorSystemHash == 0 || modelHash == 0) {
        return false;
    }

    OBJECT::ADD_DOOR_TO_SYSTEM(
        static_cast<Hash>(doorSystemHash),
        static_cast<Hash>(modelHash),
        position.x,
        position.y,
        position.z,
        FALSE,
        FALSE,
        TRUE,
        0);
    return doorExists(doorSystemHash);
}

void NativeInteriorDoorAdapter::removeDoorFromSystem(const std::uint32_t doorSystemHash) {
    if (doorExists(doorSystemHash)) {
        OBJECT::REMOVE_DOOR_FROM_SYSTEM(static_cast<Hash>(doorSystemHash), 0);
    }
}

void NativeInteriorDoorAdapter::setClosestDoorLocked(
    const std::uint32_t modelHash,
    const Vec3& position,
    const bool locked,
    const float heading) {

    if (modelHash == 0) {
        return;
    }
    OBJECT::SET_STATE_OF_CLOSEST_DOOR_OF_TYPE(
        static_cast<Hash>(modelHash),
        position.x,
        position.y,
        position.z,
        locked ? TRUE : FALSE,
        heading,
        FALSE);
}

BlipHandle NativeUiAdapter::addBlip(const Vec3& position, const BlipStyle& style) {
    const Blip blip = HUD::ADD_BLIP_FOR_COORD(position.x, position.y, position.z);
    if (blip == 0) {
        return 0;
    }

    HUD::SET_BLIP_SPRITE(blip, style.sprite);
    HUD::SET_BLIP_COLOUR(blip, style.color);
    HUD::SET_BLIP_SCALE(blip, style.scale);
    HUD::SET_BLIP_AS_SHORT_RANGE(blip, style.shortRange ? TRUE : FALSE);
    return blip;
}

void NativeUiAdapter::removeBlip(BlipHandle& blip) {
    if (blip == 0) {
        return;
    }

    Blip nativeBlip = blip;
    if (HUD::DOES_BLIP_EXIST(nativeBlip) != FALSE) {
        HUD::REMOVE_BLIP(&nativeBlip);
    }
    blip = 0;
}

void NativeUiAdapter::setBlipName(const BlipHandle blip, const std::string_view text) {
    if (blip == 0 || HUD::DOES_BLIP_EXIST(blip) == FALSE) {
        return;
    }

    const std::string value(text);
    HUD::BEGIN_TEXT_COMMAND_SET_BLIP_NAME("STRING");
    HUD::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME(value.c_str());
    HUD::END_TEXT_COMMAND_SET_BLIP_NAME(blip);
}

void NativeUiAdapter::subtitle(
    const std::string_view text,
    const int durationMs,
    const bool drawImmediately) {

    if (text.empty() || durationMs <= 0) {
        return;
    }

    const std::string value(text);
    HUD::BEGIN_TEXT_COMMAND_PRINT("STRING");
    HUD::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME(value.c_str());
    HUD::END_TEXT_COMMAND_PRINT(durationMs, drawImmediately ? TRUE : FALSE);
}

void NativeUiAdapter::helpText(const std::string_view text, const bool beep) {
    if (text.empty()) {
        return;
    }

    const std::string value(text);
    HUD::BEGIN_TEXT_COMMAND_DISPLAY_HELP("STRING");
    HUD::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME(value.c_str());
    HUD::END_TEXT_COMMAND_DISPLAY_HELP(0, TRUE, beep ? TRUE : FALSE, -1);
}

bool NativeAudioAdapter::playAmbientSpeech(
    const PedHandle ped,
    const std::string_view speechName,
    const std::string_view speechParam) {

    if (ped == 0
        || ENTITY::DOES_ENTITY_EXIST(ped) == FALSE
        || ENTITY::IS_ENTITY_A_PED(ped) == FALSE
        || speechName.empty()
        || speechParam.empty()) {
        return false;
    }

    const std::string speech(speechName);
    const std::string parameter(speechParam);
    if (AUDIO::DOES_CONTEXT_EXIST_FOR_THIS_PED(ped, speech.c_str(), FALSE) == FALSE) {
        return false;
    }

    AUDIO::PLAY_PED_AMBIENT_SPEECH_NATIVE(ped, speech.c_str(), parameter.c_str(), 0);
    return true;
}

void NativeAudioAdapter::stopPedSpeaking(const PedHandle ped) {
    if (ped != 0 && ENTITY::DOES_ENTITY_EXIST(ped) != FALSE) {
        AUDIO::STOP_CURRENT_PLAYING_AMBIENT_SPEECH(ped);
    }
}

ControlBinding NativeInputAdapter::binding(const InputAction action) const {
    return bindingFor(action);
}

bool NativeInputAdapter::pressed(const InputAction action) const {
    const auto bind = binding(action);
    return PAD::IS_CONTROL_PRESSED(bind.inputGroup, bind.control) != FALSE;
}

bool NativeInputAdapter::justPressed(const InputAction action) const {
    const auto bind = binding(action);
    return PAD::IS_CONTROL_JUST_PRESSED(bind.inputGroup, bind.control) != FALSE;
}

bool NativeInputAdapter::justReleased(const InputAction action) const {
    const auto bind = binding(action);
    return PAD::IS_CONTROL_JUST_RELEASED(bind.inputGroup, bind.control) != FALSE;
}

float NativeInputAdapter::normal(const InputAction action) const {
    const auto bind = binding(action);
    return PAD::GET_CONTROL_NORMAL(bind.inputGroup, bind.control);
}

void NativeDebugDrawAdapter::line(const Vec3& from, const Vec3& to, const Rgba& color) {
    GRAPHICS::DRAW_LINE(
        from.x,
        from.y,
        from.z,
        to.x,
        to.y,
        to.z,
        color.r,
        color.g,
        color.b,
        color.a);
}

void NativeDebugDrawAdapter::box(const Vec3& min, const Vec3& max, const Rgba& color) {
    GRAPHICS::DRAW_BOX(
        min.x,
        min.y,
        min.z,
        max.x,
        max.y,
        max.z,
        color.r,
        color.g,
        color.b,
        color.a);
}

void NativeDebugDrawAdapter::witnessCone(
    const Vec3& origin,
    const float headingDegrees,
    const float halfAngleDegrees,
    const float range,
    const Rgba& color) {

    if (!(range > 0.0f) || !(halfAngleDegrees > 0.0f)) {
        return;
    }

    const auto endpoint = [&](const float degrees) {
        const float radians = degrees * (kPi / 180.0f);
        return Vec3{
            origin.x + std::sin(radians) * range,
            origin.y + std::cos(radians) * range,
            origin.z
        };
    };

    const Vec3 left = endpoint(headingDegrees - halfAngleDegrees);
    const Vec3 right = endpoint(headingDegrees + halfAngleDegrees);
    line(origin, left, color);
    line(origin, right, color);
    line(left, right, color);
}

} // namespace gco::platform
