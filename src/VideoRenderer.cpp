#include "spatial/VideoRenderer.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/glcorearb.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace spatial {

namespace {

bool isSupportedYuvFormat(AVPixelFormat format) {
    return format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P;
}

} // namespace

VideoRenderer::~VideoRenderer() {
    destroyGpuResources();
}

bool VideoRenderer::initialize(std::string& errorMessage) {
    if (initialized_) {
        return true;
    }

    if (!shader_.loadFromFiles("shaders/yuv.vert", "shaders/yuv.frag", errorMessage)) {
        return false;
    }

    glGenTextures(3, textures_.data());
    for (unsigned int texture : textures_) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    initialized_ = true;
    return true;
}

void VideoRenderer::uploadDecodedFrame(const MediaPipeline::DecodedFrame& decodedFrame) {
    if (!initialized_ || decodedFrame.frame == nullptr) {
        return;
    }

    const AVFrame* frame = frameForUpload(decodedFrame.frame.get());
    if (frame == nullptr) {
        ++droppedFrames_;
        return;
    }

    if (frame->width <= 0 || frame->height <= 0) {
        ++droppedFrames_;
        return;
    }

    ensureGpuResourcesForFrame(frame->width, frame->height);

    uploadPlane(textures_[0], planeStates_[0], frame->data[0], frame->linesize[0], frameWidth_, frameHeight_);
    uploadPlane(textures_[1], planeStates_[1], frame->data[1], frame->linesize[1], (frameWidth_ + 1) / 2, (frameHeight_ + 1) / 2);
    uploadPlane(textures_[2], planeStates_[2], frame->data[2], frame->linesize[2], (frameWidth_ + 1) / 2, (frameHeight_ + 1) / 2);

    hasUploadedFrame_ = true;
}

void VideoRenderer::render(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection) {
    if (!initialized_) {
        return;
    }

    shader_.bind();
    shader_.setMat4("uModel", model);
    shader_.setMat4("uView", view);
    shader_.setMat4("uProjection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    shader_.setInt("uTexY", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    shader_.setInt("uTexU", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures_[2]);
    shader_.setInt("uTexV", 2);
}

std::uint64_t VideoRenderer::droppedFrameCount() const {
    return droppedFrames_;
}

bool VideoRenderer::hasUploadedFrame() const {
    return hasUploadedFrame_;
}

void VideoRenderer::ensureGpuResourcesForFrame(int width, int height) {
    if (width == frameWidth_ && height == frameHeight_) {
        return;
    }

    frameWidth_ = width;
    frameHeight_ = height;

    const int widths[3] = {frameWidth_, (frameWidth_ + 1) / 2, (frameWidth_ + 1) / 2};
    const int heights[3] = {frameHeight_, (frameHeight_ + 1) / 2, (frameHeight_ + 1) / 2};

    for (int i = 0; i < 3; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R8,
            std::max(widths[i], 1),
            std::max(heights[i], 1),
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            nullptr);

        if (planeStates_[i].pbo[0] == 0 && planeStates_[i].pbo[1] == 0) {
            glGenBuffers(2, planeStates_[i].pbo.data());
        }

        planeStates_[i].primed = false;
        planeStates_[i].writeIndex = 0;
        planeStates_[i].byteSize = 0;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void VideoRenderer::destroyGpuResources() {
    destroyConversionResources();

    for (auto& state : planeStates_) {
        if (state.pbo[0] != 0 || state.pbo[1] != 0) {
            glDeleteBuffers(2, state.pbo.data());
            state.pbo = {0, 0};
        }
        state.primed = false;
        state.byteSize = 0;
        state.writeIndex = 0;
    }

    if (textures_[0] != 0 || textures_[1] != 0 || textures_[2] != 0) {
        glDeleteTextures(3, textures_.data());
        textures_ = {0, 0, 0};
    }

    initialized_ = false;
    hasUploadedFrame_ = false;
    frameWidth_ = 0;
    frameHeight_ = 0;
}

void VideoRenderer::uploadPlane(
    unsigned int texture,
    PlaneUploadState& uploadState,
    const std::uint8_t* src,
    int srcStride,
    int width,
    int height) {
    if (src == nullptr || srcStride <= 0 || width <= 0 || height <= 0) {
        ++droppedFrames_;
        return;
    }

    const int readIndex = uploadState.writeIndex;
    const int writeIndex = (uploadState.writeIndex + 1) % 2;

    const std::size_t rowBytes = static_cast<std::size_t>(width);
    const std::size_t totalBytes = rowBytes * static_cast<std::size_t>(height);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, uploadState.pbo[writeIndex]);
    if (uploadState.byteSize != totalBytes) {
        glBufferData(GL_PIXEL_UNPACK_BUFFER, static_cast<GLsizeiptr>(totalBytes), nullptr, GL_STREAM_DRAW);
        uploadState.byteSize = totalBytes;
    }

    if (static_cast<std::size_t>(srcStride) == rowBytes) {
        glBufferSubData(
            GL_PIXEL_UNPACK_BUFFER,
            0,
            static_cast<GLsizeiptr>(totalBytes),
            src);
    } else {
        std::vector<std::uint8_t> contiguous(totalBytes, 0);
        for (int row = 0; row < height; ++row) {
            std::memcpy(contiguous.data() + row * rowBytes, src + row * srcStride, rowBytes);
        }
        glBufferSubData(
            GL_PIXEL_UNPACK_BUFFER,
            0,
            static_cast<GLsizeiptr>(totalBytes),
            contiguous.data());
    }

    if (uploadState.primed) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, uploadState.pbo[readIndex]);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            width,
            height,
            GL_RED,
            GL_UNSIGNED_BYTE,
            nullptr);
    } else {
        // Prime texture from the first uploaded PBO so first frame is visible.
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, uploadState.pbo[writeIndex]);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            width,
            height,
            GL_RED,
            GL_UNSIGNED_BYTE,
            nullptr);
        uploadState.primed = true;
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    uploadState.writeIndex = writeIndex;
}

