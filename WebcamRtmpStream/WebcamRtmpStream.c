// WebcamRtmpStream.cpp

#define _CRT_SECURE_NO_WARNINGS
#include "WebcamRtmpStream.h"

int main(int argc, char* argv[])
{
    if (argc != 8)
    {
        fprintf(stderr, "Usage: %s [video_device] [audio_device] [output_path] [output_format] [width] [height] [fps]\n", argv[0]);
        return 1;
    }

    const char* device = argv[1];
    const char* adevice = argv[2];
    const char* output_path = argv[3];
    const char* output_format = argv[4];
    int width = atoi(argv[5]);
    int height = atoi(argv[6]);
    int fps = atoi(argv[7]);

    end_stream = false;
    signal(SIGINT, handle_signal);
    stream_ctx_t* stream_ctx = malloc(sizeof(stream_ctx_t));

    if (init(stream_ctx, device, adevice, output_path, output_format, width, height, fps))
    {
        fprintf(stderr, "Error initializing, exiting now...\n");
        //clean_up(stream_ctx);
        return 1;
    }

    fprintf(stdout, "Video and audio initialized, starting streaming...\n");
    stream(stream_ctx);
    clean_up(stream_ctx);

    return 0;
}

void handle_signal(int signal)
{
    fprintf(stderr, "Caught SIGINT, exiting now...\n");
    end_stream = true;
}

void clean_up(stream_ctx_t* stream_ctx)
{
    av_write_trailer(stream_ctx->ofmt_ctx);
    avio_close(stream_ctx->ofmt_ctx->pb);
    avformat_free_context(stream_ctx->ofmt_ctx);
    avio_close(stream_ctx->ifmt_ctx->pb);
    avio_close(stream_ctx->ifmt_ctx_a->pb);
    avformat_free_context(stream_ctx->ifmt_ctx);
    avformat_free_context(stream_ctx->ifmt_ctx_a);
    free(stream_ctx->output_path);
    free(stream_ctx->output_format);
    free(stream_ctx->device_index);
    free(stream_ctx->adevice_index);
    free(stream_ctx->ifmt);
    free(stream_ctx->ifmt_a);
    free(stream_ctx->ifmt_ctx);
    free(stream_ctx->ifmt_ctx_a);
    free(stream_ctx->ofmt_ctx);
    free(stream_ctx->out_codec);
    free(stream_ctx->out_codec_a);
    free(stream_ctx->out_stream);
    free(stream_ctx->out_stream_a);
    free(stream_ctx->out_codec_ctx);
    free(stream_ctx->out_codec_ctx_a);
    free(stream_ctx->filter_graph);
    free(stream_ctx->buffer_sink_ctx);
    free(stream_ctx->buffer_src_ctx);
    free(stream_ctx);
}

int init(stream_ctx_t* stream_ctx, const char* device_index, const char* adevice_index, const char* output_path, const char* output_format, int width, int height, int fps)
{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avdevice_register_all();
    avformat_network_init();

    stream_ctx->output_path = malloc(strlen(output_path) + 1);
    stream_ctx->output_format = malloc(strlen(output_format) + 1);
    stream_ctx->device_index = malloc(strlen(device_index) + 1);
    stream_ctx->adevice_index = malloc(strlen(adevice_index) + 1);
    stream_ctx->width = width;
    stream_ctx->height = height;
    stream_ctx->fps = fps;
    stream_ctx->ifmt = NULL;
    stream_ctx->ifmt_a = NULL;
    stream_ctx->ifmt_ctx = NULL;
    stream_ctx->ifmt_ctx_a = NULL;
    stream_ctx->ofmt_ctx = NULL;
    stream_ctx->out_codec = NULL;
    stream_ctx->out_codec_a = NULL;
    stream_ctx->out_stream = NULL;
    stream_ctx->out_stream_a = NULL;
    stream_ctx->out_codec_ctx = NULL;
    stream_ctx->out_codec_ctx_a = NULL;
    stream_ctx->filter_graph = NULL;
    stream_ctx->buffer_sink_ctx = NULL;
    stream_ctx->buffer_src_ctx = NULL;

    memcpy(stream_ctx->output_path, output_path, strlen(output_path));
    stream_ctx->output_path[strlen(output_path)] = '\0';
    memcpy(stream_ctx->output_format, output_format, strlen(output_format));
    stream_ctx->output_format[strlen(output_format)] = '\0';
    memcpy(stream_ctx->device_index, device_index, strlen(device_index));
    stream_ctx->device_index[strlen(device_index)] = '\0';
    memcpy(stream_ctx->adevice_index, adevice_index, strlen(adevice_index));
    stream_ctx->adevice_index[strlen(adevice_index)] = '\0';

    if (init_video(stream_ctx)) return 1;
    if (init_audio(stream_ctx)) return 1;
    return 0;
}

