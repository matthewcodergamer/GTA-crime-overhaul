#include "robbery/StoreTarget.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
int failures = 0;
void expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void writeText(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}
} // namespace

int main() {
    using namespace gco::robbery;

    const auto file = std::filesystem::temp_directory_path() / "gco-prototype-store-target-test.json";

    writeText(file, R"JSON({
      "schemaVersion": 2,
      "businesses": [{
        "id": "prototype_24_7",
        "enabled": false,
        "displayName": "Prototype 24/7",
        "validation": {"status":"REFERENCE_ONLY","notes":"pending"},
        "activationRadius": 75,
        "clerkAcquireRadius": 2,
        "volume": null,
        "threat": {"maxDistance":8,"sustainMs":1100,"sessionTimeoutMs":120000,"playerLeaveGraceMs":4000},
        "clerkPolicy": {"replacementDelayMs":1000,"recoveryMs":2000,"personalityWeights":{"cowardly":1,"compliant":2,"defiant":1,"panicky":1,"armed":1,"experienced":1,"reckless":1}},
        "cashProfile": {"registerMin":300,"registerMax":1500,"safeMin":1000,"safeMax":5000},
        "anchors": {"clerk":null,"registers":[],"safe":null,"customerZones":[],"entrances":[],"exits":[]}
      }]
    })JSON");

    PrototypeStoreTarget target;
    auto report = PrototypeStoreTargetLoader::load(file, target);
    expect(report.parsed, "gated placeholder target parses");
    expect(!report.productionReady, "REFERENCE_ONLY/unset target cannot silently activate");
    expect(target.validation == TargetValidationStatus::ReferenceOnly, "validation status is preserved");

    writeText(file, R"JSON({
      "schemaVersion": 2,
      "businesses": [{
        "id": "prototype_24_7",
        "enabled": true,
        "displayName": "Validated Prototype",
        "validation": {"status":"VERIFIED_IN_GAME","legacyBuild":"test-legacy","enhancedBuild":"test-enhanced","notes":"fixture only"},
        "activationRadius": 70,
        "clerkAcquireRadius": 1.75,
        "volume": {"min":{"x":0,"y":0,"z":0},"max":{"x":10,"y":10,"z":4}},
        "threat": {"maxDistance":7,"sustainMs":1200,"sessionTimeoutMs":90000,"playerLeaveGraceMs":3000},
        "clerkPolicy": {"replacementDelayMs":1000,"recoveryMs":2000,"personalityWeights":{"cowardly":1,"compliant":2,"defiant":1,"panicky":1,"armed":1,"experienced":1,"reckless":1}},
        "cashProfile": {"registerMin":400,"registerMax":900,"safeMin":1500,"safeMax":3000},
        "anchors": {
          "clerk":{"position":{"x":5,"y":6,"z":1},"heading":90},
          "registers":[{"position":{"x":5,"y":5,"z":1},"heading":180}],
          "safe":null,
          "customerZones":[{"min":{"x":1,"y":1,"z":0},"max":{"x":4,"y":4,"z":3}}],
          "entrances":[{"position":{"x":0.5,"y":5,"z":1},"heading":270}],
          "exits":[{"position":{"x":0.5,"y":5,"z":1},"heading":270}]
        }
      }]
    })JSON");

    report = PrototypeStoreTargetLoader::load(file, target);
    expect(report.parsed && report.productionReady, "fully populated VERIFIED_IN_GAME fixture becomes production-ready");
    expect(target.businessVolume.contains({5, 5, 1}), "volume contains interior point");
    expect(!target.businessVolume.contains({50, 50, 1}), "volume rejects distant point");
    expect(target.registers.size() == 1 && target.exits.size() == 1, "register/exit anchors load from data");
    expect(target.tuning.threatSustainMs == 1200, "threat tuning remains data-defined");
    expect(target.registerCashMin == 400 && target.registerCashMax == 900, "cash-source range remains data-defined");

    std::error_code ec;
    std::filesystem::remove(file, ec);
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "StoreTargetTests passed.\n";
    return EXIT_SUCCESS;
}
