# Spatial Video Player

Linux-first spatial 3D video player prototype that combines OpenGL rendering with a phased media pipeline roadmap.

Current implementation status: Phase 1 foundation is in place (window, render loop, shader pipeline, geometry, and camera controls).

## Project Goals

- Render video content on 3D geometry (flat virtual screen and immersive sphere).
- Maintain smooth interactive rendering while scaling toward full A/V playback.
- Preserve audio-master-clock synchronization once media playback is integrated.

## Current Status

Implemented now:
- CMake-based C++17 project scaffold.
- OpenGL 3.3 Core + GLFW window/context initialization.
- Main loop structure: Initialization -> Update -> Render -> Shutdown.
- Shader load/compile/link pipeline from file assets.
- Mesh generation for textured quad and inverted sphere.
- Quaternion-based camera with mouse look and movement controls.
- MVP matrix upload path and indexed draw call.

Planned next major phases:
- FFmpeg demux/decode threading.
- YUV upload and GPU color conversion.
- Audio pipeline and A/V synchronization.
- Linux hardware acceleration path (VA-API + DMA-BUF interop).

## Repository Layout

- [CMakeLists.txt](CMakeLists.txt): Build configuration and dependency wiring.
- [src/main.cpp](src/main.cpp): App lifecycle, OpenGL setup, input routing, frame loop.
- [src/Camera.cpp](src/Camera.cpp): Camera movement/orientation and matrix generation.
- [src/Geometry.cpp](src/Geometry.cpp): Quad and inverted sphere mesh generation.
- [src/ShaderProgram.cpp](src/ShaderProgram.cpp): Shader source load, compile/link, uniform upload.
- [include/spatial/Camera.hpp](include/spatial/Camera.hpp): Camera API.
- [include/spatial/Geometry.hpp](include/spatial/Geometry.hpp): Mesh data + geometry factories.
- [include/spatial/ShaderProgram.hpp](include/spatial/ShaderProgram.hpp): Shader program API.
- [shaders/basic.vert](shaders/basic.vert): Vertex shader.
- [shaders/basic.frag](shaders/basic.frag): Fragment shader.
- [Architecture.md](Architecture.md): System architecture and technical specification.
- [Plan.md](Plan.md): Phased implementation plan and verification checklist.
- [Implementation.md](Implementation.md): Implemented architecture details and control-flow documentation.

## Dependencies

Build-time/runtime dependencies expected on Linux:
- CMake (3.16+)
- C++17 compiler (GCC or Clang)
- OpenGL 3.3 Core
- GLFW 3.3+
- GLM
- FFmpeg development libraries:
	- libavcodec
	- libavformat
	- libavutil
	- libswscale
	- libswresample

### Install Dependencies (Linux)

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y \
	build-essential cmake pkg-config \
	libgl1-mesa-dev libglfw3-dev libglm-dev \
	libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

Fedora:

```bash
sudo dnf install -y \
	gcc-c++ cmake pkgconf-pkg-config \
	mesa-libGL-devel glfw-devel glm-devel \
	ffmpeg-devel
```

Arch Linux:

```bash
sudo pacman -S --needed \
	base-devel cmake pkgconf \
	mesa glfw glm ffmpeg
```

Notes:
- On some Fedora setups, FFmpeg development packages may come from RPM Fusion.
- Wayland/X11 runtime support is handled through GLFW and system graphics drivers.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Binary output:

```bash
./build/spatial_player
```

Release build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/spatial_player
```

## Runtime Controls (Current)

- Mouse: look around (yaw/pitch).
- W/A/S/D: move forward/left/back/right.
- E/Q: move up/down.
- ESC: exit application.

## Verification Notes

Phase 1 verification workflow is tracked in [Plan.md](Plan.md), including:
- Build pass checks.
- Runtime smoke checks.
- Camera/matrix path checks.
- FPS target verification procedure.

## Documentation

- Architecture specification: [Architecture.md](Architecture.md)
- Delivery plan and checkpoints: [Plan.md](Plan.md)
- Implemented code architecture and sequence diagrams: [Implementation.md](Implementation.md)

## License

MIT License. See [LICENSE](LICENSE) for details.
