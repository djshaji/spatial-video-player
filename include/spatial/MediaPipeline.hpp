#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "spatial/BoundedQueue.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

namespace spatial {

class MediaPipeline {
public:
    struct PacketDeleter {
        void operator()(AVPacket* packet) const;
    };

    struct FrameDeleter {
        void operator()(AVFrame* frame) const;
    };

    struct DecodedFrame {
        std::unique_ptr<AVFrame, FrameDeleter> frame;
        double ptsSeconds = 0.0;
    };

    using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

    MediaPipeline();
    ~MediaPipeline();

    MediaPipeline(const MediaPipeline&) = delete;
    MediaPipeline& operator=(const MediaPipeline&) = delete;

    bool initialize(const std::string& mediaPath, std::string& errorMessage);
    bool start(std::string& errorMessage);
    void stop();

    bool tryPopDecodedFrame(DecodedFrame& outFrame);

    std::size_t packetQueueDepth() const;
    std::size_t frameQueueDepth() const;
    std::uint64_t decodedFrameCount() const;

private:
    void demuxLoop();
    void decodeLoop();
    void clearQueues();

    BoundedQueue<PacketPtr> packetQueue_;
    BoundedQueue<DecodedFrame> frameQueue_;

    AVFormatContext* formatContext_ = nullptr;
    AVCodecContext* videoCodecContext_ = nullptr;
    int videoStreamIndex_ = -1;
    AVRational videoTimeBase_{};
    std::string videoCodecName_;

    std::thread demuxThread_;
    std::thread decodeThread_;

    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> decodedFrameCount_{0};
};

} // namespace spatial
