#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int USER_ID_SYSTEM = 0xFF;
static constexpr std::uint32_t MIN_SUPPORTED_3D_LATENCY = 1;
static constexpr std::uint32_t MAX_SUPPORTED_3D_LATENCY = 2;
static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80260502);
static constexpr std::uint32_t PORT_ATTRIBUTE_ID_PCM = 0;
static constexpr std::uint32_t PORT_ATTRIBUTE_ID_GAIN = 1;
static constexpr std::uint32_t PORT_ATTRIBUTE_ID_AMBISONICS = 8;

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void) {
    return 0;
}

int APS5_VABI sceAudioOut2Set3DLatency(int userId, std::uint32_t latency) {
    if (userId != USER_ID_SYSTEM || latency < MIN_SUPPORTED_3D_LATENCY || latency > MAX_SUPPORTED_3D_LATENCY) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (!state) return static_cast<int>(0x80260502);
    state->loudness = 0.0f;
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    (void)param;
    return 0;
}

int APS5_VABI sceAudioOut2UserGetSupportedAttributes(AudioOut2UserHandle handle, std::uint32_t* contextAttributes, std::uint32_t* portAttributes) {
    if (contextAttributes == nullptr || portAttributes == nullptr || handle == 0) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    *contextAttributes = 0;
    *portAttributes = (1u << PORT_ATTRIBUTE_ID_PCM) | (1u << PORT_ATTRIBUTE_ID_GAIN) | (1u << PORT_ATTRIBUTE_ID_AMBISONICS);
    return 0;
}

}