const AVFrame* VideoRenderer::frameForUpload(const AVFrame* sourceFrame) {
    if (sourceFrame == nullptr) {
        return nullptr;
    }

    const AVPixelFormat sourceFormat = static_cast<AVPixelFormat>(sourceFrame->format);
    if (isSupportedYuvFormat(sourceFormat)) {
        return sourceFrame;
    }

    if (sourceFrame->width <= 0 || sourceFrame->height <= 0) {
        return nullptr;
    }

    const bool conversionConfigChanged =
        swsContext_ == nullptr ||
        convertedFrame_ == nullptr ||
        convertedWidth_ != sourceFrame->width ||
        convertedHeight_ != sourceFrame->height ||
        convertedSourceFormat_ != sourceFrame->format;

    if (conversionConfigChanged) {
        destroyConversionResources();

        swsContext_ = sws_getContext(
            sourceFrame->width,
            sourceFrame->height,
            sourceFormat,
            sourceFrame->width,
            sourceFrame->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr);

        if (swsContext_ == nullptr) {
            std::cerr << "VideoRenderer: sws_getContext failed for pixel format " << sourceFrame->format << "\n";
            return nullptr;
        }

        convertedFrame_ = av_frame_alloc();
        if (convertedFrame_ == nullptr) {
            std::cerr << "VideoRenderer: failed to allocate converted frame.\n";
            destroyConversionResources();
            return nullptr;
        }

        convertedFrame_->format = AV_PIX_FMT_YUV420P;
        convertedFrame_->width = sourceFrame->width;
        convertedFrame_->height = sourceFrame->height;

        if (av_frame_get_buffer(convertedFrame_, 32) < 0) {
            std::cerr << "VideoRenderer: av_frame_get_buffer failed for converted frame.\n";
            destroyConversionResources();
            return nullptr;
        }

        convertedWidth_ = sourceFrame->width;
        convertedHeight_ = sourceFrame->height;
        convertedSourceFormat_ = sourceFrame->format;
    }

    if (av_frame_make_writable(convertedFrame_) < 0) {
        std::cerr << "VideoRenderer: converted frame not writable.\n";
        return nullptr;
    }

    const int scaledRows = sws_scale(
        swsContext_,
        sourceFrame->data,
        sourceFrame->linesize,
        0,
        sourceFrame->height,
        convertedFrame_->data,
        convertedFrame_->linesize);

    if (scaledRows != sourceFrame->height) {
        std::cerr << "VideoRenderer: sws_scale produced " << scaledRows << " rows, expected " << sourceFrame->height << "\n";
        return nullptr;
    }

    convertedFrame_->pts = sourceFrame->pts;
    return convertedFrame_;
}

void VideoRenderer::destroyConversionResources() {
    if (convertedFrame_ != nullptr) {
        av_frame_free(&convertedFrame_);
    }

    if (swsContext_ != nullptr) {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }

    convertedWidth_ = 0;
    convertedHeight_ = 0;
    convertedSourceFormat_ = -1;
}

} // namespace spatial
