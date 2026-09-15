#include "PlatformTypes.h"

#include <cmath>

namespace gco::platform {

GridCell gridCellFor(const Vec3& position, const float cellSize) noexcept {
    if (!(cellSize > 0.0f) || !std::isfinite(cellSize)) {
        return {};
    }

    return {
        static_cast<int>(std::floor(position.x / cellSize)),
        static_cast<int>(std::floor(position.y / cellSize)),
        static_cast<int>(std::floor(position.z / cellSize))
    };
}

} // namespace gco::platform