int init_video(stream_ctx_t* stream_ctx)
{
    int ret;
    const char* device_family = get_device_family();
    char fps_str[5], width_str[5], height_str[5];
    sprintf(fps_str, "%d", stream_ctx->fps);
    sprintf(width_str, "%d", stream_ctx->width);
    sprintf(height_str, "%d", stream_ctx->height);

    char* tmp = concat_str(width_str, "x");
    char* size = concat_str(tmp, height_str);
    free(tmp);

    AVDictionary* ioptions = NULL;
    av_dict_set(&ioptions, "video_size", size, 0);
    av_dict_set(&ioptions, "framerate", fps_str, 0);
    av_dict_set(&ioptions, "pixel_format", av_get_pix_fmt_name(AV_PIX_FMT_YUYV422), 0);
    av_dict_set(&ioptions, "probesize", "7000000", 0);

    stream_ctx->ifmt = av_find_input_format(device_family);
    if (avformat_open_input(&stream_ctx->ifmt_ctx, stream_ctx->device_index, stream_ctx->ifmt, &ioptions) != 0)
    {
        fprintf(stderr, "cannot initialize video input device!\n");
        return 1;
    }
    av_dict_free(&ioptions);

    if (avformat_find_stream_info(stream_ctx->ifmt_ctx, 0) != 0)
    {
        fprintf(stderr, "cannot find video input info!\n");
        return 1;
    }

    int stream_index = -1;
	stream_index = av_find_best_stream(stream_ctx->ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream_index == -1)
    {
        fprintf(stderr, "cannot find video stream input!\n");
        return 1;
    }
    stream_ctx->stream_index = stream_index;

    stream_ctx->in_codec = avcodec_find_decoder(stream_ctx->ifmt_ctx->streams[stream_index]->codecpar->codec_id);
    if (!stream_ctx->in_codec)
    {
        fprintf(stderr, "cannot find video decoder!\n");
        return 1;
    }

    stream_ctx->in_codec_ctx = avcodec_alloc_context3(stream_ctx->in_codec);
    if (!stream_ctx->in_codec_ctx)
    {
        fprintf(stderr, "cannot allocate video decoder context!\n");
        return 1;
    }

    avcodec_parameters_to_context(stream_ctx->in_codec_ctx, stream_ctx->ifmt_ctx->streams[stream_index]->codecpar);
    if (avcodec_open2(stream_ctx->in_codec_ctx, stream_ctx->in_codec, NULL) != 0)
    {
        fprintf(stderr, "cannot initialize video decoder!\n");
        return 1;
    }

    stream_ctx->out_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!stream_ctx->out_codec)
    {
        fprintf(stderr, "cannot find video encoder!\n");
        return 1;
    }

    stream_ctx->out_codec_ctx = avcodec_alloc_context3(stream_ctx->out_codec);
    if (!stream_ctx->out_codec_ctx)
    {
        fprintf(stderr, "cannot allocate video encoder context!\n");
        return 1;
    }

    const int out_fps = stream_ctx->fps;
    const AVRational dst_fps = {out_fps, 1};
    stream_ctx->out_codec_ctx->codec_tag = 0;
    stream_ctx->out_codec_ctx->codec_id = AV_CODEC_ID_H264;
    stream_ctx->out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	//stream_ctx->out_codec_ctx->bit_rate = 400000;
	stream_ctx->out_codec_ctx->width = stream_ctx->ifmt_ctx->streams[stream_index]->codecpar->width;
	stream_ctx->out_codec_ctx->height = stream_ctx->ifmt_ctx->streams[stream_index]->codecpar->height;
    stream_ctx->out_codec_ctx->framerate = dst_fps;
    stream_ctx->out_codec_ctx->time_base = av_inv_q(dst_fps);
	stream_ctx->out_codec_ctx->gop_size = 12;
	//stream_ctx->out_codec_ctx->max_b_frames = 0;
	stream_ctx->out_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	stream_ctx->out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary* eoptions = NULL;
    av_dict_set(&eoptions, "preset", "superfast", 0);
    av_dict_set(&eoptions, "tune", "zerolatency", 0);
