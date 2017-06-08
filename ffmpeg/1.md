```cpp
// FFmpeg.cpp : 定义控制台应用程序的入口点。
//

#include <stdio.h>
#include <tchar.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#include <libavutil\imgutils.h>
#include <libavutil\time.h>
#include <libavutil\avutil.h>
#include <libsdl\SDL.h>
#include <libavdevice\avdevice.h>
#include <libavformat\internal.h>
}
#else
//Linux
#endif

//SDL beign
//Refresh Event
#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)

int thread_exit = 0;
int thread_pause = 0;

int sfp_refresh_thread(void *opaque)
{
	thread_exit = 0;
	thread_pause = 0;

	while(!thread_exit)
	{
		if(!thread_pause)
		{
			SDL_Event event;
			event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&event);
		}
		SDL_Delay(40);
	}
	thread_exit = 0;
	thread_pause = 0;
	//break
	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}
//SDL end
/*
void get_dsshow_device(AVFormatContext *pFormatCtx)
{
	AVDictionary *options = NULL;
	av_dict_set(&options,"list_devices","true",0);
	AVInputFormat *iformat = av_find_input_format("dshow");  
	printf("Device Info=============\n");  
    avformat_open_input(&pFormatCtx,"video=dummy",iformat,&options);  
    printf("========================\n");  

}
*/
enum {
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META  = 0x12,
};
/*
int get_video_extradata(AVFormatContext *s, int video_index)
{
   int  type, size, flags, pos, stream_type;
   int ret = -1;
   int64_t dts;
   bool got_extradata = false;

   if (!s || video_index < 0 || video_index > 2)
      return ret;

   for (;; avio_skip(s->pb, 4)) {
      pos  = avio_tell(s->pb);
      type = avio_r8(s->pb);
      size = avio_rb24(s->pb);
      dts  = avio_rb24(s->pb);
      dts |= avio_r8(s->pb) << 24;
      avio_skip(s->pb, 3);

       if (0 == size)
          break;
       if (FLV_TAG_TYPE_AUDIO == type || FLV_TAG_TYPE_META == type) {
          //if audio or meta tags, skip them.
          avio_seek(s->pb, size, SEEK_CUR);
       } else if (type == FLV_TAG_TYPE_VIDEO) {
         //if the first video tag, read the sps/pps info from it. then break.
          size -= 5;
          s->streams[video_index]->codec->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
          if (NULL == s->streams[video_index]->codec->extradata)
             break;
          memset(s->streams[video_index]->codec->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
          memcpy(s->streams[video_index]->codec->extradata, s->pb->buf_ptr + 5, size);
          s->streams[video_index]->codec->extradata_size = size;
          ret = 0;
          got_extradata = true;
       } else  {
           break;
       }

       if (got_extradata)
          break;
   }

   return ret;
}
*/
static AVPacket *add_to_pktbuf(AVPacketList **packet_buffer, AVPacket *pkt, AVPacketList **plast_pktl)
{
    AVPacketList *pktl = (AVPacketList *)av_mallocz(sizeof(AVPacketList));
    if (!pktl)
        return NULL;

    if (*packet_buffer)
        (*plast_pktl)->next = pktl;
    else
        *packet_buffer = pktl;

    /* Add the packet in the buffered packet list. */
    *plast_pktl = pktl;
    pktl->pkt   = *pkt;
    return &pktl->pkt;
}

void init_VideoStream(AVFormatContext *formatCtx)
{
    /*
    int videoIndex = 0;
    AVStream *st = avformat_new_stream(formatCtx, NULL);
    if (!st)
        return;
    //Init the video codec(H264)
    st->codec->codec_id = AV_CODEC_ID_H264;
    st->codec->width = 640;
    st->codec->height = 480;
    st->codec->ticks_per_frame = 2;
    st->codec->pix_fmt = AV_PIX_FMT_YUV444P;//AV_PIX_FMT_YUV420P;
    st->pts_wrap_bits = 32;
    st->codec->time_base.den = 1000;
    st->codec->time_base.num = 1;
    st->codec->sample_fmt = AV_SAMPLE_FMT_NONE;
    st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st->time_base.den = 1000;
    st->time_base.num = 1;
    st->r_frame_rate.den = 2;
    st->r_frame_rate.num = 60;
    st->avg_frame_rate.den = 2;
    st->avg_frame_rate.num = 60;
    
    //s->streams[video_index]->codec->frame_size = 0;
    //s->streams[video_index]->codec->frame_number = 7;
    //s->streams[video_index]->codec->has_b_frames = 0;
    /*
    formatCtx->streams[videoIndex]->codec->codec_tag = 0;
    formatCtx->streams[videoIndex]->codec->bit_rate = 0;
    formatCtx->streams[videoIndex]->codec->refs = 1;
    formatCtx->streams[videoIndex]->codec->sample_rate = 0;
    formatCtx->streams[videoIndex]->codec->channels = 0;
    formatCtx->streams[videoIndex]->codec->profile = 66;
    formatCtx->streams[videoIndex]->codec->level = 31;
	 
    //formatCtx->nb_streams = 1;
    formatCtx->duration = 0;
    formatCtx->start_time = 0;
    formatCtx->bit_rate = 0;
    formatCtx->iformat->flags = 0;
    formatCtx->duration_estimation_method = AVFMT_DURATION_FROM_STREAM;
    */
    // H264 need sps/pps for decoding, so read it from the first video tag.
    //printf("get_video_extradata ret = %d\n",get_video_extradata(formatCtx, 0));

    //set packet!
    AVPacket packet;
    av_init_packet(&packet);
    while (true)
    {
        int ret1 = av_read_frame(formatCtx, &packet);
        if (packet.flags & AV_PKT_FLAG_KEY)
        {
            break;
        }
    }
    //void* pbs = &formatCtx->internal;
    //void* rpbs = &formatCtx->internal->packet_buffer;
    add_to_pktbuf(&formatCtx->internal->packet_buffer,&packet,&formatCtx->internal->packet_buffer_end);
    //formatCtx->streams[0] = st;
    AVStream *st = formatCtx->streams[0];
    //Init the video codec(H264).
    //st->codec->codec_id = AV_CODEC_ID_H264;
    st->codec->width = 640;
    st->codec->height = 480;
    //st->codec->ticks_per_frame = 2;
    st->codec->pix_fmt = AV_PIX_FMT_YUV444P;//AV_PIX_FMT_YUV420P;
    //st->pts_wrap_bits = 32;
    st->codec->time_base.den = st->time_base.den;
    st->codec->time_base.num = st->time_base.num;
    //st->codec->sample_fmt = AV_SAMPLE_FMT_NONE;
    //st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    //st->time_base.den = 1000;
    //st->time_base.num = 1;
    st->r_frame_rate.den = st->avg_frame_rate.den;
    st->r_frame_rate.num = st->avg_frame_rate.num;
    //st->avg_frame_rate.den = 2;
    //st->avg_frame_rate.num = 60;
    //empty the buffer.
    // H264 need sps/pps for decoding, so read it from the first video tag.
    //printf("get_video_extradata ret = %d\n",get_video_extradata(formatCtx, 0));
    //formatCtx->pb->buf_ptr = formatCtx->pb->buf_end;
    //formatCtx->pb->pos = (int64_t) formatCtx->pb->buf_end;
}

int _tmain(int argc, char* argv[])
{
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame,*pFrameYUV;
    AVPacket *packet;
    struct SwsContext *img_convert_ctx;

    int y_size;
    int ret,got_pic;
    int i,videoIndex;
    unsigned char* out_buffer;
    char filePath[] = "rtmp://192.168.1.201/live/mystream";
    //SDL begin
    int screen_w = 0,screen_h = 0;
    SDL_Window *screen;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
    SDL_Thread *video_tid;
    SDL_Event event;
    //SDL end
    //------
    av_register_all();
    avdevice_register_all();
    avformat_network_init();
    
    pFormatCtx = avformat_alloc_context();
    //try to find device from window
    //get_dsshow_device(pFormatCtx);
    //AVInputFormat *pFmt = av_find_input_format("dshow");
    int64_t start = av_gettime();
    printf("==========>begin avformat_open_input : %0.3fs\n",(av_gettime()-start) / 1000000.0);

    if(avformat_open_input(&pFormatCtx,filePath,NULL,NULL))
    //if(avformat_open_input(&pFormatCtx,"video=e2eSoft VCam",pFmt,NULL) != 0)
    {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    //pFormatCtx->probesize = 4096;

    printf("==========>begin find stream info : %0.3fs\n",(av_gettime()-start) / 1000000.0 );
    if(1){
        init_VideoStream(pFormatCtx);
    }else{
        if(avformat_find_stream_info(pFormatCtx,NULL) < 0)
        {
            printf("Couldn't find stream information.\n");
            return -1;
        }
    }
    printf("==========>after find stream info :%0.3fs\n",(av_gettime()-start) / 1000000.0 );

    printf("----------------File Information----------------\n");
    av_dump_format(pFormatCtx,0,filePath,0);
    printf("------------------------------------------------\n");

    videoIndex = -1;

    for(i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
            break;
        }
    }

    if(videoIndex == -1)
    {
        printf("Couldn't find video stream.\n");
        return -1;
    }

    pCodecCtx = pFormatCtx->streams[videoIndex]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if(pCodec == NULL)
    {
        printf("Codec not found.\n");
        return -1;
    }

    if(avcodec_open2(pCodecCtx,pCodec,NULL)< 0)
    {
        printf("Could not open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    out_buffer =  (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,pCodecCtx->width,pCodecCtx->height,1));
    av_image_fill_arrays(pFrameYUV->data,pFrameYUV->linesize,out_buffer,AV_PIX_FMT_YUV420P,pCodecCtx->width,pCodecCtx->height,1);

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    img_convert_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,
        pCodecCtx->width,pCodecCtx->height,AV_PIX_FMT_YUV420P,SWS_BICUBIC,NULL,NULL,NULL);

    //SDL begin
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        printf("Could not initialize SDL - %s\n",SDL_GetError());
        return -1;
    }

    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;
    screen = SDL_CreateWindow("Simple ffmpeg player's window",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,screen_w,screen_h,SDL_WINDOW_OPENGL);

    if(!screen)
    {
        printf("SDL: could not create window - exiting:%s\n",SDL_GetError());    
        return -1;
    }

    sdlRenderer = SDL_CreateRenderer(screen,-1,0);
    //IYUV: Y + U + V
    //YV12: Y + V + U
    sdlTexture = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,screen_w,screen_h);
    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;

    video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);
    //SDL end

    //EventLoop
    for(;;)
    {
        //Wait
        SDL_WaitEvent(&event);
        if(event.type == SFM_REFRESH_EVENT)
        {
            while(1)
            {
                if(av_read_frame(pFormatCtx,packet) < 0)
                {
                    thread_exit = 1;
                }
                if(packet->stream_index == videoIndex)
                    break;
            }
            ret = avcodec_decode_video2(pCodecCtx,pFrame,&got_pic,packet);
            if(ret < 0)
            {
                printf("Decode Error.\n");
                return -1;
            }
            if(got_pic)
            {
                sws_scale(img_convert_ctx,pFrame->data,pFrame->linesize,
                    0,pCodecCtx->height,pFrameYUV->data,pFrameYUV->linesize);

                //SDL begin
                SDL_UpdateTexture(sdlTexture,NULL,pFrameYUV->data[0],pFrameYUV->linesize[0]);
                SDL_RenderClear(sdlRenderer);
                SDL_RenderCopy(sdlRenderer,sdlTexture,NULL,&sdlRect);
                SDL_RenderPresent(sdlRenderer);
            }
            av_free_packet(packet);
        }else if(event.type == SDL_KEYDOWN)
        {
            //Pause
            if(event.key.keysym.sym == SDLK_SPACE)
            {
                thread_pause = !thread_pause;
            }
        }else if(event.type == SDL_QUIT)
        {
            thread_exit = 1;
        }else if(event.type == SFM_BREAK_EVENT)
        {
            break;
        }
    }

    sws_freeContext(img_convert_ctx);

    SDL_Quit();

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}
```
