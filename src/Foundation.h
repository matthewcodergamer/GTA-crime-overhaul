#pragma once

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
    bool witnessSystem = true;
    bool persistentCases = true;
    bool vehicleIdentity = true;
    bool robberySystem = true;

    static RuntimeConfig load(const std::filesystem::path& file);
};

class WorldStateStore final {
public:
    static constexpr std::uint32_t SchemaVersion = 1;

    explicit WorldStateStore(RuntimePaths paths);

    bool ensureInitialized();
    bool writeEmptyWorldAtomically();

private:
    RuntimePaths paths_;
};

std::string trim(std::string value);
bool parseBool(const std::string& value, bool fallback);

} // namespace gco
