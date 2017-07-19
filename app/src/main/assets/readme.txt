如果MyToast.jar没有经过优化，使用PathClassLoader动态加载会报错，
则将打包好的jar文件拷贝到android的安装目录中的platform-tools目录下，
使用dx命令:   ./dx --dex --output=dynamic_temp.jar dynamic.jar

DexClassLoader动态加载jar/apk/dex，也可以从SD卡中加载
