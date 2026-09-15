#include "VehicleIdentitySystem.h"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace gco::vehicle {
namespace {

float clampConfidence(const float value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

std::optional<int> parseIntField(const std::string_view value) {
    int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return std::nullopt;
    return parsed;
}

std::optional<std::uint32_t> parseHexHash(const std::string_view value) {
    std::string_view text = value;
    if (text.size() > 2 && text.substr(0, 2) == "0x") text.remove_prefix(2);
    std::uint32_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 16);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return std::nullopt;
    return parsed;
}

std::optional<std::string_view> descriptorField(const std::string_view descriptor, const std::string_view key) {
    const std::string prefix = std::string(key) + "=";
    const auto start = descriptor.find(prefix);
    if (start == std::string_view::npos) return std::nullopt;
    const auto valueStart = start + prefix.size();
    const auto end = descriptor.find(';', valueStart);
    return descriptor.substr(valueStart, end == std::string_view::npos ? descriptor.size() - valueStart : end - valueStart);
}

bool sameObservedBoloFacts(const VehicleBoloState& a, const VehicleBoloState& b) noexcept {
    return a.active == b.active
        && a.modelHash == b.modelHash
        && a.primaryColor == b.primaryColor
        && a.secondaryColor == b.secondaryColor
        && a.plateText == b.plateText
        && a.modelConfidence == b.modelConfidence
        && a.colorConfidence == b.colorConfidence
        && a.plateConfidence == b.plateConfidence;
}

} // namespace

OwnedVehicleRecord& VehicleIdentitySystem::registerVehicle(
    const VehicleRecordKind kind,
    VehicleAppearance appearance,
    VehicleModificationState modifications,
    const std::uint64_t nowMs) {

    OwnedVehicleRecord record{};
    record.id = ids_.next(LogicalIdDomain::Vehicle);
    record.kind = kind;
    record.appearance = std::move(appearance);
    record.modifications = std::move(modifications);
    record.storageStatus = VehicleStorageStatus::World;
    record.createdAtMs = nowMs;
    record.updatedAtMs = nowMs;
    if (kind == VehicleRecordKind::Stolen) record.crimeHistory.reportedStolen = true;
    records_.push_back(std::move(record));
    return records_.back();
}

OwnedVehicleRecord& VehicleIdentitySystem::ensureTemporaryVehicle(
    const platform::VehicleHandle liveHandle,
    const platform::VehicleSnapshot& snapshot,
    VehicleModificationState modifications,
    const bool stolen,
    const std::uint64_t nowMs) {

    if (const auto bound = logicalIdForHandle(liveHandle); bound.has_value()) {
        if (auto* existing = find(*bound); existing != nullptr) return *existing;
    }
    if (auto* existing = findByAppearance(snapshot); existing != nullptr) {
        bindLiveHandle(liveHandle, existing->id);
        return *existing;
    }

    VehicleAppearance appearance{};
    appearance.modelHash = snapshot.modelHash;
    appearance.primaryColor = snapshot.primaryColor;
    appearance.secondaryColor = snapshot.secondaryColor;
    appearance.plateText = snapshot.plate;
    appearance.plateStyle = snapshot.plateStyle;
    auto& record = registerVehicle(
        stolen ? VehicleRecordKind::Stolen : VehicleRecordKind::Temporary,
        std::move(appearance),
        std::move(modifications),
        nowMs);
    bindLiveHandle(liveHandle, record.id);
    return record;
}

bool VehicleIdentitySystem::bindLiveHandle(const platform::VehicleHandle handle, const LogicalId vehicleId) {
    if (handle == 0 || find(vehicleId) == nullptr) return false;
    liveToLogical_[handle] = vehicleId;
    return true;
}

void VehicleIdentitySystem::unbindLiveHandle(const platform::VehicleHandle handle) noexcept {
    liveToLogical_.erase(handle);
}

std::optional<LogicalId> VehicleIdentitySystem::logicalIdForHandle(const platform::VehicleHandle handle) const noexcept {
    const auto it = liveToLogical_.find(handle);
    return it == liveToLogical_.end() ? std::nullopt : std::optional<LogicalId>{it->second};
}

OwnedVehicleRecord* VehicleIdentitySystem::find(const LogicalId vehicleId) noexcept {
    const auto it = std::find_if(records_.begin(), records_.end(), [vehicleId](const OwnedVehicleRecord& record) {
        return record.id == vehicleId;
    });
    return it == records_.end() ? nullptr : &*it;
}

const OwnedVehicleRecord* VehicleIdentitySystem::find(const LogicalId vehicleId) const noexcept {
    const auto it = std::find_if(records_.begin(), records_.end(), [vehicleId](const OwnedVehicleRecord& record) {
        return record.id == vehicleId;
    });
    return it == records_.end() ? nullptr : &*it;
}

