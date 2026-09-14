# GPU Overlay

A Windows system-tray application that injects a configurable GPU overlay into DirectX 11 games. It supports AMD, NVIDIA, and Intel telemetry, with a DXGI fallback for other GPUs.

## Metrics

- GPU usage
- VRAM usage
- FPS
- GPU clock
- GPU temperature

Each metric can be enabled or disabled independently.

## Requirements

- Windows 10 or 11
- A DirectX 11 game and compatible GPU
- A build matching the game's architecture (`x64` or `Win32`)
- Current GPU drivers for vendor telemetry

Intel vendor telemetry is available in x64 builds. Win32 Intel builds fall back to FPS and DXGI VRAM usage.

## Build

Install Visual Studio 2019 or 2022 with Desktop development with C++ and CMake. From a Developer Command Prompt:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Use `-A Win32` for a 32-bit game. The build downloads pinned versions of MinHook, the official NVIDIA NVAPI SDK, and the official Intel Graphics Control Library headers.

The release binaries are written to `build\Release\`. `GPUOverlayHook.dll` is copied next to `GPUOverlay.exe` automatically. GitHub Actions also publishes architecture-specific ZIP packages containing both binaries, this README, and the default configuration.

## Usage

1. Start `GPUOverlay.exe`.
2. Right-click its system-tray icon and choose **Settings...** to configure the overlay.
3. Choose **Inject into process...**, select the running game, and click **Inject**.
4. Run the tray app as Administrator if injection into the target process is denied.

Do not inject the overlay into anti-cheat-protected games unless the game's policies explicitly allow it.

## Configuration

Settings are saved in `GPUOverlay.ini` next to the executable and hook DLL. An injected overlay reloads the file once per second, so most changes apply without reinjection.

The settings dialog controls:

- Position: any screen corner
- Scale: `0.5` through `3.0`
- Text and background colors in `#RRGGBB` format
- Visible metrics
- Toggle and cycle-position hotkeys

The default hotkeys are `F11` to show or hide the overlay and `F10` to cycle its position. Hotkeys may include `Ctrl`, `Alt`, or `Shift` plus A-Z, 0-9, F1-F24, or a navigation key; for example, `Ctrl+Shift+F11`.

## Diagnostics

The tray app and injected hook write rotating logs to `%LOCALAPPDATA%\GPUOverlay\Logs`. Right-click the tray icon and choose **Open logs** to open that folder.

The logs record injection results, DirectX hook startup, adapter and provider selection, fallback decisions, renderer initialization, configuration reloads, and unavailable telemetry. Each log rotates at 1 MB and keeps three previous generations.

## Telemetry providers

The hook selects a telemetry provider from the DXGI adapter used by the game's swap chain:

| GPU | Provider | Usage | Clock | Temperature | VRAM |
| --- | --- | --- | --- | --- | --- |
| AMD | ADL | Yes | Yes | Yes | DXGI |
| NVIDIA | NVAPI | Yes | Yes | Yes | DXGI |
| Intel x64 | IGCL | Yes | Yes | Yes | DXGI |
| Other / unavailable provider | DXGI fallback | N/A | N/A | N/A | Yes |

NVIDIA and Intel adapter handles are matched to the game's DXGI adapter by LUID. Vendor APIs are supplied by the installed display driver; no separate runtime SDK installation is required.

## How it works

- The tray app lists running processes and injects `GPUOverlayHook.dll`.
- The hook intercepts `IDXGISwapChain::Present` for DirectX 11.
- A provider interface collects vendor metrics while DXGI reports local video-memory use.
- The renderer preserves and restores the game's Direct3D 11 state around each overlay draw.

DirectX 12 and Vulkan are not supported.
