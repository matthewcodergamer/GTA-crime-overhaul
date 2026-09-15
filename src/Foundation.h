#pragma once

#include "CoreServices.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace gco {

struct RuntimePaths {
    std::filesystem::path gameRoot;
    std::filesystem::path modRoot;
    std::filesystem::path configFile;
    std::filesystem::path dataRoot;
    std::filesystem::path saveRoot;
    std::filesystem::path logRoot;
    std::filesystem::path worldSave;
    std::filesystem::path worldBackup;

    static RuntimePaths discover();
    void ensureDirectories() const;
};

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger final {
public:
    static Logger& instance();

    bool open(const std::filesystem::path& file);
    void close();
    void write(LogLevel level, const std::string& message);

    void debug(const std::string& message) { write(LogLevel::Debug, message); }
    void info(const std::string& message) { write(LogLevel::Info, message); }
    void warn(const std::string& message) { write(LogLevel::Warning, message); }
    void error(const std::string& message) { write(LogLevel::Error, message); }

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    std::ofstream stream_;
};

struct RuntimeConfig {
    bool enabled = true;
    bool debugLogging = true;
    bool debugOverlay = false;
    bool debugHotkeys = true;
    bool witnessSystem = true;
    bool persistentCases = true;
    bool vehicleIdentity = true;
    bool robberySystem = true;

    static RuntimeConfig load(const std::filesystem::path& file);
};

enum class PersistenceStatus : std::uint8_t {
    LoadedPrimary,
    RecoveredBackup,
    CreatedNew,
    Failed
};

struct PersistenceReport final {
    PersistenceStatus status = PersistenceStatus::Failed;
    std::uint32_t schemaVersion = 0;
    std::string detail;

    [[nodiscard]] bool ok() const noexcept { return status != PersistenceStatus::Failed; }
};

class WorldStateStore final {
public:
    static constexpr std::uint32_t SchemaVersion = BuildInfo::SaveSchemaVersion;

    explicit WorldStateStore(RuntimePaths paths);

    PersistenceReport loadOrCreate();
    bool ensureInitialized();
    bool writeEmptyWorldAtomically();
    bool writeWorldAtomically(const std::string& document);
    bool validateFile(const std::filesystem::path& file, std::string* reason = nullptr) const;
    [[nodiscard]] std::string emptyWorldJson() const;

private:
    bool validateDocument(const std::string& document, std::string* reason) const;
    bool restoreBackup(std::string* reason);
    void quarantineCorruptPrimary();

    RuntimePaths paths_;
};

std::string trim(std::string value);
bool parseBool(const std::string& value, bool fallback);

} // namespace gco
