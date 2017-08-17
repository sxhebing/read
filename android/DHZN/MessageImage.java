package com.dct.patrol.utils;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.NinePatchDrawable;
import android.support.v7.widget.AppCompatDrawableManager;
import android.widget.ImageView;

import com.blankj.utilcode.util.LogUtils;
import com.bumptech.glide.Glide;
import com.bumptech.glide.load.engine.DiskCacheStrategy;
import com.dct.patrol.R;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Created by Hebing on 2017/8/17.
 */

public class MessageImage {

    /**
     * 仿微信、QQ类聊天消息不规则的图片效果
     *
     * @param context
     * @param file      for save
     * @param imageView for show
     * @param source    origin bitmap
     * @param self      send or receive
     */
    public static void displayAuto(Context context, File file, ImageView imageView, Bitmap source, boolean self) {
        if (source == null) {
            Glide.with(context).load(R.drawable.ic_empty_picture)
                    .diskCacheStrategy(DiskCacheStrategy.ALL)
                    .into(imageView);
            return;
        }
        if (file != null && file.exists()) {
            Glide.with(context).load(file)
                    .diskCacheStrategy(DiskCacheStrategy.ALL)
                    .error(R.drawable.ic_empty_picture)
                    .into(imageView);
            return;
        }

        NinePatchDrawable bg = (NinePatchDrawable) AppCompatDrawableManager.get().getDrawable(context, self ? R.drawable.icon_send_msg : R.drawable.icon_receive_msg);
        float tw = context.getResources().getDimensionPixelSize(R.dimen.Size_56) * 2;//93
        //float th = bg.getIntrinsicHeight() * 2;//71

        float sw = source.getWidth();
        float sh = source.getHeight();

        float th = tw * sh / sw;

        Matrix scale = new Matrix();
        scale.setScale((tw + 10) / sw, (th + 10) / sh);
        Bitmap target = Bitmap.createBitmap(source, 0, 0, (int) sw, (int) sh, scale, false);

        int ow = (int) tw;
        int oh = (int) th;
        LogUtils.d("displayAuto:" + tw + "-" + th + "," + ow + "-" + oh);

        Bitmap obg = Bitmap.createBitmap(ow, oh, Bitmap.Config.ARGB_8888);
        bg.setBounds(0, 0, ow, oh);
        Canvas canvas = new Canvas(obg);
        bg.draw(canvas);
        canvas.save();

        Paint paint = new Paint();
        paint.setAntiAlias(true);
        PorterDuffXfermode mode = new PorterDuffXfermode(PorterDuff.Mode.SRC_IN);
        paint.setXfermode(mode);

        Canvas targetCanvas = new Canvas(obg);
        targetCanvas.drawBitmap(target, 0, 0, paint);
        targetCanvas.save();

        source.recycle();
        target.recycle();

        Bitmap finalB = obg;
        //bak
        if (file != null) {
            try {
                file.deleteOnExit();
                file.createNewFile();
                FileOutputStream out = new FileOutputStream(file);
                finalB.compress(Bitmap.CompressFormat.PNG, 50, out);
                out.flush();
                out.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        Glide.with(context).load(file)
                .diskCacheStrategy(DiskCacheStrategy.ALL)
                .error(R.drawable.ic_empty_picture)
                .into(imageView);
    }
}
