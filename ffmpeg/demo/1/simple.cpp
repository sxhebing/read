//avformat_find_stream_info优化简易demo，测试 H264 + AAC RTMP直播流
//测试rtmp://live.hkstv.hk.lxdns.com/live/hks

#include "stdafx.h"

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
#include <libswresample\swresample.h>
#include <libavdevice\avdevice.h>
}
#else
//Linux
#endif

#include <libavformat/internal.h>
#define GET_ARRAY_LEN(array,len) {len = (sizeof(array) / sizeof(array[0]));}
#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

char* url;

typedef struct _DecodeParams{
    unsigned char *extra;
    int extra_size;
    AVCodecID code_id;
    int format;
    int profile;
    int level;
    AVRational stream_time_base;
    AVRational codec_time_base;
    int pts_wrap_bits;

    //for video
    AVFieldOrder field_order;
    int qmin;
    int qmax;
    int width;
    int height;
    AVRational aspect;
    AVRational r_frame_rate;
    AVRational avg_frame_rate;

    //for audio
    int channels;
    int channel_layout;
    int sample_rate;
    int bits_per_coded_sample;
    int frame_size;
    AVRational pkt_timebase;

}DecodeParams;

typedef struct _AVPacketQueue{
    AVPacket *cur;
    struct _AVPacketQueue *next;
}AVPacketQueue;

typedef struct _AVFrameQueue{
    AVFrame *cur;
    struct _AVFrameQueue *next;
}AVFrameQueue;

typedef struct SimplePlayer{
    //decode context
    AVFormatContext *formatCtx;
    //packet for decode
    AVPacket *packet;

    /*for video decode*/
    AVCodecContext *vCodecCtx;
    AVCodec *vCodec;
    AVFrame *vFrame,*vFrameYUV;
    struct SwsContext *img_convert_ctx;
    /*for audio decode*/
    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;
    AVFrame *aFrame;  
    struct SwrContext *au_convert_ctx;
    Uint8 *out_buffer;

    //stream index
    int videoIndex;
    int audioIndex;

    //Out params for Video
    AVPixelFormat vFormat;
    //Out params for Audio
    uint64_t out_channel_layout;
    int out_nb_samples;
    AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    int out_channels;
    int out_buffer_size;

    //play flag
    int exit;
    int pause;
    int64_t start;

    //queue
    AVPacketQueue *videoPkts;
    AVPacketQueue *audioPkts;
    AVFrameQueue *videoFrame;
    AVFrameQueue *audioFrame;

    //thread
    SDL_Thread *decVideo;
    SDL_Thread *decAudio;
    SDL_Thread *playVideo;
    SDL_Thread *playAudio;
    SDL_Thread *read;
    SDL_mutex *pktMutex;
    SDL_mutex *decAudioMutex;

    //SDL
    int screen_w,screen_h;
    SDL_Window *screen;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
 
    SDL_Event event;
    //add Audio
    SDL_AudioSpec audioSpec;
    //SDL end

}SimplePlayer;

//SDL beign
//Refresh Event
#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)

//Audio begin
static Uint8 *audio_chunk;
static Uint32 audio_len;
static Uint8 *audio_pos;
static DecodeParams *videoParams,*audioParams;
/* The audio function callback takes the following parameters:  
* stream: A pointer to the audio buffer to be filled  
* len: The length (in bytes) of the audio buffer  
*/
//Buffer:  
//|-----------|-------------|  
//chunk-------pos---len-----|
void  fill_audio(void *udata,Uint8 *stream,int len){   
    //SDL 2.0  
    SDL_memset(stream, 0, len);  
    if(audio_len==0)  
        return;   

    len=(len>audio_len?audio_len:len);   /*  Mix  as  much  data  as  possible  */   

    SDL_MixAudio(stream,audio_pos,len,SDL_MIX_MAXVOLUME);  
    audio_pos += len;   
    audio_len -= len;   
} 
//Audio end

AVPacket * get_packet(SimplePlayer *sp,bool video){
    SDL_LockMutex(sp->pktMutex);
    AVPacket *pkt = video ? sp->videoPkts->cur : sp->audioPkts->cur;
    if(pkt == NULL){
        SDL_UnlockMutex(sp->pktMutex);
        return NULL;
    }
    AVPacketQueue *old = video ? sp->videoPkts : sp->audioPkts;
    if(old->next){
        if(video){
            sp->videoPkts = old->next;
        }else{
            sp->audioPkts = old->next;
        }
        av_free(old);
    }else{
        old->cur = NULL;
    }
    SDL_UnlockMutex(sp->pktMutex);
    return pkt;
}

