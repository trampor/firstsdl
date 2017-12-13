#include "stdafx.h"
#include "Media.h"

int CMedia::g_audioindex = 1;

CMedia::CMedia(const char* pmediapath) :
filepath(pmediapath),
pinput_fmt_context(NULL),
paudiostream(NULL),
pvideostream(NULL),
img_convert_ctx(NULL),
pvideotexture(NULL),
pinput_audio_codecctx(NULL),
pinput_video_codecctx(NULL),
audio_index(-1),
video_index(-1),
samplesize(2),
bpaused(false),
bsilence(false),
paudiodatabuf(NULL),
g_quit(0),
duration(0),
data_over(0),
audio_over(0),
video_over(0),
cur_audio_pts(0),
audiowritepos(0),
audioreadpos(0),
frame_nums(0),
audio_avg_l(-100), 
audio_avg_r(-100),
start_audio_pts(-1),
start_video_pts(-1),
paudio_packet_lock(NULL),
pvideo_packet_lock(NULL),
paudio_frame_lock(NULL),
pvideo_frame_lock(NULL),
pvideo_texture_lock(NULL),
paudiodecodethread(NULL),
pvideodecodethread(NULL),
preadpacketthread(NULL)
{
	audiodatalength.value = 0;
	audio_packet_num.value = 0;
	video_packet_num.value = 0;
	audio_frame_num.value = 0;
	video_frame_num.value = 0;
	paudiodatabuf = new Uint8[AUDIOBUFSIZE];
}

CMedia::~CMedia()
{
	delete paudiodatabuf;
}

int CMedia::InitMediaObj()
{
	cout << "init media : " << filepath.c_str() << endl;
	int ret = avformat_open_input(&pinput_fmt_context, filepath.c_str(), NULL, NULL);
	if (ret < 0)
	{
		return ret;
	}
	ret = avformat_find_stream_info(pinput_fmt_context, NULL);
	if (ret < 0)
	{
		return ret;
	}
	duration = (pinput_fmt_context->duration + 5000) * 1000 / AV_TIME_BASE;
	cout << "media duration: " << duration << endl;

	AVStream *pstream = NULL;
	for (int i = 0; i < pinput_fmt_context->nb_streams; i++)
	{
		pstream = pinput_fmt_context->streams[i];
		if ((pstream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) && (audio_index == -1))
		{
			paudiostream = pstream;
			audio_index = pstream->index;
		}
		else if (pstream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			pvideostream = pstream;
			video_index = pstream->index;
		}

	}
	if (audio_index != -1)
	{
		AVCodec *paudiocodec = avcodec_find_decoder(paudiostream->codecpar->codec_id);
		if (paudiocodec == NULL)
		{
			return -1;
		}
		cout << "audio format: " << paudiocodec->name << endl;
		pinput_audio_codecctx = avcodec_alloc_context3(paudiocodec);
		if (pinput_audio_codecctx == NULL)
		{
			return -1;
		}

		ret = avcodec_parameters_to_context(pinput_audio_codecctx, paudiostream->codecpar);
		if (ret < 0)
		{
			return ret;
		}

		ret = avcodec_open2(pinput_audio_codecctx, paudiocodec, NULL);
		if (ret < 0)
		{
			return ret;
		}
	}
	if (video_index != -1)
	{
		AVCodec *pvideocodec = avcodec_find_decoder(pvideostream->codecpar->codec_id);
		if (pvideocodec == NULL)
		{
			return -1;
		}
		cout << "video format: " << pvideocodec->name << endl;
		pinput_video_codecctx = avcodec_alloc_context3(pvideocodec);
		if (pinput_video_codecctx == NULL)
		{
			return -1;
		}

		ret = avcodec_parameters_to_context(pinput_video_codecctx, pvideostream->codecpar);
		if (ret < 0)
		{
			return ret;
		}

		ret = avcodec_open2(pinput_video_codecctx, pvideocodec, NULL);
		if (ret < 0)
		{
			return ret;
		}
	}

	if (audio_index == -1 && video_index == -1)
		return -2;

	audio_over = 1;
	video_over = 1;

	return 0;
}

