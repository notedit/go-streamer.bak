#ifndef _STREAMER_H
#define _STREAMER_H


#include <libavformat/avformat.h>
#include <stdbool.h>
#include <stdint.h>


struct SInput {
    AVFormatContext *format_ctx;
    int video_stream_index;
};


struct SOutput {
    AVFormatContext *format_ctx;
    int64_t  last_dts;
};


void s_setup(void);

struct SInput *
s_open_input(const char * const, 
        const char * const, const bool);


void s_destroy_input(struct SInput * const);


struct SOutput * 
s_open_output(const char * const,
        const char * const, const struct SInput * const,
        const bool);


void 
s_destroy_output(struct SOutput * const);


int 
s_read_packet(const struct SInput *, AVPacket * const,
        const bool);

int 
s_write_packet(const struct SInput * const,
        struct SOutput * const, AVPacket * const, const bool);

#endif















