#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM = static_cast<int>(0x80268201);
static constexpr int SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_STATES_ID = static_cast<int>(0x80268204);
static constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT = 1;
static constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_ID_V2 = 2;
static constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_COMPRESSOR_DEFAULT = 0x01010001;
static constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_COMPRESSOR_V2 = 0x01020001;
static constexpr std::uint32_t AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_LIMITER_DEFAULT = 0x01010003;

extern "C" {

int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags) {
    if (flags != 0) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2MasteringTerm(void) {
    return 0;
}

int APS5_VABI sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user) {
    (void)output;
    (void)user;
    if (state == nullptr) return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_API_PARAM;
    switch (state->states_id) {
        case AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT: {
            auto* full_state = reinterpret_cast<AudioOut2MasteringStates*>(state);
            std::memset(full_state, 0, sizeof(AudioOut2MasteringStates));
            full_state->states_header.states_id = AUDIO_OUT2_MASTERING_STATES_ID_DEFAULT;
            full_state->compressor_states.descriptor.id = AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_COMPRESSOR_DEFAULT;
            full_state->compressor_states.descriptor.size = sizeof(AudioOut2MasteringCompressorStates);
            full_state->limiter_states.descriptor.id = AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_LIMITER_DEFAULT;
            full_state->limiter_states.descriptor.size = sizeof(AudioOut2MasteringLimiterStates);
            return 0;
        }
        case AUDIO_OUT2_MASTERING_STATES_ID_V2: {
            auto* full_state = reinterpret_cast<AudioOut2MasteringStatesV2*>(state);
            std::memset(full_state, 0, sizeof(AudioOut2MasteringStatesV2));
            full_state->states_header.states_id = AUDIO_OUT2_MASTERING_STATES_ID_V2;
            full_state->compressor_states.descriptor.id = AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_COMPRESSOR_V2;
            full_state->compressor_states.descriptor.size = sizeof(AudioOut2MasteringCompressorStatesV2);
            full_state->limiter_states.descriptor.id = AUDIO_OUT2_MASTERING_STATES_STRUCT_ID_LIMITER_DEFAULT;
            full_state->limiter_states.descriptor.size = sizeof(AudioOut2MasteringLimiterStates);
            return 0;
        }
        default: return SCE_AUDIO_OUT2_ERROR_MASTERING_INVALID_STATES_ID;
    }
}

int APS5_VABI sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags) {
    (void)output;
    (void)flags;
    if (param == nullptr) NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
