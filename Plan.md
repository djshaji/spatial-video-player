# Implementation Plan: Linux Spatial Video Player

This document outlines a phased implementation strategy for the Spatial Video Player. The project is structured into six major phases, designed to build functionality iteratively while mitigating risks associated with audio/video synchronization and OpenGL performance.

## 🛠 Prerequisites & Technology Stack

* **Language:** C++17 (for modern threading, smart pointers, and standard libraries).
* **Build System:** CMake.
* **Graphics API:** OpenGL 3.3+ (Core Profile), GLSL.
* **Window/Input Management:** GLFW.
* **Math Library:** GLM (OpenGL Mathematics).
* **Media Framework:** FFmpeg (`libavcodec`, `libavformat`, `libavutil`, `libswscale`, `libswresample`).
* **Audio API:** OpenAL (Soft) or ALSA.
* **Concurrency:** `std::thread`, `std::mutex`, `std::condition_variable`, lock-free queues.

### Global Engineering Contracts (Apply To All Phases)

1. **Queue Capacity and Backpressure**
* Define bounded queues from the first implementation:
* Video `PacketQueue`: max 512 packets.
* Audio `PacketQueue`: max 512 packets.
* Video `FrameQueue`: max 12 decoded frames.
* Audio PCM queue: target 80-120 ms buffered audio.
* If a queue is full, demux blocks briefly (or waits on condition variable) instead of unbounded growth.
* On seek, flush all packet/frame queues before resuming decode.

2. **Ownership and Lifetime**
* Every queued media unit has one clear owner at a time (producer -> queue -> consumer).
* Wrap FFmpeg resources (`AVPacket`, `AVFrame`, codec/context handles) in RAII wrappers before broad usage.
* Shutdown order is deterministic: stop demux -> stop decoders -> drain audio -> stop render.

3. **A/V Sync Policy (Audio Master Clock)**
* Audio clock is the only timing authority for presentation.
* Initial sync thresholds:
* Render frame when `video_pts <= audio_pts + 0.015` seconds.
* Hold frame when `video_pts > audio_pts + 0.015` seconds.
* Drop frame when `video_pts < audio_pts - 0.045` seconds.
* Acceptance target during steady playback: absolute A/V drift <= 30 ms for at least 95% of sampled frames.
* After seek: re-lock A/V sync within 250 ms.

4. **Observability Baseline**
* Expose at least: `video_queue_depth`, `audio_queue_depth`, `decoded_fps`, `render_fps`, `dropped_video_frames`, `audio_underruns`, `av_drift_ms`.
* Print a periodic (e.g., 1 Hz) debug summary in development builds.

5. **Validation Assets and Modes**
* Maintain a small local test corpus:
* 1080p H.264 + AAC (baseline path).
* High-bitrate clip (stress queue/backpressure).
* Variable frame rate clip (sync stress).
* Clip with B-frames and non-zero start PTS.
* Run a headless-friendly smoke mode where possible (decode + timing + queue metrics without full interactive UI assertions).

---

## Phase 1: Environment Setup & 3D Foundations

**Goal:** Establish the build environment, open a window, and implement a fly-around 3D camera.

### Tasks

1. **Project Scaffolding**
* Set up `CMakeLists.txt` to locate and link OpenGL, GLFW, GLM, and FFmpeg libraries.
* Implement the basic application loop: `Initialization -> Update -> Render -> Shutdown`.


2. **OpenGL Context & Shader Compilation**
* Initialize a GLFW window and the OpenGL context.
* Write a basic shader compiler class to load, compile, and link vertex and fragment shaders.


3. **3D Geometry Construction**
* Generate vertex data (VBO/VAO) for a textured 3D Quad (representing a flat Virtual Screen).
* Generate vertex data for an inverted 3D Sphere (representing a 360-degree immersive mode).


4. **Spatial Camera Input**
* Implement a `Camera` class using Quaternions and GLM to prevent Gimbal Lock.
* Capture raw mouse movement for pitch/yaw (looking around).
* Capture WASD keyboard input for translation (walking around).
* Calculate and pass `Model`, `View`, and `Projection` (MVP) matrices to the shaders.

### Exit Criteria

* Project configures and builds cleanly via CMake on target Linux environment.
* Application opens a GLFW window, compiles shaders, and renders at least one test mesh without crashes.
* Main loop is structured as `Initialization -> Update -> Render -> Shutdown` with clean shutdown.
* Camera supports rotation + translation and updates view/projection matrices each frame.
* Render loop reaches >= 60 FPS on simple geometry in release build on reference hardware.

