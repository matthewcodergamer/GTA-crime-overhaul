#include "Foundation.h"

#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace gco {
namespace {

std::optional<std::uint64_t> extractUnsignedAfter(
    const std::string& document,
    const std::string_view key,
    const std::size_t start) {

    const std::string quoted = "\"" + std::string(key) + "\"";
    const auto keyPos = document.find(quoted, start);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }
    const auto colon = document.find(':', keyPos + quoted.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    const char* begin = document.data() + colon + 1;
    const char* end = document.data() + document.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    std::uint64_t value = 0;
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

} // namespace

bool WorldStateStore::loadLogicalIdState(LogicalIdGenerator& generator, std::string* reason) const {
    std::ifstream input(paths_.worldSave, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        if (reason != nullptr) {
            *reason = "unable to open world save while restoring logical ID state";
        }
        return false;
    }

    const std::string document{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };

    const auto nextIdsPos = document.find("\"nextIds\"");
    if (nextIdsPos == std::string::npos) {
        if (reason != nullptr) {
            *reason = "world save is missing nextIds";
        }
        return false;
    }

    struct CounterField final {
        std::string_view key;
        LogicalIdDomain domain;
    };

    constexpr CounterField fields[] = {
        {"case", LogicalIdDomain::Case},
        {"business", LogicalIdDomain::Business},
        {"clerk", LogicalIdDomain::Clerk},
        {"vehicle", LogicalIdDomain::Vehicle},
        {"lootContainer", LogicalIdDomain::LootContainer}
    };

    for (const auto& field : fields) {
        const auto value = extractUnsignedAfter(document, field.key, nextIdsPos);
        if (!value.has_value() || !generator.setNextSequence(field.domain, *value)) {
            if (reason != nullptr) {
                *reason = "invalid nextIds counter for " + std::string(field.key);
            }
            return false;
        }
    }

    return true;
}

} // namespace gco