void add_packet(AVPacketQueue *tag,AVPacket *pkt){
    if(tag->cur == NULL){
        tag->cur = av_packet_clone(pkt);
        tag->next = NULL;
        return;
    }
    AVPacketQueue *pktq = (AVPacketQueue *)av_malloc(sizeof(AVPacketQueue));
    pktq->cur = av_packet_clone(pkt);
    pktq->next = NULL;
    
    //max packets in queue?
    while(tag->next != NULL){
        tag = tag->next;
    }
    tag->next = pktq;
}

int read_thread(void *opaque)
{
    SimplePlayer *sp = (SimplePlayer *)opaque;

    while(sp && !sp->exit)
    {
        if(!sp->pause)
        {
            if(av_read_frame(sp->formatCtx,sp->packet) < 0)
            {
                sp->exit = 1;
                continue;
            }
            if(sp->packet->stream_index == sp->videoIndex){
                SDL_LockMutex(sp->pktMutex);
                //printf("put video %d \n",sp->packet->size);
                add_packet(sp->videoPkts,sp->packet);
                SDL_UnlockMutex(sp->pktMutex);
            }else if(sp->packet->stream_index == sp->audioIndex){
                SDL_LockMutex(sp->pktMutex);
                add_packet(sp->audioPkts,sp->packet);
                SDL_UnlockMutex(sp->pktMutex);
            }
            av_free_packet(sp->packet);
        }
        SDL_Delay(1);
    }

    //SDL_DestroyMutex(sp->decAudioMutex);
    //break
    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);
    return 0;
}

int read_video_thread(void *opaque)
{
    int ret,got;
    SimplePlayer *sp = (SimplePlayer *)opaque;
    while(sp && !sp->exit)
    {
        if(!sp->pause)
        {
           AVPacket *pkt = get_packet(sp,1);
           if(pkt){
               //printf("get video %lld - %lld - %lld \n",pkt->dts,pkt->pts,pkt->duration);
               ret = avcodec_decode_video2(sp->vCodecCtx,sp->vFrame,&got,pkt);
               if(ret < 0)
               {
                   printf("Decode Error %d for %d.\n",ret,pkt->size);
                   return -1;
               }
               if(got)
               {
                   /*if(sp->vFrame->key_frame){
                       printf("Get I Frame!\n");
                   }*/
                   sws_scale(sp->img_convert_ctx,sp->vFrame->data,sp->vFrame->linesize,
                       0,sp->vCodecCtx->height,sp->vFrameYUV->data,sp->vFrameYUV->linesize);

                   //SDL begin
                   SDL_UpdateTexture(sp->sdlTexture,NULL,sp->vFrameYUV->data[0],sp->vFrameYUV->linesize[0]);
                   SDL_RenderClear(sp->sdlRenderer);
                   SDL_RenderCopy(sp->sdlRenderer,sp->sdlTexture,NULL,&sp->sdlRect);
                   SDL_RenderPresent(sp->sdlRenderer);
               }else{
                //printf("Decode Error 2.\n");
               }
              av_free_packet(pkt);
           }
       }
       SDL_Delay(1);
    }
    return 0;
}

int read_audio_thread(void *opaque)
{
    int ret,got;
    SimplePlayer *sp = (SimplePlayer *)opaque;
    while(sp && !sp->exit)
    {
        if(!sp->pause)
        {
           AVPacket *pkt = get_packet(sp,0);
           if(pkt){
               ret = avcodec_decode_audio4(sp->aCodecCtx,sp->aFrame,&got,pkt);
               if(ret < 0)
               {
                   printf("Decode audio Error.\n");
                   //return -1;
               }
               if(got)
               {
                   swr_convert(sp->au_convert_ctx,&sp->out_buffer,MAX_AUDIO_FRAME_SIZE,(const uint8_t **)sp->aFrame->data,sp->aFrame->nb_samples);
                   audio_chunk = (Uint8  *)sp->out_buffer;
                   audio_len = sp->out_buffer_size;
                   audio_pos = audio_chunk;
                   //printf("Decode audio success.\n");
               }else{
                   //printf("Decode audio failed.\n");
               }
              av_free_packet(pkt);
          }
       }
       SDL_Delay(1);
    }
    return 0;
}
//SDL end

void get_dsshow_device(AVFormatContext *pFormatCtx)
{
    AVDictionary *options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    AVInputFormat *iformat = av_find_input_format("dshow");  
    printf("Device Info=============\n");  
    avformat_open_input(&pFormatCtx,"video=dummy",iformat,&options);  
    printf("========================\n");  

}

