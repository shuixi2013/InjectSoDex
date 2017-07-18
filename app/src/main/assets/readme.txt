MyToast.jar如果没有经过优化，则将打包好的jar文件拷贝到android的安装目录中的platform-tools目录下，
使用dx命令:   ./dx --dex --output=dynamic_temp.jar dynamic.jar