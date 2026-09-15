#include "Foundation.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void writeText(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::out | std::ios::binary | std::ios::trunc);
    out << text;
}
} // namespace

int main() {
    using namespace gco;

    const auto root = std::filesystem::temp_directory_path()
        / (L"gco-stage0-test-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    RuntimePaths paths{};
    paths.gameRoot = root;
    paths.modRoot = root / L"GTA_Crime_Overhaul";
    paths.configFile = paths.modRoot / L"GTA_Crime_Overhaul.ini";
    paths.dataRoot = paths.modRoot / L"data";
    paths.saveRoot = paths.modRoot / L"saves";
    paths.logRoot = paths.modRoot / L"logs";
    paths.worldSave = paths.saveRoot / L"world.json";
    paths.worldBackup = paths.saveRoot / L"world.backup.json";
    paths.ensureDirectories();

    WorldStateStore store(paths);
    const auto first = store.loadOrCreate();
    expect(first.status == PersistenceStatus::CreatedNew, "missing save creates new world");
    expect(first.schemaVersion == WorldStateStore::SchemaVersion, "created save reports schema");
    expect(store.validateFile(paths.worldSave), "created world validates");

    const std::string valid = store.emptyWorldJson();
    expect(store.writeWorldAtomically(valid), "validated atomic rewrite succeeds");
    expect(std::filesystem::exists(paths.worldBackup), "atomic rewrite creates backup");
    expect(store.validateFile(paths.worldBackup), "backup validates");

    writeText(paths.worldSave, "{ definitely not valid json");
    const auto recovered = store.loadOrCreate();
    expect(recovered.status == PersistenceStatus::RecoveredBackup, "corrupt primary restores backup");
    expect(store.validateFile(paths.worldSave), "restored primary validates");

    writeText(paths.worldSave, "{ broken primary");
    writeText(paths.worldBackup, "{ broken backup");
    const auto reset = store.loadOrCreate();
    expect(reset.status == PersistenceStatus::CreatedNew, "double corruption creates clean world after quarantine");
    expect(store.validateFile(paths.worldSave), "clean world after corruption validates");

    bool foundQuarantine = false;
    for (const auto& entry : std::filesystem::directory_iterator(paths.saveRoot)) {
        const auto name = entry.path().filename().wstring();
        if (name.rfind(L"world.corrupt-", 0) == 0) {
            foundQuarantine = true;
            break;
        }
    }
    expect(foundQuarantine, "corrupt primary is quarantined for diagnosis/recovery");

    std::string forbidden = store.emptyWorldJson();
    const auto closeBrace = forbidden.find_last_of('}');
    forbidden.insert(closeBrace, ",\n  \"entityHandle\": 123\n");
    expect(!store.writeWorldAtomically(forbidden), "raw GTA handle key is rejected from persistence");
    expect(store.validateFile(paths.worldSave), "failed write leaves valid primary intact");

    writeText(paths.worldSave,
        "{\n  \"schemaVersion\": 999,\n  \"nextIds\": {},\n  \"cases\": [],\n  \"businesses\": [],\n  \"clerks\": [],\n  \"ownedVehicles\": [],\n  \"loot\": [],\n  \"economy\": {},\n  \"casing\": []\n}\n");
    std::string reason;
    expect(!store.validateFile(paths.worldSave, &reason), "unsupported schema is rejected");
    expect(reason.find("unsupported schemaVersion") != std::string::npos, "schema rejection is diagnostic");

    std::filesystem::remove_all(root, ec);
    if (failures == 0) {
        std::cout << "Persistence tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
