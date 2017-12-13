#pragma once

using namespace std;
#include <string>
#include <iostream>
#include <list>

#include "sdl.h"
#include "sdl_thread.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
}

#define AUDIOBUFSIZE 150*1024

class CMedia
{
public:
	CMedia(const char* pmediapath);
	~CMedia();
	int InitMediaObj();
	int InitSDLObj(SDL_Renderer *prender, int width, int height);
	SDL_Texture* GetVideoTexture();
	int GetAudioId();
	bool HasVideo();
	void Pause(bool bpause);
	void ToggleSilence();
	bool IsSilence();
	void Exit();
	void LockTextureSem();
	void ReleaseTextureSem();
	bool IsOver();
	void WaitForOver();
	int GetRunnedTime(int& dur);
	void GetAudioVolume(int& l,int& r);

private:
	static void  fill_audio(void *udata, Uint8 *stream, int len);
	static int AudioDecodeThread(void* ptr);
	static int VideoDecodeThread(void* ptr);
	static int ReadPacketThread(void* ptr);

private:
	AVFormatContext *pinput_fmt_context;
	AVStream *paudiostream ;
	AVStream *pvideostream ;
	AVCodecContext *pinput_audio_codecctx ;
	AVCodecContext *pinput_video_codecctx ;
	int audio_index, video_index;

	SDL_Texture *pvideotexture;
	SwsContext *img_convert_ctx = NULL;
	uint8_t *dst_data[4];
	int dst_linesize[4];

	bool bpaused;
	bool bsilence;
	string filepath;
	int samplesize ; //每个采样占用字节数
	Uint8* paudiodatabuf;
	SDL_atomic_t audiodatalength, audio_packet_num, video_packet_num, audio_frame_num, video_frame_num;
	int audiowritepos ;
	int audioreadpos ;
	int audio_avg_l, audio_avg_r;
	int frame_nums;
	int64_t duration;

	list<AVPacket*> audio_packet_list;
	list<AVPacket*> video_packet_list;
	SDL_sem *paudio_packet_lock, *pvideo_packet_lock;//packet list 信号量 

	list<AVFrame*> audio_frame_list;
	list<AVFrame*> video_frame_list;
	SDL_sem *paudio_frame_lock, *pvideo_frame_lock, *pvideo_texture_lock;//frame list 信号量 

	int g_quit ;
	int data_over ;
	int audio_over ;
	int video_over ;
	int64_t cur_audio_pts, cur_video_pts, start_audio_pts, start_video_pts;
	SDL_AudioDeviceID aid;

	SDL_Thread *paudiodecodethread = NULL, *pvideodecodethread = NULL, *preadpacketthread = NULL;

	static int g_audioindex;
};

