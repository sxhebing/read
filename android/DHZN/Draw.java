    //2017-9-11 最近一直在码应用，技术整体上是 避难就易，不在其位就不瞎操心了，下面是最近自定义View绘制文本经常需要用到的一段代码（实现文字居中）：
    /**Begin*/
    //绘制文本Paint
    Paint textPaint = new Paint();
    textPaint.setAntiAlias(true);
    textPaint.setColor(Color.WHITE);
    textPaint.setTextAlign(Paint.Align.CENTER);
    //设置字大小
    int textSize = getResources().getDimensionPixelSize(R.dimen.textSize_x_small);
    textPaint.setTextSize(textSize);
    //计算绘制字体的坐标x、y
    Paint.FontMetrics fontMetrics = textPaint.getFontMetrics();
    float top = fontMetrics.top;//为基线到字体上边框的距离,即上图中的top
    float bottom = fontMetrics.bottom;//为基线到字体下边框的距离,即上图中的bottom
    //w和h为View的宽跟高
    int x = w / 2;
    int y = (int) (h / 2 - top / 2 - bottom / 2);//基线中间点的y轴计算公式
    /**End*/
