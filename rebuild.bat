@echo off
:: 设置字符集为 UTF-8 防止中文乱码
chcp 65001 >nul

echo ===========================================
echo   Pro Input Recorder 一键重新生成脚本
echo ===========================================

:: 1. 执行清理
echo [1/4] 清理旧文件...
del /Q *.obj *.res *.lib *.exp *.dll *.exe >nul 2>nul

:: 2. 编译资源文件 (请确保 rc.exe 在路径中或使用绝对路径)
echo [2/4] 编译资源: resource.rc...
rc resource.rc
if %errorlevel% neq 0 (echo 资源编译失败！ && pause && exit /b)

:: 3. 编译核心 DLL
echo [3/4] 编译 DLL: myhook.dll...
cl /LD /EHsc /utf-8 /std:c++17 /DUNICODE /D_UNICODE myhook.cpp logger.cpp util.cpp config.cpp /Fe:myhook.dll /link /SECTION:.shared,RWS ws2_32.lib User32.lib >nul
if %errorlevel% neq 0 (echo DLL 编译失败！ && pause && exit /b)

:: 4. 编译主程序 EXE
echo [4/4] 编译 EXE: MonitorRecorder.exe...
cl /EHsc /utf-8 /std:c++17 /DUNICODE /D_UNICODE monitor.cpp config.cpp resource.res /Fe:MonitorRecorder.exe /link user32.lib shell32.lib comctl32.lib Ole32.lib >nul
if %errorlevel% neq 0 (echo EXE 编译失败！ && pause && exit /b)

echo.
echo ===========================================
echo   重新生成成功！可以直接运行 MonitorRecorder.exe
echo ===========================================
pause