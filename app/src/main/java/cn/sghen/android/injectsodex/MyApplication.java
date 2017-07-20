package cn.sghen.android.injectsodex;

import android.app.Application;
import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Array;
import java.lang.reflect.Field;

import dalvik.system.DexClassLoader;
import dalvik.system.PathClassLoader;

/**
 * Created by lgb on 17-7-19.
 * MyApplication
 */

public class MyApplication extends Application {

    private final static String TAG = MyApplication.class.getSimpleName();

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        //Log.d(TAG, "attachBaseContext() hasDexClassLoader=" + hasDexClassLoader());
        if (getSharedPreferences("hotfix", MODE_PRIVATE).getBoolean("isHotFix", false)) {
            installJar();
        }
    }

    private static boolean hasDexClassLoader() {
        try {
            Class.forName("dalvik.system.BaseDexClassLoader");
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    private void installJar() {
        Log.d(TAG, "installJar()");
        String dexPath = this.getDir("dex", 0).getAbsolutePath() + "/MyHotFix.jar";
        File file = new File(dexPath);
        if (!file.exists())
            copyJar(file);
        PathClassLoader pathClassLoader = (PathClassLoader) this.getClassLoader();
        DexClassLoader dexClassLoader = new DexClassLoader(dexPath, getDir("odex", 0).getAbsolutePath(),
                getApplicationInfo().nativeLibraryDir, this.getClassLoader());
        try {
            Log.d(TAG, "installJar() loadClass");
            Object dexElements = combineArray(
                    getDexElements(getPathList(pathClassLoader)),
                    getDexElements(getPathList(dexClassLoader)));

            Object pathList = getPathList(pathClassLoader);
            Log.d(TAG, "installJar() setField");
            setField(pathList, pathList.getClass(), "dexElements", dexElements);
            //pathClassLoader.loadClass("cn.sghen.android.hotfixlib.Student");
        } catch (ClassNotFoundException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (NoSuchFieldException e) {
            e.printStackTrace();
        }
    }

    private static void setField(Object obj, Class<?> cl, String field, Object value)
            throws NoSuchFieldException, IllegalArgumentException, IllegalAccessException {
        Field localField = cl.getDeclaredField(field);
        localField.setAccessible(true);
        localField.set(obj, value);
    }

    private static Object combineArray(Object arrayLhs, Object arrayRhs) {
        Class<?> localClass = arrayLhs.getClass().getComponentType();
        int i = Array.getLength(arrayRhs);
        int j = i + Array.getLength(arrayLhs);
        Log.e(TAG, "originLoadLength=" + i + "   dynamicLoadLength=" + j);
        Object result = Array.newInstance(localClass, j);
        for (int k = 0; k < j; ++k) {
            if (k < i) {
                Array.set(result, k, Array.get(arrayRhs, k));
            } else {
                Array.set(result, k, Array.get(arrayLhs, k - i));
            }
        }
        return result;
    }

    private static Object getDexElements(Object paramObject)
            throws IllegalArgumentException, NoSuchFieldException, IllegalAccessException {
        return getField(paramObject, paramObject.getClass(), "dexElements");
    }

    private static Object getPathList(Object baseDexClassLoader)
            throws IllegalArgumentException, NoSuchFieldException, IllegalAccessException, ClassNotFoundException {
        return getField(baseDexClassLoader, Class.forName("dalvik.system.BaseDexClassLoader"), "pathList");
    }

    private static Object getField(Object obj, Class<?> cl, String field)
            throws NoSuchFieldException, IllegalArgumentException, IllegalAccessException {
        Field localField = cl.getDeclaredField(field);
        localField.setAccessible(true);
        return localField.get(obj);
    }

    private void copyJar(File file) {
        File folder = this.getDir("dex", 0);
        if (!folder.exists()) {
            folder.mkdirs();
        }
        try {
            InputStream inputStream = this.getAssets().open("MyHotFix.jar");
            FileOutputStream outputStream = new FileOutputStream(file);
            byte[] data = new byte[1024];
            int result;
            while ((result = inputStream.read(data)) != -1) {
                outputStream.write(data, 0, result);
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