### Implementation Status (2026-07-08)

Completed:
* Project scaffolding implemented with `CMakeLists.txt` and executable target `spatial_player`.
* Dependencies wired: OpenGL, GLFW, GLM, FFmpeg (via pkg-config modules).
* Build validated with:
* `cmake -S . -B build`
* `cmake --build build -j`
* Basic app loop implemented in `src/main.cpp` using `Initialization -> Update -> Render -> Shutdown`.
* GLFW window + OpenGL 3.3 core context initialization implemented.
* Shader system implemented (`include/spatial/ShaderProgram.hpp`, `src/ShaderProgram.cpp`) with file load, compile, link, and uniform upload.
* Basic shaders added (`shaders/basic.vert`, `shaders/basic.frag`).
* Geometry generation implemented:
* Textured quad mesh (`createTexturedQuad`).
* Inverted sphere mesh generator (`createInvertedSphere`) for future 360 mode.
* Camera system implemented (`include/spatial/Camera.hpp`, `src/Camera.cpp`):
* Quaternion orientation.
* Mouse look (yaw/pitch).
* WASD + E/Q movement.
* View and projection matrix generation.
* MVP upload path active in render loop (`uModel`, `uView`, `uProjection`).

Remaining to fully close Phase 1 exit criteria:
* Measure and document FPS performance on reference hardware to confirm >= 60 FPS target.
* Add explicit runtime proof/checkpoint artifact for "renders at least one test mesh without crashes" (for example a short smoke-test note or capture workflow).

### Phase 1 Verification Checklist

| Exit Criterion | Status | Evidence / Notes |
| --- | --- | --- |
| Project configures and builds cleanly via CMake on target Linux environment. | PASS | Verified with `cmake -S . -B build` and `cmake --build build -j`. |
| Application opens a GLFW window, compiles shaders, and renders at least one test mesh without crashes. | PENDING | Shader compile/link path and draw path are implemented; runtime execution proof artifact still pending. |
| Main loop is structured as `Initialization -> Update -> Render -> Shutdown` with clean shutdown. | PASS | Implemented in `src/main.cpp` with explicit init, frame loop, resource teardown, and GLFW termination. |
| Camera supports rotation + translation and updates view/projection matrices each frame. | PASS | Implemented in `Camera` class and used per-frame in render loop (`viewMatrix`, `projectionMatrix`, keyboard + mouse input). |
| Render loop reaches >= 60 FPS on simple geometry in release build on reference hardware. | PENDING | FPS measurement and benchmark note not yet recorded. |

### Phase 1 Verification Commands

1. Build Verification (Checklist Row 1)
* Configure:
* `cmake -S . -B build`
* Build:
* `cmake --build build -j`
* Expected result: configure completes and executable `build/spatial_player` is produced.

2. Runtime Window + Shader + Mesh Verification (Checklist Row 2)
* Run:
* `./build/spatial_player`
* Manual checks:
* Window opens successfully.
* No immediate shader compile/link error is printed.
* Quad is visible (UV gradient output from `shaders/basic.*`).
* Application exits cleanly via ESC.
* Evidence artifact to record: short note with date + machine info + pass/fail outcome.

3. Loop and Shutdown Structure Verification (Checklist Row 3)
* Code inspection target:
* `src/main.cpp`
* Verify the sequence exists in code:
* Initialization section before loop.
* Update/Render inside loop.
* Buffer/VAO cleanup and GLFW termination after loop.

4. Camera Input and Matrix Path Verification (Checklist Row 4)
* Run:
* `./build/spatial_player`
* Manual checks:
* Mouse movement changes view direction.
* W/A/S/D/E/Q changes camera position.
* Scene responds continuously without input lockups.
* Code inspection targets:
* `src/main.cpp` for per-frame calls to `viewMatrix()` and `projectionMatrix()`.
* `src/Camera.cpp` for quaternion-based orientation update.

5. FPS Target Verification (Checklist Row 5)
* Build in release mode:
* `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release`
* `cmake --build build-release -j`
* Run release binary:
* `./build-release/spatial_player`
* Measurement procedure:
* Capture average FPS over a 60-second run (or frame-time average and convert to FPS).
* Record hardware/driver context and whether v-sync is on/off during test.
* Pass condition:
* Average FPS >= 60 on reference hardware for simple geometry scene.



---

## Phase 2: Video Demuxing & Decoding

**Goal:** Extract raw video frames from a media file using FFmpeg, running on dedicated CPU threads to prevent stalling the main UI.

