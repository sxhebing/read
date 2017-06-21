
```cpp
/*
	测试环境：vs2010、ffmpeg3.3.1、sdl2.0（sdl与优化无关）
	
	相关文章https://jiya.io/archives/vlc_optimize_1.html 与 http://blog.csdn.net/leo2007608/article/details/53421528
	
	基于win10测试，直接采用ffmpg命令推送rtmp流，命令如下,以H264编码方式推送桌面左上角640x480区域至rtmp服务器:
	
	ffmpeg -f gdigrab -video_size 640x480 -i desktop 
	-vcode libx264 -preset:v ultrafast -tune:v zerolatency -f flv rtmp://192.168.1.201/live/mystream
	
	整体思路参照上面二篇文章，但有不少出入：在avformat_open_input后，不采用avformat_find_stream_info，而采用自定义函数初始化流信息。
	目前实现方式有如下二种：
	1.通过调用av_read_frame函数，获取首个音频+视频包后，再初始化一些必要的配置信息即可。当av_read_frame获取到音频/视频包时会初始化
	  相关stream的信息。优点是操作简单，缺点是首个视频包的获取时间不确定性，如果获取时间过长则达不到优化效果，当然你可以考虑
	  减少I帧的间隔|CDN服务器缓存首个I帧等策略来实现优化。
	2.另外一种方式，理解起来则相对简单暴力，直接自己手动构建音频流和视频流。优点是理解起来更直观，同时如果设计合理，可以比第一种情况优化得更
	  彻底，效果更好，其缺点也比较突出，因为H264的解码器解码时需要sps和pps信息，那么我们必须要从网络流pb中，获取视频的extradata，基于此
	  获取时间难以保持稳定，即优化结果可能时好时坏。此方案最好能快速的获取exreadata时采用，比如直接从服务器获取，然后赋值避开对网络流pb的
	  读取操作，否则不如方案一稳定。
	
	另外需要注意的是，测试的ffmpeg版本是3.3.1，其音视频相关的解码器中多了一个codecpar的概念，所以对此在代码中，做了相关注释说明如下：
	**add for new API begin**
	//code	
	**add for new API end**
	同样的，如果你的版本相对较老，则可以去掉此代码块
	
	基于上面分析二种方案的实现代码如下：
*/
//方案一:
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
//Audio+Video:  ffmpeg -f dshow -i audio="FrontMic (Realtek High Definition Audio)" -f gdigrab 
//		-video_size 640x480 -i desktop  -vcodec libx264 -codec:a aac -pix_fmt yuv420p 
//		-tune zerolatency -preset ultrafast -f flv rtmp://192.168.1.201/live/mystream
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
    st->codec->width = 640;
    st->codec->height = 480;
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

    //at->pts_wrap_bits = 32;
    //at->time_base.den = 1000;
    //at->time_base.num = 1;
}
//方案二：
enum {
    FLV_TAG_TYPE_AUDIO = 0x08,
    FLV_TAG_TYPE_VIDEO = 0x09,
    FLV_TAG_TYPE_META  = 0x12,
};

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
        if (FLV_TAG_TYPE_AUDIO == type || FLV_TAG_TYPE_META == type) {
            //if audio or meta tags, skip them.
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
            got_extradata = true;
        } else  {
            break;
        }

        if (got_extradata)
            break;
    }

    return ret;
}
void init_Stream2(AVFormatContext *formatCtx, int64_t start)
{
    /**
    * 完全自己构建音视频流信息,次方法严重依赖第一个flv video tag的读取速度。
    * 如果想提速，可以考虑通过其它方式传输extradata信息，来实现真正优化。
    **/
    AVStream *st = avformat_new_stream(formatCtx, NULL);
    AVStream *at  = avformat_new_stream(formatCtx, NULL);
    if (!st || !at)
        return;
    /***************************** Video ********************************/
    //h264 (libx264) ([7][0][0][0] / 0x0007), yuv420p(progressive), 640x480, q=-1--1, 59 fps, 1k tbn, 59 tbc
    /*add for new API begin*/
    st->codecpar->codec_id = AV_CODEC_ID_H264;
    st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    /*add for new API end*/
    st->codec->codec_id = AV_CODEC_ID_H264;
    st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    st->codec->width = 640;
    st->codec->height = 480;
    st->codec->ticks_per_frame = 1;
    st->codec->pix_fmt = AV_PIX_FMT_YUV420P;//AV_PIX_FMT_YUV420P;
    st->pts_wrap_bits = 32;
    st->codec->time_base.den = st->time_base.den = 1000;
    st->codec->time_base.num = st->time_base.num = 1;
    st->codec->sample_fmt = AV_SAMPLE_FMT_NONE;
    st->r_frame_rate.den = st->avg_frame_rate.den = 2;
    st->r_frame_rate.num = st->avg_frame_rate.num = 60;
    // H264 need sps/pps for decoding, so read it from the first video tag.
    printf("==========>begin find get_video_extradata: %0.3fs\n",(av_gettime()-start) / 1000000.0 );
    printf("get_video_extradata ret = %d\n",get_video_extradata(formatCtx, 0));
    printf("==========>after find get_video_extradata: %0.3fs\n",(av_gettime()-start) / 1000000.0 );
    formatCtx->pb->buf_ptr = formatCtx->pb->buf_end;
    formatCtx->pb->pos = (int64_t) formatCtx->pb->buf_end;

    /***************************** Audio ********************************/
    //Audio: aac (LC) ([10][0][0][0] / 0x000A), 44100 Hz, stereo, fltp, 128 kb/s
    /*add for new API begin*/
    at->codecpar->codec_id = AV_CODEC_ID_AAC;
    at->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    at->codecpar->channels = 2;
    at->codecpar->channel_layout = 3;
    at->codecpar->bit_rate = 128000;
    at->codecpar->sample_rate = 44100;
    /*add for new API end*/
    at->codec->codec_id = AV_CODEC_ID_AAC;
    at->codec->codec_type = AVMEDIA_TYPE_AUDIO;
    at->codec->sample_rate = 44100;
    at->codec->time_base.den = 44100;
    at->codec->time_base.num = 1;
    at->codec->bits_per_coded_sample = 16; //
    at->codec->channels = 2;
    at->codec->channel_layout = 3;
    at->codec->bit_rate = 128000;//128000
    at->codec->refs = 1;
    at->codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    at->codec->profile = 1;
    at->codec->level = -99;
    at->pts_wrap_bits = 32;
    at->time_base.den = 1000;
    at->time_base.num = 1;
}
```
	//优化后，二种方案相比差别不大，其实质都是需要先获取到一个视频帧，区别在于一个读取视频帧，一个读取视频的flv tag
	//测试结果波动很大：时间从0.588s - ??? ，原因可以看上面的解释

	==========>begin avformat_open_input : 0.000s
	Metadata:
	  Server                NGINX RTMP (github.com/arut/nginx-rtmp-module)
	  width                 640.00
	  height                480.00
	  displayWidth          640.00
	  displayHeight         480.00
	  duration              0.00
	  framerate             30.00
	  fps                   30.00
	  videodatarate         0.00
	  videocodecid          7.00
	  audiodatarate         125.00
	  audiocodecid          10.00
	==========>begin find stream info 1: 0.378s
	==========>begin find get_video_extradata: 0.379s
	find video flv tag success 43.
	get_video_extradata ret = 0
	==========>after find get_video_extradata: 0.588s
	==========>after find stream info :0.588s
	----------------File Information----------------
	Input #0, live_flv, from 'rtmp://192.168.1.201/live/mystream':
	  Duration: N/A, start: 0.000000, bitrate: N/A
		Stream #0:0: Video: h264, none, 30 fps, 30 tbr, 1k tbn, 1k tbc
		Stream #0:1: Audio: aac, 44100 Hz, stereo, 128 kb/s
	------------------------------------------------

	补充说明：
	Stream #0:0: Video: h264 (High), yuv420p(progressive), 480x288 [SAR 16:15 DAR 16:9], 25 fps, 25 tbr, 1k tbn, 50 tbc
	1. 0:0 实时流的索引:音视频等流的序号
	2. Video 即 st->codecpar->codec_type 流类型
	3. h264 (High) 即 st->codecpar->codec_id、st->codecpar->profile、st->codecpar->level 编码
	4. yuv420p(progressive)即 st->codecpar->format、st->codecpar->field_order 图片格式
	5. 480x288 图像大小
	6. [SAR 16:15 DAR 16:9] 即 st->sample_aspect_ratio、st->codecpar->sample_aspect_ratio、st->codecpar->width、
		st->codecpar->height 像素宽高比
	7. 25 fps 即 st->avg_frame_rate	平均帧率
	8. 25 tbr 即 st->r_frame_rate		帧率
	9. 1k tbn 即 st->time_base			流时间精度
	10.50 tbc 即 st->codec->time_base 	视频时间精度

[Demo](https://github.com/sxhebing/read/blob/master/ffmpeg/demo/1)
