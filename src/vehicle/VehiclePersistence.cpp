#include "VehiclePersistence.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string_view>

namespace gco::vehicle {
namespace {

constexpr std::uint32_t kSchemaVersion = 1;

bool readText(const std::filesystem::path& path, std::string& out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

std::string escapeJson(const std::string_view value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
}

std::optional<std::string> stringField(const std::string& object, const std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + needle.size());
    if (colon == std::string::npos) return std::nullopt;
    const auto first = object.find('"', colon + 1);
    if (first == std::string::npos) return std::nullopt;
    std::string result;
    bool escaped = false;
    for (std::size_t i = first + 1; i < object.size(); ++i) {
        const char ch = object[i];
        if (escaped) {
            switch (ch) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default: result.push_back(ch); break;
            }
            escaped = false;
        } else if (ch == '\\') escaped = true;
        else if (ch == '"') return result;
        else result.push_back(ch);
    }
    return std::nullopt;
}

template <typename T>
std::optional<T> integerField(const std::string& object, const std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + needle.size());
    if (colon == std::string::npos) return std::nullopt;
    const char* begin = object.data() + colon + 1;
    const char* end = object.data() + object.size();
    while (begin < end && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n')) ++begin;
    T value{};
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{}) return std::nullopt;
    return value;
}

std::optional<float> floatField(const std::string& object, const std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + needle.size());
    if (colon == std::string::npos) return std::nullopt;
    std::size_t start = colon + 1;
    while (start < object.size() && std::isspace(static_cast<unsigned char>(object[start]))) ++start;
    std::size_t end = start;
    while (end < object.size()) {
        const char ch = object[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')) break;
        ++end;
    }
    if (end == start) return std::nullopt;
    try { return std::stof(object.substr(start, end - start)); }
    catch (...) { return std::nullopt; }
}

std::optional<bool> boolField(const std::string& object, const std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = object.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto colon = object.find(':', keyPos + needle.size());
    if (colon == std::string::npos) return std::nullopt;
    const auto end = object.find_first_of(",}", colon + 1);
    const auto truePos = object.find("true", colon + 1);
    const auto falsePos = object.find("false", colon + 1);
    if (truePos != std::string::npos && (end == std::string::npos || truePos < end)) return true;
    if (falsePos != std::string::npos && (end == std::string::npos || falsePos < end)) return false;
    return std::nullopt;
}

std::optional<std::string> arrayBody(const std::string& document, const std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPos = document.find(needle);
    if (keyPos == std::string::npos) return std::nullopt;
    const auto open = document.find('[', keyPos + needle.size());
    if (open == std::string::npos) return std::nullopt;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = open; i < document.size(); ++i) {
        const char ch = document[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }
        if (ch == '[') ++depth;
        else if (ch == ']') {
            --depth;
            if (depth == 0) return document.substr(open + 1, i - open - 1);
        }
    }
    return std::nullopt;
}

std::vector<std::string> objectEntries(const std::string_view body) {
    std::vector<std::string> entries;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    std::size_t start = std::string_view::npos;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char ch = body[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') { inString = true; continue; }
        if (ch == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && start != std::string_view::npos) {
                entries.emplace_back(body.substr(start, i - start + 1));
                start = std::string_view::npos;
            }
        }
    }
    return entries;
}

std::string joinMods(const std::array<int, kVehicleModSlotCount>& mods) {
    std::ostringstream out;
    for (std::size_t i = 0; i < mods.size(); ++i) {
        if (i != 0) out << ',';
        out << mods[i];
    }
    return out.str();
}

bool parseMods(const std::string_view text, std::array<int, kVehicleModSlotCount>& mods) {
    std::size_t start = 0;
    for (std::size_t i = 0; i < mods.size(); ++i) {
        const auto end = text.find(',', start);
        const auto part = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        int value = -1;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size()) return false;
        mods[i] = value;
        if (i + 1 < mods.size()) {
            if (end == std::string_view::npos) return false;
            start = end + 1;
        }
    }
    return true;
}

