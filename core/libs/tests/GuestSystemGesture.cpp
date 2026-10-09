#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>

extern "C" {
std::int32_t APS5_VABI sceSystemGestureOpen(std::int32_t, const void*);
int APS5_VABI sceSystemGestureInitializePrimitiveTouchRecognizer(const void*);
int APS5_VABI sceSystemGestureUpdatePrimitiveTouchRecognizer(std::int32_t, const void*);
int APS5_VABI sceSystemGestureFinalizePrimitiveTouchRecognizer(void);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventsCount(std::int32_t);
int APS5_VABI sceSystemGestureGetPrimitiveTouchEventByIndex(std::int32_t, std::uint32_t, SystemGesturePrimitiveTouchEvent*);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidHandle = static_cast<int>(0x80D10003);
    constexpr int indexOutOfArray = static_cast<int>(0x80D10005);

    const std::int32_t handle = sceSystemGestureOpen(0, nullptr);
    Require(handle > 0);
    Require(sceSystemGestureInitializePrimitiveTouchRecognizer(nullptr) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);

    SystemGesturePrimitiveTouchEvent event{};
    event.event_state = 77;
    const std::uint32_t indices[] = {0, 1, std::numeric_limits<std::uint32_t>::max()};
    for (std::uint32_t index : indices) {
        Require(sceSystemGestureGetPrimitiveTouchEventByIndex(handle, index, &event) == indexOutOfArray);
        Require(event.event_state == 77);
    }

    const std::int32_t invalidHandles[] = {0, -1, 2, std::numeric_limits<std::int32_t>::min()};
    for (std::int32_t invalid : invalidHandles) {
        Require(sceSystemGestureGetPrimitiveTouchEventsCount(invalid) == invalidHandle);
        Require(sceSystemGestureGetPrimitiveTouchEventByIndex(invalid, 0, &event) == invalidHandle);
        Require(event.event_state == 77);
    }

    Require(sceSystemGestureUpdatePrimitiveTouchRecognizer(handle, nullptr) == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);
    Require(sceSystemGestureFinalizePrimitiveTouchRecognizer() == 0);
    Require(sceSystemGestureGetPrimitiveTouchEventsCount(handle) == 0);
}
