#pragma once

#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifndef GCO_VERSION
#define GCO_VERSION "0.0.0-dev"
#endif
#ifndef GCO_BUILD_CONFIG
#define GCO_BUILD_CONFIG "Unknown"
#endif
#ifndef GCO_GIT_SHA
#define GCO_GIT_SHA "unknown"
#endif

namespace gco {

struct BuildInfo final {
    static constexpr std::string_view ProjectName = "GTA Crime Overhaul";
    static constexpr std::string_view BinaryName = "GTA_Crime_Overhaul.asi";
    static constexpr std::string_view Version = GCO_VERSION;
    static constexpr std::string_view BuildConfig = GCO_BUILD_CONFIG;
    static constexpr std::string_view GitSha = GCO_GIT_SHA;
    static constexpr std::uint32_t SaveSchemaVersion = 1;
};

using LogicalId = std::uint64_t;

enum class LogicalIdDomain : std::uint8_t {
    Case = 1,
    Business = 2,
    Clerk = 3,
    Vehicle = 4,
    LootContainer = 5
};

constexpr std::uint64_t kLogicalIdSequenceMask = 0x00FFFFFFFFFFFFFFull;

constexpr LogicalId makeLogicalId(const LogicalIdDomain domain, const std::uint64_t sequence) noexcept {
    if (sequence == 0 || sequence > kLogicalIdSequenceMask) {
        return 0;
    }
    return (static_cast<LogicalId>(domain) << 56U) | sequence;
}

constexpr LogicalIdDomain logicalIdDomain(const LogicalId id) noexcept {
    return static_cast<LogicalIdDomain>((id >> 56U) & 0xFFU);
}

constexpr std::uint64_t logicalIdSequence(const LogicalId id) noexcept {
    return id & kLogicalIdSequenceMask;
}

class LogicalIdGenerator final {
public:
    LogicalId next(LogicalIdDomain domain);
    bool setNextSequence(LogicalIdDomain domain, std::uint64_t nextSequence) noexcept;
    [[nodiscard]] std::uint64_t nextSequence(LogicalIdDomain domain) const noexcept;

private:
    static std::size_t indexOf(LogicalIdDomain domain) noexcept;
    std::array<std::uint64_t, 5> nextSequences_{1, 1, 1, 1, 1};
};

struct RuntimeEvent final {
    std::string topic;
    LogicalId subjectId = 0;
    std::string payload;
};

class EventBus final {
public:
    using SubscriptionId = std::uint64_t;
    using Callback = std::function<void(const RuntimeEvent&)>;

    SubscriptionId subscribe(std::string topic, Callback callback);
    bool unsubscribe(SubscriptionId id);
    void publish(const RuntimeEvent& event) const;
    void clear();
    [[nodiscard]] std::size_t subscriberCount() const;

private:
    struct Subscription final {
        SubscriptionId id = 0;
        std::string topic;
        Callback callback;
    };

    mutable std::mutex mutex_;
    std::vector<Subscription> subscriptions_;
    SubscriptionId nextSubscriptionId_ = 1;
};

enum class SchedulerLane : std::uint8_t {
    Frame,
    FiveHz,
    TwoHz,
    OneHz
};

class Scheduler final {
public:
    using TaskId = std::uint64_t;
    using Callback = std::function<void()>;
    using ErrorHandler = std::function<void(std::string_view, std::exception_ptr)>;

    TaskId addRecurring(SchedulerLane lane, std::string name, Callback callback);
    bool remove(TaskId id);
    void post(std::string name, Callback callback);
    void tick(std::uint64_t nowMs);
    void clear();
    void setErrorHandler(ErrorHandler handler);
    [[nodiscard]] std::size_t recurringTaskCount() const noexcept;
    [[nodiscard]] std::size_t queuedTaskCount() const noexcept;

private:
    struct RecurringTask final {
        TaskId id = 0;
        SchedulerLane lane = SchedulerLane::Frame;
        std::string name;
        Callback callback;
        std::uint64_t lastRunMs = 0;
        bool hasRun = false;
    };

    struct QueuedTask final {
        std::string name;
        Callback callback;
    };

    static std::uint64_t intervalMs(SchedulerLane lane) noexcept;
    void invokeSafely(std::string_view name, const Callback& callback) const;

    std::vector<RecurringTask> recurring_;
    std::vector<QueuedTask> queued_;
    TaskId nextTaskId_ = 1;
    ErrorHandler errorHandler_;
};

enum class MissionCompatibilityLevel : std::uint8_t {
    Normal,
    Restricted,
    Suspended
};

struct MissionCompatibilitySignals final {
    bool missionFlag = false;
    bool cutsceneActive = false;
    bool cutscenePlaying = false;
    bool playerControlOn = true;
};

class MissionCompatibilityGate final {
public:
    MissionCompatibilityLevel update(const MissionCompatibilitySignals& signals) noexcept;
    [[nodiscard]] MissionCompatibilityLevel level() const noexcept { return level_; }
    [[nodiscard]] bool gameplayAllowed() const noexcept { return level_ == MissionCompatibilityLevel::Normal; }
    [[nodiscard]] bool persistenceAllowed() const noexcept { return level_ != MissionCompatibilityLevel::Suspended; }

private:
    MissionCompatibilityLevel level_ = MissionCompatibilityLevel::Normal;
};

class DebugCommandRegistry final {
public:
    using Callback = std::function<void()>;

    bool registerCommand(std::string name, Callback callback);
    bool execute(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> commandNames() const;

private:
    std::unordered_map<std::string, Callback> commands_;
};

} // namespace gco
