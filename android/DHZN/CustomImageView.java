
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.support.annotation.RequiresApi;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.widget.ImageView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.request.target.BitmapImageViewTarget;
import com.dct.patrol.app.GApp;
import com.dct.patrol.tools.StringUtils;

/**
 * Created by Hebing on 2017/8/8.
 * 基于 Glide 用户头像显示，可用于显示圆形的头像或者圆形的随机背景的默认头像（仿钉钉效果）
 */

@SuppressLint("AppCompatCustomView")
public class CustomImageView extends ImageView {
    private static int[] COLORS = {
            R.color.main_color, R.color.main_focus_ripple_color,
            R.color.role_yellow_gray, R.color.blue_check, R.color.default_cursor_color,
            R.color.orange, R.color.colorAccent};
    private boolean flag = false;
    private Paint paint;
    private Paint bgPaint;
    private int x, y;
    private String name;

    public CustomImageView(Context context) {
        this(context, null);
    }

    public CustomImageView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public CustomImageView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    public CustomImageView(Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        init();
    }

    protected synchronized void init() {
        if (bgPaint == null) {
            bgPaint = new Paint();
            bgPaint.setAntiAlias(true);
            bgPaint.setStyle(Paint.Style.FILL);
        }

        final int width = getWidth();
        final int height = getHeight();
        if (width != 0 && height != 0) {
            if (paint == null) {
                paint = new Paint();
                paint.setAntiAlias(true);
                paint.setColor(Color.WHITE);
                paint.setTextAlign(Paint.Align.CENTER);
            }
            final int s120 = getResources().getDimensionPixelSize(R.dimen.Size_120);//120dp
            final int s100 = getResources().getDimensionPixelSize(R.dimen.Size_100);//100dp
            final int s80 = getResources().getDimensionPixelSize(R.dimen.Size_80);//80dp
            final int s60 = getResources().getDimensionPixelSize(R.dimen.Size_60);//60dp
            if (width >= s120) {
                paint.setTextSize(getResources().getDimensionPixelSize(R.dimen.textSize_x_large));//24sp
            } else if (width >= s100) {
                paint.setTextSize(getResources().getDimensionPixelSize(R.dimen.textSize_larger));//22sp
            } else if (width >= s80) {
                paint.setTextSize(getResources().getDimensionPixelSize(R.dimen.textSize_large));//20sp
            } else if (width >= s60) {
                paint.setTextSize(getResources().getDimensionPixelSize(R.dimen.textSize_middle));//18sp
            } else {
                paint.setTextSize(getResources().getDimensionPixelSize(R.dimen.textSize_small));//16sp
            }
            Paint.FontMetrics fontMetrics = paint.getFontMetrics();
            float top = fontMetrics.top;//为基线到字体上边框的距离,即上图中的top
            float bottom = fontMetrics.bottom;//为基线到字体下边框的距离,即上图中的bottom
            x = width / 2;
            y = (int) (height / 2 - top / 2 - bottom / 2);//基线中间点的y轴计算公式
            //LogUtils.d("init:" + width + "," + height + "-" + x + "," + y);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (flag) {
            canvas.drawCircle(x, x, x, bgPaint);
            //canvas.drawBitmap(bgBmp, 0, 0, paint);
            canvas.drawText(String.valueOf(name.charAt(0)).toUpperCase(), x, y, paint);
        } else {
            super.onDraw(canvas);
        }
    }

    public void setName(String name) {
        this.name = name;
        if (!TextUtils.isEmpty(name)) {
            flag = true;
            bgPaint.setColor(getResources().getColor(getRandomColor(name)));
        }
        invalidate();
    }

    private int getRandomColor(String name) {
        int code = name.hashCode();
        return COLORS[Math.abs(code % COLORS.length)];
    }

    @Override
    public void setImageDrawable(@Nullable Drawable drawable) {
        super.setImageDrawable(drawable);
        flag = false;
        invalidate();
    }

    @Override
    public void setImageBitmap(Bitmap bm) {
        super.setImageBitmap(bm);
        flag = false;
        invalidate();
    }

    @Override
    public void setImageResource(@DrawableRes int resId) {
        super.setImageResource(resId);
        flag = false;
        invalidate();
    }


    public static void setCustImage(final CustomImageView imageView, final String name, String url) {
        if (!StringUtils.isEmpty(url) && imageView != null) {
            Glide.with(GApp.getAppContext()).load(url).asBitmap().centerCrop().into(new BitmapImageViewTarget(imageView) {
                @Override
                protected void setResource(Bitmap resource) {
                    RoundedBitmapDrawable circularBitmapDrawable = RoundedBitmapDrawableFactory.create(GApp.getAppResources(), resource);
                    circularBitmapDrawable.setCircular(true);
                    imageView.setImageDrawable(circularBitmapDrawable);
                }

                @Override
                public void onLoadFailed(Exception e, Drawable errorDrawable) {
                    super.onLoadFailed(e, errorDrawable);
                    imageView.setName(name);
                }
            });
        } else if (imageView != null) {
            imageView.setName(name);
        }
    }

    //    private static class BitmapCreator {
//        private LruCache<String, Bitmap> caches;
//        private static BitmapCreator creator = new BitmapCreator();
//
//        BitmapCreator() {
//            // 获取应用程序最大可用内存
//            int maxMemory = (int) Runtime.getRuntime().maxMemory();
//            int cacheSize = maxMemory / 16;
//            // 设置图片缓存大小为程序最大可用内存的1/16
//            caches = new LruCache<String, Bitmap>(cacheSize) {
//                @Override
//                protected int sizeOf(String key, Bitmap bitmap) {
//                    return bitmap.getByteCount();
//                }
//            };
//        }
//
//        public static BitmapCreator getCreator() {
//            return creator;
//        }
//
//        public static Bitmap getBitmap(Context context, int w, int h) {
//            final String key = w + "x" + h;
//            Bitmap bitmap = creator.caches.get(key);
//            if (bitmap == null) {
//                Drawable bg = AppCompatDrawableManager.get().getDrawable(context, R.drawable.custom_circle);
//                bitmap = drawableToBitmap(bg, w, h);
//                creator.caches.put(key, bitmap);
//            }
//            return bitmap;
//        }
//    }
//
//    public static Bitmap drawableToBitmap(Drawable drawable, int w, int h) {
//        Bitmap bitmap = Bitmap.createBitmap(
//                w,
//                h,
//                drawable.getOpacity() != PixelFormat.OPAQUE ? Bitmap.Config.ARGB_8888
//                        : Bitmap.Config.RGB_565);
//        Canvas canvas = new Canvas(bitmap);
//        //canvas.setBitmap(bitmap);
//        drawable.setBounds(0, 0, w, h);
//        drawable.draw(canvas);
//        return bitmap;
//    }
}