bool VehicleIdentitySystem::updateAppearanceFromLive(
    const LogicalId vehicleId,
    const platform::VehicleSnapshot& snapshot,
    const std::uint64_t nowMs) {

    auto* record = find(vehicleId);
    if (record == nullptr) return false;
    record->appearance.modelHash = snapshot.modelHash;
    record->appearance.primaryColor = snapshot.primaryColor;
    record->appearance.secondaryColor = snapshot.secondaryColor;
    record->appearance.plateText = snapshot.plate;
    record->appearance.plateStyle = snapshot.plateStyle;
    record->updatedAtMs = nowMs;
    return true;
}

bool VehicleIdentitySystem::changePlate(
    const LogicalId vehicleId,
    std::string normalizedPlate,
    const int plateStyle,
    const std::uint64_t nowMs) {

    auto* record = find(vehicleId);
    if (record == nullptr) return false;
    const auto validation = validatePlateText(normalizedPlate);
    if (!validation.valid) return false;
    record->appearance.plateText = validation.normalized;
    record->appearance.plateStyle = plateStyle;
    record->updatedAtMs = nowMs;
    return true;
}

bool VehicleIdentitySystem::repaint(
    const LogicalId vehicleId,
    const int primaryColor,
    const int secondaryColor,
    const std::uint64_t nowMs) {

    auto* record = find(vehicleId);
    if (record == nullptr) return false;
    record->appearance.primaryColor = primaryColor;
    record->appearance.secondaryColor = secondaryColor;
    record->updatedAtMs = nowMs;
    return true;
}

bool VehicleIdentitySystem::setStorageStatus(
    const LogicalId vehicleId,
    const VehicleStorageStatus status,
    std::string storageKey,
    const std::uint64_t nowMs) {

    auto* record = find(vehicleId);
    if (record == nullptr) return false;
    record->storageStatus = status;
    record->storageKey = std::move(storageKey);
    record->updatedAtMs = nowMs;
    return true;
}

bool VehicleIdentitySystem::markCrimeAssociation(
    const LogicalId vehicleId,
    const LogicalId caseId,
    const bool getaway,
    const std::uint64_t nowMs) {

    auto* record = find(vehicleId);
    if (record == nullptr || logicalIdDomain(caseId) != LogicalIdDomain::Case) return false;
    record->crimeHistory.usedInCrime = true;
    record->crimeHistory.usedAsGetaway = record->crimeHistory.usedAsGetaway || getaway;
    ++record->crimeHistory.crimeCount;
    record->crimeHistory.lastCrimeAtMs = nowMs;
    if (std::find(record->crimeHistory.caseIds.begin(), record->crimeHistory.caseIds.end(), caseId)
        == record->crimeHistory.caseIds.end()) {
        record->crimeHistory.caseIds.push_back(caseId);
    }
    record->updatedAtMs = nowMs;
    return true;
}

bool VehicleIdentitySystem::refreshBoloFromCase(
    const crime::CaseFile& file,
    const std::optional<LogicalId> linkedVehicleId,
    const std::uint64_t nowMs) {

    VehicleBoloState rebuilt{};
    rebuilt.caseId = file.id;
    rebuilt.active = false;

    for (const auto& evidence : file.evidence) {
        if (evidence.kind == crime::EvidenceKind::Vehicle) {
            std::uint32_t model = 0;
            int primary = 0;
            int secondary = 0;
            if (parseVehicleDescriptor(evidence.snapshot.descriptor, model, primary, secondary)) {
                rebuilt.modelHash = model;
                rebuilt.primaryColor = primary;
                rebuilt.secondaryColor = secondary;
                rebuilt.modelConfidence = std::max(rebuilt.modelConfidence, clampConfidence(evidence.confidence));
                rebuilt.colorConfidence = std::max(rebuilt.colorConfidence, clampConfidence(evidence.confidence));
                rebuilt.active = true;
            }
        } else if (evidence.kind == crime::EvidenceKind::Plate) {
            const auto plate = parsePlateDescriptor(evidence.snapshot.descriptor);
            if (plate.has_value()) {
                rebuilt.plateText = *plate;
                rebuilt.plateConfidence = std::max(rebuilt.plateConfidence, clampConfidence(evidence.confidence));
                rebuilt.active = true;
            }
        }
    }

    if (!rebuilt.active) return false;
    const auto it = std::find_if(bolos_.begin(), bolos_.end(), [caseId = file.id](const VehicleBoloState& value) {
        return value.caseId == caseId;
    });

    if (it == bolos_.end()) {
        rebuilt.linkedVehicleId = linkedVehicleId;
        rebuilt.physicalContinuityKnown = linkedVehicleId.has_value();
        rebuilt.updatedAtMs = nowMs;
        bolos_.push_back(std::move(rebuilt));
        return true;
    }

    // Rebuilding evidence facts must never erase a physical-continuity result established by an
    // observed swap. A null linkedVehicleId means "no new continuity information", not "forget it".
    rebuilt.linkedVehicleId = linkedVehicleId.has_value() ? linkedVehicleId : it->linkedVehicleId;
    rebuilt.physicalContinuityKnown = linkedVehicleId.has_value() ? true : it->physicalContinuityKnown;
    if (sameObservedBoloFacts(*it, rebuilt)
        && it->linkedVehicleId == rebuilt.linkedVehicleId
        && it->physicalContinuityKnown == rebuilt.physicalContinuityKnown) {
        return true;
    }
    rebuilt.updatedAtMs = nowMs;
    *it = std::move(rebuilt);
    return true;
}

