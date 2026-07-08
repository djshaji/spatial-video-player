#include "spatial/MediaPipeline.hpp"

#include <iostream>

extern "C" {
#include <libavutil/error.h>
}

namespace spatial {

namespace {

std::string ffmpegErrorString(int errorCode) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errorCode, buffer, sizeof(buffer));
    return std::string(buffer);
}

} // namespace

void MediaPipeline::PacketDeleter::operator()(AVPacket* packet) const {
    av_packet_free(&packet);
}

void MediaPipeline::FrameDeleter::operator()(AVFrame* frame) const {
    av_frame_free(&frame);
}

MediaPipeline::MediaPipeline()
    : packetQueue_(512),
      frameQueue_(12) {}

MediaPipeline::~MediaPipeline() {
    stop();

    if (videoCodecContext_ != nullptr) {
        avcodec_free_context(&videoCodecContext_);
    }

    if (formatContext_ != nullptr) {
        avformat_close_input(&formatContext_);
    }
}

bool MediaPipeline::initialize(const std::string& mediaPath, std::string& errorMessage) {
    if (mediaPath.empty()) {
        errorMessage = "Media path is empty.";
        return false;
    }

    int rc = avformat_open_input(&formatContext_, mediaPath.c_str(), nullptr, nullptr);
    if (rc < 0) {
        errorMessage = "avformat_open_input failed: " + ffmpegErrorString(rc);
        return false;
    }

    rc = avformat_find_stream_info(formatContext_, nullptr);
    if (rc < 0) {
        errorMessage = "avformat_find_stream_info failed: " + ffmpegErrorString(rc);
        return false;
    }

    rc = av_find_best_stream(formatContext_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (rc < 0) {
        errorMessage = "No video stream found: " + ffmpegErrorString(rc);
        return false;
    }

    videoStreamIndex_ = rc;
    AVStream* videoStream = formatContext_->streams[videoStreamIndex_];
    videoTimeBase_ = videoStream->time_base;

    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (codec == nullptr) {
        errorMessage = "Failed to find video decoder for stream codec.";
        return false;
    }
    videoCodecName_ = codec->name != nullptr ? codec->name : "unknown";

    videoCodecContext_ = avcodec_alloc_context3(codec);
    if (videoCodecContext_ == nullptr) {
        errorMessage = "Failed to allocate AVCodecContext.";
        return false;
    }

    rc = avcodec_parameters_to_context(videoCodecContext_, videoStream->codecpar);
    if (rc < 0) {
        errorMessage = "avcodec_parameters_to_context failed: " + ffmpegErrorString(rc);
        return false;
    }

    rc = avcodec_open2(videoCodecContext_, codec, nullptr);
    if (rc < 0) {
        errorMessage = "avcodec_open2 failed: " + ffmpegErrorString(rc);
        return false;
    }

    return true;
}

bool MediaPipeline::start(std::string& errorMessage) {
    if (formatContext_ == nullptr || videoCodecContext_ == nullptr || videoStreamIndex_ < 0) {
        errorMessage = "MediaPipeline::initialize must succeed before start().";
        return false;
    }

    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        errorMessage = "Media pipeline already running.";
        return false;
    }

    demuxThread_ = std::thread(&MediaPipeline::demuxLoop, this);
    decodeThread_ = std::thread(&MediaPipeline::decodeLoop, this);
    return true;
}

void MediaPipeline::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    packetQueue_.close();
    frameQueue_.close();

    if (demuxThread_.joinable()) {
        demuxThread_.join();
    }

    if (decodeThread_.joinable()) {
        decodeThread_.join();
    }

    clearQueues();
}

bool MediaPipeline::tryPopDecodedFrame(DecodedFrame& outFrame) {
    return frameQueue_.tryPop(outFrame);
}

std::size_t MediaPipeline::packetQueueDepth() const {
    return packetQueue_.size();
}

std::size_t MediaPipeline::frameQueueDepth() const {
    return frameQueue_.size();
}

std::uint64_t MediaPipeline::decodedFrameCount() const {
    return decodedFrameCount_.load();
}

