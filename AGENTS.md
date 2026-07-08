# AGENTS

## Project State

This repository is currently in planning mode. It contains architecture and implementation planning docs, but no source code, build scripts, or test targets yet.

Primary references:
- [Architecture](Architecture.md)
- [Implementation Plan](Plan.md)

## Scope and Platform

- Target platform: Linux (X11/Wayland)
- Intended stack: C++17, CMake, OpenGL 3.3 Core, GLFW, GLM, FFmpeg, OpenAL or ALSA
- Performance target: smooth interactive rendering at 60+ FPS

## Agent Priorities

1. Link to existing docs instead of repeating large sections from them.
2. Keep changes aligned to phased delivery in Plan.md unless the user asks to skip phases.
3. Preserve the architecture invariants in Architecture.md:
   - Audio clock is the master clock for A/V sync.
   - Demux/decode work must stay off the render thread.
   - Avoid synchronous per-frame GPU upload patterns that stall rendering.
4. Keep Linux-first assumptions for graphics and media acceleration paths.

## Implementation Guidance For This Repo Stage

- Do not assume commands exist before creating the corresponding files.
- If scaffolding begins, create minimal, working foundations first:
  - CMake project setup
  - App loop: Initialization -> Update -> Render -> Shutdown
  - OpenGL context and shader compilation path
- Add placeholders only when they unblock incremental progress and are clearly marked.

## Build and Test Commands

No authoritative build or test commands are available yet in the repository.

When build files are added, prefer recording commands in this file and keeping them synced with the project files.

## Documentation Discipline

- Treat Architecture.md as the source of truth for subsystem behavior.
- Treat Plan.md as the source of truth for sequencing and milestones.
- If implementation decisions diverge from either document, update the docs in the same change set.
