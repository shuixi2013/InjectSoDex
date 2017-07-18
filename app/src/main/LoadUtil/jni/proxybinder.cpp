/*
 * importdex.cpp
 *
 *  Created on: 2014年6月24日
 *      Author: boyliang
 */

#include <stdio.h>
#include <stddef.h>
#include <jni.h>
#include <android_runtime/AndroidRuntime.h>
#include <binder/IServiceManager.h>
#include <binder/Binder.h>
#include <utils/RefBase.h>
#include <android/log.h>

#include "log/log.h"
#include "proxybinder.h"
#include "DummyJavaBBinder.h"


#include <unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <jni.h>
#include <dlfcn.h>

using namespace android;

static const char JSTRING[] = "Ljava/lang/String;";
static const char JCLASS_LOADER[] = "Ljava/lang/ClassLoader;";
static const char JCLASS[] = "Ljava/lang/Class;";

static JNIEnv* jni_env;
static char sig_buffer[512];

#define LOG_TAG "INJECT"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)



//__attribute__ ((__constructor__))
void invoke_dex_method(char* pkg, int status) {
	LOGI("invoke_dex_method is Executing!! %s %d", pkg, status);
    jni_env = AndroidRuntime::getJNIEnv();
	//JavaVM* jvm = AndroidRuntime::getJavaVM();
    //LOGI("jvm is %p",jvm);

	//jvm->AttachCurrentThread(&jni_env, NULL);
	//LOGI("jni is %p",jni_env);


	jclass class_loader_claxx = jni_env->FindClass("java/lang/ClassLoader");
    //snprintf(sig_buffer, 512, "()%s", JCLASS_LOADER);
    jmethodID getSystemClassLoader_method = jni_env->GetStaticMethodID(class_loader_claxx, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = jni_env->CallStaticObjectMethod(class_loader_claxx, getSystemClassLoader_method);
    if(class_loader == NULL)
        LOGI("class_loader is null...");
    LOGI("apk_path NewStringUTF");
    jstring apk_path = jni_env->NewStringUTF("/data/data/cn.dream.android.systemaccelerate/myfile/dexInject.dex");
    // dex_out_path可写可读
    char* cache_path;
    char* method_sign;
    char* method_name;
    if(status == 0) {
        LOGI("compare status = 0");
        cache_path = "/data/data/cn.dream.android.systemaccelerate/cache";
        method_sign = "(I)[Ljava/lang/Object;";
        method_name = "invoke";
    } else {
        LOGI("compare status = 1");
        char *pre = "/data/data/";
        char *nex = "/cache";
	cache_path = (char*)malloc(strlen(pre) + strlen(pkg) + strlen(nex) + 1);
        strcpy(cache_path, pre);
        strcat(cache_path, pkg);
        strcat(cache_path, nex);
        method_sign = "()V";
        method_name = "dexInject";
    }
    LOGI("method_name %s", cache_path);
	jstring dex_out_path = jni_env->NewStringUTF(cache_path);
    LOGI("compare dex_out_path 2");
	//snprintf(sig_buffer, 512, "(%s%s%s%s)V", JSTRING, JSTRING, JSTRING, JCLASS_LOADER);
	//LOGI("jni is %s",sig_buffer);
	jclass dexloader_claxx = jni_env->FindClass("dalvik/system/DexClassLoader");
	if(dexloader_claxx == NULL)
        LOGI("dexloader_claxx is null...");

	jmethodID dexloader_init_method = jni_env->GetMethodID(dexloader_claxx, "<init>",
	    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if(dexloader_init_method == NULL)
        LOGI("dexloader_init_method is null..");
	//snprintf(sig_buffer, 512, "(%s)%s", JSTRING, JCLASS);
	//LOGI("jni is %s",sig_buffer);
	//check_value(class_loader);


	LOGI("step1...");
	jobject dex_loader_obj = jni_env->NewObject(dexloader_claxx, dexloader_init_method, apk_path, dex_out_path, NULL, class_loader);
	if(dex_loader_obj == NULL)
        LOGI("dex_loader_obj is null...");

    snprintf(sig_buffer, 512, "(%s)%s", JSTRING, JCLASS);
    LOGI("step2-1... %s", sig_buffer);
    jmethodID loadClass_method = jni_env->GetMethodID(dexloader_claxx, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if(loadClass_method == NULL)
        LOGI("loadClass_method is null...");

    LOGI("step2-2...");
	jstring class_name = jni_env->NewStringUTF("cn/dream/android/adscreen/adUtils/HookUtil");

    LOGI("step2-3...dex_out_path=%s method_name=%s", jni_env->GetStringUTFChars(dex_out_path,0),method_name);
	jclass entry_class = (jclass)jni_env->CallObjectMethod(dex_loader_obj, loadClass_method, class_name);
	LOGI("step2-4...");
	if(entry_class == NULL)
        LOGI("entry_class is null...");

	LOGI("step3... "); // (I)[Ljava/lang/Object;   Ljava/lang/Object表示Object函数类型, [表示数组, (I)表示参数int类型, 则对应表示 Object[] invoke(int arg);
	// 根据状态选择要调用的Java函数: dexInject() or invoke()
	jmethodID invoke_method = jni_env->GetStaticMethodID(entry_class, method_name, method_sign);
	if(invoke_method == NULL)
        LOGI("invoke_method is null...");
	//check_value(invoke_method);


    LOGI("step4...%d", status);
    if(status == 0) {
	LOGI("step5-1...");
        jobjectArray objectarray = (jobjectArray) jni_env->CallStaticObjectMethod(entry_class, invoke_method, 0);
        //check_value(objectarray);

        LOGI("step5-2...");
        jsize size = jni_env->GetArrayLength(objectarray);

        LOGI("step6...");
        sp<IServiceManager> servicemanager = defaultServiceManager();

        LOGI("size is : %d",size);

        for (jsize i = 0; i < size; i += 2) {
            jstring name = static_cast<jstring>(jni_env->GetObjectArrayElement(objectarray, i));
            jobject obj = jni_env->GetObjectArrayElement(objectarray, i + 1);

            const char* c_name = jni_env->GetStringUTFChars(name, NULL);
            LOGI("c_name is %s",c_name);

            DummyJavaBBinder* binder = (DummyJavaBBinder*) servicemanager->getService(String16(c_name)).get();
            binder->changObj(jni_env->NewGlobalRef(obj));

            //jvm->DetachCurrentThread();
        }
    } else {
        jni_env->CallStaticVoidMethod(entry_class, invoke_method);
    }
    LOGI("invoke_dex_method is finished!!");

}



extern "C" int hook_entry(char* status){
    //TODO Just a test Log
    LOGI("Hook0 success, pid = %d\n", getpid());
    LOGI("Hello world %s\n", status);

    //TODO A stationary parameter test
    if(strcmp(status, "system_server") == 0)
        invoke_dex_method(status, 0);
    else invoke_dex_method(status, 1);
    LOGI("Hello end\n");

    return 0;
}


void Main() {
    //invoke_dex_method();
    LOGI("Hook load Main()");
}