int  CMedia::InitSDLObj(SDL_Renderer *prender, int width, int height)
{
	int ret;
	if (audio_index != -1)
	{
		SDL_AudioSpec wanted_spec;
		wanted_spec.freq = paudiostream->codecpar->sample_rate;
		if (paudiostream->codecpar->format == AV_SAMPLE_FMT_U8P)
		{
			samplesize = 4;
			wanted_spec.format = AUDIO_S8;
		}
		else if (paudiostream->codecpar->format == AV_SAMPLE_FMT_S16P)
		{
			wanted_spec.format = AUDIO_S16SYS;
		}
		else if (paudiostream->codecpar->format == AV_SAMPLE_FMT_S32P)
		{
			samplesize = 4;
			wanted_spec.format = AUDIO_S32SYS;
		}
		else if (paudiostream->codecpar->format == AV_SAMPLE_FMT_FLTP)
		{
			samplesize = 4;
			wanted_spec.format = AUDIO_F32SYS;
		}
		wanted_spec.channels = paudiostream->codecpar->channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = paudiostream->codecpar->frame_size;
		wanted_spec.callback = fill_audio;
		wanted_spec.userdata = this;
		if (g_audioindex == 1)
		{
			if ((ret = SDL_OpenAudio(&wanted_spec, NULL)) < 0){
				printf("can't open audio,error=%s.\n", SDL_GetError());
				return -5;
			}
			aid = 1;
			g_audioindex++;
		}
		else
		{
			aid = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, NULL, 1);
			if (aid == 0)
			{
				printf("can't open audio,error=%s.\n", SDL_GetError());
				return -5;
			}
		}

		audio_over = 0;
		cout << "audio timebase : " << paudiostream->time_base.num << " " << paudiostream->time_base.den << endl;
		cout << "audio samplerate: " << (int)wanted_spec.freq << endl;
		cout << "audio channels: " << (int)wanted_spec.channels << endl;
		cout << "audio samplesize: " << (int)samplesize << endl;
		cout << "audio samples: " << (int)wanted_spec.samples << endl;

		//创建视频相关的线程和信号量
		paudio_packet_lock = SDL_CreateSemaphore(1);
		paudio_frame_lock = SDL_CreateSemaphore(1);
		audio_packet_num.value = 0;
		audio_frame_num.value = 0;
		paudiodecodethread = SDL_CreateThread(AudioDecodeThread, NULL, this);
	}
	if (video_index != -1)
	{
		cout << "video timebase : " << pvideostream->time_base.num << " " << pvideostream->time_base.den << endl;
		video_over = 0;
		cout << "video framerate: " << (int)pvideostream->avg_frame_rate.num << "/" << (int)pvideostream->avg_frame_rate.den << "=" << (int)pvideostream->avg_frame_rate.num / pvideostream->avg_frame_rate.den << endl;
		if (pvideostream->codecpar->format == AV_PIX_FMT_YUV420P)
			cout << "video pixformat: " << "AV_PIX_FMT_YUV420P" << endl;
		else if (pvideostream->codecpar->format == AV_PIX_FMT_YUYV422)
			cout << "video pixformat: " << "AV_PIX_FMT_YUYV422";
		if (pvideostream->codecpar->format == AV_PIX_FMT_RGB24)
			cout << "video pixformat: " << "AV_PIX_FMT_RGB24";
		cout << "video width: " << (int)pvideostream->codecpar->width << endl;
		cout << "video height: " << (int)pvideostream->codecpar->height << endl;

		//缩放叠加部分组件创建 
		img_convert_ctx = sws_alloc_context();
		//Show AVOption  
		//av_opt_show2(img_convert_ctx, stdout, AV_OPT_FLAG_VIDEO_PARAM, 0);
		//Set Value  
		av_opt_set_int(img_convert_ctx, "sws_flags", SWS_FAST_BILINEAR | SWS_PRINT_INFO, 0);
		av_opt_set_int(img_convert_ctx, "srcw", pvideostream->codecpar->width, 0);
		av_opt_set_int(img_convert_ctx, "srch", pvideostream->codecpar->height, 0);
		av_opt_set_int(img_convert_ctx, "src_format", pvideostream->codecpar->format, 0);
		//'0' for MPEG (Y:0-235);'1' for JPEG (Y:0-255)  
		av_opt_set_int(img_convert_ctx, "src_range", 1, 0);
		int dstw = (width % 16) ? (width + 15) / 16 * 16 : width;
		av_opt_set_int(img_convert_ctx, "dstw", dstw, 0);
		av_opt_set_int(img_convert_ctx, "dsth", height, 0);
		av_opt_set_int(img_convert_ctx, "dst_format", pvideostream->codecpar->format, 0);
		av_opt_set_int(img_convert_ctx, "dst_range", 1, 0);
		sws_init_context(img_convert_ctx, NULL, NULL);
		ret = av_image_alloc(dst_data, dst_linesize, dstw, height, (AVPixelFormat)pvideostream->codecpar->format, 1);
		if (ret< 0) {
			printf("Could not allocate destination image\n");
			return -1;
		}

		//视频纹理
		pvideotexture = SDL_CreateTexture(prender, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, dstw, height);
		SDL_SetTextureBlendMode(pvideotexture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(pvideotexture, 200);

		//创建视频相关的线程和信号量
		pvideo_packet_lock = SDL_CreateSemaphore(1);
		pvideo_frame_lock = SDL_CreateSemaphore(1);
		pvideo_texture_lock = SDL_CreateSemaphore(1);
		video_packet_num.value = 0;
		video_frame_num.value = 0;
		pvideodecodethread = SDL_CreateThread(VideoDecodeThread, NULL, this);
	}
	preadpacketthread = SDL_CreateThread(ReadPacketThread, NULL, this);

	return 0;
}


