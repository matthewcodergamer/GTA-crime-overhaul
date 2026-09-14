#include "Foundation.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
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

std::filesystem::path executableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return std::filesystem::current_path();
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
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
        else if (key == "WitnessSystem") config.witnessSystem = parseBool(value, config.witnessSystem);
        else if (key == "PersistentCases") config.persistentCases = parseBool(value, config.persistentCases);
        else if (key == "VehicleIdentity") config.vehicleIdentity = parseBool(value, config.vehicleIdentity);
        else if (key == "RobberySystem") config.robberySystem = parseBool(value, config.robberySystem);
    }

    return config;
}

WorldStateStore::WorldStateStore(RuntimePaths paths)
    : paths_(std::move(paths)) {}

bool WorldStateStore::ensureInitialized() {
    std::error_code ec;
    if (std::filesystem::exists(paths_.worldSave, ec)) {
        return true;
    }
    return writeEmptyWorldAtomically();
}

bool WorldStateStore::writeEmptyWorldAtomically() {
    const auto tempFile = paths_.saveRoot / L"world.tmp.json";

    {
        std::ofstream out(tempFile, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }

        out
            << "{\n"
            << "  \"schemaVersion\": " << SchemaVersion << ",\n"
            << "  \"cases\": [],\n"
            << "  \"businesses\": [],\n"
            << "  \"clerks\": [],\n"
            << "  \"ownedVehicles\": [],\n"
            << "  \"loot\": [],\n"
            << "  \"economy\": { \"cleanCash\": 0, \"hotCash\": 0 },\n"
            << "  \"casing\": []\n"
            << "}\n";

        out.flush();
        if (!out.good()) {
            return false;
        }
    }

    std::error_code ec;
    if (std::filesystem::exists(paths_.worldSave, ec)) {
        std::filesystem::copy_file(
            paths_.worldSave,
            paths_.worldBackup,
            std::filesystem::copy_options::overwrite_existing,
            ec);
        ec.clear();
    }

    std::filesystem::rename(tempFile, paths_.worldSave, ec);
    if (!ec) {
        return true;
    }

    // Windows rename can fail when the destination exists. Fall back to replace.
    if (MoveFileExW(
            tempFile.c_str(),
            paths_.worldSave.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }

    std::filesystem::remove(tempFile, ec);
    return false;
}

} // namespace gco
