package cn.sghen.android.injectsodex;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.lang.reflect.Constructor;

import cn.sghen.android.plugindemo.IPlugin;
import dalvik.system.DexClassLoader;

/**
 * Created by lgb on 17-7-19.
 */

public class ProxyActivity extends Activity {

    private final static String TAG = ProxyActivity.class.getSimpleName();

    private String pluginPath;
    private String pluginClass;

    private Class<?> mLoadClass;
    private Object mLoadObject;
    private IPlugin iPlugin;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "onCreate()");

        Intent intent = getIntent();
        if (intent != null) {
            pluginPath = intent.getStringExtra("pluginPath");
            if (pluginPath == null) {
                Toast.makeText(this, "pluginPath is null", Toast.LENGTH_LONG).show();
                this.finish();
            } else {
                pluginClass = intent.getStringExtra("pluginClass");
                if (pluginClass == null) {
                    startPluginDefaultActivity();
                } else {
                    startPluginActivity(pluginClass);
                }
            }
        }
    }

    private void startPluginDefaultActivity() {
        Log.d(TAG, "startPluginDefaultActivity() " + pluginPath);
        PackageInfo info = getPackageManager().getPackageArchiveInfo(pluginPath, PackageManager.GET_ACTIVITIES);
        if (info != null && info.activities != null && info.activities.length > 0) {
            pluginClass = info.activities[0].name;
            startPluginActivity(pluginClass);
        }
    }

    private void startPluginActivity(String pluginClass) {
        Log.d(TAG, "startPluginActivity() " + pluginPath + " \n" + pluginClass);
        File dexOutputDir = this.getDir("odex", 0);
        String dexOutputPath = dexOutputDir.getAbsolutePath();
        DexClassLoader dexClassLoader = new DexClassLoader(pluginPath, dexOutputPath, null,
                ProxyActivity.this.getClassLoader()); // ClassLoader.getSystemClassLoader(),用这个会出现接口转换失败??
        try {
            mLoadClass = dexClassLoader.loadClass(pluginClass);
            Constructor<?> localConstructor = mLoadClass.getConstructor(new Class[]{});
            mLoadObject = localConstructor.newInstance(new Object[]{});

            Class superClass = mLoadObject.getClass().getSuperclass();
            Log.d(TAG, "superClass = " + superClass.getName());
            //Class[] interfaces = superClass.getInterfaces();
            //Log.d(TAG, "interfaces[0] = " + interfaces[0].getName());
            // use the interface instead of the reflect
            iPlugin = (IPlugin) mLoadObject;
            iPlugin.setProxy(this, pluginPath);
            iPlugin.onPluginCreate(new Bundle());

//            /* 反射 调用插件中的设置代理 */
//            Method setProxy = mLoadClass.getMethod("setProxy", new Class[]{Activity.class, String.class});
//            setProxy.setAccessible(true);
//            setProxy.invoke(mLoadObject, new Object[]{this, pluginPath});
//
//            /* 反射告诉插件是被宿主调起的*/
//            Method onCreate = mLoadClass.getDeclaredMethod("onCreate", new Class[]{Bundle.class});
//            onCreate.setAccessible(true);
//            Bundle bundle = new Bundle();
//            bundle.putInt("Host", 1);
//            onCreate.invoke(mLoadObject, new Object[]{bundle});

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