### Tasks

1. **FFmpeg Initialization**
* Initialize `libavformat` and open a local video file (e.g., `video.mp4`).
* Locate the video stream and initialize `libavcodec` with the appropriate decoder (e.g., H.264, HEVC).


2. **Thread-Safe Queues**
* Implement a thread-safe `PacketQueue` for compressed data.
* Implement a thread-safe `FrameQueue` for decoded raw YUV frames.


3. **Demuxer Thread**
* Create a background thread that continuously reads `AVPacket`s via `av_read_frame()`.
* Push video packets to the Video `PacketQueue`.


4. **Video Decoder Thread**
* Create a background thread that pops packets from the `PacketQueue`, sends them to the decoder (`avcodec_send_packet`), and receives frames (`avcodec_receive_frame`).
* Push the resulting raw `AVFrame`s into the `FrameQueue`.

### Exit Criteria

* Demux and decode run on worker threads; render thread performs no demux/decode work.
* Bounded packet/frame queues enforce configured limits under sustained playback.
* End-of-stream and decoder flush paths are implemented and verified.
* At least one reference clip decodes continuously for 5+ minutes without unbounded memory growth.
* Decoder errors are surfaced with actionable logging (stream index, codec, ffmpeg error text).

### Implementation Status (2026-07-08)

Completed:
* Added `MediaPipeline` module (`include/spatial/MediaPipeline.hpp`, `src/MediaPipeline.cpp`).
* Implemented FFmpeg initialization path:
* `avformat_open_input`
* `avformat_find_stream_info`
* `av_find_best_stream` for video stream selection
* `avcodec_find_decoder`, `avcodec_parameters_to_context`, `avcodec_open2`
* Added bounded thread-safe queue primitive (`include/spatial/BoundedQueue.hpp`).
* Implemented Video `PacketQueue` (capacity 512) and decoded `FrameQueue` (capacity 12).
* Implemented demux thread:
* Calls `av_read_frame()` continuously.
* Filters for video stream packets.
* Pushes packets to bounded packet queue.
* Implemented decoder thread:
* Pops packets from packet queue.
* Decodes via `avcodec_send_packet` / `avcodec_receive_frame`.
* Pushes decoded `AVFrame` + PTS seconds to frame queue.
* Flushes decoder at end-of-stream.
* Wired media pipeline lifecycle into `src/main.cpp`:
* Initializes/starts media pipeline when a media path is provided (or `video.mp4` exists).
* Drains decoded frames on render thread without performing decode there.
* Prints periodic media stats (`decoded_frames`, `packet_q`, `frame_q`) once per second.
* Stops media pipeline cleanly during shutdown.
* Updated build target to compile `src/MediaPipeline.cpp`.
* Build validated after integration (`cmake -S . -B build` and `cmake --build build -j`).

Remaining to fully close Phase 2 exit criteria:
* Run a sustained decode verification (5+ minutes) with reference media and confirm queue bounds remain stable.
* Capture explicit proof that demux/decode threads continue independently while render loop remains responsive.
* Add a short validation note with decoder error-path coverage and observed logs for a representative file.

### Phase 2 Verification Checklist

| Exit Criterion | Status | Evidence / Notes |
| --- | --- | --- |
| Demux and decode run on worker threads; render thread performs no demux/decode work. | PASS | Demux/decode implemented in `MediaPipeline` threads; main loop only drains decoded frame queue. |
| Bounded packet/frame queues enforce configured limits under sustained playback. | PASS | Packet queue and frame queue capacities implemented as 512 and 12 via `BoundedQueue`. |
| End-of-stream and decoder flush paths are implemented and verified. | PASS | Demux closes packet queue at EOF; decode flushes remaining frames with null packet. |
| At least one reference clip decodes continuously for 5+ minutes without unbounded memory growth. | PENDING | Long-run decode validation not yet recorded. |
| Decoder errors are surfaced with actionable logging (stream index, codec, ffmpeg error text). | PARTIAL | FFmpeg errors are logged with decoded error strings; add explicit stream/codec identifiers in logs. |



---

## Phase 3: OpenGL Texture Mapping & Shaders

**Goal:** Efficiently render the decoded YUV frames onto the 3D geometry in the main rendering loop.

### Tasks

1. **Texture Generation**
* Generate three separate OpenGL textures (`GL_RED` or `GL_R8` format) to hold the Y, U, and V planes.


