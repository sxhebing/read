package tv.danmaku.ijk.media.player;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.util.ArrayMap;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Created by SHI-PC on 2017/6/22.
 */

@TargetApi(Build.VERSION_CODES.KITKAT)
public class ConfigParse {


    static ArrayMap<String, Config> configs = new ArrayMap<>();

    public static class AVRational {
        public int num;
        public int den;

        @Override
        public String toString() {
            return num + "/" + den;
        }
    }

    public static class ConfigParams {
        public String extra;
        public int code_id;
        public int format;
        public int profile;
        public int level;
        public AVRational stream_time_base;
        public AVRational codec_time_base;
        public int pts_wrap_bits;

        //for video
        public int field_order;
        public int qmin;
        public int qmax;
        public int width;
        public int height;
        public AVRational aspect;
        public AVRational r_frame_rate;
        public AVRational avg_frame_rate;

        //for audio
        public int channels;
        public int channel_layout;
        public int sample_rate;
        public int bits_per_coded_sample;
        public int frame_size;
        public AVRational pkt_timebase;

        ConfigParams() {
            stream_time_base = new AVRational();
            codec_time_base = new AVRational();
            aspect = new AVRational();
            r_frame_rate = new AVRational();
            avg_frame_rate = new AVRational();
            pkt_timebase = new AVRational();
        }

        @Override
        public String toString() {
            return "\n " + extra
                    + "\n code_id=" + code_id
                    + "\n format=" + format
                    + "\n profile=" + profile
                    + "\n level=" + level
                    + "\n stream_time_base=" + stream_time_base
                    + "\n codec_time_base=" + codec_time_base
                    + "\n pts_wrap_bits=" + pts_wrap_bits
                    + "\n field_order=" + field_order
                    + "\n qmin=" + qmin
                    + "\n qmax=" + qmax
                    + "\n width=" + width
                    + "\n height=" + height
                    + "\n aspect=" + aspect
                    + "\n r_frame_rate=" + r_frame_rate
                    + "\n avg_frame_rate=" + avg_frame_rate
                    + "\n channels=" + channels
                    + "\n channel_layout=" + channel_layout
                    + "\n sample_rate=" + sample_rate
                    + "\n bits_per_coded_sample=" + bits_per_coded_sample
                    + "\n frame_size=" + frame_size
                    + "\n pkt_timebase=" + pkt_timebase;
        }
    }

    static class Config {
        public String url;
        String configName;
        public ConfigParams videoParams;
        public ConfigParams audioParams;
    }

    public static boolean hasConfig(String palyUrl) {
        return configs.containsKey(palyUrl);
    }

    public static ConfigParams getVideoParams(String palyUrl) {
        if (!hasConfig(palyUrl)) return null;
        return configs.get(palyUrl).videoParams;
    }

    public static ConfigParams getAudioParams(String palyUrl) {
        if (!hasConfig(palyUrl)) return null;
        return configs.get(palyUrl).audioParams;
    }

    public static void prepare(Context context, String[]... confs) {
        if (confs == null) return;
        for (String[] conf : confs) {
            if (conf.length != 2) continue;
            final Config config = new Config();
            config.url = conf[0];
            config.configName = conf[1];
            if (prepare(context, config)) {
                configs.put(config.url, config);
            }
        }
    }

