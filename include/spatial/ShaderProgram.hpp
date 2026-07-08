#pragma once

#include <glm/mat4x4.hpp>
#include <string>

namespace spatial {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath, std::string& errorMessage);
    void bind() const;
    void setMat4(const std::string& uniformName, const glm::mat4& value) const;
    unsigned int id() const;

private:
    unsigned int programId_ = 0;
};

} // namespace spatial
