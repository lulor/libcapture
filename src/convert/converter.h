#pragma once

#include "common/common.h"

class Converter {
    av::FilterGraphUPtr filter_graph_;
    AVFilterContext *buffersrc_ctx_ = nullptr;
    AVFilterContext *buffersink_ctx_ = nullptr;
    av::FrameUPtr frame_;

protected:
    /**
     * Create a new converter
     */
    Converter();

    Converter(Converter &&other);

    Converter &operator=(Converter &&other);

    /**
     * Initialize the converter (note that the converter won't be usable without initialization)
     * @param src_filter_name   the name of the src filter to use
     * @param sink_filter_name  the name of the sink filter to use
     * @param src_filter_args   the arguments to use for the src filter
     * @param filter_spec       the specifications of the conversion filter
     */
    void init(const std::string &src_filter_name, const std::string &sink_filter_name,
              const std::string &src_filter_args, const std::string &filter_spec);

    /**
     * Throw an std::runtime_error exception
     * @param msg the message to throw
     */
    void throwError(const std::string &msg) const;

public:
    Converter(const Converter &) = delete;

    ~Converter() = default;

    Converter &operator=(const Converter &) = delete;

    /**
     * Send a frame to convert
     */
    void sendFrame(av::FrameUPtr frame);

    /**
     * Get a converted frame
     * @return a new converted frame if it was possible to build it, nullptr otherwise
     */
    av::FrameUPtr getFrame();
};