#ifndef _WIN32
    av_dict_set(&eoptions, "profile", "high", 0);
#endif // !_WIN32

    //avcodec_parameters_to_context(stream_ctx->out_codec_ctx, stream_ctx->ifmt_ctx->streams[stream_index]->codecpar);
    if (avcodec_open2(stream_ctx->out_codec_ctx, stream_ctx->out_codec, &eoptions) != 0)
    {
        fprintf(stderr, "cannot initialize video encoder!\n");
        return 1;
    }
    av_dict_free(&eoptions);

    avformat_alloc_output_context2(&stream_ctx->ofmt_ctx, 0, stream_ctx->output_format, stream_ctx->output_path);
    if (!stream_ctx->ofmt_ctx)
    {
        fprintf(stderr, "cannot initialize video output format context!\n");
        return 1;
    }

    stream_ctx->out_stream = avformat_new_stream(stream_ctx->ofmt_ctx, stream_ctx->out_codec);
    if (!stream_ctx->out_stream)
    {
        fprintf(stderr, "cannot initialize video output stream!\n");
        return 1;
    }

    if (avcodec_parameters_from_context(stream_ctx->out_stream->codecpar, stream_ctx->out_codec_ctx) != 0)
    {
        fprintf(stderr, "cannot get output video parameters!\n");
        return 1;
    }

    stream_ctx->out_stream->codecpar->extradata = stream_ctx->out_codec_ctx->extradata;
    stream_ctx->out_stream->codecpar->extradata_size = stream_ctx->out_codec_ctx->extradata_size;

    return 0;
}

