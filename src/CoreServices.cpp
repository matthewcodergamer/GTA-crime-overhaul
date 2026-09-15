#include "CoreServices.h"

#include <algorithm>
#include <utility>

namespace gco {
namespace {
constexpr std::size_t kLogicalIdDomainCount = 5;
}

std::size_t LogicalIdGenerator::indexOf(const LogicalIdDomain domain) noexcept {
    const auto raw = static_cast<std::uint8_t>(domain);
    if (raw < static_cast<std::uint8_t>(LogicalIdDomain::Case)
        || raw > static_cast<std::uint8_t>(LogicalIdDomain::LootContainer)) {
        return kLogicalIdDomainCount;
    }
    return static_cast<std::size_t>(raw - 1U);
}

LogicalId LogicalIdGenerator::next(const LogicalIdDomain domain) {
    const auto index = indexOf(domain);
    if (index >= nextSequences_.size()) {
        return 0;
    }

    const auto sequence = nextSequences_[index];
    if (sequence == 0 || sequence > kLogicalIdSequenceMask) {
        return 0;
    }

    const LogicalId id = makeLogicalId(domain, sequence);
    if (sequence == kLogicalIdSequenceMask) {
        nextSequences_[index] = 0;
    } else {
        ++nextSequences_[index];
    }
    return id;
}

bool LogicalIdGenerator::setNextSequence(
    const LogicalIdDomain domain,
    const std::uint64_t nextSequenceValue) noexcept {

    const auto index = indexOf(domain);
    if (index >= nextSequences_.size()
        || nextSequenceValue == 0
        || nextSequenceValue > kLogicalIdSequenceMask) {
        return false;
    }
    nextSequences_[index] = nextSequenceValue;
    return true;
}

std::uint64_t LogicalIdGenerator::nextSequence(const LogicalIdDomain domain) const noexcept {
    const auto raw = static_cast<std::uint8_t>(domain);
    if (raw < static_cast<std::uint8_t>(LogicalIdDomain::Case)
        || raw > static_cast<std::uint8_t>(LogicalIdDomain::LootContainer)) {
        return 0;
    }
    return nextSequences_[static_cast<std::size_t>(raw - 1U)];
}

EventBus::SubscriptionId EventBus::subscribe(std::string topic, Callback callback) {
    if (topic.empty() || !callback) {
        return 0;
    }

    std::lock_guard lock(mutex_);
    const SubscriptionId id = nextSubscriptionId_++;
    subscriptions_.push_back(Subscription{id, std::move(topic), std::move(callback)});
    return id;
}

bool EventBus::unsubscribe(const SubscriptionId id) {
    if (id == 0) {
        return false;
    }

    std::lock_guard lock(mutex_);
    const auto before = subscriptions_.size();
    std::erase_if(subscriptions_, [id](const Subscription& entry) { return entry.id == id; });
    return subscriptions_.size() != before;
}

void EventBus::publish(const RuntimeEvent& event) const {
    if (event.topic.empty()) {
        return;
    }

    std::vector<Callback> callbacks;
    {
        std::lock_guard lock(mutex_);
        callbacks.reserve(subscriptions_.size());
        for (const auto& entry : subscriptions_) {
            if (entry.topic == event.topic) {
                callbacks.push_back(entry.callback);
            }
        }
    }

    for (const auto& callback : callbacks) {
        callback(event);
    }
}

void EventBus::clear() {
    std::lock_guard lock(mutex_);
    subscriptions_.clear();
}

std::size_t EventBus::subscriberCount() const {
    std::lock_guard lock(mutex_);
    return subscriptions_.size();
}

std::uint64_t Scheduler::intervalMs(const SchedulerLane lane) noexcept {
    switch (lane) {
    case SchedulerLane::Frame: return 0;
    case SchedulerLane::FiveHz: return 200;
    case SchedulerLane::TwoHz: return 500;
    case SchedulerLane::OneHz: return 1000;
    }
    return 0;
}

Scheduler::TaskId Scheduler::addRecurring(
    const SchedulerLane lane,
    std::string name,
    Callback callback) {

    if (name.empty() || !callback) {
        return 0;
    }

    const TaskId id = nextTaskId_++;
    recurring_.push_back(RecurringTask{id, lane, std::move(name), std::move(callback), 0});
    return id;
}

bool Scheduler::remove(const TaskId id) {
    if (id == 0) {
        return false;
    }
    const auto before = recurring_.size();
    std::erase_if(recurring_, [id](const RecurringTask& task) { return task.id == id; });
    return recurring_.size() != before;
}

void Scheduler::post(std::string name, Callback callback) {
    if (name.empty() || !callback) {
        return;
    }
    queued_.push_back(QueuedTask{std::move(name), std::move(callback)});
}

void Scheduler::invokeSafely(std::string_view name, const Callback& callback) const {
    try {
        callback();
    } catch (...) {
        if (errorHandler_) {
            errorHandler_(name, std::current_exception());
        }
    }
}

void Scheduler::tick(const std::uint64_t nowMs) {
    auto queued = std::move(queued_);
    queued_.clear();
    for (const auto& task : queued) {
        invokeSafely(task.name, task.callback);
    }

    struct DueTask final {
        std::string name;
        Callback callback;
    };
    std::vector<DueTask> due;
    due.reserve(recurring_.size());

    for (auto& task : recurring_) {
        const auto interval = intervalMs(task.lane);
        const bool shouldRun = task.lane == SchedulerLane::Frame
            || task.lastRunMs == 0
            || nowMs - task.lastRunMs >= interval;
        if (!shouldRun) {
            continue;
        }
        task.lastRunMs = nowMs;
        due.push_back(DueTask{task.name, task.callback});
    }

    for (const auto& task : due) {
        invokeSafely(task.name, task.callback);
    }
}

void Scheduler::clear() {
    recurring_.clear();
    queued_.clear();
}

void Scheduler::setErrorHandler(ErrorHandler handler) {
    errorHandler_ = std::move(handler);
}

std::size_t Scheduler::recurringTaskCount() const noexcept {
    return recurring_.size();
}

std::size_t Scheduler::queuedTaskCount() const noexcept {
    return queued_.size();
}

MissionCompatibilityLevel MissionCompatibilityGate::update(
    const MissionCompatibilitySignals& signals) noexcept {

    if (signals.missionFlag || signals.cutsceneActive || signals.cutscenePlaying) {
        level_ = MissionCompatibilityLevel::Suspended;
    } else if (!signals.playerControlOn) {
        level_ = MissionCompatibilityLevel::Restricted;
    } else {
        level_ = MissionCompatibilityLevel::Normal;
    }
    return level_;
}

bool DebugCommandRegistry::registerCommand(std::string name, Callback callback) {
    if (name.empty() || !callback || commands_.contains(name)) {
        return false;
    }
    commands_.emplace(std::move(name), std::move(callback));
    return true;
}

bool DebugCommandRegistry::execute(const std::string_view name) const {
    const auto it = commands_.find(std::string(name));
    if (it == commands_.end()) {
        return false;
    }
    it->second();
    return true;
}

std::vector<std::string> DebugCommandRegistry::commandNames() const {
    std::vector<std::string> result;
    result.reserve(commands_.size());
    for (const auto& [name, _] : commands_) {
        result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace gco
