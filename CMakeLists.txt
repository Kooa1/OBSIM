cmake_minimum_required(VERSION 3.24)
project(OBSIM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC_VERBOSE ON)

if (WIN32)
    # 明确使用 MSVC 版本的 Qt（根据你的编译器）
    set(CMAKE_PREFIX_PATH "D:/application/Qt/6.9.0/msvc2022_64")
    
    # OpenCV
    set(OpenCV_DIR "D:/develop-src/opencv/build")
    

    
    # FFmpeg 配置
    include_directories("D:/develop-src/ffmpeg-7.1.1/include")
    link_directories("D:/develop-src/ffmpeg-7.1.1/lib")
    
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
        OpenGL
        OpenGLWidgets
        Multimedia
        Concurrent
        Test
        REQUIRED
)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 查找 OpenGL（跨平台）
find_package(OpenGL REQUIRED)

# ------------------------------------------------------------
# 1) obsim_core_static — core 层 + utils 工具层（无 Qt GUI 依赖）
# ------------------------------------------------------------
file(GLOB_RECURSE CORE_SOURCES
        "src/core/*.cpp" "src/core/*.c" "src/core/*.cc" "src/core/*.cxx"
)

file(GLOB_RECURSE UTILS_SOURCES
        "src/utils/*.cpp" "src/utils/*.c" "src/utils/*.cc" "src/utils/*.cxx"
)

file(GLOB_RECURSE CORE_HEADERS
        "include/*.h" "include/*.hpp" "include/*.hxx"
        "src/core/*.h" "src/core/*.hpp" "src/core/*.hxx"
        "src/utils/*.h" "src/utils/*.hpp" "src/utils/*.hxx"
)

file(GLOB_RECURSE FRONTEND_HEADERS
        "src/frontend/*.h" "src/frontend/*.hpp" "src/frontend/*.hxx"
)

set(OBSIM_CORE_SOURCES ${CORE_SOURCES} ${UTILS_SOURCES})

add_library(obsim_core_static STATIC
        ${OBSIM_CORE_SOURCES}
        ${CORE_HEADERS}
)

target_compile_definitions(obsim_core_static PRIVATE
        HAVE_OBSCONFIG_H
        NOMINMAX
)

