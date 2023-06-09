#pragma once

#include <array>
#include <mutex>
#include <string>

#include "common/common.h"

namespace av {

class Muxer {
    FormatContextUPtr fmt_ctx_;
    std::string filename_;
    std::array<const AVStream *, MediaType::NUM_TYPES> streams_{};
    std::array<AVRational, MediaType::NUM_TYPES> encoders_time_bases_{};
    bool file_inited_{};
    bool file_finalized_{};

public:
    /**
     * Create a new muxer
     * @param filename the name of the output file
     */
    explicit Muxer(std::string filename);

    Muxer(const Muxer &) = delete;

    ~Muxer();

    Muxer &operator=(const Muxer &) = delete;

    /**
     * Add a stream to the muxer. It's only possible to add a single audio stream and video stream:
     * if this condition is violated, an exception will be thrown.
     * WARNING: This function must be called before opening the file with initFile()
     * @param enc_ctx   the context of the encoder generating the packet stream
     */
    void addStream(const AVCodecContext *enc_ctx);

    /**
     * Open the output file and write the header.
     * WARNING: After calling this function, it won't be possible to add streams to the muxer
     */
    void initFile();

    /**
     * Write the trailer and close the file.
     * WARNING: After calling this function, it won't be possible to send other packets to the muxer
     */
    void finalizeFile();

    /**
     * Whether the file managed by the muxer has been initialized (and hence the muxer cannot be modifed anymore)
     * @return true if the file managed by the muxer has been initialized, false otherwise
     */
    [[nodiscard]] bool isInited() const;

    /**
     * Write a packet to the output file.
     * WARNING: the muxer must be initialized with init() in order to accept packets, otherwise
     * an exception will be thrown
     * @param packet        the packet to write. If nullptr, the output queue will be flushed
     * @param packet_type   the type of the packet (audio or video). If the packet is nullptr,
     * this parameter is irrelevant
     */
    void writePacket(PacketUPtr packet, MediaType packet_type);

    /**
     * Print informations about the streams
     */
    void printInfo() const;

    /**
     * Get the global header flags of the output format
     * @return the global header flags
     */
    [[nodiscard]] int getGlobalHeaderFlags() const;
};

}  // namespace av
