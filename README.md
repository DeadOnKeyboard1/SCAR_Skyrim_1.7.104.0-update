# SCAR - Skyrim Combos AI Revolution

Universal Flatrim SE/AE port and compatibility update for Skyrim Special Edition and Anniversary Edition.

- **Mandatory Primary Target:** Skyrim AE 1.7.104.0 (SKSE 2.3.1)
- **Universal Baseline:** Skyrim SE 1.5.97.0 / AE 1.6.1170.0 / AE 1.7.104.0
- **Original Author & Upstream Project:** Maxsu (https://github.com/max-su-2019/SCAR)
- **Primary License:** GNU General Public License v3.0 or later (GPL-3.0-or-later) with Modding Exception and GPL-3.0 Linking Exception (see [LICENSE](LICENSE) and [EXCEPTIONS.md](EXCEPTIONS.md))
- **Original Upstream License:** MIT License, Copyright (c) 2023 max-su-2019 (see [LICENSE_Original_MIT.txt](LICENSE_Original_MIT.txt))

## Requirements & Assets
- **Original Mod Assets:** Original animation annotations, moveset mods, and Nemesis/Pandora behavior generation remain required. This release provides the updated native SKSE plugin DLL.
- **SKSE64:** Compatible with the SKSE64 build matching your Skyrim runtime version (SKSE 2.0.20 for 1.5.97, SKSE 2.2.8 for 1.6.1170, SKSE 2.3.1 for 1.7.104).
- **Address Library for SKSE Plugins:** All-in-one format matching your game runtime.

## Compatibility Method & Preserved Support
- **CommonLibSSE-NG:** Pinned to official stable release v12.1.0 (`8ffc2b74af015f837de53cc02b0cb470a15678e4`), built in Flatrim mode (SE+AE enabled, VR disabled matching original plugin design).
- **Relocations & Native Hooks:** Native CALL sites validated against instruction boundaries and Address Library database IDs. Dynamic trampoline allocation uses safe DKUtil stubs and verified boundary checks.
- **Runtime Matrix:**
  - Skyrim SE 1.5.97.0: Supported by build; original SE branch preserved
  - Skyrim AE 1.6.1170.0: Supported by build; AE branch preserved
  - Skyrim AE 1.7.104.0: Supported by build; verified against reference binaries; confirmed working in-game by user

## Credits & Attribution
- **Original Mod & Author:** Maxsu ([Nexus Mod](https://www.nexusmods.com/skyrimspecialedition/mods/72014) | [GitHub](https://github.com/max-su-2019/SCAR))
- **Original Project & Contributors:** Full upstream history and acknowledgments can be found in the [upstream repository](https://github.com/max-su-2019/SCAR).

## Building from Source

### Prerequisites
- CMake 3.25+
- Visual Studio 2022 (MSVC toolset with C++23 support)
- vcpkg or system dependencies: `spdlog`, `fmt`, `DirectXTK`
- CommonLibSSE-NG v12.1.0 (under `extern/CommonLibSSE-NG` or specified via `CommonLibPath`)
- DKUtil (specified via `DKUtilPath` or placed alongside dependencies)

### Build Commands
```sh
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
