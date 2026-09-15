#pragma once

#include "CrimeRegistry.h"
#include "Foundation.h"

#include <string>

namespace gco::crime {

class CrimePersistenceStore final {
public:
    CrimePersistenceStore(RuntimePaths paths, WorldStateStore& worldState)
        : paths_(std::move(paths)), worldState_(worldState) {}

    bool load(CrimeRegistry& registry, LogicalIdGenerator& ids, std::string* reason = nullptr) const;
    bool save(
        const CrimeRegistry& registry,
        const LogicalIdGenerator& ids,
        std::string* reason = nullptr) const;

private:
    RuntimePaths paths_;
    WorldStateStore& worldState_;
};

} // namespace gco::crime
