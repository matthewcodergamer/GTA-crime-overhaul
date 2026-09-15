#include "Foundation.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>

namespace gco {
namespace {

std::string levelName(const LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error: return "ERROR";
    default: return "UNKNOWN";
    }
}

std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &t);

    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::wstring fileTimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &t);

    std::wostringstream out;
    out << std::put_time(&local, L"%Y%m%d-%H%M%S");
    return out.str();
}

std::filesystem::path executableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return std::filesystem::current_path();
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

bool atomicReplaceFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
    return MoveFileExW(
        source.c_str(),
        destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool readTextFile(const std::filesystem::path& file, std::string& out) {
    std::ifstream input(file, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

bool bracesAndStringsLookValid(const std::string& text) {
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (const char ch : text) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth < 0) {
                return false;
            }
        }
    }

    return !inString && depth == 0;
}

std::optional<std::uint32_t> extractSchemaVersion(const std::string& document) {
    constexpr std::string_view key = "\"schemaVersion\"";
    const auto keyPos = document.find(key);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = document.find(':', keyPos + key.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    auto begin = document.data() + colon + 1;
    const auto end = document.data() + document.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    std::uint32_t value = 0;
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

} // namespace

RuntimePaths RuntimePaths::discover() {
    RuntimePaths paths{};
    paths.gameRoot = executableDirectory();
    paths.modRoot = paths.gameRoot / L"GTA_Crime_Overhaul";
    paths.configFile = paths.modRoot / L"GTA_Crime_Overhaul.ini";
    paths.dataRoot = paths.modRoot / L"data";
    paths.saveRoot = paths.modRoot / L"saves";
    paths.logRoot = paths.modRoot / L"logs";
    paths.worldSave = paths.saveRoot / L"world.json";
    paths.worldBackup = paths.saveRoot / L"world.backup.json";
    return paths;
}

void RuntimePaths::ensureDirectories() const {
    std::error_code ec;
    std::filesystem::create_directories(modRoot, ec);
    std::filesystem::create_directories(dataRoot, ec);
    std::filesystem::create_directories(saveRoot, ec);
    std::filesystem::create_directories(logRoot, ec);
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    close();
}

bool Logger::open(const std::filesystem::path& file) {
    std::lock_guard lock(mutex_);
    if (stream_.is_open()) {
        stream_.close();
    }
    stream_.open(file, std::ios::out | std::ios::app);
    return stream_.good();
}

void Logger::close() {
    std::lock_guard lock(mutex_);
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

void Logger::write(const LogLevel level, const std::string& message) {
    std::lock_guard lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }
    stream_ << '[' << timestampNow() << "] [" << levelName(level) << "] " << message << '\n';
    stream_.flush();
}

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool parseBool(const std::string& value, const bool fallback) {
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        return false;
    }
    return fallback;
}

RuntimeConfig RuntimeConfig::load(const std::filesystem::path& file) {
    RuntimeConfig config{};
    std::ifstream input(file);
    if (!input.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';' || line.front() == '[') {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "Enabled") config.enabled = parseBool(value, config.enabled);
        else if (key == "DebugLogging") config.debugLogging = parseBool(value, config.debugLogging);
        else if (key == "DebugOverlay") config.debugOverlay = parseBool(value, config.debugOverlay);
        else if (key == "DebugHotkeys") config.debugHotkeys = parseBool(value, config.debugHotkeys);
        else if (key == "WitnessSystem") config.witnessSystem = parseBool(value, config.witnessSystem);
        else if (key == "PersistentCases") config.persistentCases = parseBool(value, config.persistentCases);
        else if (key == "VehicleIdentity") config.vehicleIdentity = parseBool(value, config.vehicleIdentity);
        else if (key == "RobberySystem") config.robberySystem = parseBool(value, config.robberySystem);
    }

    return config;
}

