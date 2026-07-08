#include "spatial/ShaderProgram.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace spatial {

namespace {

std::string readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

unsigned int compileStage(unsigned int stageType, const std::string& source, std::string& errorMessage) {
    const char* sourcePtr = source.c_str();
    const unsigned int shader = glCreateShader(stageType);
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    int compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> buffer(static_cast<std::size_t>(length > 0 ? length : 1));
    glGetShaderInfoLog(shader, static_cast<int>(buffer.size()), nullptr, buffer.data());
    errorMessage.assign(buffer.data());

    glDeleteShader(shader);
    return 0;
}

} // namespace

ShaderProgram::~ShaderProgram() {
    if (programId_ != 0) {
        glDeleteProgram(programId_);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept {
    programId_ = other.programId_;
    other.programId_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (programId_ != 0) {
        glDeleteProgram(programId_);
    }

    programId_ = other.programId_;
    other.programId_ = 0;
    return *this;
}

bool ShaderProgram::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath, std::string& errorMessage) {
    const std::string vertexSource = readTextFile(vertexPath);
    if (vertexSource.empty()) {
        errorMessage = "Failed to read vertex shader file: " + vertexPath;
        return false;
    }

    const std::string fragmentSource = readTextFile(fragmentPath);
    if (fragmentSource.empty()) {
        errorMessage = "Failed to read fragment shader file: " + fragmentPath;
        return false;
    }

    std::string stageError;
    const unsigned int vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource, stageError);
    if (vertexShader == 0) {
        errorMessage = "Vertex shader compile failed: " + stageError;
        return false;
    }

    const unsigned int fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource, stageError);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        errorMessage = "Fragment shader compile failed: " + stageError;
        return false;
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        int length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> buffer(static_cast<std::size_t>(length > 0 ? length : 1));
        glGetProgramInfoLog(program, static_cast<int>(buffer.size()), nullptr, buffer.data());
        errorMessage.assign(buffer.data());
        glDeleteProgram(program);
        return false;
    }

    if (programId_ != 0) {
        glDeleteProgram(programId_);
    }

    programId_ = program;
    return true;
}

void ShaderProgram::bind() const {
    glUseProgram(programId_);
}

void ShaderProgram::setInt(const std::string& uniformName, int value) const {
    const int location = glGetUniformLocation(programId_, uniformName.c_str());
    if (location >= 0) {
        glUniform1i(location, value);
    }
}

void ShaderProgram::setMat4(const std::string& uniformName, const glm::mat4& value) const {
    const int location = glGetUniformLocation(programId_, uniformName.c_str());
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
    }
}

unsigned int ShaderProgram::id() const {
    return programId_;
}

} // namespace spatial
