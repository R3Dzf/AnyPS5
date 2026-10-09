#include <cstddef>
#include <cstdint>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <sys/random.h>
#include <system_error>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_RANDOM_ERROR_INVALID = static_cast<int>(0x817C0016);
constexpr std::size_t SCE_RANDOM_MAX_SIZE = 64;

}

extern "C" {

int APS5_VABI sceRandomGetRandomNumber(void* buf, size_t size) {
    if (!buf || size > SCE_RANDOM_MAX_SIZE) return SCE_RANDOM_ERROR_INVALID;
    if (size == 0) return 0;
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf), static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("sceRandomGetRandomNumber: BCryptGenRandom failed");
    }
#else
    auto* bytes = static_cast<unsigned char*>(buf);
    std::size_t filled = 0;
    while (filled < size) {
        const auto received = getrandom(bytes + filled, size - filled, 0);
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) throw std::system_error(received < 0 ? errno : EIO, std::generic_category(), "sceRandomGetRandomNumber: getrandom failed");
        filled += static_cast<std::size_t>(received);
    }
#endif
    return 0;
}

}
