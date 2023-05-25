#include "pipeline.h"

#include <cassert>
#include <iostream>
#include <map>

namespace {
std::string errMsg(const std::string &msg) { return ("Pipeline: " + msg); }
}  // namespace

av::Pipeline::Pipeline(const std::string &output_file, const bool async) : muxer_(output_file), async_(async) {}

av::Pipeline::~Pipeline() {
    if (async_ && !terminated_) stopProcessors();
}

void av::Pipeline::startProcessor(const MediaType type) {
    assert(!terminated_);
    assert(isMediaTypeValid(type));
    assert(managed_types_[type]);
    assert(!processors_[type].joinable());

    processors_[type] = std::thread([this, type]() {
        try {
            while (true) {
                PacketUPtr packet;
                {
                    std::unique_lock ul(processors_m_);
                    packets_cv_[type].wait(ul, [this, type]() { return (packets_[type] || terminated_); });
                    if (!packets_[type] && terminated_) break;
                    packet = std::move(packets_[type]);
                }
                processPacket(packet.get(), type);
            }
        } catch (...) {
            const std::lock_guard lg(processors_m_);
            e_ptrs_[type] = std::current_exception();
        }
    });
}

void av::Pipeline::stopProcessors() {
    {
        const std::lock_guard lg(processors_m_);
        terminated_ = true;
        for (auto &cv : packets_cv_) cv.notify_all();
    }
    for (auto &p : processors_) {
        if (p.joinable()) p.join();
    }
}

void av::Pipeline::checkExceptions() {
    for (auto &e_ptr : e_ptrs_) {
        if (e_ptr) std::rethrow_exception(e_ptr);
    }
}

void av::Pipeline::initVideo(const Demuxer &demuxer, const AVCodecID codec_id, const AVPixelFormat pix_fmt,
                             const VideoParameters &video_params) {
    const auto type = MediaType::Video;

    if (muxer_.isInited()) throw std::logic_error(errMsg("output has already been initialized"));
    if (terminated_) throw std::logic_error(errMsg("already terminated"));
    if (managed_types_[type]) throw std::logic_error(errMsg("video pipeline already initialized"));

    managed_types_[type] = true;

    /* Init decoder */
    decoders_[type] = Decoder(demuxer.getStreamParams(type));

    auto dec_ctx = decoders_[type].getContext();
    auto [width, height] = video_params.getVideoSize();
    auto [offset_x, offset_y] = video_params.getVideoOffset();
    if (!width) width = dec_ctx->width;
    if (!height) height = dec_ctx->height;

    if (width > dec_ctx->width) throw std::runtime_error("Specified width exceeds the display one");
    if (height > dec_ctx->height) throw std::runtime_error("Specified height exceeds the display one");
    if (offset_x + width > dec_ctx->width) throw std::runtime_error("Specified horizontal offset is too high");
    if (offset_y + height > dec_ctx->height) throw std::runtime_error("Specified veritcal offset is too high");

    /* Init encoder */
    std::map<std::string, std::string> enc_options;
    /*
     * Possible presets from fastest (and worst quality) to slowest (and best quality):
     * ultrafast -> superfast -> veryfast -> faster -> fast -> medium
     */
    enc_options.insert({"preset", "ultrafast"});
    encoders_[type] = Encoder(codec_id, width, height, pix_fmt, demuxer.getStreamTimeBase(type),
                              muxer_.getGlobalHeaderFlags(), enc_options);

    /* Init converter */
    converters_[type] = Converter(decoders_[type].getContext(), encoders_[type].getContext(),
                                  demuxer.getStreamTimeBase(type), offset_x, offset_y);

    muxer_.addStream(encoders_[type].getContext());

    if (async_) startProcessor(type);
}

