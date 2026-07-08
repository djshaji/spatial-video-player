# Spatial Video Player

## System Architecture & Technical Specification v1.0

**Target Environment:** Linux OS (X11/Wayland)
**Graphics API:** OpenGL 3.3+ Core Profile
**Multimedia Framework:** FFmpeg (libav)

---

### 1. Executive Summary

This document outlines the software architecture for a high-performance, spatial 3D video player built for Linux environments. The application converges multimedia decoding and 3D graphics rendering, allowing a user to navigate a virtual environment while a video stream is mapped dynamically onto 3D geometry (e.g., flat virtual screens, curved surfaces, or 360-degree immersive spheres).

The core challenge addressed by this architecture is maintaining strict synchronization between independent CPU-bound tasks (demuxing, audio playback) and highly parallel GPU-bound tasks (hardware decoding, texture sampling, and 3D projection), all while sustaining a continuous 60+ FPS interaction loop.

---

### 2. High-Level Architecture

The system is heavily multi-threaded to prevent blocking operations (like disk I/O and frame decoding) from stalling the main OpenGL rendering loop. The architecture relies on concurrent queues passing data structures between specialized subsystems.

(Note: The visual flow diagram illustrating the pipeline from Media Source down to the OpenGL Render Loop can be found in the original document.)

---

### 3. Core Subsystems

**3.1. Window and Context Management**
The application relies on GLFW or SDL2 to abstract the display server protocols (X11 or Wayland). This layer is responsible for creating the OpenGL context, managing the window swap chain (double buffering), and capturing raw input events (keyboard states, mouse deltas) for the 3D camera.

**3.2. Media Pipeline (FFmpeg/libav)**
Video processing is isolated to worker threads. The Demuxer Thread reads packets from the container format and routes them. The Video Decoder Thread receives compressed packets and decodes them into raw frames (typically YUV420p). These frames are tagged with a Presentation Time Stamp (PTS) and pushed to a thread-safe Ring Buffer.

**3.3. Audio / Video Synchronization**
Because audio is highly sensitive to stuttering, the application utilizes the Audio Clock as the Master Clock. As the audio subsystem (e.g., ALSA or PipeWire) continuously requests PCM samples, it reports the current playback time. The OpenGL rendering loop checks the frame queue: if a video frame's PTS is earlier than the audio clock, it is drawn; if it is too late, it is dropped to catch up.

---

### 4. OpenGL Rendering Pipeline

To render the video inside a spatial 3D context, the system processes raw video frames through a dedicated shader pipeline.

**4.1. Texture Upload Strategy**
Uploading 4K video frames to the GPU every 16ms using a naive `glTexImage2D` blocks the CPU and stalls the render pipeline. This architecture implements asynchronous uploads using Pixel Buffer Objects (PBOs).

* Two PBOs are created for a ping-pong buffer strategy.


* While Frame N is mapped via DMA to the CPU for writing decoded data, Frame N-1 is transferred asynchronously from the PBO to the OpenGL Texture.


* **Hardware Acceleration (Linux Specific):** For maximum performance, FFmpeg can be configured to use VA-API (Video Acceleration API) for hardware decoding. Using the EGL extension `EGL_EXT_image_dma_buf_import`, decoded frames can be shared directly with OpenGL as zero-copy textures without ever passing through system RAM.



**4.2. YUV to RGB Color Space Conversion**
Most video formats store pixels in the YUV (YCbCr) color space to save bandwidth. Converting this to RGB on the CPU is highly inefficient. Instead, the application uploads the raw Y, U, and V planes as three separate GL_R8 textures. A custom Fragment Shader samples these three textures and performs the matrix multiplication on the GPU:

$$R=Y+1.402\times(V-0.5)$$

$$G=Y-0.344\times(U-0.5)-0.714\times(V-0.5)$$

$$B=Y+1.772\times(U-0.5)$$

---

### 5. 3D Spatiality & Mathematics

To simulate walking around the video player, the application utilizes standard 3D MVP (Model, View, Projection) transformations. The video texture is bound to a 3D mesh (a quad for a virtual TV, or an inverted sphere for 360-degree video).

**5.1. View Transformation (Camera)**
The user's perspective is defined by a View Matrix, calculated every frame using the camera's position vector and orientation. To prevent "Gimbal Lock" during complex look-arounds, orientation is tracked using Quaternions rather than Euler angles.

**5.2. Projection Transformation**
A perspective projection matrix is applied to simulate the human eye, causing the video geometry to distort realistically depending on distance and viewing angle. It is defined by:

* **Field of View (FOV):** Typically set between 75° and 90°.


* **Aspect Ratio:** Derived from the OS window dimensions.


* **Near/Far Clipping Planes:** To cull geometry that is immediately behind the camera or too far away.



**5.3. Directional Audio (Optional Extension)**
To complete the spatial experience, an audio library such as OpenAL can be used. By passing the Camera's Position and Look Vectors to the audio listener, and setting the Video Screen's 3D coordinates as the audio source, the volume and panning of the audio track will automatically adjust as the user walks closer to or around the screen.