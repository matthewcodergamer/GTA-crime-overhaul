#pragma once

#include "CrimeRegistry.h"

#include <string>

namespace gco::crime {

class CrimeDebugInspector final {
public:
    [[nodiscard]] static std::string formatRegistry(const CrimeRegistry& registry);
    [[nodiscard]] static std::string formatCase(const CrimeRegistry& registry, LogicalId caseId);
};

} // namespace gco::crime
