package cn.sghen.android.injectsodex;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import cn.sghen.android.toastlib.IToast;
import dalvik.system.PathClassLoader;

public class MainActivity extends AppCompatActivity {

    private final static String TAG = MainActivity.class.getSimpleName();

    private Button showToast;
    private Toast toast;
    private IToast iToast;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        showToast = (Button) findViewById(R.id.showToast);
        showToast.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File file = new File(Environment.getExternalStorageDirectory() + "/inject/MyToast.jar");
                if (!file.exists()) {
                    try {
                        InputStream inputStream = MainActivity.this.getAssets().open("MyToast.jar");
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