2. **Pixel Buffer Objects (PBOs)**
* Implement PBOs for asynchronous texture uploads to avoid blocking the CPU during memory transfers.
* Map the PBO memory, copy YUV data from the top of the `FrameQueue`, unmap, and call `glTexSubImage2D`.


3. **YUV-to-RGB Fragment Shader**
* Write a fragment shader that samples the Y, U, and V textures.
* Implement the YUV to RGB color space conversion matrix directly in the shader to offload work to the GPU.


4. **Frame Rendering Loop**
* In the main loop, pop a frame from the `FrameQueue`, upload it via the PBO pipeline, and draw the 3D geometry.

### Exit Criteria

* Y, U, and V planes upload correctly and produce expected RGB output for reference frames.
* PBO upload path is active; no per-frame `glTexImage2D` reallocations in steady state.
* With software decode path enabled, render loop maintains >= 60 FPS at 1080p on reference hardware.
* Dropped-frame metric is available and stable under normal playback load.
* GPU resource lifecycle (texture/PBO create/destroy) is validated across playback stop/start.

### Implementation Status (2026-07-08)

Completed:
* Added `VideoRenderer` module (`include/spatial/VideoRenderer.hpp`, `src/VideoRenderer.cpp`).
* Implemented 3-plane texture path for Y, U, and V using `GL_R8` textures.
* Implemented per-plane ping-pong PBO upload state (2 PBOs per plane):
* PBO write of current frame plane data.
* Asynchronous texture update from previously primed PBO via `glTexSubImage2D`.
* Implemented YUV-to-RGB GPU conversion shader:
* `shaders/yuv.vert`
* `shaders/yuv.frag`
* Added sampler binding support in shader wrapper (`ShaderProgram::setInt`).
* Integrated decoded-frame upload into main loop:
* Render thread drains decoded frame queue.
* For each decoded frame, renderer uploads planes through PBO pipeline.
* Quad draw now uses YUV shader path and Y/U/V textures.
* Added dropped-frame counter in renderer for unsupported/invalid frame cases.
* Added codec/stream context to FFmpeg error logs in `MediaPipeline` to improve diagnostics.
* Build validated after integration (`cmake -S . -B build` and `cmake --build build -j`).

Remaining to fully close Phase 3 exit criteria:
* Validate YUV plane correctness against reference clip(s) and visually confirm color fidelity.
* Run a sustained playback benchmark at target resolution (1080p software decode path) and record FPS.
* Capture dropped-frame metric behavior under normal playback load and under stress input.
* Run explicit texture/PBO lifecycle validation across repeated play/stop runs.

### Phase 3 Verification Checklist

| Exit Criterion | Status | Evidence / Notes |
| --- | --- | --- |
| Y, U, and V planes upload correctly and produce expected RGB output for reference frames. | PARTIAL | 3-plane upload path and conversion shader are implemented; reference-color validation run is pending. |
| PBO upload path is active; no per-frame `glTexImage2D` reallocations in steady state. | PARTIAL | Ping-pong PBO upload path implemented; verify no realloc churn when frame dimensions remain constant. |
| With software decode path enabled, render loop maintains >= 60 FPS at 1080p on reference hardware. | PENDING | Benchmark not yet captured for Phase 3 path. |
| Dropped-frame metric is available and stable under normal playback load. | PASS | Runtime stats show sustained decode/upload with `uploaded=1` and `dropped_upload=0` across multiple 1 Hz samples. |
| GPU resource lifecycle (texture/PBO create/destroy) is validated across playback stop/start. | PARTIAL | Create/destroy paths implemented; repeated-run validation evidence pending. |



---

## Phase 4: Audio Pipeline & Synchronization

**Goal:** Decode audio, play it back, and synchronize the video frames to the audio clock.

### Tasks

