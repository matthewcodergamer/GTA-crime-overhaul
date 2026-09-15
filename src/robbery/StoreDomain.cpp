#include "StoreDomain.h"

#include <algorithm>
#include <cmath>

namespace gco::robbery {
namespace {

std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31U);
}

float unitRoll(const std::uint64_t seed, const std::uint64_t salt) noexcept {
    const auto mixed = mix64(seed ^ salt);
    return static_cast<float>(mixed & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

int boundedAmount(const int minValue, const int maxValue, const std::uint64_t seed) noexcept {
    const int lo = std::max(0, std::min(minValue, maxValue));
    const int hi = std::max(lo, std::max(minValue, maxValue));
    if (lo == hi) return lo;
    const auto span = static_cast<std::uint64_t>(hi - lo + 1);
    return lo + static_cast<int>(mix64(seed) % span);
}

float clamp01(const float value) noexcept {
    if (!std::isfinite(value)) return 0.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

ClerkPersonality choosePersonality(const StoreTuning& tuning, const std::uint64_t seed) noexcept {
    float total = 0.0f;
    for (const float weight : tuning.personalityWeights) {
        if (std::isfinite(weight) && weight > 0.0f) total += weight;
    }
    if (total <= 0.0f) return ClerkPersonality::Compliant;

    float cursor = unitRoll(seed, 0xA11CEull) * total;
    for (std::size_t index = 0; index < tuning.personalityWeights.size(); ++index) {
        const float weight = std::max(0.0f, tuning.personalityWeights[index]);
        if (cursor <= weight) return static_cast<ClerkPersonality>(index);
        cursor -= weight;
    }
    return ClerkPersonality::Compliant;
}

void applyPersonalityStats(ClerkProfile& profile) noexcept {
    switch (profile.personality) {
    case ClerkPersonality::Cowardly:
        profile.fear = 0.90f; profile.compliance = 0.96f; profile.alarmTendency = 0.10f;
        profile.resistance = 0.01f; profile.withholdingTendency = 0.04f; break;
    case ClerkPersonality::Compliant:
        profile.fear = 0.66f; profile.compliance = 0.86f; profile.alarmTendency = 0.20f;
        profile.resistance = 0.04f; profile.withholdingTendency = 0.08f; break;
    case ClerkPersonality::Defiant:
        profile.fear = 0.46f; profile.compliance = 0.42f; profile.alarmTendency = 0.42f;
        profile.resistance = 0.24f; profile.withholdingTendency = 0.52f; break;
    case ClerkPersonality::Panicky:
        profile.fear = 0.98f; profile.compliance = 0.52f; profile.alarmTendency = 0.34f;
        profile.resistance = 0.02f; profile.withholdingTendency = 0.10f; break;
    case ClerkPersonality::Armed:
        profile.fear = 0.42f; profile.compliance = 0.50f; profile.alarmTendency = 0.24f;
        profile.resistance = 0.72f; profile.withholdingTendency = 0.20f; break;
    case ClerkPersonality::Experienced:
        profile.fear = 0.54f; profile.compliance = 0.74f; profile.alarmTendency = 0.72f;
        profile.resistance = 0.12f; profile.withholdingTendency = 0.24f; break;
    case ClerkPersonality::Reckless:
        profile.fear = 0.32f; profile.compliance = 0.34f; profile.alarmTendency = 0.46f;
        profile.resistance = 0.66f; profile.withholdingTendency = 0.36f; break;
    case ClerkPersonality::Count:
        profile.personality = ClerkPersonality::Compliant;
        applyPersonalityStats(profile);
        break;
    }
}

} // namespace

void PrototypeStoreModel::initializePersistent(LogicalIdGenerator& ids, const std::uint64_t nowMs, const StoreTuning& tuning) {
    if (persistent_.businessId == 0) persistent_.businessId = ids.next(LogicalIdDomain::Business);
    if (persistent_.clerk.id == 0 && !persistent_.clerkVacant) {
        const auto clerkId = ids.next(LogicalIdDomain::Clerk);
        persistent_.clerk = makeClerkProfile(clerkId, 1, nowMs, tuning, clerkId ^ persistent_.businessId);
    }
}

bool PrototypeStoreModel::ensureReplacementClerk(LogicalIdGenerator& ids, const std::uint64_t nowMs, const StoreTuning& tuning) {
    if (!persistent_.clerkVacant || nowMs < persistent_.replacementEligibleAtMs) return false;
    const std::uint32_t nextGeneration = std::max<std::uint32_t>(1, persistent_.clerk.generation + 1);
    const auto newId = ids.next(LogicalIdDomain::Clerk);
    if (newId == 0) return false;
    persistent_.clerk = makeClerkProfile(newId, nextGeneration, nowMs, tuning,
        newId ^ persistent_.businessId ^ static_cast<std::uint64_t>(nextGeneration));
    persistent_.clerkVacant = false;
    persistent_.replacementEligibleAtMs = 0;
    return true;
}

void PrototypeStoreModel::markClerkDead(const std::uint64_t nowMs, const StoreTuning& tuning) {
    if (persistent_.clerk.id != 0) {
        persistent_.clerk.alive = false;
        persistent_.clerk.diedAtMs = nowMs;
    }
    persistent_.clerkVacant = true;
    persistent_.replacementEligibleAtMs = nowMs + tuning.clerkReplacementDelayMs;
    if (hasActiveSession()) abort(RobberyAbortReason::ClerkDied, nowMs);
}

void PrototypeStoreModel::beginThreat(const std::uint64_t nowMs) {
    if (session_.state == RobberySessionState::Idle) {
        session_ = {};
        session_.state = RobberySessionState::ThreatDetected;
        session_.threatStartedAtMs = nowMs;
        session_.lastProgressAtMs = nowMs;
    }
}

void PrototypeStoreModel::clearThreat() {
    if (session_.state == RobberySessionState::ThreatDetected) session_ = {};
}

bool PrototypeStoreModel::threatSustained(const std::uint64_t nowMs, const std::uint32_t sustainMs) const noexcept {
    return session_.state == RobberySessionState::ThreatDetected
        && nowMs >= session_.threatStartedAtMs
        && nowMs - session_.threatStartedAtMs >= sustainMs;
}

bool PrototypeStoreModel::beginSession(
    const LogicalId crimeId,
    const LogicalId caseId,
    const bool physicalClerkArmed,
    const std::uint64_t nowMs,
    const StoreTuning& tuning,
    std::vector<CashSource> cashSources) {

    if (persistent_.clerk.id == 0 || !persistent_.clerk.alive || crimeId == 0 || caseId == 0
        || (session_.state != RobberySessionState::ThreatDetected && session_.state != RobberySessionState::Idle)) return false;

    session_.state = RobberySessionState::Active;
    session_.crimeId = crimeId;
    session_.caseId = caseId;
    session_.clerkId = persistent_.clerk.id;
    session_.startedAtMs = nowMs;
    session_.lastProgressAtMs = nowMs;
    session_.openingReaction = chooseOpeningReaction(persistent_.clerk, physicalClerkArmed, crimeId ^ caseId ^ persistent_.clerk.id);
    session_.taskRefreshAtMs = nowMs + tuning.taskRefreshMs;
    if (session_.openingReaction == ClerkReaction::SecretAlarm) session_.alarmDueAtMs = nowMs + tuning.secretAlarmDelayMs;
    if (session_.openingReaction == ClerkReaction::PhoneReport) session_.reportDueAtMs = nowMs + tuning.reportDelayMs;

    cashSources_ = std::move(cashSources);
    ++persistent_.robberyCount;
    persistent_.lastRobberyAtMs = nowMs;
    persistent_.lastCaseId = caseId;
    return true;
}

DemandResult PrototypeStoreModel::issueDemand(const StoreDemand demand, const std::uint64_t nowMs) {
    DemandResult result{};
    if (session_.state != RobberySessionState::Active || !persistent_.clerk.alive) return result;
    session_.activeDemand = demand;
    session_.lastProgressAtMs = nowMs;

    switch (demand) {
    case StoreDemand::OpenRegister:
        result.usesCashSource = true; result.cashSourceIndex = 0; result.action = cashDemand(0, nowMs);
        result.accepted = result.action.kind != StoreActionKind::RefuseDemand; return result;
    case StoreDemand::OpenSecondRegister:
        result.usesCashSource = true; result.cashSourceIndex = 1; result.action = cashDemand(1, nowMs);
        result.accepted = result.action.kind != StoreActionKind::RefuseDemand; return result;
    case StoreDemand::EmptySafe: {
        auto found = std::find_if(cashSources_.begin(), cashSources_.end(), [](const CashSource& source) { return source.safe; });
        if (found == cashSources_.end()) {
            result.action = StoreAction{StoreActionKind::RefuseDemand, 0, 0, "no configured safe cash source"};
            return result;
        }
        result.usesCashSource = true;
        result.cashSourceIndex = static_cast<std::size_t>(std::distance(cashSources_.begin(), found));
        result.action = cashDemand(result.cashSourceIndex, nowMs);
        result.accepted = result.action.kind != StoreActionKind::RefuseDemand;
        return result;
    }
    case StoreDemand::HandsUp:
        result.accepted = true; result.action = {StoreActionKind::HandsUp, 0, 0, "explicit hands-up demand"}; return result;
    case StoreDemand::GetDown:
        result.accepted = true; result.action = {StoreActionKind::Cower, 0, 0, "explicit get-down demand"}; return result;
    case StoreDemand::MoveAwayFromAlarm:
        if (!session_.alarmTriggered) { session_.alarmOpportunitySuppressed = true; session_.alarmDueAtMs = 0; }
        result.accepted = true; result.action = {StoreActionKind::HandsUp, 0, 0, "alarm opportunity suppressed before trigger"}; return result;
    case StoreDemand::DontMove:
        result.accepted = true; result.action = {StoreActionKind::HoldPosition, 0, 0, "hold-position demand"}; return result;
    }
    return result;
}

std::optional<StoreAction> PrototypeStoreModel::tick(const std::uint64_t nowMs, const StoreTuning& tuning, const bool threatMaintained) {
    if (session_.state != RobberySessionState::Active) return std::nullopt;
    if (nowMs >= session_.startedAtMs && nowMs - session_.startedAtMs >= tuning.sessionTimeoutMs) {
        abort(RobberyAbortReason::SessionTimeout, nowMs);
        return StoreAction{StoreActionKind::AbortSession, 0, 0, "session timeout"};
    }
    if (session_.alarmDueAtMs != 0 && nowMs >= session_.alarmDueAtMs
        && !session_.alarmTriggered && !session_.alarmOpportunitySuppressed) {
        session_.alarmTriggered = true;
        session_.alarmDueAtMs = 0;
        session_.reportDueAtMs = nowMs + tuning.reportDelayMs;
        session_.lastProgressAtMs = nowMs;
        return StoreAction{StoreActionKind::TriggerSecretAlarm, 0, 0, "personality-triggered secret alarm"};
    }
    if (session_.reportDueAtMs != 0 && nowMs >= session_.reportDueAtMs && !session_.reportCompleted) {
        session_.reportDueAtMs = 0;
        session_.lastProgressAtMs = nowMs;
        return StoreAction{StoreActionKind::BeginPhoneReport, 0, 0, "report timer completed"};
    }
    if (session_.openingReaction == ClerkReaction::ArmedResistance && !session_.clerkFought && !threatMaintained) {
        session_.clerkFought = true;
        session_.lastProgressAtMs = nowMs;
        return StoreAction{StoreActionKind::FightPlayer, 0, 0, "armed clerk found an opening"};
    }
    if (nowMs >= session_.taskRefreshAtMs) {
        session_.taskRefreshAtMs = nowMs + tuning.taskRefreshMs;
        switch (session_.openingReaction) {
        case ClerkReaction::HandsUp:
        case ClerkReaction::SecretAlarm: return StoreAction{StoreActionKind::HandsUp, 0, 0, "bounded task refresh"};
        case ClerkReaction::Cower:
        case ClerkReaction::PhoneReport: return StoreAction{StoreActionKind::Cower, 0, 0, "bounded task refresh/fallback"};
        case ClerkReaction::Flee:
            if (!session_.clerkFled) return StoreAction{StoreActionKind::Flee, 0, 0, "flee reaction refresh"};
            break;
        case ClerkReaction::ArmedResistance:
            if (!session_.clerkFought) return StoreAction{StoreActionKind::HandsUp, 0, 0, "armed clerk waiting for opening"};
            break;
        case ClerkReaction::HoldPosition: return StoreAction{StoreActionKind::HoldPosition, 0, 0, "bounded hold refresh"};
        }
    }
    return std::nullopt;
}

void PrototypeStoreModel::markReportCompleted(const std::uint64_t nowMs) {
    if (session_.state != RobberySessionState::Active) return;
    session_.reportCompleted = true;
    session_.reportDueAtMs = 0;
    session_.lastProgressAtMs = nowMs;
}

void PrototypeStoreModel::markClerkFled(const std::uint64_t nowMs) {
    session_.clerkFled = true;
    session_.lastProgressAtMs = nowMs;
    abort(RobberyAbortReason::ClerkFled, nowMs);
}

void PrototypeStoreModel::markClerkFought(const std::uint64_t nowMs) {
    session_.clerkFought = true;
    session_.lastProgressAtMs = nowMs;
}

void PrototypeStoreModel::abort(const RobberyAbortReason reason, const std::uint64_t nowMs) {
    if (session_.state == RobberySessionState::Idle || session_.state == RobberySessionState::Completed) return;
    session_.state = RobberySessionState::Aborting;
    session_.abortReason = reason;
    session_.lastProgressAtMs = nowMs;
}

void PrototypeStoreModel::complete(const std::uint64_t nowMs, const StoreTuning& tuning) {
    int exposed = 0;
    for (const auto& source : cashSources_) exposed += source.offeredAmount;
    persistent_.totalCashExposed += exposed;
    persistent_.recoveryUntilMs = std::max(persistent_.recoveryUntilMs, nowMs + tuning.businessRecoveryMs);
    session_.state = RobberySessionState::Completed;
    session_.lastProgressAtMs = nowMs;
}

ClerkProfile PrototypeStoreModel::makeClerkProfile(
    const LogicalId id,
    const std::uint32_t generation,
    const std::uint64_t nowMs,
    const StoreTuning& tuning,
    const std::uint64_t seed) {
    ClerkProfile profile{};
    profile.id = id; profile.generation = generation; profile.createdAtMs = nowMs; profile.alive = true;
    profile.personality = choosePersonality(tuning, seed);
    applyPersonalityStats(profile);
    return profile;
}

ClerkReaction PrototypeStoreModel::chooseOpeningReaction(
    const ClerkProfile& profile,
    const bool physicalClerkArmed,
    const std::uint64_t seed) noexcept {

    const float roll = unitRoll(seed, 0xBEEF22ull);
    switch (profile.personality) {
    case ClerkPersonality::Cowardly: return ClerkReaction::HandsUp;
    case ClerkPersonality::Compliant: return roll < 0.18f ? ClerkReaction::Cower : ClerkReaction::HandsUp;
    case ClerkPersonality::Defiant: return roll < profile.alarmTendency ? ClerkReaction::SecretAlarm : ClerkReaction::HandsUp;
    case ClerkPersonality::Panicky:
        if (roll < 0.34f) return ClerkReaction::Flee;
        if (roll < 0.64f) return ClerkReaction::PhoneReport;
        return ClerkReaction::Cower;
    case ClerkPersonality::Armed:
        return physicalClerkArmed && roll < profile.resistance ? ClerkReaction::ArmedResistance : ClerkReaction::HandsUp;
    case ClerkPersonality::Experienced:
        if (roll < profile.alarmTendency) return ClerkReaction::SecretAlarm;
        if (roll < 0.88f) return ClerkReaction::PhoneReport;
        return ClerkReaction::HandsUp;
    case ClerkPersonality::Reckless:
        if (physicalClerkArmed && roll < profile.resistance) return ClerkReaction::ArmedResistance;
        return roll < 0.45f ? ClerkReaction::Flee : ClerkReaction::SecretAlarm;
    case ClerkPersonality::Count: break;
    }
    return ClerkReaction::HandsUp;
}

std::vector<CashSource> PrototypeStoreModel::makeCashSources(
    const std::size_t registerCount,
    const bool hasSafe,
    const int registerMin,
    const int registerMax,
    const int safeMin,
    const int safeMax,
    const ClerkProfile& profile,
    const std::uint64_t seed) {

    std::vector<CashSource> result;
    result.reserve(registerCount + (hasSafe ? 1u : 0u));
    for (std::size_t index = 0; index < registerCount; ++index) {
        CashSource source{};
        source.id = "register_" + std::to_string(index + 1);
        source.totalAmount = boundedAmount(registerMin, registerMax, seed ^ (0x1000ull + index));
        source.remainingAmount = source.totalAmount;
        const float withholdRoll = unitRoll(seed, 0x2000ull + index);
        if (withholdRoll < clamp01(profile.withholdingTendency)) {
            source.withheldAmount = std::max(1, static_cast<int>(source.totalAmount * (0.20f + 0.35f * withholdRoll)));
        }
        result.push_back(std::move(source));
    }
    if (hasSafe) {
        CashSource source{};
        source.id = "safe"; source.safe = true;
        source.totalAmount = boundedAmount(safeMin, safeMax, seed ^ 0x7777ull);
        source.remainingAmount = source.totalAmount;
        const float withholdRoll = unitRoll(seed, 0x8888ull);
        if (withholdRoll < clamp01(profile.withholdingTendency + 0.15f)) {
            source.withheldAmount = std::max(1, static_cast<int>(source.totalAmount * (0.25f + 0.30f * withholdRoll)));
        }
        result.push_back(std::move(source));
    }
    return result;
}

StoreAction PrototypeStoreModel::cashDemand(const std::size_t sourceIndex, const std::uint64_t nowMs) {
    if (sourceIndex >= cashSources_.size()) return {StoreActionKind::RefuseDemand, sourceIndex, 0, "cash source not configured"};
    auto& source = cashSources_[sourceIndex];
    if (source.state == CashSourceState::Empty || source.remainingAmount <= 0) {
        source.state = CashSourceState::Empty;
        return {StoreActionKind::RefuseDemand, sourceIndex, 0, "cash source already empty"};
    }
    if (source.state == CashSourceState::Unavailable) return {StoreActionKind::RefuseDemand, sourceIndex, 0, "cash source unavailable"};
    if (source.state == CashSourceState::PartiallyDispensed) {
        return {StoreActionKind::RefuseDemand, sourceIndex, 0, "clerk claims source is empty after partial payout"};
    }

    const float refusalRoll = unitRoll(session_.crimeId ^ session_.clerkId, 0xCA55ull + sourceIndex);
    if (refusalRoll > persistent_.clerk.compliance && persistent_.clerk.personality != ClerkPersonality::Cowardly) {
        return {StoreActionKind::RefuseDemand, sourceIndex, 0, "personality refusal/stall"};
    }

    const int protectedAmount = std::clamp(source.withheldAmount, 0, source.remainingAmount);
    const int amount = std::max(0, source.remainingAmount - protectedAmount);
    if (amount <= 0) return {StoreActionKind::RefuseDemand, sourceIndex, 0, "clerk claims source is empty"};

    source.state = CashSourceState::Dispensing;
    source.offeredAmount += amount;
    source.remainingAmount -= amount;
    if (source.remainingAmount <= 0) {
        source.remainingAmount = 0;
        source.state = CashSourceState::Empty;
    } else {
        source.state = CashSourceState::PartiallyDispensed;
    }
    session_.lastProgressAtMs = nowMs;
    return {StoreActionKind::MoveToCashSource, sourceIndex, amount,
        protectedAmount > 0 ? "partial/withheld cash behavior" : "full cash-source compliance"};
}

std::string_view clerkPersonalityName(const ClerkPersonality value) noexcept {
    switch (value) {
    case ClerkPersonality::Cowardly: return "cowardly";
    case ClerkPersonality::Compliant: return "compliant";
    case ClerkPersonality::Defiant: return "defiant";
    case ClerkPersonality::Panicky: return "panicky";
    case ClerkPersonality::Armed: return "armed";
    case ClerkPersonality::Experienced: return "experienced";
    case ClerkPersonality::Reckless: return "reckless";
    case ClerkPersonality::Count: break;
    }
    return "unknown";
}

std::optional<ClerkPersonality> clerkPersonalityFromString(const std::string_view value) noexcept {
    for (std::size_t i = 0; i < static_cast<std::size_t>(ClerkPersonality::Count); ++i) {
        const auto candidate = static_cast<ClerkPersonality>(i);
        if (clerkPersonalityName(candidate) == value) return candidate;
    }
    return std::nullopt;
}

std::string_view storeDemandName(const StoreDemand value) noexcept {
    switch (value) {
    case StoreDemand::OpenRegister: return "open_register";
    case StoreDemand::OpenSecondRegister: return "open_second_register";
    case StoreDemand::EmptySafe: return "empty_safe";
    case StoreDemand::HandsUp: return "hands_up";
    case StoreDemand::GetDown: return "get_down";
    case StoreDemand::MoveAwayFromAlarm: return "move_away_from_alarm";
    case StoreDemand::DontMove: return "dont_move";
    }
    return "unknown";
}

std::string_view clerkReactionName(const ClerkReaction value) noexcept {
    switch (value) {
    case ClerkReaction::HandsUp: return "hands_up";
    case ClerkReaction::Cower: return "cower";
    case ClerkReaction::Flee: return "flee";
    case ClerkReaction::PhoneReport: return "phone_report";
    case ClerkReaction::SecretAlarm: return "secret_alarm";
    case ClerkReaction::ArmedResistance: return "armed_resistance";
    case ClerkReaction::HoldPosition: return "hold_position";
    }
    return "unknown";
}

std::string_view sessionStateName(const RobberySessionState value) noexcept {
    switch (value) {
    case RobberySessionState::Idle: return "idle";
    case RobberySessionState::ThreatDetected: return "threat_detected";
    case RobberySessionState::Active: return "active";
    case RobberySessionState::Aborting: return "aborting";
    case RobberySessionState::Completed: return "completed";
    }
    return "unknown";
}

std::string_view abortReasonName(const RobberyAbortReason value) noexcept {
    switch (value) {
    case RobberyAbortReason::None: return "none";
    case RobberyAbortReason::PlayerLeft: return "player_left";
    case RobberyAbortReason::ClerkInvalid: return "clerk_invalid";
    case RobberyAbortReason::ClerkDied: return "clerk_died";
    case RobberyAbortReason::ClerkFled: return "clerk_fled";
    case RobberyAbortReason::MissionSuspended: return "mission_suspended";
    case RobberyAbortReason::SessionTimeout: return "session_timeout";
    case RobberyAbortReason::TaskInterrupted: return "task_interrupted";
    case RobberyAbortReason::RuntimeShutdown: return "runtime_shutdown";
    }
    return "unknown";
}

} // namespace gco::robbery
