#include "spatial/Camera.hpp"

#include <algorithm>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace spatial {

namespace {
constexpr float kMaxPitchDeg = 89.0f;
constexpr glm::vec3 kWorldUp(0.0f, 1.0f, 0.0f);
} // namespace

Camera::Camera(float width, float height)
    : position_(0.0f, 0.0f, 3.0f),
      orientation_(glm::identity<glm::quat>()),
      yawDeg_(-90.0f),
      pitchDeg_(0.0f),
      moveSpeed_(3.5f),
      mouseSensitivity_(0.09f),
      fovDeg_(75.0f),
      aspectRatio_(width > 0.0f && height > 0.0f ? width / height : 16.0f / 9.0f),
      nearPlane_(0.1f),
      farPlane_(100.0f) {
    processMouseDelta(0.0f, 0.0f);
}

void Camera::processKeyboard(CameraMove move, float deltaSeconds) {
    const float velocity = moveSpeed_ * std::max(deltaSeconds, 0.0f);

    const glm::vec3 forward = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 right = orientation_ * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = orientation_ * glm::vec3(0.0f, 1.0f, 0.0f);

    switch (move) {
        case CameraMove::Forward:
            position_ += forward * velocity;
            break;
        case CameraMove::Backward:
            position_ -= forward * velocity;
            break;
        case CameraMove::Left:
            position_ -= right * velocity;
            break;
        case CameraMove::Right:
            position_ += right * velocity;
            break;
        case CameraMove::Up:
            position_ += up * velocity;
            break;
        case CameraMove::Down:
            position_ -= up * velocity;
            break;
    }
}

void Camera::processMouseDelta(float deltaX, float deltaY) {
    yawDeg_ += deltaX * mouseSensitivity_;
    pitchDeg_ += deltaY * mouseSensitivity_;
    clampPitch();

    const glm::quat yawRot = glm::angleAxis(glm::radians(yawDeg_), kWorldUp);
    const glm::vec3 localRight = yawRot * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::quat pitchRot = glm::angleAxis(glm::radians(pitchDeg_), localRight);
    orientation_ = glm::normalize(pitchRot * yawRot);
}

glm::mat4 Camera::viewMatrix() const {
    const glm::vec3 forward = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 up = orientation_ * glm::vec3(0.0f, 1.0f, 0.0f);
    return glm::lookAt(position_, position_ + forward, up);
}

glm::mat4 Camera::projectionMatrix() const {
    return glm::perspective(glm::radians(fovDeg_), aspectRatio_, nearPlane_, farPlane_);
}

glm::vec3 Camera::position() const {
    return position_;
}

void Camera::clampPitch() {
    pitchDeg_ = std::clamp(pitchDeg_, -kMaxPitchDeg, kMaxPitchDeg);
}

} // namespace spatial
