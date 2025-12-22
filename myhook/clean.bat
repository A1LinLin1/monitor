@echo off
echo [Cleaning] Removing temporary build files...

:: 删除编译器生成的对象文件
del /Q /F *.obj
:: 删除资源编译器生成的二进制文件
del /Q /F *.res
:: 删除链接器生成的导出文件和静态库
del /Q /F *.exp *.lib
:: 删除可能存在的调试数据库文件
del /Q /F *.pdb *.idb
:: 删除上一次生成的执行文件和动态库（可选，如果只想删中间件请注释掉这两行）
:: del /Q /F *.exe *.dll

echo [Done] All temporary files removed.
pause