void MediaPipeline::demuxLoop() {
    while (running_.load()) {
        PacketPtr packet(av_packet_alloc());
        if (!packet) {
            std::cerr << "Demux(stream=" << videoStreamIndex_ << ", codec=" << videoCodecName_ << "): failed to allocate packet.\n";
            break;
        }

        const int rc = av_read_frame(formatContext_, packet.get());
        if (rc == AVERROR_EOF) {
            break;
        }

        if (rc < 0) {
            std::cerr << "Demux(stream=" << videoStreamIndex_ << ", codec=" << videoCodecName_ << "): av_read_frame failed: "
                      << ffmpegErrorString(rc) << "\n";
            break;
        }

        if (packet->stream_index != videoStreamIndex_) {
            continue;
        }

        if (!packetQueue_.push(std::move(packet))) {
            break;
        }
    }

    packetQueue_.close();
}

void MediaPipeline::decodeLoop() {
    PacketPtr packet;

    while (packetQueue_.pop(packet)) {
        const int sendRc = avcodec_send_packet(videoCodecContext_, packet.get());
        if (sendRc < 0) {
            std::cerr << "Decode(stream=" << videoStreamIndex_ << ", codec=" << videoCodecName_ << "): avcodec_send_packet failed: "
                      << ffmpegErrorString(sendRc) << "\n";
            continue;
        }

        while (true) {
            std::unique_ptr<AVFrame, FrameDeleter> frame(av_frame_alloc());
            if (!frame) {
                std::cerr << "Decode(stream=" << videoStreamIndex_ << ", codec=" << videoCodecName_ << "): failed to allocate frame.\n";
                break;
            }

            const int recvRc = avcodec_receive_frame(videoCodecContext_, frame.get());
            if (recvRc == AVERROR(EAGAIN) || recvRc == AVERROR_EOF) {
                break;
            }

            if (recvRc < 0) {
                std::cerr << "Decode(stream=" << videoStreamIndex_ << ", codec=" << videoCodecName_ << "): avcodec_receive_frame failed: "
                          << ffmpegErrorString(recvRc) << "\n";
                break;
            }

            const std::int64_t pts = frame->best_effort_timestamp;
            const double ptsSeconds = pts == AV_NOPTS_VALUE ? 0.0 : pts * av_q2d(videoTimeBase_);

            DecodedFrame out;
            out.frame = std::move(frame);
            out.ptsSeconds = ptsSeconds;

            if (!frameQueue_.push(std::move(out))) {
                frameQueue_.close();
                return;
            }

            decodedFrameCount_.fetch_add(1);
        }
    }

    // Flush buffered frames from decoder after packet stream ends.
    avcodec_send_packet(videoCodecContext_, nullptr);
    while (true) {
        std::unique_ptr<AVFrame, FrameDeleter> frame(av_frame_alloc());
        if (!frame) {
            break;
        }

        const int recvRc = avcodec_receive_frame(videoCodecContext_, frame.get());
        if (recvRc == AVERROR_EOF || recvRc == AVERROR(EAGAIN)) {
            break;
        }

        if (recvRc < 0) {
            std::cerr << "Decode flush(stream=" << videoStreamIndex_ << ", codec=" << videoCodecName_ << "): avcodec_receive_frame failed: "
                      << ffmpegErrorString(recvRc) << "\n";
            break;
        }

        const std::int64_t pts = frame->best_effort_timestamp;
        const double ptsSeconds = pts == AV_NOPTS_VALUE ? 0.0 : pts * av_q2d(videoTimeBase_);

        DecodedFrame out;
        out.frame = std::move(frame);
        out.ptsSeconds = ptsSeconds;
        if (!frameQueue_.push(std::move(out))) {
            break;
        }

        decodedFrameCount_.fetch_add(1);
    }

    frameQueue_.close();
}

void MediaPipeline::clearQueues() {
    PacketPtr packet;
    while (packetQueue_.tryPop(packet)) {
    }

    DecodedFrame frame;
    while (frameQueue_.tryPop(frame)) {
    }
}

} // namespace spatial