enum {
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META  = 0x12,
};

 
int loadCfg(const char* path) 
 { 
     FILE *fp; 
     char line[1024];             //每行最大读取的字符数
     if((fp = fopen(path,"r")) == NULL) //判断文件是否存在及可读
     { 
         printf("read cfg error!"); 
         return -1; 
     } 

     DecodeParams *tag;
     while (!feof(fp)) 
     { 
         fgets(line,1024,fp);  //读取一行
         if(strncmp("#",line,1) == 0){
            //ignore
         }else if(strncmp("[url]",line,5) == 0){
             //find url
         }else if(strncmp("path=",line,5) == 0){
             url = av_strdup(line + 5);
             int len = strlen(url);
             //importmant for read from file!
             url[len-1] = '\0';
             printf("find url %d %s \n",len,url);
         }else if(strncmp("[video]",line,7) == 0){
            //find video tag
            //printf("find video tag.\n");
            tag = videoParams = (DecodeParams *)av_malloc(sizeof(DecodeParams));
         }else if(strncmp("[audio]",line,7) == 0){
            //find audio tag
            //printf("find audio tag.\n");
            tag = audioParams = (DecodeParams *)av_malloc(sizeof(DecodeParams));
         }else{
             if(strncmp("extra=",line,6) == 0){
                 char* tok = ",";
                 char* sub = strtok(line + 6,tok);
                 unsigned char tmp[256];
                 int size = 0;
                 while(sub != NULL){
                     //printf("%s ",sub);
                     tmp[size++] = atoi(sub);
                     sub = strtok(NULL,tok);
                 }
                 tag->extra = (unsigned char *)av_malloc(sizeof(unsigned char) * size);
                 for(int i=0; i < size; i++){
                    tag->extra[i] = tmp[i];
                    //printf("%hhu ",tag->extra[i]);
                 }
                 tag->extra_size = size;
             }else if(strncmp("code_id=",line,8) == 0){
                 tag->code_id = (AVCodecID)atoi(line + 8);
             }else if(strncmp("format=",line,7) == 0){
                 tag->format = atoi(line + 7);
             }else if(strncmp("field_order=",line,12) == 0){
                 tag->field_order = (AVFieldOrder)atoi(line + 12);
             }else if(strncmp("profile=",line,8) == 0){
                 tag->profile = atoi(line + 8);
             }else if(strncmp("level=",line,6) == 0){
                 tag->level = atoi(line + 6);
             }else if(strncmp("pts_wrap_bits=",line,14) == 0){
                 tag->pts_wrap_bits = atoi(line + 14);
             
             }else if(strncmp("qmin=",line,5) == 0){
                 tag->qmin = atoi(line + 5);
             }else if(strncmp("qmax=",line,5) == 0){
                 tag->qmax = atoi(line + 5);
             }else if(strncmp("width=",line,6) == 0){
                 tag->width = atoi(line + 6);
             }else if(strncmp("height=",line,7) == 0){
                 tag->height = atoi(line + 7);

             }else if(strncmp("aspect_num=",line,11) == 0){
                 tag->aspect.num = atoi(line + 11);
             }else if(strncmp("aspect_den=",line,11) == 0){
                 tag->aspect.den = atoi(line + 11);

             }else if(strncmp("stream_time_base_num=",line,21) == 0){
                 tag->stream_time_base.num = atoi(line + 21);
             }else if(strncmp("stream_time_base_den=",line,21) == 0){
                 tag->stream_time_base.den = atoi(line + 21);

             }else if(strncmp("codec_time_base_num=",line,20) == 0){
                 tag->codec_time_base.num = atoi(line + 20);
             }else if(strncmp("codec_time_base_den=",line,20) == 0){
                 tag->codec_time_base.den = atoi(line + 20);

             }else if(strncmp("r_frame_rate_num=",line,17) == 0){
                 tag->r_frame_rate.num = atoi(line + 17);
             }else if(strncmp("r_frame_rate_den=",line,17) == 0){
                 tag->r_frame_rate.den = atoi(line + 17);

             }else if(strncmp("avg_frame_rate_num=",line,19) == 0){
                 tag->avg_frame_rate.num = atoi(line + 19);
             }else if(strncmp("avg_frame_rate_den=",line,19) == 0){
                 tag->avg_frame_rate.den = atoi(line + 19);

             }else if(strncmp("channels=",line,9) == 0){
                 tag->channels = atoi(line + 9);
             }else if(strncmp("channel_layout=",line,15) == 0){
                 tag->channel_layout = atoi(line + 15);
             }else if(strncmp("sample_rate=",line,12) == 0){
                 tag->sample_rate = atoi(line + 12);
             }else if(strncmp("bits_per_coded_sample=",line,22) == 0){
                 tag->bits_per_coded_sample = atoi(line + 22);
             }else if(strncmp("frame_size=",line,11) == 0){
                 tag->frame_size = atoi(line + 11);

             }else if(strncmp("pkt_timebase_num=",line,17) == 0){
                 tag->pkt_timebase.num = atoi(line + 17);
             }else if(strncmp("pkt_timebase_den=",line,17) == 0){
                 tag->pkt_timebase.den = atoi(line + 17);
             }
         }
     } 
     fclose(fp);                     //关闭文件
     return 0; 
}

