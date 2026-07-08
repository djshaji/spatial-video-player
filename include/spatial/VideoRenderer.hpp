#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/mat4x4.hpp>

extern "C" {
#include <libswscale/swscale.h>
}

#include "spatial/MediaPipeline.hpp"
#include "spatial/ShaderProgram.hpp"

namespace spatial {

class VideoRenderer {
public:
    VideoRenderer() = default;
    ~VideoRenderer();

    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;

    bool initialize(std::string& errorMessage);
    void uploadDecodedFrame(const MediaPipeline::DecodedFrame& decodedFrame);
    void render(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection);

    std::uint64_t droppedFrameCount() const;
    bool hasUploadedFrame() const;

private:
    struct PlaneUploadState {
        std::array<unsigned int, 2> pbo{0, 0};
        int writeIndex = 0;
        bool primed = false;
        std::size_t byteSize = 0;
    };

    void ensureGpuResourcesForFrame(int width, int height);
    void destroyGpuResources();
    void uploadPlane(
        unsigned int texture,
        PlaneUploadState& uploadState,
        const std::uint8_t* src,
        int srcStride,
        int width,
        int height);

    const AVFrame* frameForUpload(const AVFrame* sourceFrame);
    void destroyConversionResources();

    ShaderProgram shader_;

    std::array<unsigned int, 3> textures_{0, 0, 0};
    std::array<PlaneUploadState, 3> planeStates_{};

    int frameWidth_ = 0;
    int frameHeight_ = 0;

    SwsContext* swsContext_ = nullptr;
    AVFrame* convertedFrame_ = nullptr;
    int convertedWidth_ = 0;
    int convertedHeight_ = 0;
    int convertedSourceFormat_ = -1;

    bool initialized_ = false;
    bool hasUploadedFrame_ = false;
    std::uint64_t droppedFrames_ = 0;
};

} // namespace spatial