void av::Pipeline::initAudio(const Demuxer &demuxer, const AVCodecID codec_id) {
    const auto type = MediaType::Audio;

    if (muxer_.isInited()) throw std::logic_error(errMsg("output has already been initialized"));
    if (terminated_) throw std::logic_error(errMsg("already terminated"));
    if (managed_types_[type]) throw std::logic_error(errMsg("audio pipeline already initialized"));

    managed_types_[type] = true;

    /* Init decoder */
    decoders_[type] = Decoder(demuxer.getStreamParams(type));

    auto dec_ctx = decoders_[type].getContext();

#ifdef FFMPEG_5
    const AVChannelLayout *channel_layout = &dec_ctx->ch_layout;
#else
    const uint64_t channel_layout =
        dec_ctx->channel_layout ? dec_ctx->channel_layout : av_get_default_channel_layout(dec_ctx->channels);
#endif

    /* Init encoder */
    encoders_[type] = Encoder(codec_id, dec_ctx->sample_rate, channel_layout, muxer_.getGlobalHeaderFlags(),
                              std::map<std::string, std::string>());

    /* Init converter */
    converters_[type] =
        Converter(decoders_[type].getContext(), encoders_[type].getContext(), demuxer.getStreamTimeBase(type));

    muxer_.addStream(encoders_[type].getContext());

    if (async_) startProcessor(type);
}

void av::Pipeline::initOutput() {
    if (muxer_.isInited()) throw std::logic_error(errMsg("output has already been initialized"));
    if (terminated_) throw std::logic_error(errMsg("already terminated"));
    muxer_.initFile();
}

void av::Pipeline::processPacket(const AVPacket *packet, const MediaType type) {
    assert(isMediaTypeValid(type));
    assert(managed_types_[type]);

    Decoder &decoder = decoders_[type];
    Converter &converter = converters_[type];

    bool decoder_received = false;
    while (!decoder_received) {
        decoder_received = decoder.sendPacket(packet);

        while (true) {
            auto frame = decoder.getFrame();
            if (!frame) break;
            converter.sendFrame(std::move(frame));

            while (true) {
                auto converted_frame = converter.getFrame();
                if (!converted_frame) break;
                processConvertedFrame(converted_frame.get(), type);
            }
        }
    }
}

void av::Pipeline::processConvertedFrame(const AVFrame *frame, const MediaType type) {
    assert(isMediaTypeValid(type));
    assert(managed_types_[type]);

    Encoder &encoder = encoders_[type];

    bool encoder_received = false;
    while (!encoder_received) {
        encoder_received = encoder.sendFrame(frame);

        while (true) {
            auto packet = encoder.getPacket();
            if (!packet) break;
            const std::lock_guard lg(muxer_m_);
            muxer_.writePacket(std::move(packet), type);
        }
    }
}

void av::Pipeline::feed(PacketUPtr packet, const MediaType packet_type) {
    if (!packet) throw std::invalid_argument(errMsg("received packet is null"));
    if (!isMediaTypeValid(packet_type)) throw std::invalid_argument(errMsg("received media type is invalid"));
    if (!muxer_.isInited()) throw std::logic_error(errMsg("the output file hasn't been initialized yet"));
    if (terminated_) throw std::logic_error(errMsg("has been terminated"));
    if (!managed_types_[packet_type])
        throw std::logic_error(errMsg("received media type is not handled by the pipeline"));

    if (async_) {
        const std::lock_guard lg(processors_m_);
        checkExceptions();
        if (!packets_[packet_type]) {  // if previous packet has been fully processed
            packets_[packet_type] = std::move(packet);
            packets_cv_[packet_type].notify_all();
        }
    } else {
        processPacket(packet.get(), packet_type);
    }
}

void av::Pipeline::terminate() {
    if (!muxer_.isInited()) throw std::logic_error(errMsg("the output file hasn't been initialized yet"));
    if (terminated_) throw std::logic_error(errMsg("already terminated"));

    if (async_) {
        stopProcessors();  // sets terminated_ = true
        checkExceptions();
    } else {
        terminated_ = true;
    }

    /* flush the pipelines */
    for (auto type : validMediaTypes) {
        if (managed_types_[type]) {
            processPacket(nullptr, type);
            processConvertedFrame(nullptr, type);
        }
    }

    muxer_.writePacket(nullptr, MediaType::None);
    muxer_.finalizeFile();
}

void av::Pipeline::printInfo() const {
    muxer_.printInfo();
    for (auto type : validMediaTypes) {
        if (managed_types_[type]) {
            std::cout << "Decoder " << type << ": " << decoders_[type].getName() << std::endl;
            std::cout << "Encoder " << type << ": " << encoders_[type].getName() << std::endl;
        }
    }
}
