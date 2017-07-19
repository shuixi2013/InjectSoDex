package cn.sghen.android.plugindemo;

import android.app.Activity;
import android.os.Bundle;

/**
 * Created by lgb on 17-7-19.
 * IPlugin: a interface for the host activity to call
 */

public interface IPlugin {
    void setProxy(Activity proxyActivity, String pluginPath);
    void onPluginCreate(Bundle savedInstanceState);
}