std::string joinToggles(const std::array<bool, kVehicleModSlotCount>& toggles) {
    std::string result;
    result.reserve(toggles.size());
    for (const bool value : toggles) result.push_back(value ? '1' : '0');
    return result;
}

bool parseToggles(const std::string_view text, std::array<bool, kVehicleModSlotCount>& toggles) {
    if (text.size() != toggles.size()) return false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '0' && text[i] != '1') return false;
        toggles[i] = text[i] == '1';
    }
    return true;
}

std::string joinCaseIds(const std::vector<LogicalId>& values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << ',';
        out << values[i];
    }
    return out.str();
}

bool parseCaseIds(const std::string_view text, std::vector<LogicalId>& values) {
    if (text.empty()) return true;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find(',', start);
        const auto part = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
        LogicalId id = 0;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), id);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size()
            || logicalIdDomain(id) != LogicalIdDomain::Case) return false;
        values.push_back(id);
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return true;
}

std::string optionalString(const std::optional<std::string>& value) {
    return value.has_value() ? "\"" + escapeJson(*value) + "\"" : "null";
}

template <typename T>
std::string optionalNumber(const std::optional<T>& value) {
    return value.has_value() ? std::to_string(*value) : "null";
}

} // namespace

VehiclePersistenceStore::VehiclePersistenceStore(RuntimePaths paths)
    : paths_(std::move(paths)),
      primary_(paths_.saveRoot / L"vehicle_identity.json"),
      backup_(paths_.saveRoot / L"vehicle_identity.backup.json") {}

bool VehiclePersistenceStore::load(VehicleIdentitySystem& system, std::string* reason) {
    if (!std::filesystem::exists(primary_)) {
        system.restore({}, {});
        if (reason) *reason = "vehicle persistence did not exist; initialized empty state";
        return save(system, reason);
    }

    std::string document;
    if (!readText(primary_, document)) {
        if (reason) *reason = "unable to read vehicle persistence";
        return false;
    }
    std::vector<OwnedVehicleRecord> records;
    std::vector<VehicleBoloState> bolos;
    if (!decode(document, records, bolos, reason)) {
        std::string backupDocument;
        if (!readText(backup_, backupDocument) || !decode(backupDocument, records, bolos, reason)) return false;
        if (!CopyFileW(backup_.c_str(), primary_.c_str(), FALSE)) {
            if (reason) *reason = "vehicle primary corrupt and backup valid, but restore copy failed";
            return false;
        }
        if (reason) *reason = "vehicle persistence recovered from backup";
    } else if (reason) {
        *reason = "vehicle persistence loaded";
    }

    system.restore(std::move(records), std::move(bolos));
    return true;
}

bool VehiclePersistenceStore::save(const VehicleIdentitySystem& system, std::string* reason) const {
    return atomicWrite(encode(system), reason);
}

