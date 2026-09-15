#include "PedPresentationAdapter.h"

#include <natives.h>

namespace gco::platform {

PedPresentationTraits NativePedPresentationAdapter::classify(const PedHandle ped) const {
    PedPresentationTraits result{};
    if (ped == 0
        || ENTITY::DOES_ENTITY_EXIST(ped) == FALSE
        || ENTITY::IS_ENTITY_A_PED(ped) == FALSE) {
        return result;
    }

    result.human = PED::IS_PED_HUMAN(ped) != FALSE;
    if (!result.human) {
        return result;
    }

    result.voiceGender = PED::IS_PED_MALE(ped) != FALSE
        ? PedVoiceGender::Masculine
        : PedVoiceGender::Feminine;
    return result;
}

} // namespace gco::platform
