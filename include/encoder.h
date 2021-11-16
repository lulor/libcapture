#pragma once

#include <map>
#include <string>

#include "deleter.h"
#include "ffmpeg_libs.h"

class Encoder {
protected:
    using unique_ptr_codec_ctx = std::unique_ptr<AVCodecContext, DeleterPP<avcodec_free_context>>;

    AVCodec *codec_;
    AVDictionary *options_;
    unique_ptr_codec_ctx codec_ctx_;

    void cleanup();

    Encoder(AVCodecID codec_id, const std::map<std::string, std::string> &options, int global_header_flags);

public:
    ~Encoder();

    /**
     * Send a frame to the encoder
     * @param frame the frame to send to the encoder. It can be nullptr to flush the encoder
     * @return true if the frame has been correctly sent, false if the encoder could not receive it
     */
    bool sendFrame(std::shared_ptr<const AVFrame> frame) const;

    /**
     * Get a converted packet from the encoder
     * @return a packet if it was possible to get it, nullptr if the encoder had nothing to write
     * because it is empty or flushed
     */
    std::shared_ptr<AVPacket> getPacket() const;

    const AVCodecContext *getCodecContext() const;
};