//set h264 sps&pps from cache?
int copy_from_cache(AVStream* st,AVStream* at){
    /**
    * 这里要注意的是，虽然直接赋值extra可以最快速度的跳过find stream info，但是画面的显示快慢与首个I帧相关！！！
    **/
    unsigned char* bak;
    int size;
    //for rtmp://live.hkstv.hk.lxdns.com/live/hks
    //GET_ARRAY_LEN(hk_sp,size);
    //bak = hk_sp;
    GET_ARRAY_LEN(videoParams->extra,size);
    bak = videoParams->extra;
    size = videoParams->extra_size;
    printf("copy_from_cache width %d\n",size);

    st->codec->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(st->codec->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(st->codec->extradata, bak, size);
    st->codec->extradata_size = size;
    /***add for new API begin***/
    st->codecpar->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(st->codecpar->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(st->codecpar->extradata, bak, size);
    st->codecpar->extradata_size = size;
    /***add for new API end***/

    //GET_ARRAY_LEN(hk_au,size);
    //bak = hk_au;
    GET_ARRAY_LEN(audioParams->extra,size);
    bak = audioParams->extra;
    size = audioParams->extra_size;

    printf("copy_from_cache width %d\n",size);
    at->codec->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(at->codec->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(at->codec->extradata, bak, size);
    at->codec->extradata_size = size;
    /***add for new API begin***/
    at->codecpar->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
    memset(at->codecpar->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
    memcpy(at->codecpar->extradata, bak, size);
    at->codecpar->extradata_size = size;
    /***add for new API end***/

    return 1;
}

int get_video_extradata(AVFormatContext *s, int video_index)
{
    int  type, size;
    int ret = -1;
    //int64_t dts;
    bool got_extradata = false;

    if (!s || video_index < 0 || video_index > 2)
        return ret;

    //Skip Previous Tag Size
    for (;; avio_skip(s->pb, 4)) {
        avio_tell(s->pb);
        /*FLV TAG:  
        **|-8--|-24-|----24---|-----8------|---24----|----|
        **|type|size|timestamp|timestamp ex|Stream ID|Data|
        **1.type: TAG中第1个字节中的前5位表示这个TAG中包含数据的类型,8 = audio,9 = video,18 = script data.
        **2.size:Stream ID之后的数据长度.
        **3.timestamp和timestampExtended组成了这个TAG包数据的PTS信息,记得刚开始做FVL demux的时候，
        **并没有考虑TimestampExtended的值,直接就把Timestamp默认为是PTS，后来发生的现 象就是画面有跳帧的现象,
        **后来才仔细看了一下文档发现真正数据的PTS是PTS= Timestamp | TimestampExtended<<24.
        **4.StreamID之后的数据就是每种格式的情况不一样了，接下格式进行详细的介绍.
        **  http://blog.csdn.net/leixiaohua1020/article/details/17934487    
        **  http://www.cnblogs.com/musicfans/archive/2012/11/07/2819291.html
        */
        type = avio_r8(s->pb);
        size = avio_rb24(s->pb);
        avio_skip(s->pb, 7);//Skip TimeStamp && Ex && Stream ID(always zero).
        //dts  = avio_rb24(s->pb);
        //dts |= avio_r8(s->pb) << 24;
        //avio_skip(s->pb, 3);//Skip Stream ID, always zero.

        if (0 == size)
            break;
        if (FLV_TAG_TYPE_META == type) {
            //if audio or meta tags, skip them.
            avio_seek(s->pb, size, SEEK_CUR);
        }else if(FLV_TAG_TYPE_AUDIO == type ){
            avio_seek(s->pb, size, SEEK_CUR);
        }else if (type == FLV_TAG_TYPE_VIDEO) {
            printf("find video flv tag success %d.\n",size);
            //if the first video tag, read the sps/pps info from it. then break.
            //For H264(avc),skip 5!
            size -= 5;
            s->streams[video_index]->codec->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
            if (NULL == s->streams[video_index]->codec->extradata)
                break;

            memset(s->streams[video_index]->codec->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
            memcpy(s->streams[video_index]->codec->extradata, s->pb->buf_ptr + 5, size);
            s->streams[video_index]->codec->extradata_size = size;
            /***add for new API begin***/
            s->streams[video_index]->codecpar->extradata = (uint8_t *)av_mallocz(size + FF_INPUT_BUFFER_PADDING_SIZE);
            if (NULL == s->streams[video_index]->codecpar->extradata)
                break;
            memset(s->streams[video_index]->codecpar->extradata, 0, size + FF_INPUT_BUFFER_PADDING_SIZE);
            memcpy(s->streams[video_index]->codecpar->extradata, s->pb->buf_ptr + 5, size);
            s->streams[video_index]->codecpar->extradata_size = size;
            /***add for new API end***/
            ret = 0;
            //got_extradata = true;
        } else  {
            break;
        }

        if (got_extradata)
            break;
    }

    return ret;
}

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
//Video:        ffmpeg -f gdigrab -video_size 640x480 -i desktop  -vcodec libx264 -preset:v ultrafast -pix_fmt yuv420p -tune:v zerolatency -f flv rtmp://192.168.1.201/live/mystream
//Audio+Video:    ffmpeg -f dshow -i audio="FrontMic (Realtek High Definition Audio)" -f gdigrab -video_size 640x480 -i desktop  -vcodec libx264 -codec:a aac -pix_fmt yuv420p -tune zerolatency -preset ultrafast -f flv rtmp://192.168.1.201/live/mystream
void init_Stream1(AVFormatContext *formatCtx, int64_t start)
{
    /*
    * 根据读取Packet方式来自动获取一些参数信息：Video+Audio
    * 如果只有Audio或者Video则优化很明显，但是当同时存在时，获取时间会大幅度提升，基于此方案，
    * 可以通过提高首个视频I帧的时机来达到优化效果。理论上来说，只要打开流时，能快速获取第一个
    * 视频包+音频包则可以达到优化效果。
    */
    //const int genpts = formatCtx->flags & AVFMT_FLAG_GENPTS;
    //formatCtx->flags |= AVFMT_FLAG_GENPTS;

    AVPacket packet;
    av_init_packet(&packet);
    while (true)
    {
        int ret1 = av_read_frame(formatCtx, &packet);
        if (packet.flags & AV_PKT_FLAG_KEY)
        {
            if(formatCtx->nb_streams == 1 && formatCtx->streams[0]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
                //we must get video stream!
                continue;
            }
            printf("Find I Frame here %d \n",formatCtx->nb_streams);
            break;
        }
    }
    printf("==========>begin find stream info 2: %0.3fs\n",(av_gettime()-start) / 1000000.0 );
    //void* pbs = &formatCtx->internal;
    //void* rpbs = &formatCtx->internal->packet_buffer;
    add_to_pktbuf(&formatCtx->internal->packet_buffer,&packet,&formatCtx->internal->packet_buffer_end);

    int videoIndex = 0, audioIndex = 0, i = 0;
    for(i = 0; i < formatCtx->nb_streams; i++)
    {
        if(formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
        }else if(formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            audioIndex = i;
        }
    }

    /***************************** Video ********************************/
    //formatCtx->streams[0] = st;
    AVStream *st = formatCtx->streams[videoIndex];
    //Init the video codec(H264).
    //st->codec->codec_id = AV_CODEC_ID_H264;
    st->codec->width = 640;//480;
    st->codec->height = 480;//288;
    //st->codec->ticks_per_frame = 2;
    st->codec->pix_fmt = AV_PIX_FMT_YUV420P;//AV_PIX_FMT_YUV420P;
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

    /***************************** Audio ********************************/
    AVStream *at = formatCtx->streams[audioIndex];
    //Audio: aac (LC) ([10][0][0][0] / 0x000A), 44100 Hz, stereo, fltp, 128 kb/s
    //at->codec->sample_rate = 8000;//44100
    at->codec->time_base.den = 44100;
    at->codec->time_base.num = 1;
    //at->codec->bits_per_coded_sample = 16; //
    //at->codec->channels = 1;2
    //at->codec->channel_layout = 4;3
    //at->codec->codec_type = AVMEDIA_TYPE_AUDIO;
    //at->codec->bit_rate = 16000;//12800
    //at->codec->refs = 1;
    at->codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    at->codec->profile = 1;
    //at->codec->level = -99;

    for(int i =0;i<st->codecpar->extradata_size;i++){
        printf("%hhu,",st->codecpar->extradata[i]);
    }
    printf("\n");

    for(int i =0;i<at->codecpar->extradata_size;i++){
        printf("%hhu ",at->codecpar->extradata[i]);
    }
    printf("\n");

    //at->pts_wrap_bits = 32;
    //at->time_base.den = 1000;
    //at->time_base.num = 1;
}

void init_Stream2(AVFormatContext *formatCtx, int64_t start)
{
    /**
    * 完全自己构建音视频流信息,此方法严重依赖第一个flv video tag的读取速度。
    * 如果想提速，可以考虑通过其它方式传输extradata信息，来实现真正优化。
    **/
    AVStream *st = avformat_new_stream(formatCtx, NULL);
    AVStream *at  = avformat_new_stream(formatCtx, NULL);
    if (!st || !at)
        return;
    /***************************** Video ********************************/
    //h264 (libx264) ([7][0][0][0] / 0x0007), yuv420p(progressive), 640x480, q=-1--1, 59 fps, 1k tbn, 59 tbc
    //h264 (High), yuv420p(progressive), 480x288 [SAR 16:15 DAR 16:9], 25 fps, 25 tbr, 1k tbn, 50 tbc
    /*add for new API begin*/
    st->codecpar->codec_id = videoParams->code_id;
    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codecpar->format = videoParams->format;
    st->codecpar->field_order = videoParams->field_order;//(progressive)
    st->sample_aspect_ratio.num = st->codecpar->sample_aspect_ratio.num = videoParams->aspect.num;
    st->sample_aspect_ratio.den = st->codecpar->sample_aspect_ratio.den = videoParams->aspect.den;
    st->codecpar->profile = videoParams->profile;
    st->codecpar->level = videoParams->level;
    //st->codecpar->video_delay = 2;
    //st->codecpar->bits_per_raw_sample = 8;
    /*add for new API end*/
    st->codec->codec_id = videoParams->code_id;
    st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    //extra begin
    st->codec->qmin = videoParams->qmin;
    st->codec->qmax = videoParams->qmax;
    //extra end
    st->codec->width = videoParams->width;//640;
    st->codec->height = videoParams->height;//480;
    //st->codec->ticks_per_frame = 1;
    st->codec->pix_fmt = (AVPixelFormat)videoParams->format;//AV_PIX_FMT_YUV420P;
    st->pts_wrap_bits = videoParams->pts_wrap_bits;
    st->codec->time_base.den = videoParams->codec_time_base.den;//60
    st->time_base.den = videoParams->stream_time_base.den;//
    st->codec->time_base.num = st->time_base.num = videoParams->stream_time_base.num;
    //st->codec->sample_fmt = AV_SAMPLE_FMT_NONE;
    st->r_frame_rate.den = st->avg_frame_rate.den = videoParams->avg_frame_rate.den;
    st->r_frame_rate.num = st->avg_frame_rate.num = videoParams->avg_frame_rate.num;//60

    /***************************** Audio ********************************/
    //Audio: aac (LC) ([10][0][0][0] / 0x000A), 44100 Hz, stereo, fltp, 128 kb/s
    /*add for new API begin*/
    at->codecpar->codec_id = audioParams->code_id;
    at->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    at->codecpar->channels = audioParams->channels;
    at->codecpar->channel_layout = audioParams->channel_layout;
    //at->codecpar->bit_rate = 128000;
    at->codecpar->sample_rate = audioParams->sample_rate;//44100;//44100;
    at->codecpar->bits_per_coded_sample = audioParams->bits_per_coded_sample;
    at->codecpar->frame_size = audioParams->frame_size;
    at->codecpar->format = audioParams->format;
    at->codecpar->profile = audioParams->profile;
    /*add for new API end*/
    at->codec->codec_id = audioParams->code_id;
    at->codec->codec_type = AVMEDIA_TYPE_AUDIO;
    at->codec->sample_rate = audioParams->sample_rate;//44100;
    at->codec->time_base.den = audioParams->codec_time_base.den;//44100;
    at->codec->time_base.num = audioParams->codec_time_base.num;
    at->codec->bits_per_coded_sample = audioParams->bits_per_coded_sample; //
    at->codec->channels = audioParams->channels;
    at->codec->channel_layout = audioParams->channel_layout;
    //at->codec->bit_rate = 128000;//128000
    //at->codec->refs = 1;
    at->codec->sample_fmt = (AVSampleFormat)audioParams->format;
    at->pts_wrap_bits = audioParams->pts_wrap_bits;
    at->codec->pkt_timebase.den = at->time_base.den = audioParams->stream_time_base.den;
    at->codec->pkt_timebase.num = at->time_base.num = audioParams->stream_time_base.num;
    at->codec->profile = audioParams->profile;
    at->codec->frame_size = audioParams->frame_size;


    // H264 need sps/pps for decoding, so read it from the first video tag.
    printf("==========>begin find get_video_extradata: %0.3fs\n",(av_gettime()-start) / 1000000.0 );
    if(1){
        copy_from_cache(st,at);
    }else{
        printf("get_video_extradata ret = %d\n",get_video_extradata(formatCtx, 0));
    }
    printf("==========>after find get_video_extradata: %0.3fs\n",(av_gettime()-start) / 1000000.0 );
    formatCtx->pb->buf_ptr = formatCtx->pb->buf_end;
    formatCtx->pb->pos = (int64_t) formatCtx->pb->buf_end;
}

/*
    rtmp://live.hkstv.hk.lxdns.com/live/hks
    Stream #0:0: Video: h264 (High), yuv420p(progressive), 480x288 [SAR 16:15 DAR 16:9], 25 fps, 25 tbr, 1k tbn, 50 tbc
    Stream #0:1: Audio: aac (LC), 48000 Hz, stereo, fltp

*/
int _tmain(int argc, char* argv[])
{
    //loadCfg("../hk.conf");
    loadCfg("../simple.conf");
    
    SimplePlayer *sp = (SimplePlayer *)av_malloc(sizeof(SimplePlayer));

    int i;

    //------
    av_register_all();
    avdevice_register_all();
    avformat_network_init();

    sp->formatCtx = avformat_alloc_context();
    sp->start = av_gettime();
    //try to find device from window
    //get_dsshow_device(pFormatCtx);
    //AVInputFormat *pFmt = av_find_input_format("dshow");
    printf("==========>begin avformat_open_input : %0.3fs\n",(av_gettime()-sp->start) / 1000000.0);

    if(avformat_open_input(&sp->formatCtx,url,NULL,NULL))
        //if(avformat_open_input(&pFormatCtx,"video=e2eSoft VCam",pFmt,NULL) != 0)
    {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    //pFormatCtx->probesize = 4096;

    printf("==========>begin find stream info 1: %0.3fs\n",(av_gettime()-sp->start) / 1000000.0 );
    if(1){
        //init_Stream1(sp->formatCtx,sp->start);
        init_Stream2(sp->formatCtx,sp->start);
    }else{
        if(avformat_find_stream_info(sp->formatCtx,NULL) < 0){
            printf("Couldn't find stream information.\n");
            return -1;
        }
    }
    printf("==========>after find stream info :%0.3fs\n",(av_gettime()-sp->start) / 1000000.0 );
    //sample_aspect_ratio
    printf("----------------File Information----------------\n");
    av_dump_format(sp->formatCtx,0,url,0);
    printf("------------------------------------------------\n");

    sp->videoIndex = sp->audioIndex = -1;

    for(i = 0; i < sp->formatCtx->nb_streams; i++){
        if(sp->formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            sp->videoIndex = i;
            break;
        }
    }

    for(i = 0; i < sp->formatCtx->nb_streams; i++){
        if(sp->formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            sp->audioIndex = i;
            break;
        }
    }

    if(sp->audioIndex == -1 || sp->audioIndex == -1){
        printf("Couldn't find video|audio stream.\n");
        return -1;
    }

    sp->vCodecCtx = sp->formatCtx->streams[sp->videoIndex]->codec;
    sp->vCodec = avcodec_find_decoder(sp->vCodecCtx->codec_id);
    sp->aCodecCtx = sp->formatCtx->streams[sp->audioIndex]->codec;
    sp->aCodec = avcodec_find_decoder(sp->aCodecCtx->codec_id);

    //for dynamic set audio params warning
    sp->aCodec->capabilities |= AV_CODEC_CAP_PARAM_CHANGE;

    if(sp->vCodec == NULL || sp->aCodec == NULL){
        printf("Codec not found.\n");
        return -1;
    }

    if(avcodec_open2(sp->vCodecCtx,sp->vCodec,NULL)< 0 || avcodec_open2(sp->aCodecCtx,sp->aCodec,NULL)< 0){
        printf("Could not open codec.\n");
        return -1;
    }

    sp->packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(sp->packet);
    sp->vFrame = av_frame_alloc();
    sp->vFrameYUV = av_frame_alloc();
    sp->aFrame = av_frame_alloc();

    //Out params for Video
    sp->vFormat = AV_PIX_FMT_YUV420P;
    sp->out_buffer = (Uint8 *)av_malloc(av_image_get_buffer_size(sp->vFormat,sp->vCodecCtx->width,sp->vCodecCtx->height,1));
    av_image_fill_arrays(sp->vFrameYUV->data,sp->vFrameYUV->linesize,sp->out_buffer,sp->vFormat,sp->vCodecCtx->width,sp->vCodecCtx->height,1);
    sp->img_convert_ctx = sws_getContext(sp->vCodecCtx->width,sp->vCodecCtx->height,sp->vCodecCtx->pix_fmt,
        sp->vCodecCtx->width,sp->vCodecCtx->height,sp->vFormat,SWS_BICUBIC,NULL,NULL,NULL);

    //Out params for Audio
    sp->out_channel_layout = AV_CH_LAYOUT_STEREO;
    sp->out_nb_samples = sp->aCodecCtx->frame_size;
    sp->out_sample_fmt = AV_SAMPLE_FMT_S16;
    sp->out_sample_rate = 44100;
    sp->out_channels = av_get_channel_layout_nb_channels(sp->out_channel_layout);
    sp->out_buffer_size = av_samples_get_buffer_size(NULL,sp->out_channels,sp->out_nb_samples,sp->out_sample_fmt,1);
    sp->out_buffer = (Uint8 *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);

    //SDL begin
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)){
        printf("Could not initialize SDL - %s\n",SDL_GetError());
        return -1;
    }

    sp->screen_w = sp->vCodecCtx->width;
    sp->screen_h = sp->vCodecCtx->height;
    sp->screen = SDL_CreateWindow("Simple ffmpeg player's window",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,sp->screen_w,sp->screen_h,SDL_WINDOW_OPENGL);

    if(!sp->screen){
        printf("SDL: could not create window - exiting:%s\n",SDL_GetError());    
        return -1;
    }

    sp->sdlRenderer = SDL_CreateRenderer(sp->screen,-1,0);
    //IYUV: Y + U + V
    //YV12: Y + V + U
    sp->sdlTexture = SDL_CreateTexture(sp->sdlRenderer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,sp->screen_w,sp->screen_h);
    sp->sdlRect.x = 0;
    sp->sdlRect.y = 0;
    sp->sdlRect.w = sp->screen_w;
    sp->sdlRect.h = sp->screen_h;

    //for audio
    sp->audioSpec.freq = sp->out_sample_rate;
    sp->audioSpec.format = AUDIO_S16SYS;
    sp->audioSpec.channels = sp->out_channels;
    sp->audioSpec.silence = 0;
    sp->audioSpec.samples = sp->out_nb_samples;
    sp->audioSpec.callback = fill_audio;
    sp->audioSpec.userdata = sp->aCodecCtx;

    if(SDL_OpenAudio(&sp->audioSpec,NULL) < 0){
        printf("cant's  open audio.\n");
        return -1;
    }
   
    //SDL end

    //FIX:Some Codec's Context Information is missing  
    int64_t in_channel_layout=av_get_default_channel_layout(sp->aCodecCtx->channels);  
    //Swr
    sp->au_convert_ctx = swr_alloc();
    sp->au_convert_ctx = swr_alloc_set_opts(sp->au_convert_ctx,
        sp->out_channel_layout,sp->out_sample_fmt,sp->out_sample_rate,
        in_channel_layout,sp->aCodecCtx->sample_fmt,sp->aCodecCtx->sample_rate
        ,0,NULL);
    swr_init(sp->au_convert_ctx);

    //Play
    SDL_PauseAudio(0);

    sp->exit = sp->pause = 0;
    sp->videoPkts = (AVPacketQueue *)av_malloc(sizeof(AVPacketQueue));
    sp->audioPkts = (AVPacketQueue *)av_malloc(sizeof(AVPacketQueue));
    sp->videoPkts->cur = sp->audioPkts->cur = NULL;
    sp->videoPkts->next = sp->audioPkts->next = NULL; 
    //sp->decAudioMutex = SDL_CreateMutex();
    sp->pktMutex = SDL_CreateMutex();

    sp->decVideo = SDL_CreateThread(read_video_thread,NULL,sp);
    sp->decAudio = SDL_CreateThread(read_audio_thread,NULL,sp);

    sp->read = SDL_CreateThread(read_thread,NULL,sp);
    //EventLoop
    for(;;){
        //Wait
        SDL_WaitEvent(&sp->event);
        if(sp->event.type == SFM_REFRESH_EVENT){
            
        }else if(sp->event.type == SDL_KEYDOWN){
            //Pause
            if(sp->event.key.keysym.sym == SDLK_SPACE){
                sp->pause = !sp->pause;
            }
        }else if(sp->event.type == SDL_QUIT){
            sp->exit = 1;
        }else if(sp->event.type == SFM_BREAK_EVENT){
            break;
        }
    }

    sws_freeContext(sp->img_convert_ctx);
    swr_free(&sp->au_convert_ctx);

    SDL_CloseAudio();
    SDL_Quit();

    av_frame_free(&sp->vFrameYUV);
    av_frame_free(&sp->vFrame);
    av_frame_free(&sp->aFrame);
    avcodec_close(sp->vCodecCtx);
    avcodec_close(sp->aCodecCtx);
    avformat_close_input(&sp->formatCtx);

    SDL_DestroyMutex(sp->pktMutex);
    av_free(sp->videoPkts);
    av_free(sp->audioPkts);
    av_free(sp);

    if(videoParams){
        av_free(videoParams->extra);
        av_free(videoParams);
    }

    if(audioParams){
        av_free(audioParams->extra);
        av_free(audioParams);
    }

    return 0;
}
