#include "FacialAnimationAdapter.h"

#include <Windows.h>
#include <main.h>
#include <natives.h>

#include <string>

namespace gco::platform {

bool NativeFacialAnimationAdapter::play(
    const PedHandle ped,
    const std::string_view dictionary,
    const std::string_view clip,
    const std::uint32_t timeoutMs) {

    if (ped == 0
        || dictionary.empty()
        || clip.empty()
        || ENTITY::DOES_ENTITY_EXIST(ped) == FALSE
        || ENTITY::IS_ENTITY_A_PED(ped) == FALSE) {
        return false;
    }

    const std::string dict(dictionary);
    const std::string anim(clip);
    if (STREAMING::DOES_ANIM_DICT_EXIST(dict.c_str()) == FALSE) {
        return false;
    }

    STREAMING::REQUEST_ANIM_DICT(dict.c_str());
    const ULONGLONG startedAt = GetTickCount64();
    do {
        if (STREAMING::HAS_ANIM_DICT_LOADED(dict.c_str()) != FALSE) {
            PED::PLAY_FACIAL_ANIM(ped, anim.c_str(), dict.c_str());
            return true;
        }
        scriptWait(0);
    } while (GetTickCount64() - startedAt < timeoutMs);

    if (STREAMING::HAS_ANIM_DICT_LOADED(dict.c_str()) == FALSE) {
        return false;
    }

    PED::PLAY_FACIAL_ANIM(ped, anim.c_str(), dict.c_str());
    return true;
}

} // namespace gco::platform