std::string VehiclePersistenceStore::encode(const VehicleIdentitySystem& system) const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(4);
    out << "{\n  \"schemaVersion\": " << kSchemaVersion << ",\n  \"ownedVehicles\": [";
    bool first = true;
    for (const auto& record : system.records()) {
        if (!first) out << ',';
        first = false;
        out << "\n    {"
            << "\"modelVersion\":" << OwnedVehicleRecord::ModelVersion
            << ",\"id\":" << record.id
            << ",\"kind\":\"" << vehicleRecordKindName(record.kind) << "\""
            << ",\"modelHash\":" << record.appearance.modelHash
            << ",\"modelName\":\"" << escapeJson(record.appearance.modelName) << "\""
            << ",\"primaryColor\":" << record.appearance.primaryColor
            << ",\"secondaryColor\":" << record.appearance.secondaryColor
            << ",\"plateText\":\"" << escapeJson(record.appearance.plateText) << "\""
            << ",\"plateStyle\":" << record.appearance.plateStyle
            << ",\"modKit\":" << record.modifications.modKit
            << ",\"wheelType\":" << record.modifications.wheelType
            << ",\"windowTint\":" << record.modifications.windowTint
            << ",\"livery\":" << record.modifications.livery
            << ",\"mods\":\"" << joinMods(record.modifications.mods) << "\""
            << ",\"toggles\":\"" << joinToggles(record.modifications.toggleMods) << "\""
            << ",\"storageStatus\":\"" << vehicleStorageStatusName(record.storageStatus) << "\""
            << ",\"storageKey\":\"" << escapeJson(record.storageKey) << "\""
            << ",\"usedInCrime\":" << (record.crimeHistory.usedInCrime ? "true" : "false")
            << ",\"usedAsGetaway\":" << (record.crimeHistory.usedAsGetaway ? "true" : "false")
            << ",\"reportedStolen\":" << (record.crimeHistory.reportedStolen ? "true" : "false")
            << ",\"crimeCount\":" << record.crimeHistory.crimeCount
            << ",\"lastCrimeAtMs\":" << record.crimeHistory.lastCrimeAtMs
            << ",\"caseIds\":\"" << joinCaseIds(record.crimeHistory.caseIds) << "\""
            << ",\"createdAtMs\":" << record.createdAtMs
            << ",\"updatedAtMs\":" << record.updatedAtMs
            << '}';
    }
    out << "\n  ],\n  \"vehicleBolos\": [";
    first = true;
    for (const auto& bolo : system.bolos()) {
        if (!first) out << ',';
        first = false;
        out << "\n    {"
            << "\"caseId\":" << bolo.caseId
            << ",\"linkedVehicleId\":" << optionalNumber(bolo.linkedVehicleId)
            << ",\"active\":" << (bolo.active ? "true" : "false")
            << ",\"modelHash\":" << optionalNumber(bolo.modelHash)
            << ",\"primaryColor\":" << optionalNumber(bolo.primaryColor)
            << ",\"secondaryColor\":" << optionalNumber(bolo.secondaryColor)
            << ",\"plateText\":" << optionalString(bolo.plateText)
            << ",\"modelConfidence\":" << bolo.modelConfidence
            << ",\"colorConfidence\":" << bolo.colorConfidence
            << ",\"plateConfidence\":" << bolo.plateConfidence
            << ",\"physicalContinuityKnown\":" << (bolo.physicalContinuityKnown ? "true" : "false")
            << ",\"updatedAtMs\":" << bolo.updatedAtMs
            << '}';
    }
    out << "\n  ]\n}\n";
    return out.str();
}