WorldStateStore::WorldStateStore(RuntimePaths paths)
    : paths_(std::move(paths)) {}

bool WorldStateStore::validateDocument(const std::string& document, std::string* reason) const {
    const auto reject = [reason](const std::string& message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };

    const auto first = document.find_first_not_of(" \t\r\n");
    const auto last = document.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos
        || document[first] != '{' || document[last] != '}') {
        return reject("world save is not a JSON object");
    }
    if (!bracesAndStringsLookValid(document)) {
        return reject("world save has unbalanced JSON object/string structure");
    }

    const auto schema = extractSchemaVersion(document);
    if (!schema.has_value()) {
        return reject("world save is missing a numeric schemaVersion");
    }
    if (*schema != SchemaVersion) {
        std::ostringstream out;
        out << "unsupported schemaVersion " << *schema << "; runtime expects " << SchemaVersion;
        return reject(out.str());
    }

    constexpr std::array<std::string_view, 8> requiredKeys = {
        "\"nextIds\"", "\"cases\"", "\"businesses\"", "\"clerks\"",
        "\"ownedVehicles\"", "\"loot\"", "\"economy\"", "\"casing\""
    };
    for (const auto key : requiredKeys) {
        if (document.find(key) == std::string::npos) {
            return reject(std::string("world save is missing required key ") + std::string(key));
        }
    }

    constexpr std::array<std::string_view, 6> forbiddenRawHandleKeys = {
        "\"entityHandle\"", "\"pedHandle\"", "\"vehicleHandle\"",
        "\"objectHandle\"", "\"clerkPed\"", "\"runtimeHandle\""
    };
    for (const auto key : forbiddenRawHandleKeys) {
        if (document.find(key) != std::string::npos) {
            return reject(std::string("raw GTA handle key is forbidden in persistence: ") + std::string(key));
        }
    }

    return true;
}

bool WorldStateStore::validateFile(const std::filesystem::path& file, std::string* reason) const {
    std::string document;
    if (!readTextFile(file, document)) {
        if (reason != nullptr) {
            *reason = "unable to read save file";
        }
        return false;
    }
    return validateDocument(document, reason);
}

std::string WorldStateStore::emptyWorldJson() const {
    std::ostringstream out;
    out
        << "{\n"
        << "  \"schemaVersion\": " << SchemaVersion << ",\n"
        << "  \"projectVersion\": \"" << BuildInfo::Version << "\",\n"
        << "  \"nextIds\": { \"case\": 1, \"business\": 1, \"clerk\": 1, \"vehicle\": 1, \"lootContainer\": 1 },\n"
        << "  \"cases\": [],\n"
        << "  \"businesses\": [],\n"
        << "  \"clerks\": [],\n"
        << "  \"ownedVehicles\": [],\n"
        << "  \"loot\": [],\n"
        << "  \"economy\": { \"cleanCash\": 0, \"hotCash\": 0 },\n"
        << "  \"casing\": []\n"
        << "}\n";
    return out.str();
}

bool WorldStateStore::writeWorldAtomically(const std::string& document) {
    paths_.ensureDirectories();
    const auto tempFile = paths_.saveRoot / L"world.tmp.json";
    const auto backupTemp = paths_.saveRoot / L"world.backup.tmp.json";

    std::error_code ec;
    std::filesystem::remove(tempFile, ec);
    ec.clear();
    std::filesystem::remove(backupTemp, ec);

    {
        std::ofstream out(tempFile, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(document.data(), static_cast<std::streamsize>(document.size()));
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(tempFile, ec);
            return false;
        }
    }

    std::string validationReason;
    if (!validateFile(tempFile, &validationReason)) {
        std::filesystem::remove(tempFile, ec);
        return false;
    }

    if (std::filesystem::exists(paths_.worldSave, ec)) {
        ec.clear();
        if (validateFile(paths_.worldSave, nullptr)) {
            std::filesystem::copy_file(
                paths_.worldSave,
                backupTemp,
                std::filesystem::copy_options::overwrite_existing,
                ec);
            if (ec || !validateFile(backupTemp, nullptr) || !atomicReplaceFile(backupTemp, paths_.worldBackup)) {
                std::filesystem::remove(tempFile, ec);
                std::filesystem::remove(backupTemp, ec);
                return false;
            }
        }
    }

    if (!atomicReplaceFile(tempFile, paths_.worldSave)) {
        std::filesystem::remove(tempFile, ec);
        return false;
    }

    if (!validateFile(paths_.worldSave, nullptr)) {
        std::string ignored;
        restoreBackup(&ignored);
        return false;
    }
    return true;
}

