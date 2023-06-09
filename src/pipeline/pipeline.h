#pragma once

#include <array>
#include <condition_variable>
#include <thread>
#include <vector>

#include "common/common.h"
#include "format/demuxer.h"
#include "format/muxer.h"
#include "process/converter.h"
#include "process/decoder.h"
#include "process/encoder.h"
#include "video_parameters.h"

namespace av {

class Pipeline {
    const bool async_;

    std::array<bool, MediaType::NUM_TYPES> managed_types_{};
    std::array<Decoder, MediaType::NUM_TYPES> decoders_;
    std::array<Encoder, MediaType::NUM_TYPES> encoders_;
    std::array<Converter, MediaType::NUM_TYPES> converters_;
    Muxer muxer_;
    std::mutex muxer_m_;

    bool terminated_{};

    std::mutex processors_m_;
    std::array<std::thread, MediaType::NUM_TYPES> processors_;
    std::array<PacketUPtr, MediaType::NUM_TYPES> packets_;
    std::array<std::condition_variable, MediaType::NUM_TYPES> packets_cv_;
    std::array<std::exception_ptr, MediaType::NUM_TYPES> e_ptrs_;
    void startProcessor(MediaType media_type);
    /* Stop and join the processor threads */
    void stopProcessors();
    /* Check and eventually re-throw the processors exceptions */
    void checkExceptions();

    void processPacket(const AVPacket *packet, MediaType type);
    void processConvertedFrame(const AVFrame *frame, MediaType type);

public:
    /**
     * Create a new Pipeline for processing packets
     * @param output_file   the name of the output file
     * @param async         whether the pipeline should use background threads to handle the processing
     * (recommended when a single demuxer will provide both video and audio packets)
     */
    explicit Pipeline(const std::string &output_file, bool async = false);

    Pipeline(const Pipeline &) = delete;

    ~Pipeline();

    Pipeline &operator=(const Pipeline &) = delete;

    /**
     * Initialize the video processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream of packets
     * @param codec_id      the ID of the codec to use for the output video
     * @param pix_fmt       the pixel format to use for the output video
     * @param video_params  the parameters to use for the output video
     */
    void initVideo(const Demuxer &demuxer, AVCodecID codec_id, AVPixelFormat pix_fmt,
                   const VideoParameters &video_params);

    /**
     * Initialize the audio processing, by creating the corresponding decoder, converter and encoder
     * @param demuxer       the demuxer containing the input stream of packets
     * @param codec_id      the ID of the codec to use for the output audio
     */
    void initAudio(const Demuxer &demuxer, AVCodecID codec_id);

    /**
     * Initialize the output file.
     * WARNING: This function must be called after initializing all the desired processing chains
     * with initVideo() and initAudio()
     */
    void initOutput();

    /**
     * Send the packet to the processing chain corresponding to its type.
     * If 'async' was set to true when building the Pipeline,
     * the background threads will handle the packet processing and this function will
     * return immediately, otherwise the processing will be handled in
     * a synchronous way and this function will return only once it's completed
     * @param packet        the packet to send to che processing chain (if NULL, an exception will be thrown)
     * @param packet_type   the type of the packet to process
     */
    void feed(PacketUPtr packet, MediaType packet_type);

    /**
     * Flush the processing pipelines and close the output file.
     */
    void terminate();

    /**
     * Print the informations about the internal demuxer, decoders and encoders
     */
    void printInfo() const;
};

}  // namespace av
