
```cpp
/*
	测试环境：vs2010、ffmpeg3.3.1、sdl2.0（sdl与优化无关）
	
	相关文章https://jiya.io/archives/vlc_optimize_1.html 与 http://blog.csdn.net/leo2007608/article/details/53421528
	
	基于win10测试，直接采用ffmpg命令推送rtmp流，命令如下,以H264编码方式推送桌面左上角640x480区域至rtmp服务器:
	
	ffmpeg -f gdigrab -video_size 640x480 -i desktop 
	-vcode libx264 -preset:v ultrafast -tune:v zerolatency -f flv rtmp://192.168.1.201/live/mystream
	
	整体思路参照上面二篇文章，但有不少出入：在avformat_open_input后，不采用avformat_find_stream_info，而采用自定义函数初始化流信息,
	另外这里仅做了测试视频流的部分，音频流后续补充说明,测试发现需要对AVFormatContext对象中的packetBuffer进行赋值操作，否则无法播放视频，而
	调用av_read_frame会获取流信息，所以在实现过程中，可以省略部分初始化操作而仅仅关注与部分参数的设定。
	
	基于上面分析代码看起来像是这样的：
*/
	//add_to_pktbuf函数可以参考libavformat/utils.c
	static int add_to_pktbuf(AVPacketList **packet_buffer, AVPacket *pkt,
                         AVPacketList **plast_pktl)
	{
		AVPacketList *pktl = av_mallocz(sizeof(AVPacketList));
		int ret;

		if (!pktl)
			return AVERROR(ENOMEM);

		pktl->pkt = *pkt;

		if (*packet_buffer)
			(*plast_pktl)->next = pktl;
		else
			*packet_buffer = pktl;

		/* Add the packet in the buffered packet list. */
		*plast_pktl = pktl;
		return 0;
	}

	void initVideoStream(AVFormatContext *formatCtx)
	{
		//first set packet
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
		//这里要说明的就是新版中不能直接获取packet_buffer与packet_buffer_end了，需要找到ffmpeg源码中的libavformat/internal.h文件，
		//将struct AVFormatInternal的定义声明一次，才能使用如下代码
		add_to_pktbuf(&formatCtx->internal->packet_buffer,&packet,&formatCtx->internal->packet_buffer_end);
		//设置相关参数，这里需要注意：width、height、pix_fmt，另外time_base与r_frame_rate参数st与avg_frame_rate设定，当然也可以手动指定。
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
		//may be not need it！
		formatCtx->pb->pos = (int64_t) formatCtx->pb->buf_end;
	}
```
	4.5 运行结果：
	优化前
	==========>begin avformat_open_input : 0.000s
	Metadata:
	  Server                NGINX RTMP (github.com/arut/nginx-rtmp-module)
	  width                 640.00
	  height                480.00
	  displayWidth          640.00
	  displayHeight         480.00
	  duration              0.00
	  framerate             54.00
	  fps                   54.00
	  videodatarate         0.00
	  videocodecid          7.00
	  audiodatarate         0.00
	  audiocodecid          0.00
	==========>begin find stream info : 1.585s
	==========>after find stream info :10.598s
	----------------File Information----------------
	Input #0, live_flv, from 'rtmp://192.168.1.201/live/mystream':
	  Metadata:
		Server          : NGINX RTMP (github.com/arut/nginx-rtmp-module)
		displayWidth    : 640
		displayHeight   : 480
		fps             : 54
		profile         :
		level           :
	  Duration: 00:00:00.00, start: 19596.407000, bitrate: N/A
		Stream #0:0: Video: h264 (High 4:4:4 Predictive), yuv444p(progressive), 640x480, 54 fps, 54 tbr, 1k tbn, 108 tbc
	------------------------------------------------

	优化后
	==========>begin avformat_open_input : 0.000s
	Metadata:
	  Server                NGINX RTMP (github.com/arut/nginx-rtmp-module)
	  width                 640.00
	  height                480.00
	  displayWidth          640.00
	  displayHeight         480.00
	  duration              0.00
	  framerate             54.00
	  fps                   54.00
	  videodatarate         0.00
	  videocodecid          7.00
	  audiodatarate         0.00
	  audiocodecid          0.00
	==========>begin find stream info : 1.716s
	==========>after find stream info :1.716s
	----------------File Information----------------
	Input #0, live_flv, from 'rtmp://192.168.1.201/live/mystream':
	  Metadata:
		Server          : NGINX RTMP (github.com/arut/nginx-rtmp-module)
		displayWidth    : 640
		displayHeight   : 480
		fps             : 54
		profile         :
		level           :
	  Duration: 00:00:00.00, start: 0.000000, bitrate: N/A
		Stream #0:0: Video: h264, none, 54 fps, 54 tbr, 1k tbn, 1k tbc
	------------------------------------------------
	4.5.1 关于Stream信息的差异部分，待续。
	

[Demo](https://github.com/sxhebing/read/blob/master/ffmpeg/zDemo1.md)