    static boolean prepare(Context context, Config config) {
        Log.e("Parse", "prepare for " + config.url + " with " + config.configName);
        //only prepare for rtmp
        if (config.url != null && config.url.startsWith("rtmp://")) {
            String basePath = context.getFilesDir().getAbsolutePath();
            String conf = basePath + "/" + config.configName + ".conf";
            BufferedReader reader = null;
            ConfigParams tag = null;
            try {
                final Resources res= context.getResources();
                final int rawId = res.getIdentifier(config.configName,"raw",context.getPackageName());
                copyIfNotExist(context, rawId, conf);
                reader = new BufferedReader(new FileReader((conf)));
                String line = reader.readLine();
                while (line != null) {
                    if (line.startsWith("#")) {
                        //TODO
                    } else if (line.startsWith("[url]")) {
                        //TODO
                    } else if (line.startsWith("path=")) {
                        final String p = line.substring(5);
                        if (config.url.equals(p.trim())) {
                            //find it?
                            Log.d("Parse", "1 fit it !!! " + p);
                        } else {
                            Log.d("Parse", "2 not fit it ? " + config.url + " & " + p);
                            break;
                        }
                    } else if (line.startsWith("[video]")) {
                        Log.d("Parse", "find video tag");
                        tag = config.videoParams = new ConfigParams();
                    } else if (line.startsWith("[audio]")) {
                        Log.d("Parse", "find audio tag");
                        tag = config.audioParams = new ConfigParams();
                    } else {
                        if (line.startsWith("extra=")) {
                            tag.extra = line.substring(6);
                        } else if (line.startsWith("code_id=")) {
                            tag.code_id = Integer.parseInt(line.substring(8));
                        } else if (line.startsWith("format=")) {
                            tag.format = Integer.parseInt(line.substring(7));
                        } else if (line.startsWith("field_order=")) {
                            tag.field_order = Integer.parseInt(line.substring(12));
                        } else if (line.startsWith("profile=")) {
                            tag.profile = Integer.parseInt(line.substring(8));
                        } else if (line.startsWith("level=")) {
                            tag.level = Integer.parseInt(line.substring(6));
                        } else if (line.startsWith("pts_wrap_bits=")) {
                            tag.pts_wrap_bits = Integer.parseInt(line.substring(14));
                        } else if (line.startsWith("qmin=")) {
                            tag.qmin = Integer.parseInt(line.substring(5));
                        } else if (line.startsWith("qmax=")) {
                            tag.qmax = Integer.parseInt(line.substring(5));
                        } else if (line.startsWith("width=")) {
                            tag.width = Integer.parseInt(line.substring(6));
                        } else if (line.startsWith("height=")) {
                            tag.height = Integer.parseInt(line.substring(7));

                        } else if (line.startsWith("aspect_num=")) {
                            tag.aspect.num = Integer.parseInt(line.substring(11));
                        } else if (line.startsWith("aspect_den=")) {
                            tag.aspect.den = Integer.parseInt(line.substring(11));

                        } else if (line.startsWith("stream_time_base_num=")) {
                            tag.stream_time_base.num = Integer.parseInt(line.substring(21));
                        } else if (line.startsWith("stream_time_base_den=")) {
                            tag.stream_time_base.den = Integer.parseInt(line.substring(21));

                        } else if (line.startsWith("codec_time_base_num=")) {
                            tag.codec_time_base.num = Integer.parseInt(line.substring(20));
                        } else if (line.startsWith("codec_time_base_den=")) {
                            tag.codec_time_base.den = Integer.parseInt(line.substring(20));

                        } else if (line.startsWith("r_frame_rate_num=")) {
                            tag.r_frame_rate.num = Integer.parseInt(line.substring(17));
                        } else if (line.startsWith("r_frame_rate_den=")) {
                            tag.r_frame_rate.den = Integer.parseInt(line.substring(17));

                        } else if (line.startsWith("avg_frame_rate_num=")) {
                            tag.avg_frame_rate.num = Integer.parseInt(line.substring(19));
                        } else if (line.startsWith("avg_frame_rate_den=")) {
                            tag.avg_frame_rate.den = Integer.parseInt(line.substring(19));

                        } else if (line.startsWith("channels=")) {
                            tag.channels = Integer.parseInt(line.substring(9));
                        } else if (line.startsWith("channel_layout=")) {
                            tag.channel_layout = Integer.parseInt(line.substring(15));
                        } else if (line.startsWith("sample_rate=")) {
                            tag.sample_rate = Integer.parseInt(line.substring(12));
                        } else if (line.startsWith("bits_per_coded_sample=")) {
                            tag.bits_per_coded_sample = Integer.parseInt(line.substring(22));
                        } else if (line.startsWith("frame_size=")) {
                            tag.frame_size = Integer.parseInt(line.substring(11));

                        } else if (line.startsWith("pkt_timebase_num=")) {
                            tag.pkt_timebase.num = Integer.parseInt(line.substring(17));
                        } else if (line.startsWith("pkt_timebase_den=")) {
                            tag.pkt_timebase.den = Integer.parseInt(line.substring(17));
                        }
                    }
                    line = reader.readLine();
                }
                Log.d("Parse", "Success ---------------------------------------->");
                Log.d("Parse", "Success to parse:" + config.videoParams + " " + config.audioParams);
                Log.d("Parse", "Success <----------------------------------------");
                return true;
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                if (reader != null) {
                    try {
                        reader.close();
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
        return false;
    }

    static void copyIfNotExist(Context context, int ressourceId, String target) throws IOException {
        File lFileToCopy = new File(target);
        if (!lFileToCopy.exists()) {
            copyFromPackage(context, ressourceId, lFileToCopy.getName());
        }
    }

    static void copyFromPackage(Context context, int ressourceId, String target) throws IOException {
        FileOutputStream lOutputStream = context.openFileOutput(target, 0);
        InputStream lInputStream = context.getResources().openRawResource(ressourceId);
        int readByte;
        byte[] buff = new byte[8048];
        while ((readByte = lInputStream.read(buff)) != -1) {
            lOutputStream.write(buff, 0, readByte);
        }
        lOutputStream.flush();
        lOutputStream.close();
        lInputStream.close();
    }

}
