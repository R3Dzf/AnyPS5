# AnyPS5

A development fork of [boykopovar/AnyPS5](https://github.com/boykopovar/AnyPS5) for executable relinking and system-library compatibility on Linux and Windows.

The [relinker](core/relinker) converts supported, decrypted ELF inputs to the target system's native executable format. Reimplemented [system PRX libraries](core/libs/prx) provide host implementations for dynamic linking. Converted code runs in the host process; there is no separate emulation process.

Combined development work is on [integration/project-completion](https://github.com/R3Dzf/AnyPS5/tree/integration/project-completion). The status below describes that branch as of **2026-10-10**.

[Usage](docs/user/USAGE.md) · [Build](docs/dev/BUILD.md) · [Architecture](docs/dev/ARCHITECTURE.md) · [Technical debt](docs/dev/TechnicalDebt.md) · [Conventions](docs/dev/CONVENTIONS.md) · [Contributing](CONTRIBUTING.md)

## Status

| Metric | Count | Meaning |
| --- | --- | --- |
| Known PRX exports classified as implemented | 2718 / 3044 (89.29%) | The source scanner finds no recognized unimplemented path in these export bodies. |
| Known PRX exports still unimplemented | 326 / 3044 (10.71%) | Explicit unimplemented paths remain. |
| RDNA ISA names mapped by the opcode counter | 1166 / 1166 (100%) | Name coverage only; the counter does not verify translation semantics. |

**These percentages are not an estimate of whole-project completion or game compatibility.** The denominator covers functions currently known to the project and grows as more interfaces are identified. An export body can still implement only part of the console behavior. Counts come from [tools/progress.py](tools/progress.py).

Unsupported or unexpected states strictly throw `std::runtime_error`. `what()` is printed to stderr and the process terminates.

The [shader recompiler](core/shader/recompiler/Recompiler.cpp) produces SPIR-V, with [SPIRV-Tools](3rdparty/SPIRV-Tools) validation available through `ANYPS5_ENABLE_SPIRV_TOOLS`.

## Integrated improvements

- AIO batch polling from [upstream #2118](https://github.com/boykopovar/AnyPS5/pull/2118) by Ruzi.
- H.264 software decoding from [upstream #2203](https://github.com/boykopovar/AnyPS5/pull/2203) by XairRrR, with oversized access-unit rejection and recovery coverage.
- Generated font glyph images from [upstream #2150](https://github.com/boykopovar/AnyPS5/pull/2150) by erick-dsnk.
- Current-main memory and synchronization changes, including staging-buffer reuse and reduced heap-mirror work, plus device-buffer allocation/binding failure recovery.
- BDA shader-store and page-tracking optimizations from [upstream #1265](https://github.com/boykopovar/AnyPS5/pull/1265) by popcoder565 and [#1280](https://github.com/boykopovar/AnyPS5/pull/1280) by Scott Schuster.
- Windows compiler-runtime selection, exception/future lifetime fixes, native helper preservation during NID patching and cleanup after failed video initialization.
- Deferred shader preparation and acceptance of matching bound header copies from [upstream #1836](https://github.com/boykopovar/AnyPS5/pull/1836) and [#1837](https://github.com/boykopovar/AnyPS5/pull/1837) by EdouardSence. Unsupported computed calls still fail when used.
- Sampled-image heaps now hold 64 images of one class, with bindless tables retaining 16 slots, from [upstream #2347](https://github.com/boykopovar/AnyPS5/pull/2347) by Alex Collini.
- Wave64 half-wave scan matching through merged lane masks, vertex entry masks and floating-point min/max identity keys from [upstream #2449](https://github.com/boykopovar/AnyPS5/pull/2449), [#2450](https://github.com/boykopovar/AnyPS5/pull/2450) and [#2451](https://github.com/boykopovar/AnyPS5/pull/2451) by Dean Galvin. Arbitrary reads of unavailable lanes remain unsupported.
- Failed preparation requests can be captured with `APS5_DUMP_SHADERS=1` and replayed with `agc_shader_replay`, from [upstream #2313](https://github.com/boykopovar/AnyPS5/pull/2313) by Julio Cacko, adapted to this branch's preparation API.
- Failed request writes now report failure and remain retryable; partial outputs are removed. Replay reports failed input/output operations and rejects invocations with no requests.

Upstream source commits and authors are credited in the commit history.

## Validation

The full Linux/Windows build and CTest run passed for source commit [2c8969be](https://github.com/R3Dzf/AnyPS5/commit/2c8969bea60a2a6ad7c2d76b119e087e971a4e6e): [Actions run 38047302575](https://github.com/R3Dzf/AnyPS5/actions/runs/38047302575).

| Platform | Registered | Passed | Skipped | Failed |
| --- | ---: | ---: | ---: | ---: |
| Linux | 538 | 511 | 27 | 0 |
| Windows | 529 | 303 | 226 | 0 |

Both jobs built the full project and all guest libraries. The Windows patched-library loader test passed. Linux used lavapipe and enabled SPIRV-Tools. Skipped tests leave coverage unverified, particularly for graphics and display facilities.

Flat-store execution is split into dword/copy/masked stores, narrow/unaligned stores and separate 2/3/4-dword stores across wave32, wave64 and read-only modes. Each of the 15 cases checks 8192 bytes, with project and Mesa shader caches disabled and the 30-second limit retained. All 15 cold cases passed on Linux CI; the longest took 5.52 seconds. Local Linux validation on lavapipe (LLVM 20.1.2) also passed all 15; the longest took 17.00 seconds. Windows skipped these GPU execution cases because the required Vulkan device was unavailable.

[Separate Windows regression validation](https://github.com/R3Dzf/AnyPS5/actions/runs/38033956388) reproduced a crash with the old exception implementation, then passed 20 repeated exception tests with the fix. Another 400 fresh video-startup processes exited cleanly with the unavailable-facility status. This checks failure cleanup; successful game rendering was not tested by those runs.

## Remaining work

- **System libraries:** implement the 326 currently identified missing exports and verify the behavior of existing implementations. Additional interfaces may be discovered when testing more titles.
- **Audio:** `libSceAudio3d` currently produces no sound, and `libSceVoice` has six unimplemented exports. Complete Tempest/3D audio output remains unfinished.
- **GPU memory:** predictive transfer scheduling and validation under discrete-GPU VRAM pressure remain unfinished. This validation did not test a physical 6 GB GPU or PCIe performance.
- **Shaders:** full Mesh Shader and Ray Tracing compatibility requires further implementation and hardware validation. Opcode-name coverage does not establish correct rendering.
- **Input executables:** SELF containers are rejected. Automatic SELF decryption and the requested DRM-conversion pipeline are not implemented.

Detailed limitations and uncertain behavior are recorded in [TechnicalDebt](docs/dev/TechnicalDebt.md).

## Build and usage

Use the supported toolchains and dependencies in the [build instructions](docs/dev/BUILD.md). For the combined development branch:

```sh
git clone --recurse-submodules --branch integration/project-completion https://github.com/R3Dzf/AnyPS5.git
cd AnyPS5
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTING=ON
cmake --build build --parallel
cmake --build build --target libs --parallel
ctest --test-dir build --output-on-failure --timeout 30
```

The `libs` target builds and patches the guest libraries; the default build alone does not refresh the full library set. See [usage](docs/user/USAGE.md) for conversion and runtime layout, or the [relinker-only build](docs/dev/BUILD.md#relinker-only) for development without the full dependency set.

For shader diagnostics, run the converted program with `APS5_DUMP_SHADERS=1` to capture requests in its working directory. Build the replay tool separately and pass a captured request:

```sh
cmake --build build --target agc_shader_replay
build/core/libs/prx/libSceAgcDriver/agc_shader_replay --spv shader_12345cafe.req
```

Replace the example request name with the captured filename; Windows uses `agc_shader_replay.exe`. Local Linux checks recompiled a valid request to SPIR-V and reproduced the failure of an unsupported instruction. This tool diagnoses compilation; it does not run a game.

## Compatibility

See the [game compatibility list](docs/user/COMPATIBILITY.md) for recorded results. It includes the upstream Windows result for Dreaming Sarah (PPSA02929); those game measurements were not reproduced in this validation.

God of War Ragnarök, Spider-Man 2 and other AAA titles have not been verified in this fork. Passing internal tests does not guarantee that a commercial game starts or renders correctly.

## Input mapping

SDL-mapped game controllers are supported, including analog sticks and triggers. Keyboard and mouse controls can be configured with an `anyps5-input.ini` file. See [input mapping](docs/user/INPUT_MAPPING.md) for the supported devices and configuration format.

## Disclaimer

This project is intended for interoperability, research, preservation, and compatibility purposes. It does not include, distribute, or require copyrighted software, firmware, cryptographic keys, or proprietary libraries. Users are responsible for ensuring that any binaries used with this project are obtained and used in accordance with applicable laws and their respective license terms.

## License

This project is licensed under the GNU General Public License version 2 only.