int init_audio(stream_ctx_t* stream_ctx)
{
    int ret;
    const char* device_family = get_adevice_family();

    AVDictionary* ioptions = NULL;
    av_dict_set_int(&ioptions, "audio_buffer_size", 20, 0);

    stream_ctx->ifmt_a = av_find_input_format(device_family);
    if (avformat_open_input(&stream_ctx->ifmt_ctx_a, stream_ctx->adevice_index, stream_ctx->ifmt_a, &ioptions) != 0)
    {
        fprintf(stderr, "cannot initialize audio input device!\n");
        return 1;
    }
    av_dict_free(&ioptions);

    if (avformat_find_stream_info(stream_ctx->ifmt_ctx_a, 0) != 0)
    {
        fprintf(stderr, "cannot find audio input info!\n");
        return 1;
    }

    int audio_stream_index = -1;
    for (unsigned int i = 0; i < stream_ctx->ifmt_ctx_a->nb_streams; i++)
	{
		if (stream_ctx->ifmt_ctx_a->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audio_stream_index = i;
			break;
		}
	}
    if (audio_stream_index == -1)
    {
        fprintf(stderr, "cannot find audio stream input!\n");
        return 1;
    }
    stream_ctx->audio_stream_index = audio_stream_index;

    stream_ctx->in_codec_a = avcodec_find_decoder(stream_ctx->ifmt_ctx_a->streams[audio_stream_index]->codecpar->codec_id);
    if (!stream_ctx->in_codec_a)
    {
        fprintf(stderr, "cannot find audio decoder!\n");
        return 1;
    }

    stream_ctx->in_codec_ctx_a = avcodec_alloc_context3(stream_ctx->in_codec_a);
    if (!stream_ctx->in_codec_ctx_a)
    {
        fprintf(stderr, "cannot allocate audio decoder context!\n");
        return 1;
    }

    avcodec_parameters_to_context(stream_ctx->in_codec_ctx_a, stream_ctx->ifmt_ctx_a->streams[audio_stream_index]->codecpar);
    if (avcodec_open2(stream_ctx->in_codec_ctx_a, stream_ctx->in_codec_a, NULL) != 0)
    {
        fprintf(stderr, "cannot initialize audio decoder!\n");
        return 1;
    }

    stream_ctx->out_codec_a = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!stream_ctx->out_codec_a)
    {
        fprintf(stderr, "cannot find audio encoder!\n");
        return 1;
    }

    stream_ctx->out_codec_ctx_a = avcodec_alloc_context3(stream_ctx->out_codec_a);
    if (!stream_ctx->out_codec_ctx_a)
    {
        fprintf(stderr, "cannot allocate audio encoder context!\n");
        return 1;
    }
	stream_ctx->out_codec_ctx_a->codec = stream_ctx->out_codec_a;
	stream_ctx->out_codec_ctx_a->sample_rate = 48000;
	stream_ctx->out_codec_ctx_a->channel_layout = 3;
	stream_ctx->out_codec_ctx_a->channels = 2;
	stream_ctx->out_codec_ctx_a->sample_fmt = AV_SAMPLE_FMT_FLTP;
	stream_ctx->out_codec_ctx_a->codec_tag = 0;
	stream_ctx->out_codec_ctx_a->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //avcodec_parameters_to_context(stream_ctx->out_codec_ctx_a, stream_ctx->ifmt_ctx_a->streams[audio_stream_index]->codecpar);
    if (avcodec_open2(stream_ctx->out_codec_ctx_a, stream_ctx->out_codec_a, NULL) != 0)
    {
        fprintf(stderr, "cannot initialize audio encoder!\n");
        return 1;
    }
    /*
    avformat_alloc_output_context2(&stream_ctx->ofmt_ctx, 0, "adts", stream_ctx->output_path);
    if (!stream_ctx->ofmt_ctx)
    {
        fprintf(stderr, "cannot initialize audio output format context!\n");
        return 1;
    }
    */

    stream_ctx->out_stream_a = avformat_new_stream(stream_ctx->ofmt_ctx, stream_ctx->out_codec_a);
    if (!stream_ctx->out_stream_a)
    {
        fprintf(stderr, "cannot initialize audio output stream!\n");
        return 1;
    }

    if (avcodec_parameters_from_context(stream_ctx->out_stream_a->codecpar, stream_ctx->out_codec_ctx_a) != 0)
    {
        fprintf(stderr, "cannot get audio output parameters!\n");
        return 1;
    }

    if (init_audio_sample(stream_ctx) != 0)
    {
        fprintf(stderr, "cannot init audio sample!\n");
        return 1;
    }
    /*
    ret = avio_open2(&stream_ctx->ofmt_ctx->pb, stream_ctx->output_path, AVIO_FLAG_WRITE, NULL, NULL);
    if (ret != 0)
    {
        fprintf(stderr, "could not open audio RTMP context! error code: %d", ret);
        return 1;
    }

    if (avformat_write_header(stream_ctx->ofmt_ctx, NULL) != 0)
    {
        fprintf(stderr, "could not write header to audio output context!\n");
        avio_close(stream_ctx->ofmt_ctx->pb);
        return 1;
    }
    */
    return 0;
}

