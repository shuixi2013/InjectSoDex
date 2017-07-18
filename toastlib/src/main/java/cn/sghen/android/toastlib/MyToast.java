package cn.sghen.android.toastlib;

import android.content.Context;
import android.widget.Toast;

/**
 * Created by lgb on 17-7-18.
 * MyToast
 */

public class MyToast implements IToast {

    private Toast toast;

    @Override
    public void showToast(Context context, String txt) {
        if (toast == null) {
            toast = Toast.makeText(context, "", Toast.LENGTH_LONG);
        }
        toast.setText(txt);
        toast.show();
    }
}
