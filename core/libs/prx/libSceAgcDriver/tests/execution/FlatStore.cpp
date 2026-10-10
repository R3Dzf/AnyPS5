#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t MaskedThreads = 16;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t CheckedBytes = 0x2000;
constexpr std::uint8_t Fill = 0xcd;

alignas(256) constexpr std::array<std::uint32_t, 45> FlatStoreCode{
    0x34020082, 0x34040084, 0x340c0081, 0x340e0083, 0x4a060200, 0x7e080201, 0x4a1806ff, 0x00000204,
    0x7e1a0201, 0x4a1c04ff, 0x00001000, 0x4a1000ff, 0xa1b2c300, 0x4a1202ff, 0x51000000, 0x4a1404ff,
    0x00c0ffee, 0x4a160eff, 0x7e570000, 0xdc708000, 0x00000801, 0xdc700100, 0x007d0903, 0xdc708ffc,
    0x007d0a0c, 0xdc608400, 0x00000800, 0xdc688480, 0x00000906, 0xdc708601, 0x00000b07, 0xdc788000,
    0x0000080e, 0xdc748400, 0x0000080e, 0xdc7c8800, 0x0000090e, 0xdc308000, 0x0f000001, 0xdc708500,
    0x00000f01, 0x7da80090, 0xdc708300, 0x00000a01, 0xbf810000,
};

enum class StoreGroup { All, Dword, Narrow, Vector2, Vector3, Vector4 };

template<std::size_t Start, std::size_t Count, std::size_t TailStart, std::size_t TailCount>
consteval auto StoreCode() {
    std::array<std::uint32_t, 19 + Count + TailCount> code{};
    std::copy_n(FlatStoreCode.begin(), 19, code.begin());
    std::copy_n(FlatStoreCode.begin() + Start, Count, code.begin() + 19);
    std::copy_n(FlatStoreCode.begin() + TailStart, TailCount, code.begin() + 19 + Count);
    return code;
}

alignas(256) constexpr auto DwordStoreCode = StoreCode<19, 6, 37, 8>();
alignas(256) constexpr auto NarrowStoreCode = StoreCode<25, 6, 44, 1>();
alignas(256) constexpr auto Vector2StoreCode = StoreCode<33, 2, 44, 1>();
alignas(256) constexpr auto Vector3StoreCode = StoreCode<35, 2, 44, 1>();
alignas(256) constexpr auto Vector4StoreCode = StoreCode<31, 2, 44, 1>();

std::span<const std::uint32_t> CodeFor(StoreGroup group) {
    switch (group) {
    case StoreGroup::All: return FlatStoreCode;
    case StoreGroup::Dword: return DwordStoreCode;
    case StoreGroup::Narrow: return NarrowStoreCode;
    case StoreGroup::Vector2: return Vector2StoreCode;
    case StoreGroup::Vector3: return Vector3StoreCode;
    case StoreGroup::Vector4: return Vector4StoreCode;
    }
    throw std::runtime_error("flat store: unknown instruction group");
}

