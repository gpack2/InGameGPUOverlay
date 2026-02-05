# GPU Overlay

A **Windows system-tray application** that injects an in-game overlay into DirectX 11 games (including Steam games) and displays real-time **AMD Radeon** GPU metrics.

## Displayed metrics

- **GPU usage** (%)
- **VRAM usage** (GB)
- **FPS**
- **GPU clock speed** (MHz)
- **GPU temperature** (°C)

## Requirements

- **Windows 10/11** (64-bit recommended for Steam games)
- **AMD Radeon** consumer GPU with up-to-date drivers (Radeon Software Adrenalin)
- **DirectX 11** game (e.g. many Steam titles using DX11)

## Build (Windows)

1. Install **Visual Studio 2019 or 2022** with "Desktop development with C++" and **CMake**.
2. Open a **Developer Command Prompt** (or ensure `cl` and `cmake` are in PATH).
3. From the project root:

```bat
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

4. Run from `build\Release\`:
   - **GPUOverlay.exe** – tray app
   - **GPUOverlayHook.dll** – copied next to the exe by the build

## Usage

1. Start **GPUOverlay.exe**. A GPU Overlay icon appears in the system tray.
2. **Right-click** the tray icon → **Inject into process...**
3. In the list, select your **game process** (e.g. the game’s `.exe`) and click **Inject**.
4. If needed, run GPUOverlay **as Administrator** if injection fails.
5. The overlay appears in the **top-left** of the game when it uses **DirectX 11** (typical for many Steam DX11 games).

## How it works

- **Tray app** – Lists running processes and injects **GPUOverlayHook.dll** into the chosen process.
- **Hook DLL** – Loads inside the game process, hooks **IDXGISwapChain::Present** (DX11), and each frame:
  - Reads GPU metrics via **AMD ADL** (atiadlxx.dll, shipped with Radeon drivers) and **DXGI** for VRAM.
  - Counts **Present** calls for FPS.
  - Renders a small text overlay on the back buffer before present.

No AMD SDK install is required; ADL is used via the driver’s **atiadlxx.dll**.

## Compatibility

- **AMD Radeon** only (consumer GPUs). ADL is used for usage %, clock, and temperature; VRAM comes from DXGI.
- **DX11** games only. DX12/Vulkan are not supported.
- **Steam** games that use DX11 are supported; inject into the game process after the game is running.

## License

Project code is provided as-is. AMD ADL is used at runtime from the driver; see [AMD GPUOpen](https://gpuopen.com/adl/) for ADL terms.