int stream(stream_ctx_t* stream_ctx)
{

    int ret = 0;

    //av_stream_set_r_frame_rate(stream_ctx->out_stream, av_make_q(1, stream_ctx->fps));
    ret = avio_open2(&stream_ctx->ofmt_ctx->pb, stream_ctx->output_path, AVIO_FLAG_WRITE, NULL, NULL);
    if (ret != 0)
    {
        fprintf(stderr, "could not open RTMP context! error code: %d", ret);
        return 1;
    }

    if (avformat_write_header(stream_ctx->ofmt_ctx, NULL) != 0)
    {
        fprintf(stderr, "could not write header to audio/video ouput context!\n");
        avio_close(stream_ctx->ofmt_ctx->pb);
        return 1;
    }

    /* Video variables */
    AVPacket* in_packet = av_packet_alloc();
    AVPacket* packet = av_packet_alloc();

    struct SwsContext* sws_ctx = sws_getContext(stream_ctx->in_codec_ctx->width, 
        stream_ctx->in_codec_ctx->height, 
        stream_ctx->in_codec_ctx->pix_fmt, 
        stream_ctx->out_codec_ctx->width, 
        stream_ctx->out_codec_ctx->height, 
        stream_ctx->out_codec_ctx->pix_fmt, 
        SWS_BICUBIC, NULL, NULL, NULL);

	int nbytes = av_image_get_buffer_size(stream_ctx->out_codec_ctx->pix_fmt, stream_ctx->out_codec_ctx->width, stream_ctx->out_codec_ctx->height, 32);
	AVFrame* frame = av_frame_alloc();
    AVFrame* outFrame = av_frame_alloc();
    uint8_t* picture_buf = (uint8_t *)av_malloc(nbytes);
	//unsigned char* picture_buf = (uint8_t*)av_malloc(dst_bufsize);
	av_image_fill_arrays(outFrame->data,
		outFrame->linesize,
		picture_buf,
        stream_ctx->out_codec_ctx->pix_fmt,
        stream_ctx->out_codec_ctx->width,
        stream_ctx->out_codec_ctx->height,
		1);
	outFrame->width = stream_ctx->out_codec_ctx->width;
	outFrame->height = stream_ctx->out_codec_ctx->height;
	outFrame->format = stream_ctx->out_codec_ctx->pix_fmt;

	AVPacket outpkt;
	av_new_packet(&outpkt, nbytes);

	int loop = 0;
	int got_picture = -1;
	int delayedFrame = 0;
	int got_frame = 0;

    /* Audio variables */
    int loop_a = 1;
	int delayedFrame_a = 0;
    int samplebyte = 2;
	int audio_count = 0;
	AVPacket* in_packet_a = av_packet_alloc();
	AVPacket* out_packet_a = av_packet_alloc();

	AVFrame* pSrcAudioFrame = av_frame_alloc();
	int out_packet_a_size = 0;
    fprintf(stdout, "Stream initialized, sending data to RTMP server\n");

    while (!end_stream)
    {
        //video
        {
	        av_new_packet(in_packet, 0);
            if (av_read_frame(stream_ctx->ifmt_ctx, in_packet) >= 0)
            {
                if (packet->stream_index == stream_ctx->stream_index) {

                    frame = av_frame_alloc();
                    if (avcodec_send_packet(stream_ctx->in_codec_ctx, in_packet) != 0)
                    {
                        fprintf(stderr, "error sending packet to input codec context!\n");
                        av_frame_free(&frame);
                        break;
                    }

                    if (avcodec_receive_frame(stream_ctx->in_codec_ctx, frame) != 0)
                    {
                        fprintf(stderr, "error receiving frame from input codec context!\n");
                        av_frame_free(&frame);
                        break;
                    }

                    av_packet_free(&in_packet);
                    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, stream_ctx->in_codec_ctx->height, outFrame->data, outFrame->linesize);
                    av_frame_free(&frame);
                    outFrame->pts = loop;
                    loop++;

                    ret = avcodec_send_frame(stream_ctx->out_codec_ctx, outFrame);
                    if (ret == 0)
                    {
                        ret = avcodec_receive_packet(stream_ctx->out_codec_ctx, &outpkt);

                        if (0 == ret)
                        {
                            outpkt.stream_index = stream_ctx->out_stream->index;
                            /*
                            AVRational itime = stream_ctx->ifmt_ctx->streams[packet.stream_index]->time_base;
                            AVRational otime = stream_ctx->ofmt_ctx->streams[packet.stream_index]->time_base;

                            outpkt.pts = av_rescale_q_rnd(packet.pts, itime, otime, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                            outpkt.dts = av_rescale_q_rnd(packet.dts, itime, otime, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                            outpkt.duration = av_rescale_q_rnd(packet.duration, itime, otime, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                            //outpkt.pts = av_rescale_q(packet.pts, itime, otime);
                            //outpkt.dts = av_rescale_q(packet.dts, itime, otime);
                            //outpkt.duration = av_rescale_q(packet.duration, itime, otime);
                            */
                            outpkt.pts = av_rescale_q(outpkt.pts, stream_ctx->out_codec_ctx->time_base, stream_ctx->out_stream->time_base);
                            outpkt.dts = av_rescale_q(outpkt.dts, stream_ctx->out_codec_ctx->time_base, stream_ctx->out_stream->time_base);
                            outpkt.pos = -1;

                            ret = av_interleaved_write_frame(stream_ctx->ofmt_ctx, &outpkt);
                        }
                        else {
                            delayedFrame++;
                        }
                    }
                }
            }
        }
        // audio
        {
	        av_new_packet(in_packet_a, 0);
            if (av_read_frame(stream_ctx->ifmt_ctx_a, in_packet_a) >= 0)
            {
                loop_a++;
                if (0 >= in_packet_a->size)
                {
                    continue;
                }

                AVFrame* filter_frame = decode_audio(in_packet_a, pSrcAudioFrame, stream_ctx->in_codec_ctx_a, stream_ctx->buffer_sink_ctx, stream_ctx->buffer_src_ctx);

                if (filter_frame != NULL)
                {
                    //avcodec_encode_audio2(stream_ctx->out_codec_ctx_a, &out_packet, filter_frame, &got_frame);
                    ret = avcodec_send_frame(stream_ctx->out_codec_ctx_a, filter_frame);
                    if (ret < 0)
                    {
                        av_log(NULL, AV_LOG_ERROR, "avcodec_send_frame error.\n");
                        break;
                    }

                    ret = avcodec_receive_packet(stream_ctx->out_codec_ctx_a, out_packet_a);
                    if (ret == 0)
                    {
                        out_packet_a->stream_index = stream_ctx->out_stream_a->index;
                        /*
                        AVRational itime = stream_ctx->ifmt_ctx_a->streams[in_packet_a.stream_index]->time_base;
                        AVRational otime = stream_ctx->ofmt_ctx->streams[in_packet_a.stream_index]->time_base;

                        out_packet_a.pts = av_rescale_q_rnd(in_packet_a.pts, itime, otime, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                        out_packet_a.dts = av_rescale_q_rnd(in_packet_a.dts, itime, otime, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                        out_packet_a.duration = av_rescale_q_rnd(in_packet_a.duration, itime, otime, (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                        */
                        out_packet_a->pts = av_rescale_q(out_packet_a->pts, stream_ctx->out_codec_ctx_a->time_base, stream_ctx->out_stream_a->time_base);
                        out_packet_a->dts = av_rescale_q(out_packet_a->dts, stream_ctx->out_codec_ctx_a->time_base, stream_ctx->out_stream_a->time_base);
                        out_packet_a->pos = -1;

                        av_interleaved_write_frame(stream_ctx->ofmt_ctx, out_packet_a);
                        av_packet_free(&out_packet_a);
                    }
                }

            }
        }
        av_packet_free(&packet);
        av_packet_free(&in_packet_a);
    }
}

