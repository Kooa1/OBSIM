cmake_minimum_required(VERSION 3.24)
project(MonitorServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
    # 明确使用 MSVC 版本的 Qt（根据你的编译器）
    set(CMAKE_PREFIX_PATH "D:/application/Qt/6.9.0/msvc2022_64")
    
    # FFmpeg 配置
    include_directories("D:/src/ffmpeg-7.1.1-full_build-shared/include")
    link_directories("D:/src/ffmpeg-7.1.1-full_build-shared/lib")
    
    include_directories("D:/application/Visual Leak Detector/include")
    link_directories("D:/application/Visual Leak Detector/lib/Win64")

elseif (APPLE)
    set(CMAKE_PREFIX_PATH "/Users/wuwenze/Qt/6.9.0/macos/lib/cmake/Qt6")
    
    set(FFMPEG_ROOT "/opt/homebrew/Cellar/ffmpeg/8.0_1")
    include_directories("/opt/homebrew/Cellar/ffmpeg/8.0_1/include")
    link_directories("/opt/homebrew/Cellar/ffmpeg/8.0_1/lib")
endif ()


# 查找 Qt6
find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED
)

# 自动查找所有头文件
file(GLOB_RECURSE HEADER_FILES
        "include/*.h"
        "include/*.hpp"
        "include/*.hxx"
        "src/*.h"
        "src/*.hpp"
        "src/*.hxx"
)

# 自动查找所有源文件
file(GLOB_RECURSE SOURCE_FILES
        "src/*.cpp"
        "src/*.cxx"
        "src/*.cc"
        "src/*.c"
)

# 添加可执行文件
add_executable(MonitorServer
        ${SOURCE_FILES}
        ${HEADER_FILES}
        src/main.cpp
        include/datasafequeue.h
        src/captor.cpp
        include/captor.h
        include/av_err2str_cxx.h
)

# 设置包含目录
target_include_directories(MonitorServer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if (WIN32)
    target_link_libraries(MonitorServer
            vld
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            avdevice
            avformat
            avcodec
            avutil
            swscale
    )
elseif (APPLE)
    target_link_libraries(MonitorServer
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            avdevice
            avformat
            avcodec
            avutil
            swscale
    )
endif ()

# MSVC 编译器选项
if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif ()

# Windows 平台部署
if (WIN32)
    # 设置 Qt 安装路径
    set(QT_INSTALL_PATH "D:/application/Qt/6.9.0/msvc2022_64")
    
    # 调试版本后缀
    set(DEBUG_SUFFIX "")
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    
    # 复制主要的 Qt DLLs
    foreach (QT_LIB Core Gui Widgets)
        add_custom_command(TARGET MonitorServer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:MonitorServer>"
                COMMENT "Copying Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
        )
    endforeach ()
    
    # 复制平台插件
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET MonitorServer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:MonitorServer>/plugins/platforms/"
                COMMENT "Creating platforms directory"
        )
        
        add_custom_command(TARGET MonitorServer POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:MonitorServer>/plugins/platforms/"
                COMMENT "Copying qwindows${DEBUG_SUFFIX}.dll"
        )
    endif ()

endif ()

# 可选：打印找到的文件列表用于调试
message(STATUS "Found header files: ${HEADER_FILES}")
message(STATUS "Found source files: ${SOURCE_FILES}")