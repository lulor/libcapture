#include "../include/decoder.h"

Decoder::Decoder(const AVCodecParameters *params) : codec_(nullptr), codec_ctx_(nullptr) {
    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) throw std::runtime_error("Decoder: Cannot find codec");

    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) throw std::runtime_error("Decoder: Failed to allocated memory for AVCodecContext");

    if (avcodec_parameters_to_context(codec_ctx_, params) < 0)
        throw std::runtime_error("Decoder: Failed to copy codec params to codec context");

    if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) throw std::runtime_error("Decoder: Unable to open the av codec");
}

Decoder::~Decoder() {
    // TO-DO: free codec_ (how?)
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
}

void Decoder::sendPacket(AVPacket *packet) {
    int ret = avcodec_send_packet(codec_ctx_, packet);
    if (ret == AVERROR(EAGAIN)) {
        throw FullException("Decoder");
    } else if (ret == AVERROR_EOF) {
        throw FlushedException("Decoder");
    } else if (ret < 0) {
        throw std::runtime_error("Decoder: Failed to send packet to decoder");
    }
}

void Decoder::fillFrame(AVFrame *frame) {
    if (!frame) throw std::runtime_error("Decoder: Frame is not allocated");

    int ret = avcodec_receive_frame(codec_ctx_, frame);
    if (ret == AVERROR(EAGAIN)) {
        throw EmptyException("Decoder");
    } else if (ret == AVERROR_EOF) {
        throw FlushedException("Decoder");
    } else if (ret < 0) {
        throw std::runtime_error("Decoder: Failed to receive frame from decoder");
    }
}

const AVCodecContext *Decoder::getCodecContext() { return codec_ctx_; }