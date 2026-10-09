#include <cstddef>
#include <cstdint>

#include "prx/libc/include/General.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib, uint32_t flags) {
    (void)listener;
    (void)source;
    (void)out_attrib;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* out_work, uint32_t flags) {
    (void)param;
    (void)out_work;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param) {
    if (out_listener_param == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *out_listener_param = Ngs2GeomListenerParam{};
    out_listener_param->orient_front.z = 1.0f;
    out_listener_param->orient_up.y = 1.0f;
    out_listener_param->sound_speed = 343.0f;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param) {
    if (out_source_param == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *out_source_param = Ngs2GeomSourceParam{};
    out_source_param->direction.z = 1.0f;
    out_source_param->cone = {1.0f, 360.0f, 1.0f, 360.0f};
    out_source_param->rolloff = {0, 1000000.0f, 1.0f, 1.0f};
    out_source_param->doppler_factor = 1.0f;
    out_source_param->fbw_level = 1.0f;
    out_source_param->lfe_level = 1.0f;
    out_source_param->max_level = 1.0f;
    out_source_param->num_speakers = 2;
    out_source_param->matrix_format = 2;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params, uint32_t num_params, uint32_t matrix_format, float* out_volume_matrix) {
    (void)work;
    (void)params;
    (void)num_params;
    (void)matrix_format;
    (void)out_volume_matrix;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNgs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle, uint32_t num_speakers) {
    if (work == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *work = Ngs2PanWork{};
    work->unit_angle = unit_angle;
    work->num_speakers = num_speakers < 8 ? num_speakers : 8;
    if (speaker_angles != nullptr) {
        for (uint32_t i = 0; i < work->num_speakers; ++i) work->speaker_angles[i] = speaker_angles[i];
    }
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