class GuestBlock {
public:
    explicit GuestBlock(bool writable) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "flat store: cannot allocate the guest block");
        Clear();
        GuestAllocations::Mutation().Add(block, BlockBytes, true, writable);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    void Clear() { std::memset(block, Fill, BlockBytes); }
    const std::uint8_t* Data() const { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::vector<std::uint8_t> Expected(StoreGroup group) {
    std::vector<std::uint8_t> image(CheckedBytes, Fill);
    const auto put = [&](std::uint32_t offset, std::uint32_t value, std::uint32_t bytes) {
        for (std::uint32_t byte = 0; byte < bytes; ++byte) image.at(offset + byte) = static_cast<std::uint8_t>(value >> (byte * 8u));
    };
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::array<std::uint32_t, 4> data{0xa1b2c300u + tid, 0x51000000u + tid * 4u, 0x00c0ffeeu + tid * 16u, 0x7e570000u + tid * 8u};
        if (group == StoreGroup::All || group == StoreGroup::Dword) {
            put(0x000u + tid * 4u, data[0], 4);
            put(0x100u + tid * 4u, data[1], 4);
            put(0x200u + tid * 4u, data[2], 4);
            if (tid < MaskedThreads) put(0x300u + tid * 4u, data[2], 4);
            put(0x500u + tid * 4u, data[0], 4);
        }
        if (group == StoreGroup::All || group == StoreGroup::Narrow) {
            put(0x400u + tid, data[0], 1);
            put(0x480u + tid * 2u, data[1], 2);
            put(0x601u + tid * 8u, data[3], 4);
        }
        if (group == StoreGroup::All || group == StoreGroup::Vector3) {
            for (std::uint32_t dword = 0; dword < 3; ++dword) put(0x800u + tid * 16u + dword * 4u, data[dword + 1u], 4);
        }
        if (group == StoreGroup::All || group == StoreGroup::Vector4) {
            for (std::uint32_t dword = 0; dword < 4; ++dword) put(0x1000u + tid * 16u + dword * 4u, data[dword], 4);
        }
        if (group == StoreGroup::All || group == StoreGroup::Vector2) {
            for (std::uint32_t dword = 0; dword < 2; ++dword) put(0x1400u + tid * 16u + dword * 4u, data[dword], 4);
        }
    }
    return image;
}

void Dispatch(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const std::uint8_t* base, StoreGroup group) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    std::vector<std::uint32_t> userData(8, 0u);
    userData[0] = static_cast<std::uint32_t>(address);
    userData[1] = static_cast<std::uint32_t>(address >> 32u);
    const auto code = CodeFor(group);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void RunStores(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize, StoreGroup group) {
    guest.Clear();
    Dispatch(device, waveSize, guest.Data(), group);
    const auto expected = Expected(group);
    for (std::uint32_t offset = 0; offset < CheckedBytes; ++offset) {
        const auto actual = guest.Data()[offset];
        Require(actual == expected[offset], "flat store: wave" + std::to_string(waveSize) + " byte " + std::to_string(offset) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[offset]));
    }
}

void RunReadOnly(AgcDriver::VulkanDevice& device, const GuestBlock& guest, StoreGroup group) {
    Dispatch(device, 32, guest.Data(), group);
    for (std::uint32_t offset = 0; offset < CheckedBytes; ++offset) {
        Require(guest.Data()[offset] == Fill, "flat store: a store into a read-only range changed byte " + std::to_string(offset));
    }
}

}

int main(int argc, char** argv) {
    try {
        Require(argc <= 2, "flat store: expected one optional test mode");
        const std::string_view mode = argc == 2 ? argv[1] : "all";
        constexpr std::array<std::string_view, 16> modes{
            "all", "wave32", "wave32_narrow", "wave32_vector2", "wave32_vector3", "wave32_vector4",
            "wave64", "wave64_narrow", "wave64_vector2", "wave64_vector3", "wave64_vector4",
            "read_only", "read_only_narrow", "read_only_vector2", "read_only_vector3", "read_only_vector4"
        };
        Require(std::find(modes.begin(), modes.end(), mode) != modes.end(),
                "flat store: unknown test mode " + std::string(mode));
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock writable(true);
        const GuestBlock readOnly(false);
        if (mode == "all") {
            RunStores(*device, writable, 32, StoreGroup::All);
            RunStores(*device, writable, 64, StoreGroup::All);
            RunReadOnly(*device, readOnly, StoreGroup::All);
        } else {
            auto group = StoreGroup::Dword;
            if (mode.ends_with("_narrow")) group = StoreGroup::Narrow;
            else if (mode.ends_with("_vector2")) group = StoreGroup::Vector2;
            else if (mode.ends_with("_vector3")) group = StoreGroup::Vector3;
            else if (mode.ends_with("_vector4")) group = StoreGroup::Vector4;
            if (mode.starts_with("read_only")) RunReadOnly(*device, readOnly, group);
            else RunStores(*device, writable, mode.starts_with("wave64") ? 64 : 32, group);
        }
        std::puts("flat store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
