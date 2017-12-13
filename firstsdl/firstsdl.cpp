#include "stdafx.h"
#include "Media.h"
#include <list>
#include "sdl.h"
#include "sdl_ttf.h"
#include "sdl_image.h"
using namespace std;

#define VOLWIDTH 15;

int g_quit = 0;
bool bpaused = false;
char timestrbuf[50];

char* GetRunnedTime(CMedia *pmedia)
{
	int duration=0;
	int time = pmedia->GetRunnedTime(duration);
	sprintf_s(timestrbuf, "%02d:%02d:%02d.%03d			%02d:%02d:%02d.%03d"
		, duration / 3600000, (duration % 3600000) / 60000, (duration % 60000) / 1000, duration % 1000
		, time / 3600000, (time % 3600000) / 60000, (time % 60000) / 1000, time % 1000
		);
	return timestrbuf;
}

int _tmain(int argc, TCHAR* argv[])
{
	int ret = SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
	if (ret < 0)
		return -1;

	int windowwidth = 720;
	int windowheight = 576;
	SDL_Window* pwnd = SDL_CreateWindow("or", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowwidth, windowheight, SDL_WINDOW_RESIZABLE);
	if (pwnd == NULL)
		return -2;
	SDL_GetWindowSize(pwnd, &windowwidth, &windowheight);
	
	SDL_Renderer *prender = SDL_CreateRenderer(pwnd, -1, SDL_RENDERER_SOFTWARE);
	if (prender == NULL)
		return -2;

	TTF_Init();
	SDL_Color txtcolor = { 255, 0, 0, 0 }, transcolor = {255,255,255,255};
	TTF_Font *pfont = TTF_OpenFont("simsun.ttc", 50);
	TTF_SetFontStyle(pfont, TTF_STYLE_BOLD);
	SDL_Surface *psurface = TTF_RenderUNICODE_Blended(pfont, (Uint16*)_T("无视频信号"), txtcolor);
	SDL_Texture *ptxttexture = SDL_CreateTextureFromSurface(prender, psurface);
	//SDL_FreeSurface(psurface);

	SDL_Surface *poversurface = TTF_RenderUNICODE_Blended(pfont, (Uint16*)_T("结束"), txtcolor);
	SDL_Texture *povertxttexture = SDL_CreateTextureFromSurface(prender, poversurface);
	//SDL_FreeSurface(poversurface);

	TTF_Font *ptimefont = TTF_OpenFont("simsun.ttc", 20);
	//SDL_Surface *ptimesurface = TTF_RenderUNICODE_Blended(ptimefont, (Uint16*)_T("00:00:00.000"), txtcolor);
	//SDL_Texture *ptimetexture = SDL_CreateTextureFromSurface(prender, ptimesurface);
	//SDL_FreeSurface(ptimesurface);

	SDL_Surface * psilencesurface = IMG_Load("silence.jpg");
	SDL_Texture *psilencetexture = NULL;
	if (psilencesurface)
	{
 		Uint32 colorkey = SDL_MapRGB(psilencesurface->format, 255, 255, 255);
		SDL_SetColorKey(psilencesurface, 1, colorkey);
		SDL_SetSurfaceBlendMode(psilencesurface, SDL_BLENDMODE_BLEND);
		SDL_SetSurfaceAlphaMod(psilencesurface, 128);
		psilencetexture = SDL_CreateTextureFromSurface(prender, psilencesurface);
		SDL_SetTextureBlendMode(psilencetexture, SDL_BLENDMODE_BLEND);
		SDL_SetTextureAlphaMod(psilencetexture,128);
		//SDL_FreeSurface(psilencesurface);
	}

	list<string> filepathlist;
	//filepathlist.push_back("F:\\BaiduYunDownload\\[ahhfwww Classical] ORFEO C296923D CD1 APE\\[ahhfwww Classical] Mozart Le Nozze Di Figaro - Bohm, Schwarzkopf.ape");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\降央卓玛-走天涯_-最美女中音.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\林忆莲_李宗盛-当爱已成往事.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\安雯-黄土高坡.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\罗文.甄妮-铁血丹心.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\降央卓玛-多情总为无情伤_-最美女中音.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\刀郎-云朵-爱是你我.avi");
	filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\孟庭苇-雾里看花.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\戴佩妮-辛德瑞拉.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\阿杜-离别.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\叶倩文-潇洒走一回.mp4");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\付笛生_任静_-_知心爱人.avi");
	//filepathlist.push_back("E:\\tsstream\\s02e15.ts");
	//filepathlist.push_back("E:\\tsstream\\BBC.ts");
	//filepathlist.push_back("F:\\BaiduYunDownload\\small_apple.rmvb");
	//filepathlist.push_back("E:\\佛经\\占察善恶业报经.flv");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\林心如_周杰-你是风儿我是沙_1.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\黄安-新鸳鸯蝴蝶梦.avi");
	//filepathlist.push_back("f:\\BaiduYunDownload\\打开海洋百宝箱国英.720PNVCG@枉顾弃浮生.mp4");
	//filepathlist.push_back("f:\\BaiduYunDownload\\蓝莲花2.mp3");
	//filepathlist.push_back("E:\\tsstream\\cctv3_aac.ts");
	//filepathlist.push_back("E:\\tsstream\\e601-6m.ts");
	//filepathlist.push_back("E:\\tsstream\\cctv5_2k_aac.ts");
	//filepathlist.push_back("E:\\tsstream\\Sport.ts");
	//filepathlist.push_back("E:\\tsstream\\BBC.ts");
	//filepathlist.push_back("E:\\tsstream\\sample.ts");
	//filepathlist.push_back("E:\\tsstream\\JMB.ts");
	//filepathlist.push_back("E:\\tsstream\\倍耐力广告34秒.ts");
	//filepathlist.push_back("E:\\tsstream\\Infragistics_Presentation_lowRes_1.h264.ts"); 
	//filepathlist.push_back("E:\\tsstream\\SD-20min_W5_encoded_3Mbit_mux3.75Mbits.ts");
	//filepathlist.push_back("f:\\BaiduYunDownload\\448MTV\\弦子-不得不爱.mkv");

	av_register_all();
	list<CMedia*> medialist;
	CMedia *pmedia;
	for (auto filepath : filepathlist)
	{
		pmedia = new CMedia(filepath.c_str());
		if (pmedia->InitMediaObj() != 0)
			continue;
		if (pmedia->InitSDLObj(prender, windowwidth / 2, windowheight / 2) != 0)
			continue;			
		medialist.push_back(pmedia);
	}
	if (medialist.size() == 0)
		return -1;

	for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
		SDL_PauseAudioDevice((*iter)->GetAudioId(),0);

	Uint32 lasttick = 0, curtick,videoindex,tempindex;
	SDL_Rect videorc,volrc;
	int lvolume, rvolume;
	Uint8 r, g, b, a;
	bool bover;
	SDL_Event sdlevent;
	while (!g_quit)
	{
		SDL_GetWindowSize(pwnd, &windowwidth, &windowheight);
		while (SDL_PollEvent(&sdlevent) != 0)
		{
			switch (sdlevent.type)
			{
			case SDL_KEYDOWN:
				if (sdlevent.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				{
					g_quit = 1;
					for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
						(*iter)->Exit();
				}
				else if (sdlevent.key.keysym.scancode == SDL_SCANCODE_SPACE)
				{
					bpaused = !bpaused;
					for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
						(*iter)->Pause(bpaused);
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				tempindex = 0;
				videoindex = sdlevent.button.y * 2 / windowheight * 2 + sdlevent.button.x * 2 / windowwidth;
				for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
				{
					if (videoindex == tempindex++)
					{
						(*iter)->ToggleSilence();
						break;
					}
				}
				break;
			case SDL_QUIT:
				g_quit = 1;
				for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
					(*iter)->Exit();
				break;
			}
		}
		SDL_Delay(5);

		if ( !g_quit )
		{
			curtick = SDL_GetTicks();
			if (curtick - lasttick >= 40)
			{
				videoindex = 0;
				SDL_RenderClear(prender);
				lasttick = curtick;
				for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
				{
					if (videoindex == 0)
					{
						videorc.x = 0;
						videorc.y = 0;
						videorc.w = windowwidth / 2;
						videorc.h = windowheight / 2;
					}
					else if (videoindex == 1)
					{
						videorc.x = windowwidth / 2;
						videorc.y = 0;
						videorc.w = windowwidth / 2;
						videorc.h = windowheight / 2;
					}
					else if (videoindex == 2)
					{
						videorc.x = 0;
						videorc.y = windowheight / 2;
						videorc.w = windowwidth / 2;
						videorc.h = windowheight / 2;
					}
					else if (videoindex == 3)
					{
						videorc.x = windowwidth / 2;
						videorc.y = windowheight / 2;
						videorc.w = windowwidth / 2;
						videorc.h = windowheight / 2;
					}
					if ((*iter)->IsOver())
					{
						videorc.x = videorc.x + videorc.w / 2 - poversurface->w/2;
						videorc.y = videorc.y + videorc.h / 2 - poversurface->h/2;
						videorc.w = poversurface->w;
						videorc.h = poversurface->h;
						SDL_RenderCopy(prender, povertxttexture, NULL, &videorc);
					}
					else
					{
						if (!(*iter)->HasVideo())
						{
							videorc.x = videorc.x + videorc.w / 2 - psurface->w / 2;
							videorc.y = videorc.y + videorc.h / 2 - psurface->h / 2;
							videorc.w = psurface->w;
							videorc.h = psurface->h;
							SDL_RenderCopy(prender, ptxttexture, NULL, &videorc);
							if (videoindex == 0)
							{
								videorc.x = 0;
								videorc.y = 0;
								videorc.w = windowwidth / 2;
								videorc.h = windowheight / 2;
							}
							else if (videoindex == 1)
							{
								videorc.x = windowwidth / 2;
								videorc.y = 0;
								videorc.w = windowwidth / 2;
								videorc.h = windowheight / 2;
							}
							else if (videoindex == 2)
							{
								videorc.x = 0;
								videorc.y = windowheight / 2;
								videorc.w = windowwidth / 2;
								videorc.h = windowheight / 2;
							}
							else if (videoindex == 3)
							{
								videorc.x = windowwidth / 2;
								videorc.y = windowheight / 2;
								videorc.w = windowwidth / 2;
								videorc.h = windowheight / 2;
							}
						}
						else
						{
							(*iter)->LockTextureSem();
							SDL_RenderCopy(prender, (*iter)->GetVideoTexture(), NULL, &videorc);
							(*iter)->ReleaseTextureSem();
						}

						//显示左右声道音量柱
						(*iter)->GetAudioVolume(lvolume, rvolume);
						SDL_GetRenderDrawColor(prender, &r, &g, &b, &a);
						SDL_SetRenderDrawColor(prender, 255, 0, 0, 0);
						volrc.x = videorc.x + 1;
						volrc.y = videorc.y ;
						volrc.w = VOLWIDTH;
						volrc.h = videorc.h ;
						SDL_RenderDrawRect(prender, &volrc);
						volrc.x += 1;
						volrc.w -= 2;
						volrc.y += abs(lvolume) / 100.*videorc.h ;
						volrc.h -= abs(lvolume) / 100.*videorc.h ;
						SDL_SetRenderDrawColor(prender, 0, 255, 0, 0);
						SDL_RenderFillRect(prender, &volrc);

						SDL_SetRenderDrawColor(prender, 255, 0, 0, 0);
						volrc.x = videorc.x + videorc.w - VOLWIDTH-1;
						volrc.y = videorc.y ;
						volrc.w = VOLWIDTH;
						volrc.h = videorc.h ;
						SDL_RenderDrawRect(prender, &volrc);
						volrc.x += 1;
						volrc.w -= 2;
						volrc.y += abs(rvolume) / 100.*videorc.h;
						volrc.h -= abs(rvolume) / 100.*videorc.h ;
						SDL_SetRenderDrawColor(prender, 0, 255, 0, 0);
						SDL_RenderFillRect(prender, &volrc);
						SDL_SetRenderDrawColor(prender, r, g, b, a);

						//显示已播放时间
						SDL_Surface *ptimesurface = TTF_RenderText_Blended(ptimefont, GetRunnedTime(*iter), txtcolor);
						SDL_Texture *ptimetexture = SDL_CreateTextureFromSurface(prender, ptimesurface);
						videorc.x += VOLWIDTH+2;
						videorc.w = ptimesurface->w;
						videorc.h = ptimesurface->h;
						SDL_RenderCopy(prender, ptimetexture, NULL, &videorc);
						SDL_FreeSurface(ptimesurface);
						SDL_DestroyTexture(ptimetexture);

						if ((*iter)->IsSilence() && psilencetexture)
						{
							videorc.x = videorc.x + windowwidth / 2/2 - psilencesurface->w / 2;
							videorc.y = videorc.y + windowheight / 2 / 2 - psilencesurface->h / 2;
							videorc.w = psilencesurface->w;
							videorc.h = psilencesurface->h;
							SDL_RenderCopy(prender, psilencetexture, NULL, &videorc);
						}

					}
				videoindex++;
				}
				SDL_RenderPresent(prender);
			}
		}

		bover = true;
		for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
		{
			bover &= (*iter)->IsOver();
		}
		if (bover)
			break;
	}
	for (auto iter = medialist.begin(); iter != medialist.end(); iter++)
	{
		SDL_PauseAudioDevice((*iter)->GetAudioId(), 1);
		(*iter)->WaitForOver();
	}

	SDL_DestroyRenderer(prender);
	SDL_DestroyWindow(pwnd);
	SDL_Quit();

	return 0;
}