target_include_directories(obsim_core_static PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(obsim_core_static PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::Multimedia
        Qt6::Concurrent
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        OpenGL::GL
        avdevice
        avformat
        avcodec
        avutil
        swscale
        swresample
)

# ------------------------------------------------------------
# 2) OBSIM 可执行文件 — frontend (UI) + main.cpp
# ------------------------------------------------------------
file(GLOB_RECURSE FRONTEND_SOURCES
        "src/frontend/*.cpp" "src/frontend/*.c" "src/frontend/*.cc" "src/frontend/*.cxx"
)

add_executable(OBSIM
        src/main.cpp
        ${FRONTEND_SOURCES}
        resources/resources.qrc
)

target_compile_definitions(OBSIM PRIVATE
        HAVE_OBSCONFIG_H
        NOMINMAX
)

target_include_directories(OBSIM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(OBSIM PRIVATE
        obsim_core_static
        Qt6::Widgets
        Qt6::OpenGLWidgets
)

if (WIN32)
elseif (APPLE)    
endif ()

# MSVC 编译器选项
if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif ()

# ------------------------------------------------------------
# 测试开关 — 设为 OFF 可跳过所有测试编译
# ------------------------------------------------------------
option(ENABLE_TESTS "Enable unit tests" OFF)

if (ENABLE_TESTS)
    # ------------------------------------------------------------
    # 3) Google Test (通过 FetchContent 自动拉取)
    # ------------------------------------------------------------
    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" INTERNAL)
    FetchContent_MakeAvailable(googletest)

    # ------------------------------------------------------------
    # 4) 测试基础设施
    # ------------------------------------------------------------
    enable_testing()
    include(CTest)

    add_subdirectory(test)

    # Unit test for OpenCVFilter (保留旧目标兼容)
    add_executable(opencv_filter_test
            test/test_opencv_filter.cpp
            src/core/filter/opencvfilter.cpp
    )
    target_include_directories(opencv_filter_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(opencv_filter_test
            opencv_core
            opencv_imgproc
            opencv_imgcodecs
    )
endif ()

# Windows 平台部署
if (WIN32)
    # ========================= 路径定义 =========================
    set(QT_INSTALL_PATH "D:/application/Qt/6.9.0/msvc2022_64")
    set(FFMPEG_BIN_DIR "D:/develop-src/ffmpeg-7.1.1/bin")
    set(OPENCV_BIN_DIR "D:/develop-src/opencv/build/x64/vc16/bin")

    set(VLD_BIN_DIR "D:/application/Visual Leak Detector/bin/Win64")
    
    # 调试版本后缀
    set(DEBUG_SUFFIX "")
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    
    # 输出目录
    set(OUT_DIR "$<TARGET_FILE_DIR:OBSIM>")
    
    # ========================= Qt DLLs =========================
    set(QT_LIBS
            Core Gui Widgets Multimedia
            OpenGL OpenGLWidgets Concurrent
            Network
    )
    foreach (QT_LIB ${QT_LIBS})
        set(QT_DLL "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll")
        if (EXISTS "${QT_DLL}")
            add_custom_command(TARGET OBSIM POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_DLL}" "${OUT_DIR}"
                    COMMENT "Copying Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
            )
        endif ()
    endforeach ()
    
    # ========================= Qt 平台插件 =========================
    set(QT_PLATFORM_DLL "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
    if (EXISTS "${QT_PLATFORM_DLL}")
        add_custom_command(TARGET OBSIM POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "${OUT_DIR}/plugins/platforms/"
                COMMENT "Creating platforms directory"
        )
        add_custom_command(TARGET OBSIM POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_PLATFORM_DLL}" "${OUT_DIR}/plugins/platforms/"
                COMMENT "Copying qwindows${DEBUG_SUFFIX}.dll"
        )
    endif ()
    
    # ========================= Qt 多媒体插件 =========================
    set(QT_MULTIMEDIA_PLUGINS
            ffmpegmediaplugin${DEBUG_SUFFIX}.dll
            windowsmediaplugin${DEBUG_SUFFIX}.dll
    )
    foreach (MM_PLUGIN ${QT_MULTIMEDIA_PLUGINS})
        set(MM_PLUGIN_PATH "${QT_INSTALL_PATH}/plugins/multimedia/${MM_PLUGIN}")
        if (EXISTS "${MM_PLUGIN_PATH}")
            add_custom_command(TARGET OBSIM POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${OUT_DIR}/plugins/multimedia/"
                    COMMENT "Creating multimedia plugins directory"
            )
            add_custom_command(TARGET OBSIM POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MM_PLUGIN_PATH}" "${OUT_DIR}/plugins/multimedia/"
                    COMMENT "Copying ${MM_PLUGIN}"
            )
        endif ()
    endforeach ()
    
    # ========================= FFmpeg DLLs =========================
    set(FFMPEG_DLLS
            avcodec-61.dll
            avdevice-61.dll
            avfilter-10.dll
            avformat-61.dll
            avutil-59.dll
            postproc-58.dll
            swresample-5.dll
            swscale-8.dll
    )
    foreach (FFMPEG_DLL ${FFMPEG_DLLS})
        set(FFMPEG_DLL_PATH "${FFMPEG_BIN_DIR}/${FFMPEG_DLL}")
        if (EXISTS "${FFMPEG_DLL_PATH}")
            add_custom_command(TARGET OBSIM POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FFMPEG_DLL_PATH}" "${OUT_DIR}"
                    COMMENT "Copying ${FFMPEG_DLL}"
            )
        endif ()
    endforeach ()
    
    # ========================= OpenCV DLLs =========================
    set(OPENCV_DLLS
            opencv_world4120${DEBUG_SUFFIX}.dll
            opencv_videoio_ffmpeg4120_64.dll
    )
    foreach (OPENCV_DLL ${OPENCV_DLLS})
        set(OPENCV_DLL_PATH "${OPENCV_BIN_DIR}/${OPENCV_DLL}")
        if (EXISTS "${OPENCV_DLL_PATH}")
            add_custom_command(TARGET OBSIM POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OPENCV_DLL_PATH}" "${OUT_DIR}"
                    COMMENT "Copying ${OPENCV_DLL}"
            )
        endif ()
    endforeach ()
    
    # ========================= Visual Leak Detector DLL (仅 Debug) =========================
    if (WIN32)
        set(VLD_DLL "${VLD_BIN_DIR}/vld_x64.dll")
        set(VLD_DBGHELP "${VLD_BIN_DIR}/dbghelp.dll")
        set(VLD_MANIFEST "${VLD_BIN_DIR}/Microsoft.DTfW.DHL.manifest")
        add_custom_command(TARGET OBSIM POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VLD_DLL}" "${OUT_DIR}"
            COMMENT "Copying vld_x64.dll"
        )
        add_custom_command(TARGET OBSIM POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VLD_DBGHELP}" "${OUT_DIR}"
            COMMENT "Copying dbghelp.dll"
        )
        if (EXISTS "${VLD_MANIFEST}")
            add_custom_command(TARGET OBSIM POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VLD_MANIFEST}" "${OUT_DIR}"
                COMMENT "Copying Microsoft.DTfW.DHL.manifest"
            )
        endif ()
    endif ()

endif ()

# 可选：打印找到的文件列表用于调试
message(STATUS "Found core headers: ${CORE_HEADERS}")
message(STATUS "Found core+utils sources: ${OBSIM_CORE_SOURCES}")
message(STATUS "Found frontend sources: ${FRONTEND_SOURCES}")
message(STATUS "Found frontend headers: ${FRONTEND_HEADERS}")