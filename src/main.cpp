#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>

#include <glm/ext/matrix_transform.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

#include "spatial/Camera.hpp"
#include "spatial/Geometry.hpp"
#include "spatial/ShaderProgram.hpp"

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

int main() {
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

    spatial::ShaderProgram shader;
    std::string shaderError;
    if (!shader.loadFromFiles("shaders/basic.vert", "shaders/basic.frag", shaderError)) {
        std::cerr << "Shader error: " << shaderError << "\n";
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

    float lastFrameSeconds = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        const float nowSeconds = static_cast<float>(glfwGetTime());
        const float deltaSeconds = nowSeconds - lastFrameSeconds;
        lastFrameSeconds = nowSeconds;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        processMovement(window, camera, deltaSeconds);

        glClearColor(0.08f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.bind();

        const glm::mat4 model = glm::mat4(1.0f);
        const glm::mat4 view = camera.viewMatrix();
        const glm::mat4 projection = camera.projectionMatrix();

        shader.setMat4("uModel", model);
        shader.setMat4("uView", view);
        shader.setMat4("uProjection", projection);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quad.indices.size()), GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