const VehicleBoloState* VehicleIdentitySystem::findBolo(const LogicalId caseId) const noexcept {
    const auto it = std::find_if(bolos_.begin(), bolos_.end(), [caseId](const VehicleBoloState& value) {
        return value.caseId == caseId;
    });
    return it == bolos_.end() ? nullptr : &*it;
}

bool VehicleIdentitySystem::deactivateBolo(const LogicalId caseId, const std::uint64_t nowMs) {
    auto it = std::find_if(bolos_.begin(), bolos_.end(), [caseId](const VehicleBoloState& value) {
        return value.caseId == caseId;
    });
    if (it == bolos_.end()) return false;
    it->active = false;
    it->updatedAtMs = nowMs;
    return true;
}

VehicleSwapResult VehicleIdentitySystem::recordVehicleSwap(
    const LogicalId caseId,
    const std::optional<LogicalId> previousVehicleId,
    const std::optional<LogicalId> currentVehicleId,
    const bool swapObserved,
    const std::uint64_t nowMs) {

    VehicleSwapResult result{};
    result.previousVehicleId = previousVehicleId;
    result.currentVehicleId = currentVehicleId;
    auto it = std::find_if(bolos_.begin(), bolos_.end(), [caseId](const VehicleBoloState& value) {
        return value.caseId == caseId;
    });
    if (it == bolos_.end()) return result;

    if (swapObserved && currentVehicleId.has_value()) {
        it->linkedVehicleId = currentVehicleId;
        it->physicalContinuityKnown = true;
        result.continuityPreserved = true;
    } else {
        it->physicalContinuityKnown = false;
        it->linkedVehicleId.reset();
        result.continuityPreserved = false;
    }
    it->updatedAtMs = nowMs;
    return result;
}

void VehicleIdentitySystem::restore(
    std::vector<OwnedVehicleRecord> records,
    std::vector<VehicleBoloState> bolos) {

    records_ = std::move(records);
    bolos_ = std::move(bolos);
    liveToLogical_.clear();

    // world.json owns the global monotonic counters. Vehicle persistence may raise that counter
    // when it contains a higher historical ID, but it must never lower the already-restored value.
    std::uint64_t nextVehicle = ids_.nextSequence(LogicalIdDomain::Vehicle);
    for (const auto& record : records_) {
        if (logicalIdDomain(record.id) == LogicalIdDomain::Vehicle) {
            nextVehicle = std::max(nextVehicle, logicalIdSequence(record.id) + 1);
        }
    }
    ids_.setNextSequence(LogicalIdDomain::Vehicle, nextVehicle);
}

std::string VehicleIdentitySystem::debugSummary() const {
    std::ostringstream out;
    out << "VehicleIdentity: records=" << records_.size()
        << ";bolos=" << bolos_.size()
        << ";liveBindings=" << liveToLogical_.size();
    for (const auto& record : records_) {
        out << "\n - id=" << record.id
            << ";kind=" << vehicleRecordKindName(record.kind)
            << ";model=0x" << std::hex << std::uppercase << record.appearance.modelHash << std::dec
            << ";plate=" << record.appearance.plateText
            << ";colors=" << record.appearance.primaryColor << ',' << record.appearance.secondaryColor
            << ";status=" << vehicleStorageStatusName(record.storageStatus)
            << ";crimeCount=" << record.crimeHistory.crimeCount;
    }
    return out.str();
}

OwnedVehicleRecord* VehicleIdentitySystem::findByAppearance(const platform::VehicleSnapshot& snapshot) noexcept {
    const auto it = std::find_if(records_.begin(), records_.end(), [&snapshot](const OwnedVehicleRecord& record) {
        return record.appearance.modelHash == snapshot.modelHash
            && record.appearance.plateText == snapshot.plate
            && record.storageStatus != VehicleStorageStatus::Destroyed;
    });
    return it == records_.end() ? nullptr : &*it;
}

bool VehicleIdentitySystem::parseVehicleDescriptor(
    const std::string_view descriptor,
    std::uint32_t& modelHash,
    int& primaryColor,
    int& secondaryColor) {

    const auto model = descriptorField(descriptor, "model");
    const auto primary = descriptorField(descriptor, "primaryColor");
    const auto secondary = descriptorField(descriptor, "secondaryColor");
    if (!model || !primary || !secondary) return false;
    const auto parsedModel = parseHexHash(*model);
    const auto parsedPrimary = parseIntField(*primary);
    const auto parsedSecondary = parseIntField(*secondary);
    if (!parsedModel || !parsedPrimary || !parsedSecondary) return false;
    modelHash = *parsedModel;
    primaryColor = *parsedPrimary;
    secondaryColor = *parsedSecondary;
    return true;
}

std::optional<std::string> VehicleIdentitySystem::parsePlateDescriptor(const std::string_view descriptor) {
    const auto plate = descriptorField(descriptor, "plate");
    if (!plate || plate->empty()) return std::nullopt;
    return std::string(*plate);
}

} // namespace gco::vehicle
