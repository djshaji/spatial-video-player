#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>

#include <glm/ext/matrix_transform.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "spatial/Camera.hpp"
#include "spatial/Geometry.hpp"
#include "spatial/MediaPipeline.hpp"
#include "spatial/VideoRenderer.hpp"

namespace {

struct InputState {
    bool firstMouse = true;
    double lastX = 0.0;
    double lastY = 0.0;
    spatial::Camera* camera = nullptr;
};

InputState gInputState;

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    if (gInputState.camera == nullptr) {
        return;
    }

    if (gInputState.firstMouse) {
        gInputState.lastX = xpos;
        gInputState.lastY = ypos;
        gInputState.firstMouse = false;
    }

    const float deltaX = static_cast<float>(xpos - gInputState.lastX);
    const float deltaY = static_cast<float>(gInputState.lastY - ypos);

    gInputState.lastX = xpos;
    gInputState.lastY = ypos;

    gInputState.camera->processMouseDelta(deltaX, deltaY);
}

void processMovement(GLFWwindow* window, spatial::Camera& camera, float deltaSeconds) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.processKeyboard(spatial::CameraMove::Forward, deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.processKeyboard(spatial::CameraMove::Backward, deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.processKeyboard(spatial::CameraMove::Left, deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.processKeyboard(spatial::CameraMove::Right, deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        camera.processKeyboard(spatial::CameraMove::Up, deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        camera.processKeyboard(spatial::CameraMove::Down, deltaSeconds);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Spatial Player - Phase 1", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create OpenGL window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glEnable(GL_DEPTH_TEST);

    spatial::Camera camera(1280.0f, 720.0f);
    gInputState.camera = &camera;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouseCallback);

    spatial::VideoRenderer videoRenderer;
    std::string rendererError;
    if (!videoRenderer.initialize(rendererError)) {
        std::cerr << "Video renderer init failed: " << rendererError << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const spatial::MeshData quad = spatial::createTexturedQuad();

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(quad.vertices.size() * sizeof(float)),
        quad.vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(quad.indices.size() * sizeof(unsigned int)),
        quad.indices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    spatial::MediaPipeline mediaPipeline;
    bool mediaPipelineActive = false;
    bool playbackClockInitialized = false;
    double playbackBasePtsSeconds = 0.0;
    auto playbackBaseWallClock = std::chrono::steady_clock::now();
    std::optional<spatial::MediaPipeline::DecodedFrame> stagedFrame;
    auto stagedFrameWallClock = std::chrono::steady_clock::now();

    std::string mediaPath;
    if (argc > 1 && argv[1] != nullptr) {
        mediaPath = argv[1];
    } else {
        std::ifstream defaultMedia("video.mp4");
        if (defaultMedia.good()) {
            mediaPath = "video.mp4";
        }
    }

    if (!mediaPath.empty()) {
        std::string mediaError;
        if (!mediaPipeline.initialize(mediaPath, mediaError)) {
            std::cerr << "Media pipeline init failed: " << mediaError << "\n";
            glDeleteBuffers(1, &ebo);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }

        if (!mediaPipeline.start(mediaError)) {
            std::cerr << "Media pipeline start failed: " << mediaError << "\n";
            glDeleteBuffers(1, &ebo);
            glDeleteBuffers(1, &vbo);
            glDeleteVertexArrays(1, &vao);
            glfwDestroyWindow(window);
            glfwTerminate();
            return EXIT_FAILURE;
        }

        mediaPipelineActive = true;
        std::cout << "Media pipeline started with file: " << mediaPath << "\n";
    } else {
        std::cout << "Media pipeline disabled. Provide a media file path as argv[1] or place video.mp4 in working directory.\n";
    }

    float lastFrameSeconds = static_cast<float>(glfwGetTime());
    auto lastMediaStatsPrint = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {
        const float nowSeconds = static_cast<float>(glfwGetTime());
        const float deltaSeconds = nowSeconds - lastFrameSeconds;
        lastFrameSeconds = nowSeconds;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        processMovement(window, camera, deltaSeconds);

        if (mediaPipelineActive) {
            const auto now = std::chrono::steady_clock::now();

            if (!playbackClockInitialized) {
                spatial::MediaPipeline::DecodedFrame initialFrame;
                if (mediaPipeline.tryPopDecodedFrame(initialFrame)) {
                    playbackBasePtsSeconds = initialFrame.ptsSeconds;
                    playbackBaseWallClock = now;
                    playbackClockInitialized = true;
                    videoRenderer.uploadDecodedFrame(initialFrame);
                }
            } else {
                const double elapsedSeconds =
                    std::chrono::duration<double>(now - playbackBaseWallClock).count();
                const double targetPtsSeconds = playbackBasePtsSeconds + elapsedSeconds;
                constexpr double kDisplayLeadSeconds = 0.010;
                constexpr double kMaxStagedWaitSeconds = 0.200;

                bool hasCandidate = false;
                spatial::MediaPipeline::DecodedFrame candidate;

                if (stagedFrame.has_value()) {
                    if (stagedFrame->ptsSeconds <= targetPtsSeconds + kDisplayLeadSeconds) {
                        candidate = std::move(*stagedFrame);
                        stagedFrame.reset();
                        hasCandidate = true;
                    } else {
                        const double stagedWaitSeconds = std::chrono::duration<double>(now - stagedFrameWallClock).count();
                        if (stagedWaitSeconds >= kMaxStagedWaitSeconds) {
                            // Fallback: avoid freezing on a future-stamped frame by forcing progress.
                            candidate = std::move(*stagedFrame);
                            stagedFrame.reset();
                            hasCandidate = true;

                            // Re-anchor video clock at the forced frame to avoid accumulating drift.
                            playbackBasePtsSeconds = candidate.ptsSeconds;
                            playbackBaseWallClock = now;
                        }
                    }
                } else {
                    spatial::MediaPipeline::DecodedFrame decodedFrame;
                    while (mediaPipeline.tryPopDecodedFrame(decodedFrame)) {
                        if (decodedFrame.ptsSeconds <= targetPtsSeconds + kDisplayLeadSeconds) {
                            candidate = std::move(decodedFrame);
                            hasCandidate = true;
                        } else {
                            stagedFrame = std::move(decodedFrame);
                            stagedFrameWallClock = now;
                            break;
                        }
                    }
                }

                if (hasCandidate) {
                    videoRenderer.uploadDecodedFrame(candidate);
                }
            }

            if (now - lastMediaStatsPrint >= std::chrono::seconds(1)) {
                std::cout << "media stats: decoded_frames=" << mediaPipeline.decodedFrameCount()
                          << " packet_q=" << mediaPipeline.packetQueueDepth()
                          << " frame_q=" << mediaPipeline.frameQueueDepth()
                          << " uploaded=" << (videoRenderer.hasUploadedFrame() ? 1 : 0)
                          << " dropped_upload=" << videoRenderer.droppedFrameCount() << "\n";
                lastMediaStatsPrint = now;
            }
        }

        glClearColor(0.08f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 model = glm::mat4(1.0f);
        const glm::mat4 view = camera.viewMatrix();
        const glm::mat4 projection = camera.projectionMatrix();

        videoRenderer.render(model, view, projection);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quad.indices.size()), GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    if (mediaPipelineActive) {
        mediaPipeline.stop();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
