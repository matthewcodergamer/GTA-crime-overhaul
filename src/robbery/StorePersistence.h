#pragma once

#include "Foundation.h"
#include "StoreDomain.h"

#include <string>

namespace gco::robbery {

class PrototypeStorePersistence final {
public:
    PrototypeStorePersistence(RuntimePaths paths, WorldStateStore& worldState)
        : paths_(std::move(paths)), worldState_(worldState) {}

    bool load(
        PrototypeStoreModel& model,
        LogicalIdGenerator& ids,
        std::uint64_t nowMs,
        const StoreTuning& tuning,
        std::string* reason = nullptr) const;

    bool save(
        const PrototypeStoreModel& model,
        const LogicalIdGenerator& ids,
        std::string* reason = nullptr) const;

private:
    RuntimePaths paths_;
    WorldStateStore& worldState_;
};

} // namespace gco::robbery
