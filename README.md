# DXVK-GPLALL (DXVK GPLAsync-LowLatency)

A Vulkan 1.3-based translation layer for Direct3D 8/9/10/11 which allows running 3D applications on: 

1. Windows 10/11, if GPU has Vulkan driver that is Vulkan 1.3 compliant. Requires SSE2 CPU.
2. Linux using Wine, if GPU has Vulkan driver that is Vulkan 1.3 compliant. Requires SSE2 CPU.
3. MacOS using Wine/CrossOver, if GPU has Vulkan driver that is Vulkan 1.3 compliant. Requires SSE2 CPU.
4. Android using [Winlator](https://github.com/brunodev85/winlator), if GPU has Vulkan driver that is Vulkan 1.3 compliant. CPU Requirements defined by [Winlator](https://github.com/brunodev85/winlator).

For GPUs that do not have Vulkan 1.3 compliant driver, it is recommended to use [DXVK-SAO](https://github.com/Digger1955/dxvk-sao). It supports Windows 7/8/10/11, Linux/Mac/Android-over-[Winlator](https://github.com/brunodev85/winlator), requires SSE2 CPU, GPU with Vulkan driver that is Vulkan 1.1 compliant. It has implemented Direct3D 8/9/10/11 and a build with Asynchronous pipeline compilation (Async).

### Additional Info

 - [DXVK-GPLALL Wiki](https://github.com/Digger1955/dxvk-gplall/wiki)

 - [Detailed Changelog](https://github.com/Digger1955/dxvk-gplall/wiki/Detailed-Changelog)

 - [Builds Reference Guide](https://github.com/Digger1955/dxvk-gplall/wiki/Builds-Reference-Guide) - recommended to read to achieve the best results on the end-user system and **REQUIRED TO READ BEFORE PROVIDING DXVK-GPLALL AS A PART OF WINE/PROTON.**

 - [Benchmarks](https://github.com/Digger1955/dxvk-gplall/wiki/Benchmarks)

 - [Frequently Asked Questions (FAQ)](https://github.com/Digger1955/dxvk-gplall/wiki/Frequently-Asked-Questions-(FAQ)) - **REQUIRED TO READ BEFORE PROVIDING DXVK-GPLALL AS A PART OF WINE/PROTON.**

 - [`dxvk.conf` Options Guide](https://github.com/Digger1955/dxvk-gplall/wiki/dxvk.conf-Options-Guide) - recommended to read to achieve the best results on the end-user system and **REQUIRED TO READ BEFORE PROVIDING DXVK-GPLALL AS A PART OF WINE/PROTON.**

 - [Contributing Guidelines](https://github.com/Digger1955/dxvk-gplall/wiki/Contributing-Guidelines)

## Major changes compared to [upstream DXVK](https://github.com/doitsujin/dxvk)

1. Implemented Low Latency frame pacing mode that aims to greatly reduce latency with minimal impact in fps. Author - [netborg-afps](https://github.com/netborg-afps/dxvk/releases)
2. Implemented Asynchronous pipeline compilation (Async) that aims to greatly reduce shader compilation stutter by not blocking the main thread when compiling async pipelines. Authors - [jomihaka](https://github.com/jomihaka/dxvk-poe-hack) and [Sporif](https://github.com/Sporif/dxvk-async)
3. Implemented the ability to use both (together or separately) Graphics Pipeline Library (GPL) and Asynchronous pipeline compilation (Async) on DXVK 2.1 and later. Author - [Ph42oN](https://gitlab.com/Ph42oN/dxvk-gplasync/). Contributor - [Britt Yazel](https://gitlab.com/Ph42oN/dxvk-gplasync/-/merge_requests/12)
4. Implemented State Cache for DXVK 2.7 starting from DXVK-GPLALL 2.7-3. Author - [Laitinlok](https://github.com/Digger1955/dxvk-gplall/pull/29)
5. Implemented improved compared to upstream DXVK high-precision FPS Limiter starting from DXVK-GPLALL 2.6.8-1. Author - [Digger1955](https://github.com/Digger1955/dxvk-gplall/releases)
6. Implemented improved compared to upstream DXVK high-performance MSAA shader for devices without support for `shaderInt64` starting from DXVK-GPLALL 2.6.8-1. Author - [Digger1955](https://github.com/Digger1955/dxvk-gplall/releases)
5. Implemented all of aforementioned in one DXVK package. Author - [Digger1955](https://github.com/Digger1955/dxvk-gplall/releases)
7. Provided various GCC (for any OS) builds of DXVK-GPLALL:

   a) optimized for `SSE2` (`-march=x86-64`) and tuned for older CPUs (`-mtune=x86-64`) with Link-Time Optimization (`LTO`, a.k.a. `-flto=auto`) and `-O3` optimization level;

   b) optimized for `SSE4.2` (`-march=x86-64-v2`) and tuned for newer CPUs (`-mtune=generic`) with Link-Time Optimization (`LTO`, a.k.a. `-flto=auto`) and `-O3` optimization level;

   c) optimized for `SSE4.2` (`-march=x86-64-v2`) and tuned for newer Intel CPUs (`-mtune=intel`) with Link-Time Optimization (`LTO`, a.k.a. `-flto=auto`) and `-O3` optimization level.

Author - [Digger1955](https://github.com/Digger1955/dxvk-gplall/releases)

8. Provided various MSVC (requires [MSVCRT](https://www.techpowerup.com/download/visual-c-redistributable-runtime-package-all-in-one/)) builds of DXVK-GPLALL:

   a) optimized for `SSE2` (`/arch:SSE2`) and tuned for older CPUs (`/favor:blend`) with Link-Time Optimization (`LTO`, a.k.a. `/LTCG`) and `/O1, /Oi, /Ob3` optimization level;

   b) optimized for `AVX2` (`/arch:AVX2`) and tuned for newer AMD CPUs (`/favor:AMD64`) with Link-Time Optimization (`LTO`, a.k.a. `/LTCG`) and `/O1, /Oi, /Ob3` optimization level;

   c) optimized for `SSE4.2` (`/arch:SSE4.2`) and tuned for newer Intel CPUs (`/favor:INTEL64` a.k.a. `/favor:EM64T`) with Link-Time Optimization (`LTO`, a.k.a. `/LTCG`) and `/O1, /Oi, /Ob3` optimization level.

Author - [Digger1955](https://github.com/Digger1955/dxvk-gplall/releases)

9. Backporting and adapting upstream DXVK and DXVK Low Latency improvements to DXVK 2.6.x codebase for GPUs/drivers that do not meet [DXVK 2.7 requirements](https://github.com/doitsujin/dxvk/releases/tag/v2.7), [DXVK 3.0 requirements](https://github.com/doitsujin/dxvk/releases/tag/v3.0) or have any issues with DXVK versions higher than 2.6.2. Author - [Digger1955](https://github.com/Digger1955/dxvk-gplall/releases)

## How to use (Windows 10/11)

1. Download DXVK-GPLALL package from [release](https://github.com/Digger1955/dxvk-gplall/releases) page.
2. Copy appropriate [DLL dependencies](https://github.com/Digger1955/dxvk-gplall?tab=readme-ov-file#dll-dependencies) to the location of application's main executable folder.
3. Run application.

**Important**: It is **STRONGLY RECOMMENDED** to create `dxvk.conf` at application's main executable folder (per-application configuration file - first priority) or at `%APPDATA%/dxvk.conf` (one global configuration file - second priority) with your desired DXVK settings. Please refer to [Quick Start Guide](https://github.com/Digger1955/dxvk-gplall/wiki/dxvk.conf-Options-Guide#quick-start-guide) to achieve the best results on the end-user system.

## How to use (Linux/MacOS)
In order to install a DXVK-GPLALL package obtained from the [release](https://github.com/Digger1955/dxvk-gplall/releases) page into a given wine prefix, copy or symlink the DLLs into the following directories as follows, then open `winecfg` and manually add `native` DLL overrides for `d3d8`, `d3d9`, `d3d10core`, `d3d11` and `dxgi` under the Libraries tab.

In a default Wine prefix that would be as follows:
```
export WINEPREFIX=/path/to/wineprefix
cp x64/*.dll $WINEPREFIX/drive_c/windows/system32
cp x32/*.dll $WINEPREFIX/drive_c/windows/syswow64
winecfg
```

For a pure 32-bit Wine prefix (non default) the 32-bit DLLs instead go to the `system32` directory:
```
export WINEPREFIX=/path/to/wineprefix
cp x32/*.dll $WINEPREFIX/drive_c/windows/system32
winecfg
```

Verify that your application uses DXVK instead of wined3d by enabling the HUD (see notes below).

In order to remove DXVK from a prefix, remove the DLLs and DLL overrides, and run `wineboot -u` to restore the original DLL files.

Tools such as Steam Play, Lutris, Bottles, Heroic Launcher, etc will automatically handle setup of dxvk on their own when enabled.

**Important**: It is **STRONGLY RECOMMENDED** to create `dxvk.conf` at application's main executable folder (per-application configuration file - first priority) or at `/home/$USER/.config/dxvk.conf` (one global configuration file - second priority) with your desired DXVK settings. Please refer to [Quick Start Guide](https://github.com/Digger1955/dxvk-gplall/wiki/dxvk.conf-Options-Guide#quick-start-guide) to achieve the best results on the end-user system.

## DLL dependencies 
Listed below are the DLL requirements for using DXVK with any single API.

- d3d8: `d3d8.dll` and `d3d9.dll`
- d3d9: `d3d9.dll`
- d3d10: `d3d10core.dll`, `d3d11.dll` and `dxgi.dll`
- d3d11: `d3d11.dll` and `dxgi.dll`

## Notes on Vulkan drivers
Before reporting an issue, please check the [Wiki](https://github.com/doitsujin/dxvk/wiki/Driver-support) page on the current driver status and make sure you run a recent enough driver version for your hardware.

## Online multiplayer games
Manipulation of Direct3D libraries in multiplayer games may be considered cheating and can get your account **banned**. This may also apply to singleplayer games with an embedded or dedicated multiplayer portion. 

Async could theoretically trigger client-side anti-cheats, and as such, may be risky to use inside of multiplayer games. There is no information about someone getting banned for using DXVK or DXVK with Async, but - **Use at your own risk.**

## `dxvk.conf` Config File Location

By default, DXVK has [built-in configs](https://github.com/doitsujin/dxvk/blob/master/src/util/config/config.cpp) for specific games and GPU drivers. If user needs to change default DXVK settings, then it can be done by changing default settings in `dxvk.conf` configuration file or by using `DXVK_CONFIG` environment variable.

User can create `dxvk.conf` configuration file at application's main executable folder (per-application configuration file - first priority) or at `%APPDATA%/dxvk.conf` (one global configuration file - second priority) for Windows OS or at `/home/$USER/.config/dxvk.conf` (one global configuration file - second priority) for Linux/MacOS.

User can change default `dxvk.conf` global coniguration file location by specifying path in `DXVK_CONFIG_FILE` environment variable:

- Example (Windows): `DXVK_CONFIG_FILE=%USERPROFILE%/Documents/dxvk.conf`
- Example (Linux): `DXVK_CONFIG_FILE=$XDG_DATA_HOME/dxvk.conf`
- Example (MacOS): `DXVK_CONFIG_FILE=$HOME/Library/dxvk.conf`

User can create `DXVK_CONFIG` to set config variables through the environment instead of a configuration file using the same syntax as in `dxvk.conf`. `;` is used as a seperator.

- Example: `DXVK_CONFIG="dxgi.hideAmdGpu = True; dxgi.syncInterval = 0"`

## HUD
The `DXVK_HUD` environment variable controls a HUD which can display the framerate and some stat counters. It accepts a comma-separated list of the following options:
- `devinfo`: Displays the name of the GPU, Vulkan Headers version and Vulkan Driver version.
- `fps`: Shows the current frame rate.
- `frametimes`: Shows a frame time graph.
- `submissions`: Shows the number of command buffers submitted per frame.
- `drawcalls`: Shows the number of draw calls and render passes per frame.
- `pipelines`: Shows the total number of graphics and compute pipelines.
- `descriptors`: Shows the number of descriptor pools and descriptor sets.
- `memory`: Shows the amount of device memory allocated and used.
- `allocations`: Shows detailed memory chunk suballocation info.
- `gpuload`: Shows estimated GPU load. May be inaccurate.
- `version`: Shows DXVK version.
- `api`: Shows the D3D feature level used by the application.
- `cs`: Shows worker thread statistics.
- `compiler`: Shows shader compiler activity
- `swvp`: Shows the vertex processing mode and the current number of software vertex processing shaders *[D3D9 Only]*
- `scale=x`: Scales the HUD by a factor of `x` (e.g. `1.5`)
- `opacity=y`: Adjusts the HUD opacity by a factor of `y` (e.g. `0.5`, `1.0` being fully opaque).
- `renderlatency`: Shows average render latency over 100 frames. Measurements are taken from the start of frame (usually when the game starts processing input) until the GPU did finish rendering this frame. This will not work when a game's fps limiter is enabled, as there is no way to detect when a game will stall processing before reading input. 
- `jitter`: Shows average stable jitter values over 30 seconds. Jitter values are measured for frametime, latency and app-thread latency. Lower jitter values suggest better smoothness. App-thread (render-thread) latency may be important for smoothness when the game, for example, is taking timestamps to optimize synchronization of input/simulation to its render thread.
- `latencydetails`: Provides insights about GPU buffer and v-sync buffer statistics. Helpful for fine-tuning the `dxvk.lowLatencyOffset` variable to reduce GPU buffering and for fine-tuning the VRR refresh rate to minimize v-sync buffering in the VRR mode.

Additionally, `DXVK_HUD=1` has the same effect as `DXVK_HUD=devinfo,fps`, and `DXVK_HUD=full` enables all available HUD elements.

## Logs
When used with Wine, DXVK will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `DXVK_LOG_PATH` variable, where log files in the given directory will be called `app_d3d11.log`, `app_dxgi.log` etc., where `app` is the name of the game executable.

On Windows, log files will be created in the game's working directory by default, which is usually next to the game executable.

## Frame rate limit
The `DXVK_FRAME_RATE` environment variable can be used to limit the frame rate. 

Any positive value will limit rendering to the given number of frames per second. 

A value of `-1` always disables the limiter.

`DXVK_FRAME_RATE` environment variable represented in `dxvk.conf`:
- For D3D8 and D3D9 - `d3d9.maxFrameRate`. Default value is `d3d9.maxFrameRate = -1`
- For D3D10 and D3D11 - `dxgi.maxFrameRate` . Default value is `dxgi.maxFrameRate = -1`

## Device filter
Some applications do not provide a method to select a different GPU. In that case, DXVK can be forced to use a given device:
- `DXVK_FILTER_DEVICE_NAME="Device Name"` Selects devices with a matching Vulkan device name, which can be retrieved with tools such as `vulkaninfo`. Matches on substrings, so "VEGA" or "AMD RADV VEGA10" is supported if the full device name is "AMD RADV VEGA10 (LLVM 9.0.0)", for example. If the substring matches more than one device, the first device matched will be used.
- `DXVK_FILTER_DEVICE_UUID="00000000000000000000000000000001"` Selects a device by matching its Vulkan device UUID, which can also be retrieved using tools such as vulkaninfo. The UUID must be a 32-character hexadecimal string with no dashes. This method provides more precise selection, especially when using multiple identical GPUs.

**Note:** If the device filter is configured incorrectly, it may filter out all devices and applications will be unable to create a D3D device.

## Debugging
The following environment variables can be used for **debugging** purposes.
- `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` Enables Vulkan debug layers. Highly recommended for troubleshooting rendering issues and driver crashes. Requires the Vulkan SDK to be installed on the host system.
- `DXVK_LOG_LEVEL=none|error|warn|info|debug` Controls message logging.
- `DXVK_LOG_PATH=/some/directory` Changes path where log files are stored. Set to `none` to disable log file creation entirely, without disabling logging.
- `DXVK_DEBUG=markers|validation` Enables use of the `VK_EXT_debug_utils` extension for translating performance event markers, or to enable Vulkan validation, respecticely.

## Graphics Pipeline Library (GPL)
On drivers which support `VK_EXT_graphics_pipeline_library` Vulkan shaders will be compiled at the time the game loads its D3D shaders, rather than at draw time. This reduces or eliminates shader compile stutter in many games when compared to the previous system.

In games that load their shaders during loading screens or in the menu, this can lead to prolonged periods of very high CPU utilization, especially on weaker CPUs. For affected games it is recommended to wait for shader compilation to finish before starting the game to avoid stutter and low performance. Shader compiler activity can be monitored with `DXVK_HUD=compiler`.

**Note:** Games which only load their D3D shaders at draw time (e.g. most Unreal Engine games) will still exhibit some stutter, although it should still be less severe than without this feature.

**IMPORTANT**: Disabled by default since DXVK-GPLALL 2.6.1-4. Reasons have been specified in [Wiki](https://github.com/Digger1955/dxvk-gplall/wiki/dxvk.conf-Options-Guide#dxvkenablegraphicspipelinelibrary).

## State cache
DXVK-GPLALL caches pipeline state by default, so that shaders can be recompiled ahead of time on subsequent runs of an application, even if the driver's own shader cache got invalidated in the meantime. This cache is enabled by default, and generally reduces stuttering.

State cache can be used together with GPL that is not possible on upstream DXVK, but it can be useful depending on game.

The following environment variables can be used to control the cache:
- `DXVK_STATE_CACHE`: Controls the state cache. The following values are supported:
  - `disable`: Disables the cache entirely.
  - `reset`: Clears the cache file.
- `DXVK_STATE_CACHE_PATH=/some/directory` Specifies a directory where to put the cache files. Defaults to the current working directory of the application.

**Important**: The state cache has been removed from the [upstream DXVK since version 2.7](https://github.com/doitsujin/dxvk/releases/tag/v2.7). DXVK-GPLALL 2.6.x branch is not affected. State cache is available in DXVK-GPLALL 2.7-3 and later. 

## Asynchronous pipeline compilation (Async)

Originally started as hacky solution for shader compilation stutter in dxvk. Similar solution was later added to dxvk itself and promptly removed.

Enabling this solution results in a lot less shader compilation stuttering by not blocking the main thread when compiling async pipelines and (not necessarily) miscellaneous graphical issues while shaders are compiling for the first time.

Asynchronous pipeline compilation is enabled with `DXVK_ASYNC=1` environment variable and is equivalent to `dxvk.enableAsync = True` in `dxvk.conf`. It is enabled by default.

Asynchronous pipeline compilation is disabled with `DXVK_ASYNC=0` environment variable and is equivalent to `dxvk.enableAsync = False` in `dxvk.conf`.

## GPLAsync and State cache

State cache fixes for GPL and Async are enabled with `DXVK_GPLASYNCCACHE=1` environment variable and is equivalent to `dxvk.gplAsyncCache = True` in `dxvk.conf`. It is enabled by default.

State cache fixes for GPL and Async are disabled with `DXVK_GPLASYNCCACHE=0` environment variable and is equivalent to `dxvk.gplAsyncCache = False` in `dxvk.conf`.

**Important**: The state cache has been removed from the [upstream DXVK since version 2.7](https://github.com/doitsujin/dxvk/releases/tag/v2.7). DXVK-GPLALL 2.6.x branch is not affected. State cache is available in DXVK-GPLALL 2.7-3 and later. 

## Low Latency frame pacing

Enhances the original [dxvk](https://github.com/doitsujin/dxvk) with low-latency frame pacing capabilities to improve game responsiveness and input lag. It also improves latency stability over time, usually resulting in a more accurate playback speed of the generated video.

Low-Latency frame pacing mode aims to reduce latency with minimal impact in fps. Effective when operating in the GPU-limit. Efficient to be used in the CPU-limit as well.

Greatly reduces input lag variations when switching between CPU- and GPU-limit, and compared to the max-frame-latency approach, it has a much more stable input lag when GPU running times change dramatically, which can happen for example when rotating within a scene.

Latency has been decreased dramatically in some games by speeding up the dxvk-internal flush heuristic delivering GPU submissions quicker, which was presumably tuned for bandwidth/fps.

An interesting observation while playtesting was that not only the input lag was affected, but the video generated did progress more cleanly in time as well with regards to the wow and flutter effect.

Optimized for Variable Refresh Rate (VRR) displays, `VK_PRESENT_MODE_IMMEDIATE_KHR` (V-Sync Off) and `VK_PRESENT_MODE_FIFO_KHR` (V-Sync On). It also comes with its own fps-limiter which is typically used to prevent the game's fps exceeding the monitor's refresh rate.

### Usage

`DXVK_FRAME_PACE` environment variable has the next options: `max-frame-latency`, `min-latency`, `low-latency` and `low-latency-vrr-x`. Default is `DXVK_FRAME_PACE=low-latency`.

`low-latency-vrr-x` is a special option, which requires to specify display refresh rate, instead of `x`. For example, if user has 360 Hz display, then option must be `"low-latency-vrr-360"`. This mode does not strictly require VRR display, but it is designed for VRR displays and works better than `"low-latency"` on such displays. It is the recommended low-latency mode for any display.

`DXVK_FRAME_PACE` environment variable represented in the `dxvk.conf` as `dxvk.framePace`. Default is `dxvk.framePace = "max-frame-latency"`.

`dxvk.lowLatencyOffset` option in `dxvk.conf` allows for fine-tuning `"low-latency"` and `"low-latency-vrr-x"` frame pacing modes. Values are in microseconds. Positive values might improve responsiveness even further, although only very slightly, this may be relevant for edge cases. Negative values might improve fps. Default is `dxvk.lowLatencyOffset = 0`

`dxvk.lowLatencyAllowCpuFramesOverlap` option in `dxvk.conf` controls whether a frame is allowed to begin before finishing processing the cpu-part of the previous one, when low-latency frame pacing is used. Snappiness may be improved when disallowing overlap. On the other hand, this might also decrease fps in certain cases. Default is `dxvk.lowLatencyAllowCpuFramesOverlap = True`

## Build instructions

In order to pull in all submodules that are needed for building, clone the repository using the following command:
```
git clone --recursive https://github.com/Digger1955/dxvk-gplall
```

### Requirements:
- [wine 7.1](https://www.winehq.org/) or newer
- [Meson](https://mesonbuild.com/) build system (at least version 0.58)
- [Mingw-w64](https://www.mingw-w64.org) compiler and headers (at least version 10.0)
- [glslang](https://github.com/KhronosGroup/glslang) compiler

### Building DLLs

#### The simple way
Inside the DXVK directory, run:
```
./package-release.sh master /your/target/directory --no-package
```

This will create a folder `dxvk-master` in `/your/target/directory`, which contains both 32-bit and 64-bit versions of DXVK, which can be set up in the same way as the release versions as noted above.

In order to preserve the build directories for development, pass `--dev-build` to the script. This option implies `--no-package`. After making changes to the source code, you can then do the following to rebuild DXVK:
```
# change to build.32 for 32-bit
cd /your/target/directory/build.64
ninja install
```

#### Compiling manually
```
# 64-bit build. For 32-bit builds, replace
# build-win64.txt with build-win32.txt
meson setup --cross-file build-win64.txt --buildtype release --prefix /your/dxvk/directory build.w64
cd build.w64
ninja install
```

The D3D8, D3D9, D3D10, D3D11 and DXGI DLLs will be located in `/your/dxvk/directory/bin`.

### Build troubleshooting
DXVK requires threading support from your mingw-w64 build environment. If you
are missing this, you may see "error: ‘std::cv_status’ has not been declared"
or similar threading related errors.

On Debian and Ubuntu, this can be resolved by using the posix alternate, which
supports threading. For example, choose the posix alternate from these
commands:
```
update-alternatives --config x86_64-w64-mingw32-gcc
update-alternatives --config x86_64-w64-mingw32-g++
update-alternatives --config i686-w64-mingw32-gcc
update-alternatives --config i686-w64-mingw32-g++
```
For non debian based distros, make sure that your mingw-w64-gcc cross compiler 
does have `--enable-threads=posix` enabled during configure. If your distro does
ship its mingw-w64-gcc binary with `--enable-threads=win32` you might have to
recompile locally or open a bug at your distro's bugtracker to ask for it. 

## DXVK Native

DXVK Native is a version of DXVK which allows it to be used natively without Wine.

This is primarily useful for game and application ports to either avoid having to write another rendering backend, or to help with port bringup during development.

[Release builds](https://github.com/Digger1955/dxvk-gplall/releases) are built using the Steam Runtime. 

### How does it work?

DXVK Native replaces certain Windows-isms with a platform and framework-agnostic replacement, for example, `HWND`s can become `SDL_Window*`s, etc.
All it takes to do that is to add another WSI backend.

**Note:** DXVK Native requires a backend to be explicitly set via the `DXVK_WSI_DRIVER` environment variable. The current built-in options are `SDL3`, `SDL2`, and `GLFW`.

DXVK Native comes with a slim set of Windows header definitions required for D3D9/11 and the MinGW headers for D3D9/11.
In most cases, it will end up being plug and play with your renderer, but there may be certain teething issues such as:
- `__uuidof(type)` is supported, but `__uuidof(variable)` is not supported. Use `__uuidof_var(variable)` instead.