bool VehiclePersistenceStore::decode(
    const std::string& document,
    std::vector<OwnedVehicleRecord>& records,
    std::vector<VehicleBoloState>& bolos,
    std::string* reason) const {

    const auto schema = integerField<std::uint32_t>(document, "schemaVersion");
    const auto vehiclesBody = arrayBody(document, "ownedVehicles");
    const auto bolosBody = arrayBody(document, "vehicleBolos");
    if (!schema || *schema != kSchemaVersion || !vehiclesBody || !bolosBody) {
        if (reason) *reason = "vehicle persistence schema/arrays invalid";
        return false;
    }

    for (const auto& object : objectEntries(*vehiclesBody)) {
        const auto modelVersion = integerField<std::uint32_t>(object, "modelVersion");
        const auto id = integerField<LogicalId>(object, "id");
        const auto kindText = stringField(object, "kind");
        const auto kind = kindText ? vehicleRecordKindFromString(*kindText) : std::nullopt;
        const auto statusText = stringField(object, "storageStatus");
        const auto status = statusText ? vehicleStorageStatusFromString(*statusText) : std::nullopt;
        const auto modelHash = integerField<std::uint32_t>(object, "modelHash");
        const auto modelName = stringField(object, "modelName");
        const auto primary = integerField<int>(object, "primaryColor");
        const auto secondary = integerField<int>(object, "secondaryColor");
        const auto plate = stringField(object, "plateText");
        const auto plateStyle = integerField<int>(object, "plateStyle");
        const auto modKit = integerField<int>(object, "modKit");
        const auto wheelType = integerField<int>(object, "wheelType");
        const auto windowTint = integerField<int>(object, "windowTint");
        const auto livery = integerField<int>(object, "livery");
        const auto mods = stringField(object, "mods");
        const auto toggles = stringField(object, "toggles");
        const auto storageKey = stringField(object, "storageKey");
        const auto usedInCrime = boolField(object, "usedInCrime");
        const auto usedAsGetaway = boolField(object, "usedAsGetaway");
        const auto reportedStolen = boolField(object, "reportedStolen");
        const auto crimeCount = integerField<std::uint32_t>(object, "crimeCount");
        const auto lastCrimeAtMs = integerField<std::uint64_t>(object, "lastCrimeAtMs");
        const auto caseIds = stringField(object, "caseIds");
        const auto createdAtMs = integerField<std::uint64_t>(object, "createdAtMs");
        const auto updatedAtMs = integerField<std::uint64_t>(object, "updatedAtMs");

        if (!modelVersion || *modelVersion != OwnedVehicleRecord::ModelVersion || !id
            || logicalIdDomain(*id) != LogicalIdDomain::Vehicle || !kind || !status
            || !modelHash || !modelName || !primary || !secondary || !plate || !plateStyle
            || !modKit || !wheelType || !windowTint || !livery || !mods || !toggles || !storageKey
            || !usedInCrime || !usedAsGetaway || !reportedStolen || !crimeCount || !lastCrimeAtMs
            || !caseIds || !createdAtMs || !updatedAtMs) {
            if (reason) *reason = "vehicle record has missing/invalid fields";
            return false;
        }
        const auto validatedPlate = validatePlateText(*plate);
        if (!validatedPlate.valid) {
            if (reason) *reason = "vehicle record contains invalid plate text";
            return false;
        }

        OwnedVehicleRecord record{};
        record.id = *id;
        record.kind = *kind;
        record.appearance.modelHash = *modelHash;
        record.appearance.modelName = *modelName;
        record.appearance.primaryColor = *primary;
        record.appearance.secondaryColor = *secondary;
        record.appearance.plateText = validatedPlate.normalized;
        record.appearance.plateStyle = *plateStyle;
        record.modifications.modKit = *modKit;
        record.modifications.wheelType = *wheelType;
        record.modifications.windowTint = *windowTint;
        record.modifications.livery = *livery;
        if (!parseMods(*mods, record.modifications.mods) || !parseToggles(*toggles, record.modifications.toggleMods)) {
            if (reason) *reason = "vehicle modification reconstruction state invalid";
            return false;
        }
        record.storageStatus = *status;
        record.storageKey = *storageKey;
        record.crimeHistory.usedInCrime = *usedInCrime;
        record.crimeHistory.usedAsGetaway = *usedAsGetaway;
        record.crimeHistory.reportedStolen = *reportedStolen;
        record.crimeHistory.crimeCount = *crimeCount;
        record.crimeHistory.lastCrimeAtMs = *lastCrimeAtMs;
        if (!parseCaseIds(*caseIds, record.crimeHistory.caseIds)) {
            if (reason) *reason = "vehicle crime-history case IDs invalid";
            return false;
        }
        record.createdAtMs = *createdAtMs;
        record.updatedAtMs = *updatedAtMs;
        records.push_back(std::move(record));
    }

    for (const auto& object : objectEntries(*bolosBody)) {
        const auto caseId = integerField<LogicalId>(object, "caseId");
        const auto active = boolField(object, "active");
        const auto modelConfidence = floatField(object, "modelConfidence");
        const auto colorConfidence = floatField(object, "colorConfidence");
        const auto plateConfidence = floatField(object, "plateConfidence");
        const auto continuity = boolField(object, "physicalContinuityKnown");
        const auto updatedAtMs = integerField<std::uint64_t>(object, "updatedAtMs");
        if (!caseId || logicalIdDomain(*caseId) != LogicalIdDomain::Case || !active
            || !modelConfidence || !colorConfidence || !plateConfidence || !continuity || !updatedAtMs) {
            if (reason) *reason = "vehicle BOLO has missing/invalid required fields";
            return false;
        }
        VehicleBoloState bolo{};
        bolo.caseId = *caseId;
        bolo.active = *active;
        bolo.modelConfidence = *modelConfidence;
        bolo.colorConfidence = *colorConfidence;
        bolo.plateConfidence = *plateConfidence;
        bolo.physicalContinuityKnown = *continuity;
        bolo.updatedAtMs = *updatedAtMs;

        const auto linkedToken = object.find("\"linkedVehicleId\":null");
        if (linkedToken == std::string::npos) {
            const auto linked = integerField<LogicalId>(object, "linkedVehicleId");
            if (!linked || logicalIdDomain(*linked) != LogicalIdDomain::Vehicle) return false;
            bolo.linkedVehicleId = *linked;
        }
        if (object.find("\"modelHash\":null") == std::string::npos) bolo.modelHash = integerField<std::uint32_t>(object, "modelHash");
        if (object.find("\"primaryColor\":null") == std::string::npos) bolo.primaryColor = integerField<int>(object, "primaryColor");
        if (object.find("\"secondaryColor\":null") == std::string::npos) bolo.secondaryColor = integerField<int>(object, "secondaryColor");
        if (object.find("\"plateText\":null") == std::string::npos) bolo.plateText = stringField(object, "plateText");
        if ((object.find("\"modelHash\":null") == std::string::npos && !bolo.modelHash)
            || (object.find("\"primaryColor\":null") == std::string::npos && !bolo.primaryColor)
            || (object.find("\"secondaryColor\":null") == std::string::npos && !bolo.secondaryColor)
            || (object.find("\"plateText\":null") == std::string::npos && !bolo.plateText)) {
            if (reason) *reason = "vehicle BOLO optional field invalid";
            return false;
        }
        bolos.push_back(std::move(bolo));
    }
    return true;
}

