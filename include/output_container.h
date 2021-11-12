#pragma once

#include <string>

#include "ffmpeg_libs.h"

class OutputContainer {
    AVFormatContext *fmt_ctx_;
    std::string filename_;
    AVStream *video_stream_;
    AVStream *audio_stream_;

public:
    OutputContainer(const std::string &filename);

    ~OutputContainer();

    /**
     * Add a video stream and return a pointer to it
     * The ownership of the stream remains to the Container
     */
    AVStream *addVideoStream();

    /**
     * Add an audio stream and return a pointer to it
     * The ownership of the stream remains to the Container
     */
    AVStream *addAudioStream();

    /**
     * Open the file and write the header
     */
    void writeHeader();

    /**
     * Write a packet to the output file
     * The ownership of the packet remains to the caller
     */
    void writePacket(AVPacket *packet);

    /**
     * Write the trailer and close the file
     */
    void writeTrailer();

    /**
     * Print informations
     */
    void dumpInfo();
};