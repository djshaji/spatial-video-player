#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace spatial {

enum class CameraMove {
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down,
};

class Camera {
public:
    Camera(float width, float height);

    void processKeyboard(CameraMove move, float deltaSeconds);
    void processMouseDelta(float deltaX, float deltaY);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix() const;

    glm::vec3 position() const;

private:
    void clampPitch();

    glm::vec3 position_;
    glm::quat orientation_;

    float yawDeg_;
    float pitchDeg_;

    float moveSpeed_;
    float mouseSensitivity_;

    float fovDeg_;
    float aspectRatio_;
    float nearPlane_;
    float farPlane_;
};

} // namespace spatial
