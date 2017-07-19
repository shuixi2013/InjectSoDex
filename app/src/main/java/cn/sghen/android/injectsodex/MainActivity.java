package cn.sghen.android.injectsodex;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import cn.sghen.android.plugindemo.IPluginResources;
import cn.sghen.android.toastlib.IToast;
import dalvik.system.DexClassLoader;
import dalvik.system.PathClassLoader;

public class MainActivity extends AppCompatActivity {

    private final static String TAG = MainActivity.class.getSimpleName();

    private Button showToast;
    private Toast toast;
    private IToast iToast;

    private Button showPlugin;
    private ImageView imageView;

    private Button showPlugin2;
    private Button startPlugin;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        showToast = (Button) findViewById(R.id.showToast);
        showToast.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File folder = new File(Environment.getExternalStorageDirectory() + "/inject/");
                if (!folder.exists()) {
                    folder.mkdirs();
                }
                File file = new File(Environment.getExternalStorageDirectory() + "/inject/MyToast.jar");
                if (!file.exists()) {
                    try {
                        InputStream inputStream = MainActivity.this.getAssets().open("MyToast.jar");
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

                if (iToast == null) {
                    Log.d(TAG, "" +file.toString());
                    PathClassLoader pcl = new PathClassLoader(file.toString(), null, MainActivity.this.getClassLoader());
                    try {
                        Class libProviderClazz = pcl.loadClass("cn.sghen.android.toastlib.MyToast");
                        iToast = (IToast) libProviderClazz.newInstance();
                        if (iToast != null) {
                            iToast.showToast(MainActivity.this, "Hello, I'm the toast from jar");
                        } else {
                            toast.setText("IMyToast load failed");
                            toast.show();
                        }
                    } catch (Exception exception) {
                        exception.printStackTrace();
                    }
                } else {
                    iToast.showToast(MainActivity.this, "Hello, I'm the toast from jar");
                }
            }
        });

        showPlugin = (Button) findViewById(R.id.showPlugin);
        imageView = (ImageView) findViewById(R.id.imageView);

        showPlugin.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File folder = new File(Environment.getExternalStorageDirectory() + "/inject/");
                if (!folder.exists()) {
                    folder.mkdirs();
                }
                File file = new File(Environment.getExternalStorageDirectory() + "/inject/plugindemo.apk");
                file.delete();
                if (!file.exists()) {
                    try {
                        InputStream inputStream = MainActivity.this.getAssets().open("plugindemo.apk");
                        file.createNewFile();
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

                Log.d(TAG, file.toString() + " " + file.exists());
                Resources resources = Constant.getBundleResources(MainActivity.this, file.toString());
                Drawable drawable = resources.getDrawable(resources.getIdentifier("beetle", "mipmap", "cn.sghen.android.plugindemo"));
                String text = resources.getString(resources.getIdentifier("plugin_text", "string", "cn.sghen.android.plugindemo"));
                imageView.setImageDrawable(drawable);
                toast.setText(text);
                toast.show();
            }
        });

        showPlugin2 = (Button) findViewById(R.id.showPlugin2);
        showPlugin2.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File file = new File(Environment.getExternalStorageDirectory() + "/inject/plugindemo.apk");
                if (!file.exists()) {
                    toast.setText("Click the showPluginResources button to copy the plugindemo.apk");
                    toast.show();
                } else {
                    File odexPath = MainActivity.this.getDir("odex", 0);
                    if (!odexPath.exists())
                        odexPath.mkdirs();
                    String libDir = MainActivity.this.getApplicationInfo().nativeLibraryDir;

                    DexClassLoader dexClassLoader = new DexClassLoader(file.toString(),
                            odexPath.getAbsolutePath(), libDir, MainActivity.this .getClassLoader());
                    try {
                        Class aClass =  dexClassLoader.loadClass("cn.sghen.android.plugindemo.PluginResources");
                        IPluginResources iPluginResources = (IPluginResources) aClass.newInstance();
                        if (iPluginResources != null) {
                            toast.setText(iPluginResources.getString());
                        } else {
                            toast.setText("IPluginResources load failed");
                        }
                        toast.show();
                    } catch (ClassNotFoundException e) {
                        e.printStackTrace();
                    } catch (InstantiationException e) {
                        e.printStackTrace();
                    } catch (IllegalAccessException e) {
                        e.printStackTrace();
                    }
                }
            }
        });

        startPlugin = (Button) findViewById(R.id.startPlugin);
        startPlugin.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File file = new File(Environment.getExternalStorageDirectory() + "/inject/plugindemo.apk");
                if (!file.exists()) {
                    toast.setText("Click the showPluginResources button to copy the plugindemo.apk");
                    toast.show();
                } else {
                    Intent intent = new Intent(MainActivity.this, ProxyActivity.class);
                    intent.putExtra("pluginPath", file.toString());
                    //intent.putExtra("pluginClass", "cn.sghen.android.plugindemo.PluginActivity");
                    startActivity(intent);
                }
            }
        });

        toast = Toast.makeText(this, "", Toast.LENGTH_LONG);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 0);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == 0) {
        }
    }
}
