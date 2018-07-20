1.添加 android:sharedUserId="android.uid.system" 可以获取系统级权限
2.Android 8.0开始，悬浮窗口的权限有变化，除了需要在AndroidManifest配置android.permission.SYSTEM_ALERT_WINDOW权限外，动态申请的权限如下：
```
if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
    startForegroundService(startIntent);
} else {
    startService(startIntent);
}
```
3.Android 8.0开始，启动后台服务也有变化具体为：
```
if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
    startForegroundService(serviceIntent);
} else {
    startService(serviceIntent);
}
```
