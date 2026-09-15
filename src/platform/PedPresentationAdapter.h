#pragma once

#include "PlatformTypes.h"

#include <cstdint>

namespace gco::platform {

enum class PedVoiceGender : std::uint8_t {
    Unknown,
    Masculine,
    Feminine
};

struct PedPresentationTraits final {
    bool human = false;
    PedVoiceGender voiceGender = PedVoiceGender::Unknown;
};

class IPedPresentationAdapter {
public:
    virtual ~IPedPresentationAdapter() = default;
    [[nodiscard]] virtual PedPresentationTraits classify(PedHandle ped) const = 0;
};

class NativePedPresentationAdapter final : public IPedPresentationAdapter {
public:
    [[nodiscard]] PedPresentationTraits classify(PedHandle ped) const override;
};

} // namespace gco::platform