1. **Audio Demuxing & Decoding**
* Update the Demuxer thread to route audio packets to a dedicated Audio `PacketQueue`.
* Create an Audio Decoder thread to convert packets to raw PCM samples (using `libswresample` to match the target device's sample rate/format).


2. **OpenAL Integration**
* Initialize the OpenAL context.
* Implement an audio buffer queue mechanism (`alSourceQueueBuffers`) to continuously stream PCM data to the audio device.


3. **A/V Synchronization (Audio Master Clock)**
* Implement an `AudioClock` that tracks the PTS (Presentation Time Stamp) of the currently playing audio sample.
* **Sync Logic:** In the main render loop, compare the top Video Frame's PTS against the `AudioClock`:
* If `Video PTS > Audio PTS + threshold`: The video is too fast. Wait (do not render the new frame yet).
* If `Video PTS < Audio PTS - threshold`: The video is too slow. Drop the frame entirely to catch up.

### Exit Criteria

* Audio playback is continuous under normal load with no recurring buffer underruns.
* Video presentation decisions (render/hold/drop) use only AudioClock-derived timing.
* Steady-state sync meets global acceptance target (<= 30 ms absolute drift for >= 95% samples).
* Seek operation flushes decode/render queues and re-establishes sync within 250 ms.
* Pause/resume preserves clock consistency (no cumulative drift after repeated toggles).





---

## Phase 5: Advanced Spatiality & UX

**Goal:** Implement immersive rendering modes, true spatial audio, and user playback controls.

### Tasks

1. **360-Degree Immersive Mode**
* Add a runtime toggle to switch geometry from the flat Quad to the inverted 3D Sphere.
* Adjust camera constraints (e.g., lock translation, allow only rotation) when in 360 mode.


2. **3D Spatial Audio**
* Link the OpenAL Listener's position and orientation vectors to the GLM Camera's position/orientation.
* Set the OpenAL Source position to the physical 3D coordinates of the Virtual Screen.
* Test distance attenuation (volume fades as you walk away) and panning (audio shifts left/right as you turn).


3. **Playback Controls**
* Implement keyboard bindings for Play, Pause, and Seek.
* **Note:** Seeking requires flushing FFmpeg codec buffers, clearing all queues, and resetting both Audio and Video PTS clocks.

### Exit Criteria

* Flat-screen and 360 modes can be toggled at runtime without resource leaks or crashes.
* Spatial audio listener/source transforms track camera and screen transforms correctly.
* Playback controls (play/pause/seek) work during active rendering and active audio playback.
* Seek backward/forward operations maintain queue bounds and satisfy sync re-lock target.
* UX state transitions are deterministic (no duplicate mode toggles, no stuck paused/running states).



---

## Phase 6: Linux-Specific Hardware Acceleration

**Goal:** Optimize performance to reduce CPU load and system power consumption using Linux native APIs.

### Tasks

1. **VA-API Integration**
* Configure FFmpeg to initialize a VA-API hardware device context (`av_hwdevice_ctx_create`).
* Modify the decoder thread to output hardware-surface frames instead of software-memory frames.


2. **Zero-Copy OpenGL Interop (EGL/DMA-BUF)**
* Switch context creation from standard GLX to EGL.
* Utilize the `EGL_EXT_image_dma_buf_import` extension.
* Extract DRM PRIME file descriptors (DMA-BUFs) from the VA-API hardware frames.
* Bind these DMA-BUFs directly to OpenGL textures, bypassing PBOs and CPU memory copying entirely.


3. **Profiling & Debugging**
* Run memory profiling using Valgrind/Memcheck to ensure frames and packets are freed correctly.
* Use `intel_gpu_top` or `nvtop` to verify that media decoding is occurring on the GPU's dedicated hardware decoder ring.

### Exit Criteria

* VA-API decode path is selectable at runtime and falls back automatically to software decode on unsupported systems.
* DMA-BUF interop path is validated on at least one supported Linux GPU/driver stack.
* Software and hardware decode paths produce visually correct output and equivalent playback controls.
* Performance comparison is documented (CPU usage, render FPS, dropped frames) for software vs hardware decode.
* Memory/resource profiling confirms no persistent leaks across repeated play/stop/seek cycles.



---

## ⚠️ Critical Risk Assessment

| Risk Factor | Impact | Mitigation Strategy |
| --- | --- | --- |
| **A/V Desync** | High | Strictly adhere to the Audio Master Clock strategy. Never rely on the system clock, frame counting, or `sleep()`, as drift is inevitable. |
| **Render Loop Stalls** | High | Ensure absolutely zero disk I/O, synchronous memory allocation, or decoding occurs on the main OpenGL thread. Use pre-allocated PBOs. |
| **VA-API Fragmentation** | Medium | Hardware decoding API support varies greatly across Mesa/AMD/Intel and NVIDIA proprietary drivers. Keep the CPU (software) decoding pipeline as an automatic, seamless fallback. |
| **Unbounded Queue Growth** | High | Enforce bounded queues with backpressure and explicit flush behavior on seek/shutdown. |
| **Non-Deterministic Shutdown** | Medium | Enforce fixed thread stop/join order and RAII ownership for all FFmpeg/OpenGL/OpenAL resources. |
| **Late Observability** | Medium | Add baseline telemetry counters from Phase 1 onward and treat missing metrics as incomplete work. |