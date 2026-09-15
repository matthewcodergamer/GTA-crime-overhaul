#include "CoreServices.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace

int main() {
    using namespace gco;

    expect(BuildInfo::BinaryName == "GTA_Crime_Overhaul.asi", "authoritative binary name");
    expect(BuildInfo::SaveSchemaVersion == 1, "save schema version constant");

    // Existing Stage 0 domains are persisted. Their numeric values are compatibility locks.
    expect(static_cast<unsigned>(LogicalIdDomain::Case) == 1U, "case domain number remains stable");
    expect(static_cast<unsigned>(LogicalIdDomain::Business) == 2U, "business domain number remains stable");
    expect(static_cast<unsigned>(LogicalIdDomain::Clerk) == 3U, "clerk domain number remains stable");
    expect(static_cast<unsigned>(LogicalIdDomain::Vehicle) == 4U, "vehicle domain number remains stable");
    expect(static_cast<unsigned>(LogicalIdDomain::LootContainer) == 5U, "loot domain number remains stable");
    expect(static_cast<unsigned>(LogicalIdDomain::Crime) == 6U, "crime domain is appended without renumbering old IDs");

    LogicalIdGenerator ids;
    const LogicalId case1 = ids.next(LogicalIdDomain::Case);
    const LogicalId case2 = ids.next(LogicalIdDomain::Case);
    const LogicalId vehicle1 = ids.next(LogicalIdDomain::Vehicle);
    const LogicalId crime1 = ids.next(LogicalIdDomain::Crime);
    expect(case1 != 0 && case2 != 0 && vehicle1 != 0 && crime1 != 0, "logical IDs are non-zero");
    expect(case1 != case2 && case1 != vehicle1 && case1 != crime1, "logical IDs are unique across sequence/domain");
    expect(logicalIdDomain(case1) == LogicalIdDomain::Case, "case ID encodes case domain");
    expect(logicalIdDomain(vehicle1) == LogicalIdDomain::Vehicle, "vehicle ID encodes vehicle domain");
    expect(logicalIdDomain(crime1) == LogicalIdDomain::Crime, "crime ID encodes crime domain");
    expect(logicalIdSequence(case2) == 2, "case sequence increments deterministically");
    expect(ids.setNextSequence(LogicalIdDomain::Clerk, 42), "ID generator accepts valid restored sequence");
    expect(logicalIdSequence(ids.next(LogicalIdDomain::Clerk)) == 42, "restored sequence is deterministic");
    expect(ids.setNextSequence(LogicalIdDomain::Crime, 77), "crime sequence can be restored independently");
    expect(logicalIdSequence(ids.next(LogicalIdDomain::Crime)) == 77, "restored crime sequence is deterministic");
    expect(!ids.setNextSequence(LogicalIdDomain::Clerk, 0), "zero sequence is rejected");

    EventBus bus;
    int delivered = 0;
    const auto sub = bus.subscribe("test.event", [&](const RuntimeEvent& event) {
        if (event.subjectId == case1 && event.payload == "payload") {
            ++delivered;
        }
    });
    expect(sub != 0, "event subscription returns token");
    bus.publish(RuntimeEvent{"other.event", case1, "payload"});
    expect(delivered == 0, "event bus filters topics");
    bus.publish(RuntimeEvent{"test.event", case1, "payload"});
    expect(delivered == 1, "event bus delivers matching topic");
    expect(bus.unsubscribe(sub), "event unsubscribe succeeds");
    bus.publish(RuntimeEvent{"test.event", case1, "payload"});
    expect(delivered == 1, "unsubscribed callback is not called");

    Scheduler scheduler;
    int frameRuns = 0;
    int fiveHzRuns = 0;
    int queuedRuns = 0;
    int schedulerErrors = 0;
    scheduler.setErrorHandler([&](std::string_view, std::exception_ptr) { ++schedulerErrors; });
    scheduler.addRecurring(SchedulerLane::Frame, "frame", [&]() { ++frameRuns; });
    scheduler.addRecurring(SchedulerLane::FiveHz, "5hz", [&]() { ++fiveHzRuns; });
    scheduler.addRecurring(SchedulerLane::OneHz, "throws", []() { throw std::runtime_error("expected"); });
    scheduler.post("queued", [&]() { ++queuedRuns; });

    scheduler.tick(1000);
    expect(frameRuns == 1, "frame task runs on first tick");
    expect(fiveHzRuns == 1, "fixed task runs on first tick");
    expect(queuedRuns == 1, "event-driven queue runs on next tick");
    expect(schedulerErrors == 1, "scheduler contains task exception");

    scheduler.tick(1100);
    expect(frameRuns == 2, "frame task runs every tick");
    expect(fiveHzRuns == 1, "5 Hz task waits 200 ms");
    scheduler.tick(1200);
    expect(fiveHzRuns == 2, "5 Hz task runs at 200 ms");

    MissionCompatibilityGate gate;
    expect(gate.update({}) == MissionCompatibilityLevel::Normal, "mission gate defaults normal");
    MissionCompatibilitySignals noControl{};
    noControl.playerControlOn = false;
    expect(gate.update(noControl) == MissionCompatibilityLevel::Restricted, "loss of player control is restricted");
    MissionCompatibilitySignals mission{};
    mission.missionFlag = true;
    expect(gate.update(mission) == MissionCompatibilityLevel::Suspended, "mission flag suspends gameplay");
    MissionCompatibilitySignals cutscene{};
    cutscene.cutsceneActive = true;
    expect(gate.update(cutscene) == MissionCompatibilityLevel::Suspended, "cutscene suspends gameplay");

    DebugCommandRegistry commands;
    int commandRuns = 0;
    expect(commands.registerCommand("test", [&]() { ++commandRuns; }), "debug command registers");
    expect(!commands.registerCommand("test", []() {}), "duplicate debug command rejected");
    expect(commands.execute("test"), "debug command executes");
    expect(commandRuns == 1, "debug command callback runs");
    expect(!commands.execute("missing"), "missing debug command fails closed");

    if (failures == 0) {
        std::cout << "CoreServices tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
