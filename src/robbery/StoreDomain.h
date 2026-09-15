#pragma once

#include "CoreServices.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gco::robbery {

enum class ClerkPersonality : std::uint8_t {
    Cowardly,
    Compliant,
    Defiant,
    Panicky,
    Armed,
    Experienced,
    Reckless,
    Count
};

enum class StoreDemand : std::uint8_t {
    OpenRegister,
    OpenSecondRegister,
    EmptySafe,
    HandsUp,
    GetDown,
    MoveAwayFromAlarm,
    DontMove
};

enum class ClerkReaction : std::uint8_t {
    HandsUp,
    Cower,
    Flee,
    PhoneReport,
    SecretAlarm,
    ArmedResistance,
    HoldPosition
};

enum class CashSourceState : std::uint8_t {
    Available,
    Dispensing,
    PartiallyDispensed,
    Empty,
    Unavailable
};

enum class RobberySessionState : std::uint8_t {
    Idle,
    ThreatDetected,
    Active,
    Aborting,
    Completed
};

enum class RobberyAbortReason : std::uint8_t {
    None,
    PlayerLeft,
    ClerkInvalid,
    ClerkDied,
    ClerkFled,
    MissionSuspended,
    SessionTimeout,
    TaskInterrupted,
    RuntimeShutdown
};

enum class StoreActionKind : std::uint8_t {
    None,
    HandsUp,
    Cower,
    Flee,
    BeginPhoneReport,
    TriggerSecretAlarm,
    FightPlayer,
    MoveToCashSource,
    OfferCash,
    RefuseDemand,
    HoldPosition,
    AbortSession
};

struct ClerkProfile final {
    LogicalId id = 0;
    ClerkPersonality personality = ClerkPersonality::Compliant;
    bool alive = true;
    std::uint32_t generation = 1;
    std::uint64_t createdAtMs = 0;
    std::uint64_t diedAtMs = 0;
    float fear = 0.5f;
    float compliance = 0.7f;
    float alarmTendency = 0.3f;
    float resistance = 0.1f;
    float withholdingTendency = 0.1f;
};

struct StorePersistentState final {
    static constexpr std::uint32_t ModelVersion = 1;

    LogicalId businessId = 0;
    std::string targetKey = "prototype_24_7";
    ClerkProfile clerk{};
    std::uint32_t robberyCount = 0;
    std::uint64_t lastRobberyAtMs = 0;
    std::uint64_t lastCaseId = 0;
    std::int64_t totalCashExposed = 0;
    bool clerkVacant = false;
    std::uint64_t replacementEligibleAtMs = 0;
    std::uint64_t recoveryUntilMs = 0;
};

struct CashSource final {
    std::string id;
    bool safe = false;
    CashSourceState state = CashSourceState::Available;
    int totalAmount = 0;
    int remainingAmount = 0;
    int withheldAmount = 0;
    int offeredAmount = 0;
};

struct RobberySession final {
    RobberySessionState state = RobberySessionState::Idle;
    RobberyAbortReason abortReason = RobberyAbortReason::None;
    LogicalId crimeId = 0;
    LogicalId caseId = 0;
    LogicalId clerkId = 0;
    std::uint64_t threatStartedAtMs = 0;
    std::uint64_t startedAtMs = 0;
    std::uint64_t lastProgressAtMs = 0;
    std::uint64_t playerLeftAtMs = 0;
    std::uint64_t alarmDueAtMs = 0;
    std::uint64_t reportDueAtMs = 0;
    std::uint64_t taskRefreshAtMs = 0;
    bool alarmOpportunitySuppressed = false;
    bool alarmTriggered = false;
    bool reportCompleted = false;
    bool clerkFled = false;
    bool clerkFought = false;
    ClerkReaction openingReaction = ClerkReaction::HandsUp;
    std::optional<StoreDemand> activeDemand;
};

struct StoreTuning final {
    std::uint32_t threatSustainMs = 1100;
    std::uint32_t threatDecayMs = 500;
    std::uint32_t sessionTimeoutMs = 120000;
    std::uint32_t playerLeaveGraceMs = 4000;
    std::uint32_t reportDelayMs = 3000;
    std::uint32_t secretAlarmDelayMs = 1800;
    std::uint32_t taskRefreshMs = 2500;
    std::uint64_t clerkReplacementDelayMs = 6ull * 60ull * 60ull * 1000ull;
    std::uint64_t businessRecoveryMs = 20ull * 60ull * 1000ull;
    std::array<float, static_cast<std::size_t>(ClerkPersonality::Count)> personalityWeights{
        0.16f, 0.24f, 0.14f, 0.14f, 0.10f, 0.14f, 0.08f};
};

