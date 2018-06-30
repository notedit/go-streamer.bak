
#include <errno.h>
#include <libavdevice/avdevice.h>
#include <libavutil/timestamp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "streamer.h"


void
s_setup(void)
{
    
    av_register_all();

    avdevice_register_all();

    avformat_network_init();
}


struct SInput *
s_open_input(const char * const input_format_name,
        const char * const input_url, const bool verbose)
{

    if (!input_format_name || strlen(input_format_name) == 0 ||
            !input_url || strlen(input_url) == 0) {
        printf("%s\n", strerror(EINVAL));
        return NULL;
    }

    struct SInput * const input = calloc(1, sizeof(struct SInput));

    if (!input) {
        printf("%s\n", strerror(errno));
        return NULL;
    }

    AVInputFormat * const input_format = av_find_input_format(input_format_name);

    if (!input_format) {
        printf("input format not found\n");
        s_destroy_input(input);
        return NULL;
    }

    if (avformat_open_input(&input->format_ctx, input_url, input_format,NULL) != 0) {

        printf("unable to open input\n");
        s_destroy_input(input);
        return NULL;
    }

    if (verbose) {
        av_dump_format(input->format_ctx, 0, input_url, 0);
    }

    input->video_stream_index = -1;

    for (unsigned int i = 0; i < input->format_ctx->nb_streams; i++) {
        AVStream * const in_stream = input->format_ctx->streams[i];

        if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
            continue;
        }

        input->video_stream_index = (int)i;
        break;
    }

    if (input->video_stream_index == -1) {
        printf("no video stream found\n");
        s_destroy_input(input);
        return NULL;
    }

    return input;
}



void
s_destroy_input(struct SInput * const input)
{
    if (!input) {
        return;
    }

    if (input->format_ctx) {
        avformat_close_input(&input->format_ctx);
        avformat_free_context(input->format_ctx);
    }

    free(input);
}


struct SOutput *
s_open_output(const char * const output_format_name,
        const char * const output_url, const struct SInput * const input,
        const bool verbose)
{

    if (!output_format_name || strlen(output_format_name) == 0 ||
            !output_url || strlen(output_url) == 0 || 
            !input) {
        printf("%s\n", strerror(EINVAL));
        return NULL;
    }

    struct SOutput * const output = calloc(1, sizeof(struct SOutput));

    AVOutputFormat * const output_format = av_guess_format(output_format_name,
                    NULL,NULL);

    if (!output_format) {
        printf("output format not found\n");
        s_destroy_output(output);
        return NULL;
    }

    if (avformat_alloc_output_context2(&output->format_ctx, output_format, 
                NULL, NULL) < 0) {
        printf("unable to create output context\n");
        s_destroy_output(output);
        return NULL;
    }

    // copy video stream 
    
    AVStream * const out_stream = avformat_new_stream(output->format_ctx, NULL);

    AVStream * const in_stream = input->format_ctx->streams[input->video_stream_index];

    if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
        printf("unable to create output context\n");
        s_destroy_output(output);
        return NULL;
    }

    if (avio_open(&output->format_ctx->pb, output_url, AVIO_FLAG_WRITE) < 0) {
        printf("unable to open output file \n");
        s_destroy_output(output);
        return NULL;
    }

    AVDictionary * opts = NULL;

    av_dict_set_int(&opts, "flush_packets", 1, 0);

    if (avformat_write_header(output->format_ctx, &opts) < 0) {
        printf("unable to write header\n");
        s_destroy_output(output);
        av_dict_free(&opts);
        return NULL;
    }

    av_dict_free(&opts);

    output->last_dts = AV_NOPTS_VALUE;

    return output;
}


void
s_destroy_output(struct SOutput * const output)
{
    if (!output) {
        return;
    }

    if (output->format_ctx) {
        if (av_write_trailer(output->format_ctx) != 0) {
            printf("unable to write trailer\n");
        }
        if (avio_closep(&output->format_ctx->pb) != 0) {
            printf("avio_closep failed\n");
        }
        avformat_free_context(output->format_ctx);
    }
    free(output);
}


int
s_read_packet(const struct SInput * input, AVPacket * const pkt,
        const bool verbose)
{

    if (!input || !pkt) {
        return -1;
    }

    memset(pkt, 0, sizeof(AVPacket));

    // read frame as packet
    if (av_read_frame(input->format_ctx, pkt) != 0) {
        printf("unable to read frame\n");
        return -1;
    }

    if (pkt->stream_index != input->video_stream_index) {

        av_packet_unref(pkt);
        return 0;
    }

    return 1;
}


int 
s_write_packet(const struct SInput * const input, 
        struct SOutput * const output, AVPacket * const pkt, const bool verbose)
{

    if(!input || !output || !pkt) {
        return -1;
    }

    AVStream * const in_stream = input->format_ctx->streams[pkt->stream_index];

    // we should change the stream index
    
    if (pkt->stream_index != 0) {
        pkt->stream_index = 0;
    }

    AVStream * const out_stream = output->format_ctx->streams[pkt->stream_index];

    if (!out_stream) {
        printf("output stream not found with stream index %d\n", pkt->stream_index);
        return -1;
    }

    bool fix_dts = pkt->dts != AV_NOPTS_VALUE &&
                output->last_dts != AV_NOPTS_VALUE &&
                        pkt->dts <= output->last_dts;


    fix_dts |= pkt->dts == AV_NOPTS_VALUE && output->last_dts != AV_NOPTS_VALUE;



    if (fix_dts) {

        int64_t const next_dts = output->last_dts + 1;

        if (pkt->pts != AV_NOPTS_VALUE && pkt->pts >= pkt->dts) {
            pkt->pts = FFMAX(pkt->pts, next_dts);
        }
        
        if (pkt->pts == AV_NOPTS_VALUE) {
            pkt->pts = next_dts;
        }

        pkt->dts = next_dts;
    }

    if (pkt->pts == AV_NOPTS_VALUE) {
        pkt->pts = 0;
    } else {
        pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base,
                out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    }
    
    pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base,
                        out_stream->time_base);
    pkt->pos = -1;

    output->last_dts = pkt->dts;

    const int write_res = av_write_frame(output->format_ctx, pkt);

    if (write_res != 0) {
        char error_buf[256];
        memset(error_buf, 0, 256);
        av_strerror(write_res, error_buf, 256);
        printf("unable to write frame: %s\n", error_buf);
        return -1;
    }

    return 1;

}