AVFrame* decode_audio(AVPacket* in_packet, 
	AVFrame* src_audio_frame, 
	AVCodecContext* decode_codectx, 
	AVFilterContext* buffer_sink_ctx, 
	AVFilterContext* buffer_src_ctx)
{
	int ret, gotFrame;
	AVFrame* filtFrame = NULL;

	ret = avcodec_send_packet(decode_codectx, in_packet);
	if (ret != 0)
	{
        fprintf(stdout, "cannot send audio packet to the decoder\n");
        fprintf(stdout, "code %i", ret);
		return NULL;
	}

	while (ret >= 0)
	{
		ret = avcodec_receive_frame(decode_codectx, src_audio_frame);
		if (ret < 0)
		{
			break;
		}

		if (av_buffersrc_add_frame_flags(buffer_src_ctx, src_audio_frame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
			av_log(NULL, AV_LOG_ERROR, "buffe src add frame error!\n");
			return NULL;
		}

		filtFrame = av_frame_alloc();
		ret = av_buffersink_get_frame_flags(buffer_sink_ctx, filtFrame, AV_BUFFERSINK_FLAG_NO_REQUEST);
		if (ret < 0)
		{
			av_frame_free(&filtFrame);
			return NULL;
		}
		return filtFrame;
	}

	return NULL;
}



int init_audio_sample(stream_ctx_t* stream_ctx)
{
	char args[512] = {'\0'};
	int ret;
	const AVFilter* abuffersrc = avfilter_get_by_name("abuffer");
	const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();

	AVCodecParameters* audioDecoderContext = stream_ctx->ifmt_ctx_a->streams[stream_ctx->audio_stream_index]->codecpar;
	if (!audioDecoderContext->channel_layout)
		audioDecoderContext->channel_layout = av_get_default_channel_layout(audioDecoderContext->channels);

	static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
	static const int64_t out_channel_layouts[] = { AV_CH_LAYOUT_STEREO, -1 };
	static const int out_sample_rates[] = { 48000, -1 };

	AVRational time_base = stream_ctx->ifmt_ctx_a->streams[stream_ctx->audio_stream_index]->time_base;
	stream_ctx->filter_graph = avfilter_graph_alloc();
	stream_ctx->filter_graph->nb_threads = 1;

	sprintf(args, "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%lX",
		time_base.num, time_base.den, audioDecoderContext->sample_rate,
		av_get_sample_fmt_name(audioDecoderContext->format), audioDecoderContext->channel_layout);

	ret = avfilter_graph_create_filter(&stream_ctx->buffer_src_ctx, abuffersrc, "in",
		args, NULL, stream_ctx->filter_graph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
		return ret;
	}

	/* buffer audio sink: to terminate the filter chain. */
	ret = avfilter_graph_create_filter(&stream_ctx->buffer_sink_ctx, abuffersink, "out",
		NULL, NULL, stream_ctx->filter_graph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
		return ret;
	}

	ret = av_opt_set_int_list(stream_ctx->buffer_sink_ctx, "sample_fmts", out_sample_fmts, -1,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
		return ret;
	}

	ret = av_opt_set_int_list(stream_ctx->buffer_sink_ctx, "channel_layouts", out_channel_layouts, -1,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
		return ret;
	}

	ret = av_opt_set_int_list(stream_ctx->buffer_sink_ctx, "sample_rates", out_sample_rates, -1,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
		return ret;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = stream_ctx->buffer_src_ctx;;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = stream_ctx->buffer_sink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if ((ret = avfilter_graph_parse_ptr(stream_ctx->filter_graph, "anull",
		&inputs, &outputs, NULL)) < 0)
		return ret;

	if ((ret = avfilter_graph_config(stream_ctx->filter_graph, NULL)) < 0)
		return ret;

	av_buffersink_set_frame_size(stream_ctx->buffer_sink_ctx, 1024);
	return 0;
}

double av_r2d(AVRational r)
{
	if (r.num == 0 || r.den == 0) return 0.0;
	else return (double)r.num / r.den;
}

void av_free_context(AVFormatContext* ifmt_ctx, AVFormatContext* ofmt_ctx)
{
	if (NULL != ifmt_ctx)
	{
		avformat_close_input(&ifmt_ctx);
	}

	if (NULL != ofmt_ctx)
	{
		avformat_free_context(ofmt_ctx);
	}
}

char* concat_str(const char* s1, const char* s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char* result = malloc(len1 + len2 + 1);
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

const char* get_device_family()
{
#ifdef _WIN32
    const char* device_family = "dshow";
#elif __APPLE__
    const char* device_family = "avfoundation";
#elif __linux__
    const char* device_family = "v4l2";
#endif

    return device_family;
}

const char* get_adevice_family()
{
#ifdef _WIN32
    const char* adevice_family = "dshow";
#elif __APPLE__
    const char* adevice_family = "avfoundation";
#elif __linux__
    const char* adevice_family = "alsa";
#endif

    return adevice_family;
}
