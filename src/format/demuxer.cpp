#include "demuxer.h"

#include <iostream>
#include <stdexcept>

#define VERBOSE 0  // TO-DO: improve

namespace {
std::string errMsg(const std::string &msg) { return ("Demuxer: " + msg); }
}  // namespace

namespace av {
void swap(Demuxer &lhs, Demuxer &rhs) {
    std::swap(lhs.fmt_ctx_, rhs.fmt_ctx_);
    std::swap(lhs.fmt_, rhs.fmt_);
    std::swap(lhs.device_name_, rhs.device_name_);
    std::swap(lhs.options_, rhs.options_);
    for (auto type : validMediaTypes) std::swap(lhs.streams_[type], rhs.streams_[type]);
    std::swap(lhs.packet_, rhs.packet_);
}
}  // namespace av

av::Demuxer::Demuxer(const std::string &fmt_name, std::string device_name, std::map<std::string, std::string> options)
    : fmt_(av_find_input_format(fmt_name.c_str())), device_name_(std::move(device_name)), options_(std::move(options)) {
    if (!fmt_) throw std::logic_error(errMsg("cannot find the input format '" + fmt_name + "'"));
}

av::Demuxer::Demuxer(Demuxer &&other) noexcept { swap(*this, other); }

av::Demuxer &av::Demuxer::operator=(Demuxer other) {
    swap(*this, other);
    return *this;
}

void av::Demuxer::openInput(const bool listing_devices) {
    if (fmt_ctx_) throw std::logic_error(errMsg("failed to open input device (input is already open)"));
    if (!fmt_) throw std::logic_error(errMsg("input format is not set"));  // if the Demuxer was default-constructed

    {
        AVFormatContext *fmt_ctx = nullptr;
        DictionaryUPtr dict = map2dict(options_);
        AVDictionary *dict_raw = dict.release();
        const int ret = avformat_open_input(&fmt_ctx, device_name_.c_str(), fmt_, dict_raw ? &dict_raw : nullptr);
        dict = DictionaryUPtr(dict_raw);
        fmt_ctx_ = InFormatContextUPtr(fmt_ctx);
        /* If we're only listing the available devices, return without any check */
        if (listing_devices) return;
        /* Otherwise, check the outcome */
        if (ret) throw std::runtime_error(errMsg("failed to open the input device '" + device_name_ + "'"));
#if VERBOSE
        auto map = dict2map(dict.get());
        for (const auto &[key, val] : map) {
            std::cerr << "Encoder: couldn't find any '" << key << "' option" << std::endl;
        }
#endif
    }

    if (avformat_find_stream_info(fmt_ctx_.get(), nullptr) < 0)
        throw std::runtime_error(errMsg("failed to find the input streams informations"));

    for (int i = 0; i < fmt_ctx_->nb_streams; i++) {
        const AVStream *stream = fmt_ctx_->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            streams_[MediaType::Video] = stream;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streams_[MediaType::Audio] = stream;
        }
    }
}

void av::Demuxer::closeInput() {
    if (!fmt_ctx_) throw std::logic_error(errMsg("failed to close input (input is not open)"));
    fmt_ctx_.reset();
    streams_[MediaType::Video] = nullptr;
    streams_[MediaType::Audio] = nullptr;
}

void av::Demuxer::flush() {
    if (!fmt_ctx_) throw std::logic_error(errMsg("failed to flush (input is not open)"));
    if (fmt_ctx_->pb) avio_flush(fmt_ctx_->pb);
    if (avformat_flush(fmt_ctx_.get()) < 0) throw std::runtime_error(errMsg("failed to flush internal data"));
}

bool av::Demuxer::isInputOpen() const { return (fmt_ctx_ != nullptr); }

const AVCodecParameters *av::Demuxer::getStreamParams(const MediaType stream_type) const {
    if (!fmt_ctx_) throw std::logic_error(errMsg("failed to acess stream (input is not open)"));
    if (!isMediaTypeValid(stream_type)) throw std::logic_error(errMsg("invalid stream_type received"));
    if (!streams_[stream_type]) throw std::logic_error(errMsg("specified stream not present"));
    return streams_[stream_type]->codecpar;
}

AVRational av::Demuxer::getStreamTimeBase(const MediaType stream_type) const {
    if (!fmt_ctx_) throw std::logic_error(errMsg("failed to acess stream (input is not open)"));
    if (!isMediaTypeValid(stream_type)) throw std::logic_error(errMsg("invalid stream_type received"));
    if (!streams_[stream_type]) throw std::logic_error(errMsg("specified stream not present"));
    return streams_[stream_type]->time_base;
}

std::pair<av::PacketUPtr, av::MediaType> av::Demuxer::readPacket() {
    if (!fmt_ctx_) throw std::logic_error(errMsg("failed to read packet (input is not open)"));

    if (!packet_) {
        packet_ = PacketUPtr(av_packet_alloc());
        if (!packet_) throw std::runtime_error(errMsg("failed to allocate internal packet"));
    }

    auto packet_type = MediaType::None;

    const int ret = av_read_frame(fmt_ctx_.get(), packet_.get());
    if (ret == AVERROR(EAGAIN)) return std::make_pair(nullptr, packet_type);
    if (ret < 0) throw std::runtime_error(errMsg("failed to read a packet"));

    for (auto type : validMediaTypes) {
        if (streams_[type] && packet_->stream_index == streams_[type]->index) {
            packet_type = type;
            break;
        }
    }
    if (packet_type == MediaType::None) throw std::runtime_error(errMsg("unknown packet stream index"));

    return std::make_pair(std::move(packet_), packet_type);
}

void av::Demuxer::printInfo(const int index) const {
    if (!fmt_ctx_) throw std::logic_error(errMsg("failed to print info (input is not open)"));
    av_dump_format(fmt_ctx_.get(), index, device_name_.c_str(), 0);
}