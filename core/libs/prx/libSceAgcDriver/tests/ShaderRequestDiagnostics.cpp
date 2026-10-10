#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
public:
    TemporaryDirectory() : previous(std::filesystem::current_path()) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 16; ++attempt) {
            path = std::filesystem::temp_directory_path() / ("anyps5-shader-capture-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(path)) {
                std::filesystem::current_path(path);
                return;
            }
        }
        throw std::runtime_error("cannot create shader capture test directory");
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        std::filesystem::remove_all(path, error);
    }

private:
    std::filesystem::path previous;
    std::filesystem::path path;
};

std::string Read(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "shader request was not saved");
    std::ostringstream text;
    text << file.rdbuf();
    Require(!file.bad(), "shader request could not be read");
    return text.str();
}

template<typename TAction>
std::string Failure(TAction action) {
    try { action(); }
    catch (const std::exception& error) { return error.what(); }
    throw std::runtime_error("invalid shader preparation succeeded");
}

void OpenFailure(ShaderRecompiler::RecompileRequest request) {
    request.shader.codeAddress = 0x12345cafeull;
    const std::string name = "shader_12345cafe.req";
    Require(std::filesystem::create_directory(name), "cannot block shader request output");
    Require(AgcDriver::DriverDetail::Driver::DumpRequest(request.shader.codeAddress, request).empty(), "failed shader capture reported a saved request");
    const auto expected = Failure([&] { static_cast<void>(ShaderRecompiler::PrepareShader(request)); });
    Require(Failure([&] { static_cast<void>(AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request)); }) == expected, "capture failure replaced the shader preparation failure");
    Require(std::filesystem::is_directory(name), "failed capture removed an existing directory");
    Require(std::filesystem::remove(name), "cannot unblock shader request output");
    Require(Failure([&] { static_cast<void>(AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request)); }) == expected, "capture retry replaced the shader preparation failure");
    const ShaderRecompiler::RequestSerializer serializer;
    const auto text = Read(name);
    Require(text.size() > 8192 && text == serializer.Serialize(request), "capture did not recover with a complete request");
    const auto replay = serializer.Deserialize(text);
    Require(serializer.Serialize(replay.request) == text, "recovered request changed during replay");
}

void ConcurrentCapture(ShaderRecompiler::RecompileRequest request) {
    request.shader.codeAddress = 0x12345cab0ull;
    const std::string name = "shader_12345cab0.req";
    std::array<std::string, 8> outputs;
    std::vector<std::jthread> writers;
    for (auto& output : outputs) writers.emplace_back([&request, &output] {
        output = AgcDriver::DriverDetail::Driver::DumpRequest(request.shader.codeAddress, request);
    });
    writers.clear();
    for (const auto& output : outputs) Require(output == name, "concurrent captures lost the saved request");
    Require(Read(name) == ShaderRecompiler::RequestSerializer{}.Serialize(request), "concurrent captures truncated the request");
    request.layout.pushConstantSizeBytes = 64;
    Require(AgcDriver::DriverDetail::Driver::DumpRequest(request.shader.codeAddress, request) == name, "duplicate capture lost its filename");
    const auto replay = ShaderRecompiler::RequestSerializer{}.Deserialize(Read(name));
    Require(replay.request.layout.pushConstantSizeBytes == 128, "duplicate capture replaced the first saved request");
}

#ifdef __linux__
void WriteFailure(ShaderRecompiler::RecompileRequest request, bool buffered) {
    request.shader.codeAddress = buffered ? 0x12345cae0ull : 0x12345cae1ull;
    if (buffered) request.shader.header = {};
    const std::string name = buffered ? "shader_12345cae0.req" : "shader_12345cae1.req";
    std::filesystem::create_symlink("/dev/full", name);
    Require(AgcDriver::DriverDetail::Driver::DumpRequest(request.shader.codeAddress, request).empty(), "incomplete shader capture reported success");
    Require(!std::filesystem::exists(name), "incomplete shader request was retained");
    Require(AgcDriver::DriverDetail::Driver::DumpRequest(request.shader.codeAddress, request) == name, "shader capture did not retry after a write failure");
    Require(Read(name) == ShaderRecompiler::RequestSerializer{}.Serialize(request), "shader capture retry produced an incomplete request");
}
#endif

}

int main() {
    try {
        const TemporaryDirectory directory;
#ifdef _WIN32
        Require(_putenv_s("APS5_DUMP_SHADERS", "1") == 0, "cannot enable shader capture");
#else
        Require(setenv("APS5_DUMP_SHADERS", "1", 1) == 0, "cannot enable shader capture");
#endif
        const std::array<std::uint32_t, 1> code{0xffffffffu};
        const std::vector<std::byte> header(8192, std::byte{0x5a});
        ShaderRecompiler::RecompileRequest request{};
        request.shader = {ShaderRecompiler::ShaderStage::Compute, 0x1000, code, 0x20000, header};
        request.context.waveSize = 32;
        request.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{1, 1, 1}, 0, {}, false, 1, {}};
        request.target.vulkanVersion = 0x00402000;
        request.target.spirvVersion = 0x00010300;
        request.target.subgroupSize = 32;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        OpenFailure(request);
        ConcurrentCapture(request);
#ifdef __linux__
        WriteFailure(request, true);
        WriteFailure(request, false);
#endif
        std::puts("shader request diagnostic recovery passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
