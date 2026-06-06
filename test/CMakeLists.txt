# -------------------------------------------------------
# 测试目标 CMakeLists
# 所有测试目标链接 obsim_core_static 以复用 core + utils 代码
# -------------------------------------------------------

# ---- 通用辅助函数 ----
function(add_obsim_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        obsim_core_static
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_NAME}>
    )
endfunction()

# =======================================================
# 纯 C++ 类测试（无 Qt 依赖，仅 GTest）
# =======================================================

# ---- DataSafeQueue ----
add_obsim_test(test_core_utils
    test_datasafequeue.cpp
)

# ---- OpenCVFilter（从手写测试迁移到 GTest） ----
add_obsim_test(test_opencv_filter_gtest
    test_opencvfilter_gtest.cpp
)
target_link_libraries(test_opencv_filter_gtest PRIVATE
    opencv_core opencv_imgproc opencv_imgcodecs
)

# 拷贝 OpenCV DLL 到测试输出目录（避免 0xc0000135）
function(copy_opencv_dlls TARGET_NAME)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,D:/develop-src/opencv/build/x64/vc16/bin/opencv_world4120d.dll,D:/develop-src/opencv/build/x64/vc16/bin/opencv_world4120.dll>"
            $<TARGET_FILE_DIR:${TARGET_NAME}>
        COMMENT "Copying OpenCV DLL to ${TARGET_NAME} output dir"
    )
endfunction()

copy_opencv_dlls(test_opencv_filter_gtest)

# ---- VideoCaptor Mock 测试 ----
add_obsim_test(test_video_captor
    test_videocaptor.cpp
)

# ---- Recoder Mock 测试 ----
add_obsim_test(test_recoder
    test_recoder.cpp
)

# =======================================================
# Qt 依赖类测试（GTest + QTest）
# =======================================================

function(add_obsim_qt_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        obsim_core_static
        GTest::gtest
        GTest::gmock
        Qt6::Test
        Qt6::Widgets
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_NAME}>
    )
endfunction()

# ---- Source 几何计算测试 ----
add_obsim_qt_test(test_source_geometry
    test_source_geometry.cpp
)

# ---- Scene 状态机测试 ----
add_obsim_qt_test(test_scene_logic
    test_scene_state.cpp
)

# ---- ConfigManager 测试 ----
add_obsim_qt_test(test_config_manager
    test_configmanager.cpp
)

# ---- AudioManager 测试（依赖注入后） ----
add_obsim_qt_test(test_audio_manager
    test_audiomanager.cpp
)

# =======================================================
# VLD 集成（Windows Debug 构建）
# =======================================================
if (WIN32)
    set(VLD_BIN_DIR "D:/application/Visual Leak Detector/bin/Win64")
    foreach(TEST_TARGET
        test_core_utils
        test_opencv_filter_gtest
        test_video_captor
        test_recoder
        test_source_geometry
        test_scene_logic
        test_config_manager
        test_audio_manager
    )
        target_include_directories(${TEST_TARGET} PRIVATE
            "D:/application/Visual Leak Detector/include"
        )
        add_custom_command(TARGET ${TEST_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VLD_BIN_DIR}/vld_x64.dll" $<TARGET_FILE_DIR:${TEST_TARGET}>
            COMMENT "Copying vld_x64.dll to ${TEST_TARGET}"
        )
        add_custom_command(TARGET ${TEST_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VLD_BIN_DIR}/Microsoft.DTfW.DHL.manifest" $<TARGET_FILE_DIR:${TEST_TARGET}>
            COMMENT "Copying Microsoft.DTfW.DHL.manifest to ${TEST_TARGET}"
        )
    endforeach()
endif ()

# =======================================================
# Benchmarks
# =======================================================
option(ENABLE_BENCHMARKS "Build performance benchmarks" ON)

if(ENABLE_BENCHMARKS)
    function(add_bench BENCH_NAME BENCH_SOURCE)
        add_executable(${BENCH_NAME} benchmark/${BENCH_SOURCE})
        target_include_directories(${BENCH_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
        )
        target_link_libraries(${BENCH_NAME} PRIVATE obsim_core_static)
    endfunction()

    add_bench(bench_queue   bench_queue.cpp)
    add_bench(bench_recoder bench_recoder.cpp)
    add_bench(bench_scene   bench_scene.cpp)
    add_bench(bench_filter  bench_filter.cpp)
endif()