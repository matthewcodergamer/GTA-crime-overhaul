#pragma once

#include "PlatformTypes.h"

#include <cstdint>
#include <string_view>

namespace gco::platform {

class IFacialAnimationAdapter {
public:
    virtual ~IFacialAnimationAdapter() = default;

    // Asset-agnostic wrapper. Callers must supply a validated facial dictionary/clip pair.
    // The wrapper validates the ped and animation dictionary and enforces a finite load timeout.
    virtual bool play(
        PedHandle ped,
        std::string_view dictionary,
        std::string_view clip,
        std::uint32_t timeoutMs) = 0;
};

class NativeFacialAnimationAdapter final : public IFacialAnimationAdapter {
public:
    bool play(
        PedHandle ped,
        std::string_view dictionary,
        std::string_view clip,
        std::uint32_t timeoutMs) override;
};

} // namespace gco::platform
