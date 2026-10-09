#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DeviceBuffer;

int failures = 0;

void Expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

struct MockBuffer {
    VkDeviceSize bytes;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct MockDevice {
    std::uintptr_t next = 1;
    std::map<VkBuffer, MockBuffer> buffers;
    std::set<VkDeviceMemory> memories;
    VkResult allocationResult = VK_SUCCESS;
    VkResult bindingResult = VK_SUCCESS;
    unsigned creates = 0;
    unsigned allocations = 0;
    unsigned bindings = 0;
    unsigned destroys = 0;
    unsigned frees = 0;
};

MockDevice mock;

VKAPI_ATTR VkResult VKAPI_CALL mockCreateBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = reinterpret_cast<VkBuffer>(mock.next++);
    mock.buffers.emplace(*buffer, MockBuffer{info->size});
    ++mock.creates;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockGetBufferMemoryRequirements(VkDevice, VkBuffer buffer, VkMemoryRequirements* requirements) {
    *requirements = {mock.buffers.at(buffer).bytes, 1, 1};
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    ++mock.allocations;
    const auto result = std::exchange(mock.allocationResult, VK_SUCCESS);
    if (result != VK_SUCCESS) return result;
    *memory = reinterpret_cast<VkDeviceMemory>(mock.next++);
    mock.memories.insert(*memory);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBindBufferMemory(VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    ++mock.bindings;
    Expect(offset == 0 && mock.memories.contains(memory), "binding uses missing memory or a nonzero offset");
    const auto result = std::exchange(mock.bindingResult, VK_SUCCESS);
    if (result == VK_SUCCESS) mock.buffers.at(buffer).memory = memory;
    return result;
}

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) {
    Expect(mock.buffers.erase(buffer) == 1, "buffer destroyed more than once");
    ++mock.destroys;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    for (const auto& [buffer, state] : mock.buffers) Expect(state.memory != memory, "memory freed before its bound buffer was destroyed");
    Expect(mock.memories.erase(memory) == 1, "memory freed more than once");
    ++mock.frees;
}

VKAPI_ATTR void VKAPI_CALL mockUnmapMemory(VkDevice, VkDeviceMemory) {
    Expect(false, "device-local memory was unmapped");
}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> functions{
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCreateBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateMemory)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(mockBindBufferMemory)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyBuffer)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(mockFreeMemory)},
        {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockUnmapMemory)},
    };
    const auto found = functions.find(name);
    return found == functions.end() ? nullptr : found->second;
}

void TestFailureRecovery(bool failBinding, VkResult error) {
    mock = {};
    Context context{};
    context.deviceProc = mockProc;
    context.memory.memoryTypeCount = 1;
    context.memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    context.limits.maxMemoryAllocationCount = 96;
    if (failBinding) mock.bindingResult = error;
    else mock.allocationResult = error;
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bool rejected = false;
    try {
        DeviceBuffer buffer(context, 64, usage);
    } catch (const std::runtime_error& exception) {
        const std::string expected = std::string("AGC graphics: ") + (failBinding ? "vkBindBufferMemory device" : "vkAllocateMemory device buffer") + ": Vulkan result " + std::to_string(error);
        rejected = exception.what() == expected;
    }
    Expect(rejected, "the original Vulkan allocation or binding error was not propagated");
    Expect(mock.buffers.empty() && mock.memories.empty(), "failed construction retained Vulkan resources");
    Expect(mock.destroys == 1 && mock.frees == (failBinding ? 1u : 0u), "failed construction did not destroy its resources immediately");
    VkBuffer successful = VK_NULL_HANDLE;
    {
        DeviceBuffer buffer(context, 64, usage);
        successful = buffer.Handle();
        Expect(mock.buffers.at(successful).memory != VK_NULL_HANDLE, "recovery returned an unbound buffer");
        Expect(buffer.Size() == 64, "recovery changed the requested size");
    }
    Expect(mock.creates == 2 && mock.allocations == 2 && mock.bindings == (failBinding ? 2u : 1u), "recovery reused a failed allocation");
    {
        DeviceBuffer buffer(context, 64, usage);
        Expect(buffer.Handle() == successful, "a successfully bound buffer was not reused");
        Expect(mock.buffers.at(buffer.Handle()).memory != VK_NULL_HANDLE, "the reused buffer lost its bound memory");
    }
    Expect(mock.creates == 2 && mock.allocations == 2, "normal pool reuse allocated extra Vulkan resources");
    context.bufferPool.reset();
    Expect(mock.buffers.empty() && mock.memories.empty(), "pool destruction leaked Vulkan resources");
    Expect(mock.destroys == 2 && mock.frees == (failBinding ? 2u : 1u), "resource ownership caused a double destruction");
}

}

int main() {
    try {
        for (const auto error : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) {
            TestFailureRecovery(true, error);
            TestFailureRecovery(false, error);
        }
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "FAIL: %s\n", exception.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("Device-buffer failure recovery tests passed");
    return 0;
}