void  CMedia::fill_audio(void *udata, Uint8 *stream, int len)
{
	//SDL 2.0  
	SDL_memset(stream, 0, len);
	if (((CMedia*)udata)->bpaused)
		return;
	//cout << "audio id: " << ((CMedia*)udata)->aid << endl;

	if ((((CMedia*)udata)->audiodatalength.value == 0) && (((CMedia*)udata)->audio_frame_num.value == 0))
	{
		if (((CMedia*)udata)->data_over == 1 && ((CMedia*)udata)->audio_packet_num.value == 0)
		{
			((CMedia*)udata)->audio_over = 1;
			((CMedia*)udata)->cur_audio_pts = 0;
		}
		return;
	}

	short llevel, rlevel;
	float lflevel, rflevel;
	while ((((CMedia*)udata)->audiodatalength.value < len) && (((CMedia*)udata)->audio_frame_num.value > 0)) //遗留数据不够用，有新帧数据存在
	{
		SDL_SemWait(((CMedia*)udata)->paudio_frame_lock);
		AVFrame *pinput_audio_frame = *((CMedia*)udata)->audio_frame_list.begin();
		((CMedia*)udata)->audio_frame_list.pop_front();
		SDL_SemPost(((CMedia*)udata)->paudio_frame_lock);
		SDL_AtomicAdd(&((CMedia*)udata)->audio_frame_num, -1);

		//fwrite(pinput_audio_frame->data[3], 1, pinput_audio_frame->linesize[0], fp_pcm_2);
		int data_size;
		if (pinput_audio_frame->channels > 1)
			data_size = av_samples_get_buffer_size(NULL, pinput_audio_frame->channels, pinput_audio_frame->nb_samples, (AVSampleFormat)pinput_audio_frame->format, 1);
		else
			data_size = pinput_audio_frame->linesize[0];

		//cout << "audio frame size " << pinput_audio_frame->linesize[0] << endl;
		//cout << "audio frame pts " << pinput_audio_frame->pts << endl;
		((CMedia*)udata)->cur_audio_pts = pinput_audio_frame->pts;
		if (((CMedia*)udata)->start_audio_pts == -1)
			((CMedia*)udata)->start_audio_pts = pinput_audio_frame->pts;

		if (pinput_audio_frame->channels > 1)
		{
			double totall = 0, totalr = 0;
			for (int i = 0; i < pinput_audio_frame->nb_samples; i++)
			{
				for (int j = 0; j < pinput_audio_frame->channels; j++)
				{
					if (j == 0) //左声道
					{
						if (((CMedia*)udata)->samplesize == 2)
						{
							memcpy(&llevel, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize, ((CMedia*)udata)->samplesize);
							totall += abs(llevel);
						}
						else if (((CMedia*)udata)->samplesize == 4)
						{
							memcpy(&lflevel, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize,((CMedia*)udata)->samplesize);
							totall += abs(lflevel*32768);
						}
					}
					else if(j == 1) //右声道
					{
						if (((CMedia*)udata)->samplesize == 2)
						{
							memcpy(&rlevel, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize, ((CMedia*)udata)->samplesize);
							totalr += abs(rlevel);
						}
						else if (((CMedia*)udata)->samplesize == 4)
						{
							memcpy(&rflevel, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize, ((CMedia*)udata)->samplesize);
							totalr += abs(rflevel * 32768);
						}
					}

					if (((CMedia*)udata)->samplesize <= AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos)
					{
						memcpy(((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audiowritepos, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize, ((CMedia*)udata)->samplesize);
						((CMedia*)udata)->audiowritepos += ((CMedia*)udata)->samplesize;
						if (((CMedia*)udata)->audiowritepos == AUDIOBUFSIZE)
							((CMedia*)udata)->audiowritepos = 0;

					}
					else
					{
						memcpy(((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audiowritepos, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize, AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos);
						memcpy(((CMedia*)udata)->paudiodatabuf, pinput_audio_frame->data[j] + i*((CMedia*)udata)->samplesize + AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos, ((CMedia*)udata)->samplesize - (AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos));

						((CMedia*)udata)->audiowritepos = ((CMedia*)udata)->samplesize - (AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos);
					}
					//fwrite(pin_video_frame->data[j] + i*samplesize, 1, samplesize, fp_pcm);
				}
			}
			//((CMedia*)udata)->audio_avg_l = totall *2/ pinput_audio_frame->nb_samples;
			if (totall * 2 / pinput_audio_frame->nb_samples == 0)
				((CMedia*)udata)->audio_avg_l = -100;
			else
				((CMedia*)udata)->audio_avg_l = (int)(20.0*log10((double)(totall * 2 / pinput_audio_frame->nb_samples / 65535.0)));
			//((CMedia*)udata)->audio_avg_r = totalr *2/ pinput_audio_frame->nb_samples;
			if (totalr * 2 / pinput_audio_frame->nb_samples == 0)
				((CMedia*)udata)->audio_avg_r = -100;
			else
				((CMedia*)udata)->audio_avg_r = (int)(20.0*log10((double)(totalr * 2 / pinput_audio_frame->nb_samples / 65535.0)));

			//((CMedia*)udata)->audio_avg_r = totall *2/ pinput_audio_frame->nb_samples;
			//cout << "audio  " << ((CMedia*)udata)->audio_avg_l << "  " << ((CMedia*)udata)->audio_avg_r << endl;
		}
		else
		{
			//fwrite(pin_video_frame->data[0], 1, data_size, fp_pcm);
			if (data_size <= AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos)
			{
				memcpy(((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audiowritepos, pinput_audio_frame->data[0], data_size);
				((CMedia*)udata)->audiowritepos += data_size;
				if (((CMedia*)udata)->audiowritepos == AUDIOBUFSIZE)
					((CMedia*)udata)->audiowritepos = 0;
			}
			else
			{
				memcpy(((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audiowritepos, pinput_audio_frame->data[0], AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos);
				memcpy(((CMedia*)udata)->paudiodatabuf, pinput_audio_frame->data[0] + AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos, data_size - (AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos));
				((CMedia*)udata)->audiowritepos = data_size - (AUDIOBUFSIZE - ((CMedia*)udata)->audiowritepos);
			}
		}
		SDL_AtomicAdd(&((CMedia*)udata)->audiodatalength, data_size);
		av_frame_free(&pinput_audio_frame);
	}
	len = (((CMedia*)udata)->audiodatalength.value < len) ? ((CMedia*)udata)->audiodatalength.value : len;

	/*	if (audiodatalength.value < len)
	{
	if ((data_over == 1) && (audio_packet_num.value == 0))
	g_quit = 1;
	return;
	}
	*/
	if (AUDIOBUFSIZE - ((CMedia*)udata)->audioreadpos >= len)
	{
		if (!((CMedia*)udata)->bsilence)
			memcpy(stream, ((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audioreadpos, len);
		//SDL_MixAudio(stream, ((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audioreadpos, len, SDL_MIX_MAXVOLUME);
		//fwrite(paudiodatabuf + audioreadpos, 1, len, fp_pcm_2);
		SDL_AtomicAdd(&((CMedia*)udata)->audiodatalength, -1 * len);
		((CMedia*)udata)->audioreadpos += len;
		if (((CMedia*)udata)->audioreadpos == AUDIOBUFSIZE)
			((CMedia*)udata)->audioreadpos = 0;
	}
	else
	{
		if (!((CMedia*)udata)->bsilence)
		{
			memcpy(stream, ((CMedia*)udata)->paudiodatabuf + ((CMedia*)udata)->audioreadpos, AUDIOBUFSIZE - ((CMedia*)udata)->audioreadpos);
			memcpy(stream + AUDIOBUFSIZE - ((CMedia*)udata)->audioreadpos, ((CMedia*)udata)->paudiodatabuf, len - (AUDIOBUFSIZE - ((CMedia*)udata)->audioreadpos));
		}
		//SDL_MixAudio(stream, ptempdatabuf, len, SDL_MIX_MAXVOLUME);
		//fwrite(ptempdatabuf, 1, len, fp_pcm_2);
		((CMedia*)udata)->audioreadpos = len - (AUDIOBUFSIZE - ((CMedia*)udata)->audioreadpos);
		SDL_AtomicAdd(&((CMedia*)udata)->audiodatalength, -1 * len);
	}
	//cout << "read audio data: " << len << endl;
}

int CMedia::VideoDecodeThread(void *ptr)
{
	int ret;
	AVPacket *pavpacket;
	bool bshowvideo;
	Uint32 lasttick = SDL_GetTicks(), lastlasttick = 0, lastlastlasttick = 0, lastlastlastlasttick = 0, lastlastlastlastlasttick = 0, curtick, starttick = SDL_GetTicks();
	int64_t lastaudiopts = -1, lastvideopts = -1;
	AVFrame  *pinput_video_frame = av_frame_alloc();
	while (!((CMedia*)ptr)->g_quit)
	{
		while ((avcodec_receive_frame(((CMedia*)ptr)->pinput_video_codecctx, pinput_video_frame) == 0) && (!((CMedia*)ptr)->g_quit))
		{
			if (((CMedia*)ptr)->start_video_pts == -1)
				((CMedia*)ptr)->start_video_pts = pinput_video_frame->pts;
			((CMedia*)ptr)->cur_video_pts = pinput_video_frame->pts;
			bshowvideo = false;
			ret = sws_scale(((CMedia*)ptr)->img_convert_ctx, pinput_video_frame->data, pinput_video_frame->linesize, 0, pinput_video_frame->height, ((CMedia*)ptr)->dst_data, ((CMedia*)ptr)->dst_linesize);
			while (!bshowvideo && !((CMedia*)ptr)->g_quit)
			{
				curtick = SDL_GetTicks();
				//有音频流的情况下，以音频流播放进度为标尺，确定视频帧播放时间，播放pts比最新已播音频pts少的视频帧
				if ((((CMedia*)ptr)->audio_index != -1))
				{
					if(pinput_video_frame->pts*((CMedia*)ptr)->paudiostream->time_base.den <= ((CMedia*)ptr)->cur_audio_pts*((CMedia*)ptr)->pvideostream->time_base.den)
						bshowvideo = true;
					else if ((lastvideopts != -1) && (((pinput_video_frame->pts - lastvideopts)*((CMedia*)ptr)->pvideostream->time_base.num * 1000 <= (curtick - lasttick)* ((CMedia*)ptr)->pvideostream->time_base.den)
						//|| ((curtick - lasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
						//|| ((curtick - lastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 2 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
						//|| ((curtick - lastlastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 3 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
						//|| ((curtick - lastlastlastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 4 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
						//|| ((curtick - lastlastlastlastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 5 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
						))
						bshowvideo = true;
				}
				//如果pts间隔差值<时间差值，或者时间差值大于了帧时长，显示一帧
				else if ((((CMedia*)ptr)->cur_audio_pts == 0 || (((CMedia*)ptr)->audiodatalength.value == 0 && ((CMedia*)ptr)->audio_frame_num.value == 0 && ((CMedia*)ptr)->audio_packet_num.value == 0)) &&
					(((pinput_video_frame->pts - lastvideopts)*((CMedia*)ptr)->pvideostream->time_base.num * 1000 <= (curtick - lasttick)* ((CMedia*)ptr)->pvideostream->time_base.den)
					|| ((curtick - lasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
					|| ((curtick - lastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 2 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
					|| ((curtick - lastlastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 3 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
					|| ((curtick - lastlastlastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 4 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
					|| ((curtick - lastlastlastlastlasttick)*((CMedia*)ptr)->pvideostream->avg_frame_rate.num >= 5 * 1000 * ((CMedia*)ptr)->pvideostream->avg_frame_rate.den)
					))
					bshowvideo = true;
				SDL_Delay(1);
			}
			if (bshowvideo)
			{
				((CMedia*)ptr)->frame_nums++;
				lastlastlastlastlasttick = lastlastlastlasttick;
				lastlastlastlasttick = lastlastlasttick;
				lastlastlasttick = lastlasttick;
				lastlasttick = lasttick;
				lasttick = curtick;
				lastvideopts = pinput_video_frame->pts;
				SDL_SemWait(((CMedia*)ptr)->pvideo_texture_lock);
				SDL_UpdateYUVTexture(((CMedia*)ptr)->pvideotexture, NULL, ((CMedia*)ptr)->dst_data[0], ((CMedia*)ptr)->dst_linesize[0]
					, ((CMedia*)ptr)->dst_data[1], ((CMedia*)ptr)->dst_linesize[1]
					, ((CMedia*)ptr)->dst_data[2], ((CMedia*)ptr)->dst_linesize[2]);
				SDL_SemPost(((CMedia*)ptr)->pvideo_texture_lock);
			}

/*			while ((((CMedia*)ptr)->video_frame_num.value >= 5) && (!((CMedia*)ptr)->g_quit))
				SDL_Delay(1);
			//cout << "video frame size " << pinput_video_frame->linesize[0] << "  " << pinput_video_frame->linesize[1] << "  " << pinput_video_frame->linesize[2] << endl;
			//cout << "video frame pts " << pinput_video_frame->pts << endl;
			SDL_SemWait(((CMedia*)ptr)->pvideo_frame_lock);
			((CMedia*)ptr)->video_frame_list.push_back(pinput_video_frame);
			SDL_SemPost(((CMedia*)ptr)->pvideo_frame_lock);

			SDL_AtomicAdd(&((CMedia*)ptr)->video_frame_num, 1);
*/			av_frame_free(&pinput_video_frame);
			pinput_video_frame = av_frame_alloc();
		}
		while ((((CMedia*)ptr)->video_packet_num.value < 1) && (!((CMedia*)ptr)->g_quit) && (((CMedia*)ptr)->data_over == 0))
			SDL_Delay(1);

		if ((((CMedia*)ptr)->data_over == 1) && (((CMedia*)ptr)->video_packet_num.value == 0))
		{
			((CMedia*)ptr)->video_over = 1;
			break;
		}

		if (!((CMedia*)ptr)->g_quit)
		{
			SDL_AtomicAdd(&((CMedia*)ptr)->video_packet_num, -1);

			SDL_SemWait(((CMedia*)ptr)->pvideo_packet_lock);
			pavpacket = *((CMedia*)ptr)->video_packet_list.begin();
			((CMedia*)ptr)->video_packet_list.pop_front();
			SDL_SemPost(((CMedia*)ptr)->pvideo_packet_lock);

			ret = avcodec_send_packet(((CMedia*)ptr)->pinput_video_codecctx, pavpacket);
			av_packet_free(&pavpacket);
		}
	}
	((CMedia*)ptr)->video_over = 1;
	return 0;
}


int CMedia::AudioDecodeThread(void *ptr)
{
	int ret;
	AVPacket *pavpacket;
	AVFrame  *pinput_audio_frame = av_frame_alloc();
	while (!((CMedia*)ptr)->g_quit)
	{
		while ((avcodec_receive_frame(((CMedia*)ptr)->pinput_audio_codecctx, pinput_audio_frame) == 0) && (!((CMedia*)ptr)->g_quit))
		{
			while ((((CMedia*)ptr)->audio_frame_num.value > 10) && (!((CMedia*)ptr)->g_quit))
				SDL_Delay(1);

			//fwrite(pinput_audio_frame->data[0], 1, pinput_audio_frame->linesize[0], fp_pcm_2);

			SDL_SemWait(((CMedia*)ptr)->paudio_frame_lock);
			((CMedia*)ptr)->audio_frame_list.push_back(pinput_audio_frame);
			SDL_SemPost(((CMedia*)ptr)->paudio_frame_lock);
			SDL_AtomicAdd(&((CMedia*)ptr)->audio_frame_num, 1);
			pinput_audio_frame = av_frame_alloc();
		}
		while ((((CMedia*)ptr)->audio_packet_num.value < 1) && (!((CMedia*)ptr)->g_quit) && (((CMedia*)ptr)->data_over == 0))
			SDL_Delay(1);

		if ((((CMedia*)ptr)->data_over == 1) && (((CMedia*)ptr)->audio_packet_num.value == 0))
			break;

		if (!((CMedia*)ptr)->g_quit)
		{
			SDL_AtomicAdd(&((CMedia*)ptr)->audio_packet_num, -1);

			SDL_SemWait(((CMedia*)ptr)->paudio_packet_lock);
			pavpacket = *((CMedia*)ptr)->audio_packet_list.begin();
			((CMedia*)ptr)->audio_packet_list.pop_front();
			SDL_SemPost(((CMedia*)ptr)->paudio_packet_lock);

			ret = avcodec_send_packet(((CMedia*)ptr)->pinput_audio_codecctx, pavpacket);
			av_packet_free(&pavpacket);
		}
	}
	av_frame_free(&pinput_audio_frame);

	return 0;
}

int CMedia::ReadPacketThread(void *ptr)
{
	int ret;
	AVPacket *pavpacket;
	while (!((CMedia*)ptr)->g_quit)
	{
		pavpacket = av_packet_alloc();
		if ((ret = av_read_frame(((CMedia*)ptr)->pinput_fmt_context, pavpacket)) < 0)
		{
			av_packet_free(&pavpacket);
			((CMedia*)ptr)->data_over = 1;
			break;
		}
		if (pavpacket->stream_index == ((CMedia*)ptr)->audio_index)
		{
			while ((((CMedia*)ptr)->audio_packet_num.value >= 15) && (!((CMedia*)ptr)->g_quit))
				SDL_Delay(1);

			SDL_SemWait(((CMedia*)ptr)->paudio_packet_lock);
			((CMedia*)ptr)->audio_packet_list.push_back(pavpacket);
			SDL_SemPost(((CMedia*)ptr)->paudio_packet_lock);

			SDL_AtomicAdd(&((CMedia*)ptr)->audio_packet_num, 1);
		}
		else if (pavpacket->stream_index == ((CMedia*)ptr)->video_index)
		{
			while ((((((CMedia*)ptr)->audio_index == -1) && (((CMedia*)ptr)->video_packet_num.value >= 3)) || ((((CMedia*)ptr)->audio_index != -1) && (((CMedia*)ptr)->video_packet_num.value >= 110))) && (!((CMedia*)ptr)->g_quit))
				SDL_Delay(1);

			SDL_SemWait(((CMedia*)ptr)->pvideo_packet_lock);
			((CMedia*)ptr)->video_packet_list.push_back(pavpacket);
			SDL_SemPost(((CMedia*)ptr)->pvideo_packet_lock);

			SDL_AtomicAdd(&((CMedia*)ptr)->video_packet_num, 1);
		}
		else
		{
			av_packet_free(&pavpacket);
		}
	}
	return 0;
}

void CMedia::Exit()
{
	g_quit = 1;
}

void CMedia::Pause(bool bpause)
{
	bpaused = bpause;
}

void CMedia::ToggleSilence()
{
	bsilence = !bsilence;
}

int CMedia::GetAudioId()
{
	return aid;
}

bool CMedia::HasVideo()
{
	return video_index!=-1;
}

SDL_Texture* CMedia::GetVideoTexture()
{
	return pvideotexture;
}

void CMedia::LockTextureSem()
{
	SDL_SemWait(pvideo_texture_lock);
}
void CMedia::ReleaseTextureSem()
{
	SDL_SemPost(pvideo_texture_lock);
}

bool CMedia::IsOver()
{
	return data_over && audio_over && video_over;
}

void CMedia::WaitForOver()
{
	if (paudiodecodethread != NULL)
		SDL_WaitThread(paudiodecodethread, NULL);
	if (pvideodecodethread != NULL)
		SDL_WaitThread(pvideodecodethread, NULL);
	if (preadpacketthread != NULL)
		SDL_WaitThread(preadpacketthread, NULL);
}

bool CMedia::IsSilence()
{
	return bsilence;
}

int CMedia::GetRunnedTime(int& dur)
{
	dur = duration;
	if (audio_index != -1)
	{
		return (cur_audio_pts - start_audio_pts) * 1000 / paudiostream->time_base.den;
	}
	else if(video_index != -1)
	{
		return (cur_video_pts - start_video_pts) * 1000 / pvideostream->time_base.den;
	}
	return 0;
}
void CMedia::GetAudioVolume(int& l, int& r)
{
	l = audio_avg_l;
	if (l <= -100)
		l = -100;
	r = audio_avg_r;
	if (r <= -100)
		r = -100;
}
