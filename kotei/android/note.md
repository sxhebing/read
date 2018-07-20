1.添加 android:sharedUserId="android.uid.system" 可以获取系统级权限
2.Android 8.0开始，悬浮窗口的权限有变化，除了需要在AndroidManifest配置android.permission.SYSTEM_ALERT_WINDOW权限外，动态申请的权限如下：
```
//通过如下方式判断是否有权限
Settings.canDrawOverlays(this);
//通过如下方式来申请权限
if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.M) {
    checkPermissionsAndStart();
    return;
}
Intent myIntent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
myIntent.setData(Uri.parse("package:" + getPackageName()));
startActivityForResult(myIntent, 1001);
//对应的窗体，添加如下代码避免异常
int LAYOUT_FLAG;
if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
    LAYOUT_FLAG = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
} else {
    LAYOUT_FLAG = WindowManager.LayoutParams.TYPE_PHONE;
}
getWindow().setType(LAYOUT_FLAG);
```
3.Android 8.0开始，启动后台服务也有变化具体为：
```
if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
    startForegroundService(serviceIntent);
} else {
    startService(serviceIntent);
}
```
