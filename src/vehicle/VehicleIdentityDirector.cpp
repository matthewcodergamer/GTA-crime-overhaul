#include "VehicleIdentityDirector.h"

#include "Foundation.h"

#include <algorithm>
#include <string>

namespace gco::vehicle {

VehicleIdentityDirector::VehicleIdentityDirector(
    platform::NativeVehicleIdentityAdapter& nativeAdapter,
    VehicleIdentitySystem& identity,
    VehiclePersistenceStore& persistence,
    crime::CrimeRegistry& crimeRegistry,
    EventBus& events)
    : nativeAdapter_(nativeAdapter),
      identity_(identity),
      persistence_(persistence),
      crimeRegistry_(crimeRegistry),
      events_(events) {}

VehicleIdentityDirector::~VehicleIdentityDirector() {
    shutdown();
}

void VehicleIdentityDirector::initialize() {
    if (initialized_) return;
    robberyStartedSub_ = events_.subscribe("business.robbery_started", [this](const RuntimeEvent& event) {
        onRobberyStarted(event);
    });
    robberyFinishedSub_ = events_.subscribe("business.robbery_finished", [this](const RuntimeEvent& event) {
        onRobberyFinished(event);
    });
    observedSwapSub_ = events_.subscribe("vehicle.swap_observed", [this](const RuntimeEvent& event) {
        onObservedSwap(event);
    });
    initialized_ = true;
}

void VehicleIdentityDirector::shutdown() {
    if (!initialized_) return;
    if (robberyStartedSub_) events_.unsubscribe(robberyStartedSub_);
    if (robberyFinishedSub_) events_.unsubscribe(robberyFinishedSub_);
    if (observedSwapSub_) events_.unsubscribe(observedSwapSub_);
    robberyStartedSub_ = robberyFinishedSub_ = observedSwapSub_ = 0;
    activeCaseId_ = 0;
    currentVehicleId_.reset();
    currentHandle_ = 0;
    identity_.clearLiveBindings();
    initialized_ = false;
}

void VehicleIdentityDirector::onRobberyStarted(const RuntimeEvent& event) {
    const auto caseId = payloadId(event.payload, "caseId");
    if (!caseId || logicalIdDomain(*caseId) != LogicalIdDomain::Case) return;
    activeCaseId_ = *caseId;
    currentVehicleId_.reset();
    currentHandle_ = 0;
}

void VehicleIdentityDirector::onRobberyFinished(const RuntimeEvent& event) {
    const auto caseId = payloadId(event.payload, "caseId");
    if (activeCaseId_ == 0 || (caseId && *caseId != activeCaseId_)) return;
    // The robbery-specific high-detail tracker stops here. The case/BOLO record remains persistent
    // and Stage 8 later owns longer pursuit/search continuity.
    activeCaseId_ = 0;
    currentVehicleId_.reset();
    currentHandle_ = 0;
}

void VehicleIdentityDirector::onObservedSwap(const RuntimeEvent& event) {
    const auto caseId = payloadId(event.payload, "caseId");
    const auto newVehicleId = payloadId(event.payload, "newVehicleId");
    if (!caseId || !newVehicleId || logicalIdDomain(*newVehicleId) != LogicalIdDomain::Vehicle) return;
    const auto result = identity_.recordVehicleSwap(*caseId, currentVehicleId_, *newVehicleId, true, event.timestampMs);
    if (result.continuityPreserved) persistenceDirty_ = true;
}

void VehicleIdentityDirector::tickFiveHz(const std::uint64_t nowMs, const bool gameplayAllowed) {
    if (!initialized_ || !gameplayAllowed) return;

    if (activeCaseId_ != 0) {
        const auto liveVehicle = nativeAdapter_.currentPlayerVehicle();
        if (liveVehicle.has_value()) {
            std::optional<LogicalId> logical = identity_.logicalIdForHandle(*liveVehicle);
            if (!logical.has_value()) {
                const auto capture = nativeAdapter_.capture(*liveVehicle);
                if (capture.has_value()) {
                    auto& record = identity_.registerVehicle(
                        capture->reportedStolen ? VehicleRecordKind::Stolen : VehicleRecordKind::Temporary,
                        capture->appearance,
                        capture->modifications,
                        nowMs);
                    identity_.bindLiveHandle(*liveVehicle, record.id);
                    logical = record.id;
                    persistenceDirty_ = true;
                }
            }

            if (logical.has_value()) {
                if (currentVehicleId_.has_value() && *currentVehicleId_ != *logical) {
                    // A swap is not assumed witnessed. Without an explicit observation event, break
                    // physical continuity instead of granting police omniscient knowledge.
                    identity_.recordVehicleSwap(activeCaseId_, currentVehicleId_, logical, false, nowMs);
                    persistenceDirty_ = true;
                }
                currentVehicleId_ = logical;
                currentHandle_ = *liveVehicle;
                if (auto* record = identity_.find(*logical); record != nullptr) {
                    const bool alreadyAssociated = std::find(
                        record->crimeHistory.caseIds.begin(),
                        record->crimeHistory.caseIds.end(),
                        activeCaseId_) != record->crimeHistory.caseIds.end();
                    if (!alreadyAssociated && identity_.markCrimeAssociation(*logical, activeCaseId_, true, nowMs)) {
                        persistenceDirty_ = true;
                    }
                }
            }
        }
    }

    // BOLO data comes only from persisted case observations. Current live appearance is never used
    // to invent a model/color/plate fact that witnesses did not actually report.
    for (const auto* file : crimeRegistry_.cases()) {
        if (file == nullptr) continue;
        const VehicleBoloState* before = identity_.findBolo(file->id);
        const bool hadActive = before != nullptr && before->active;
        if (identity_.refreshBoloFromCase(*file, std::nullopt, nowMs)) {
            const auto* after = identity_.findBolo(file->id);
            if (!hadActive || (after != nullptr && after->updatedAtMs == nowMs)) persistenceDirty_ = true;
            if (!file->activeVehicleBolo) {
                crimeRegistry_.setBoloOrWarrant(file->id, file->activePersonWarrant, true, nowMs);
            }
        }
    }
}

bool VehicleIdentityDirector::saveIfDirty(std::string* reason) {
    if (!persistenceDirty_) {
        if (reason) *reason = "vehicle identity state clean";
        return true;
    }
    if (!persistence_.save(identity_, reason)) return false;
    persistenceDirty_ = false;
    return true;
}

std::optional<LogicalId> VehicleIdentityDirector::payloadId(
    const std::string_view payload,
    const std::string_view key) {

    const std::string prefix = std::string(key) + "=";
    const auto start = payload.find(prefix);
    if (start == std::string_view::npos) return std::nullopt;
    const auto valueStart = start + prefix.size();
    const auto end = payload.find(';', valueStart);
    const auto text = payload.substr(valueStart, end == std::string_view::npos ? payload.size() - valueStart : end - valueStart);
    try {
        const auto value = static_cast<LogicalId>(std::stoull(std::string(text)));
        return value == 0 ? std::nullopt : std::optional<LogicalId>{value};
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace gco::vehicle
