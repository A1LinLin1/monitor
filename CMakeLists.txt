cmake_minimum_required(VERSION 3.16)
project(MonitorRecorder)

# 1. 强制 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 针对 MSVC 的深度修复（解决编码、宏定义、严格模式）
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/permissive-)
endif()

# 3. 自动处理 Qt 特有的元对象系统和资源文件
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON) # 必须开启，用于处理 .qrc 图标

# 4. 指定 Qt 路径
set(CMAKE_PREFIX_PATH "D:/Tools/Qt/6.10.1/msvc2022_64")
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 5. 编译 DLL 钩子
# 注意：这里包含 config.cpp 是为了 DLL 内部使用
add_library(myhook SHARED 
    myhook/myhook.cpp 
    myhook/config.cpp 
    myhook/logger.cpp 
    myhook/util.cpp
)
target_link_libraries(myhook PRIVATE user32 shell32 ws2_32 iphlpapi)

# 6. 编译 Qt 界面程序
# [关键点] WIN32 标志用于消除启动时的控制台黑窗口
add_executable(MonitorUI WIN32
    recorder_ui/main.cpp
    recorder_ui/MainWindow.cpp
    recorder_ui/MainWindow.h
    recorder_ui/resources.qrc  # 确保你已经创建了这个资源文件
    recorder_ui/resource.rc
    myhook/config.cpp          # 主程序也需要包含它以访问共享句柄
)

# 链接 Qt 库
target_link_libraries(MonitorUI PRIVATE Qt6::Widgets)

# 确保主程序能找到 myhook 文件夹下的头文件
target_include_directories(MonitorUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 7. 构建后自动化操作
# A. 将编译好的 DLL 复制到 EXE 运行目录
add_custom_command(TARGET MonitorUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:myhook> $<TARGET_FILE_DIR:MonitorUI>
)

# B. 自动调用 windeployqt 修复找不到 Qt6Cored.dll 的问题
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET MonitorUI POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:MonitorUI>"
        COMMENT "正在自动部署 Qt 运行库..."
    )
endif()