bool WorldStateStore::writeEmptyWorldAtomically() {
    return writeWorldAtomically(emptyWorldJson());
}

bool WorldStateStore::restoreBackup(std::string* reason) {
    std::string validationReason;
    if (!validateFile(paths_.worldBackup, &validationReason)) {
        if (reason != nullptr) {
            *reason = "backup unavailable or invalid: " + validationReason;
        }
        return false;
    }

    const auto restoreTemp = paths_.saveRoot / L"world.restore.tmp.json";
    std::error_code ec;
    std::filesystem::copy_file(
        paths_.worldBackup,
        restoreTemp,
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec || !validateFile(restoreTemp, &validationReason)) {
        std::filesystem::remove(restoreTemp, ec);
        if (reason != nullptr) {
            *reason = "backup restore staging failed: " + validationReason;
        }
        return false;
    }

    if (!atomicReplaceFile(restoreTemp, paths_.worldSave)) {
        std::filesystem::remove(restoreTemp, ec);
        if (reason != nullptr) {
            *reason = "atomic backup restore failed";
        }
        return false;
    }
    return true;
}

void WorldStateStore::quarantineCorruptPrimary() {
    std::error_code ec;
    if (!std::filesystem::exists(paths_.worldSave, ec)) {
        return;
    }

    const auto quarantine = paths_.saveRoot
        / (std::wstring(L"world.corrupt-") + fileTimestampNow() + L".json");
    std::filesystem::copy_file(
        paths_.worldSave,
        quarantine,
        std::filesystem::copy_options::overwrite_existing,
        ec);
}

PersistenceReport WorldStateStore::loadOrCreate() {
    paths_.ensureDirectories();
    std::error_code ec;

    if (std::filesystem::exists(paths_.worldSave, ec)) {
        std::string reason;
        if (validateFile(paths_.worldSave, &reason)) {
            return {PersistenceStatus::LoadedPrimary, SchemaVersion, "validated primary world save"};
        }

        quarantineCorruptPrimary();

        std::string backupReason;
        if (restoreBackup(&backupReason)) {
            return {PersistenceStatus::RecoveredBackup, SchemaVersion,
                "primary invalid (" + reason + "); restored validated backup"};
        }

        if (writeEmptyWorldAtomically()) {
            return {PersistenceStatus::CreatedNew, SchemaVersion,
                "primary invalid (" + reason + "); no valid backup; corrupt copy quarantined and clean save created"};
        }

        return {PersistenceStatus::Failed, 0,
            "primary invalid (" + reason + "); recovery failed (" + backupReason + ")"};
    }

    if (validateFile(paths_.worldBackup, nullptr)) {
        std::string reason;
        if (restoreBackup(&reason)) {
            return {PersistenceStatus::RecoveredBackup, SchemaVersion,
                "primary missing; restored validated backup"};
        }
    }

    if (writeEmptyWorldAtomically()) {
        return {PersistenceStatus::CreatedNew, SchemaVersion, "created new schema-versioned world save"};
    }
    return {PersistenceStatus::Failed, 0, "unable to create initial world save"};
}

bool WorldStateStore::ensureInitialized() {
    return loadOrCreate().ok();
}

} // namespace gco