struct StoreAction final {
    StoreActionKind kind = StoreActionKind::None;
    std::size_t cashSourceIndex = 0;
    int amount = 0;
    std::string reason;
};

struct DemandResult final {
    bool accepted = false;
    bool usesCashSource = false;
    std::size_t cashSourceIndex = 0;
    StoreAction action{};
};

class PrototypeStoreModel final {
public:
    void initializePersistent(LogicalIdGenerator& ids, std::uint64_t nowMs, const StoreTuning& tuning);
    bool ensureReplacementClerk(LogicalIdGenerator& ids, std::uint64_t nowMs, const StoreTuning& tuning);
    void markClerkDead(std::uint64_t nowMs, const StoreTuning& tuning);

    void beginThreat(std::uint64_t nowMs);
    void clearThreat();
    [[nodiscard]] bool threatSustained(std::uint64_t nowMs, std::uint32_t sustainMs) const noexcept;

    bool beginSession(
        LogicalId crimeId,
        LogicalId caseId,
        bool physicalClerkArmed,
        std::uint64_t nowMs,
        const StoreTuning& tuning,
        std::vector<CashSource> cashSources);

    DemandResult issueDemand(StoreDemand demand, std::uint64_t nowMs);
    std::optional<StoreAction> tick(std::uint64_t nowMs, const StoreTuning& tuning, bool threatMaintained);
    void markReportCompleted(std::uint64_t nowMs);
    void markClerkFled(std::uint64_t nowMs);
    void markClerkFought(std::uint64_t nowMs);
    void abort(RobberyAbortReason reason, std::uint64_t nowMs);
    void complete(std::uint64_t nowMs, const StoreTuning& tuning);

    [[nodiscard]] StorePersistentState& persistent() noexcept { return persistent_; }
    [[nodiscard]] const StorePersistentState& persistent() const noexcept { return persistent_; }
    [[nodiscard]] RobberySession& session() noexcept { return session_; }
    [[nodiscard]] const RobberySession& session() const noexcept { return session_; }
    [[nodiscard]] std::vector<CashSource>& cashSources() noexcept { return cashSources_; }
    [[nodiscard]] const std::vector<CashSource>& cashSources() const noexcept { return cashSources_; }
    [[nodiscard]] bool hasActiveSession() const noexcept {
        return session_.state == RobberySessionState::ThreatDetected
            || session_.state == RobberySessionState::Active
            || session_.state == RobberySessionState::Aborting;
    }

    void restorePersistent(StorePersistentState state) { persistent_ = std::move(state); }

    static ClerkProfile makeClerkProfile(
        LogicalId id,
        std::uint32_t generation,
        std::uint64_t nowMs,
        const StoreTuning& tuning,
        std::uint64_t seed);
    static ClerkReaction chooseOpeningReaction(
        const ClerkProfile& profile,
        bool physicalClerkArmed,
        std::uint64_t seed) noexcept;
    static std::vector<CashSource> makeCashSources(
        std::size_t registerCount,
        bool hasSafe,
        int registerMin,
        int registerMax,
        int safeMin,
        int safeMax,
        const ClerkProfile& profile,
        std::uint64_t seed);

private:
    StoreAction cashDemand(std::size_t sourceIndex, std::uint64_t nowMs);

    StorePersistentState persistent_{};
    RobberySession session_{};
    std::vector<CashSource> cashSources_;
};

std::string_view clerkPersonalityName(ClerkPersonality value) noexcept;
std::optional<ClerkPersonality> clerkPersonalityFromString(std::string_view value) noexcept;
std::string_view storeDemandName(StoreDemand value) noexcept;
std::string_view clerkReactionName(ClerkReaction value) noexcept;
std::string_view sessionStateName(RobberySessionState value) noexcept;
std::string_view abortReasonName(RobberyAbortReason value) noexcept;

} // namespace gco::robbery