bool VehiclePersistenceStore::atomicWrite(const std::string& document, std::string* reason) const {
    std::vector<OwnedVehicleRecord> verifyRecords;
    std::vector<VehicleBoloState> verifyBolos;
    if (!decode(document, verifyRecords, verifyBolos, reason)) return false;

    std::error_code ec;
    std::filesystem::create_directories(paths_.saveRoot, ec);
    const auto temp = primary_.parent_path() / L"vehicle_identity.tmp.json";
    const auto backupTemp = primary_.parent_path() / L"vehicle_identity.backup.tmp.json";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            if (reason) *reason = "unable to open vehicle temp save";
            return false;
        }
        output << document;
        output.flush();
        if (!output.good()) {
            if (reason) *reason = "unable to flush vehicle temp save";
            return false;
        }
    }

    if (std::filesystem::exists(primary_)) {
        std::string current;
        if (readText(primary_, current)) {
            std::vector<OwnedVehicleRecord> oldRecords;
            std::vector<VehicleBoloState> oldBolos;
            if (decode(current, oldRecords, oldBolos, nullptr)) {
                std::filesystem::copy_file(primary_, backupTemp, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    MoveFileExW(backupTemp.c_str(), backup_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
                }
            }
        }
    }

    if (!MoveFileExW(temp.c_str(), primary_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        if (reason) *reason = "atomic vehicle save replacement failed";
        return false;
    }
    if (reason) *reason = "vehicle persistence saved atomically";
    return true;
}

} // namespace gco::vehicle
