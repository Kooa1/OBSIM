#ifdef _WIN32

#include "dxgiscreencaptor.h"
#include <iostream>
#include <chrono>
#include <cassert>

// Uncomment the following line to enable cursor debug logging
// #define DEBUG_CURSOR

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

DXGIScreenCaptor::DXGIScreenCaptor() = default;

DXGIScreenCaptor::~DXGIScreenCaptor() {
    stop();
}

void DXGIScreenCaptor::apply_config(const CaptorConfig &config) {
    assert(!is_capturing.load() && "apply_config must be called before start()");
    VideoCaptor::apply_config(config);
}

void DXGIScreenCaptor::start() {
    if (is_capturing.load()) return;
    queue = std::make_unique<DataSafeQueue<AVFramePtr>>(64);
    is_capturing.store(true);
    cap_thread = std::thread([this]() {
        dxgi_capture_loop();
    });
}

void DXGIScreenCaptor::stop() {
    is_capturing.store(false);
    if (cap_thread.joinable()) {
        cap_thread.join();
    }
    release_dxgi();
    if (queue) {
        queue->clean_queue();
    }
}

bool DXGIScreenCaptor::init_dxgi() {
    // Create D3D11 device
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL selected_level;

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,                    // no software rasterizer
        0,                          // no device flags
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &m_d3d_device,
        &selected_level,
        &m_d3d_context
    );

    if (FAILED(hr)) {
        std::cerr << "DXGIScreenCaptor: D3D11CreateDevice failed, hr=0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Get DXGI device and adapter
    ComPtr<IDXGIDevice> dxgi_device;
    hr = m_d3d_device.As(&dxgi_device);
    if (FAILED(hr)) {
        std::cerr << "DXGIScreenCaptor: QueryInterface IDXGIDevice failed" << std::endl;
        return false;
    }

    ComPtr<IDXGIAdapter> dxgi_adapter;
    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    if (FAILED(hr)) {
        std::cerr << "DXGIScreenCaptor: GetAdapter failed" << std::endl;
        return false;
    }

    // Enumerate all outputs to find the one containing the target region
    ComPtr<IDXGIOutput> dxgi_output;
    bool found = false;
    int target_x = m_config.offset_x;
    int target_y = m_config.offset_y;

    for (UINT i = 0; ; ++i) {
        ComPtr<IDXGIOutput> output;
        HRESULT hr = dxgi_adapter->EnumOutputs(i, &output);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) continue;

        DXGI_OUTPUT_DESC desc;
        if (FAILED(output->GetDesc(&desc))) continue;

        RECT& r = desc.DesktopCoordinates;
        if (target_x >= r.left && target_x < r.right &&
            target_y >= r.top  && target_y < r.bottom) {
            dxgi_output = output;
            m_monitor_left   = r.left;
            m_monitor_top    = r.top;
            m_monitor_width  = r.right - r.left;
            m_monitor_height = r.bottom - r.top;
            found = true;
            std::cout << "DXGIScreenCaptor: matched output " << i
                      << " at (" << r.left << "," << r.top << ") "
                      << m_monitor_width << "x" << m_monitor_height << std::endl;
            break;
        }
    }

    if (!found) {
        // Fallback: use the first output
        hr = dxgi_adapter->EnumOutputs(0, &dxgi_output);
        if (FAILED(hr)) {
            std::cerr << "DXGIScreenCaptor: no output found" << std::endl;
            return false;
        }
        DXGI_OUTPUT_DESC desc;
        dxgi_output->GetDesc(&desc);
        RECT& r = desc.DesktopCoordinates;
        m_monitor_left   = r.left;
        m_monitor_top    = r.top;
        m_monitor_width  = r.right - r.left;
        m_monitor_height = r.bottom - r.top;
        std::cout << "DXGIScreenCaptor: fallback to output 0 at ("
                  << r.left << "," << r.top << ") "
                  << m_monitor_width << "x" << m_monitor_height << std::endl;
    }

    // Calculate capture region relative to the selected monitor
    if (m_config.width > 0 && m_config.height > 0) {
        m_output_width  = m_config.width;
        m_output_height = m_config.height;
    } else {
        m_output_width  = m_monitor_width;
        m_output_height = m_monitor_height;
    }

    std::cout << "DXGIScreenCaptor: capture region=" << m_output_width
              << "x" << m_output_height << std::endl;

    // Get IDXGIOutput1 for DuplicateOutput
    ComPtr<IDXGIOutput1> dxgi_output1;
    hr = dxgi_output.As(&dxgi_output1);
    if (FAILED(hr)) {
        std::cerr << "DXGIScreenCaptor: QueryInterface IDXGIOutput1 failed" << std::endl;
        return false;
    }

    hr = dxgi_output1->DuplicateOutput(m_d3d_device.Get(), &m_desk_dup);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            std::cerr << "DXGIScreenCaptor: DuplicateOutput - maximum number of applications using Desktop Duplication reached" << std::endl;
        } else if (hr == DXGI_ERROR_UNSUPPORTED) {
            std::cerr << "DXGIScreenCaptor: DuplicateOutput - Desktop Duplication not supported on this system" << std::endl;
        } else {
            std::cerr << "DXGIScreenCaptor: DuplicateOutput failed, hr=0x"
                      << std::hex << hr << std::dec << std::endl;
        }
        return false;
    }

    // Create staging texture sized to the capture region
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width              = static_cast<UINT>(m_output_width);
    tex_desc.Height             = static_cast<UINT>(m_output_height);
    tex_desc.MipLevels          = 1;
    tex_desc.ArraySize          = 1;
    tex_desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_desc.SampleDesc.Count   = 1;
    tex_desc.Usage              = D3D11_USAGE_STAGING;
    tex_desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;
    tex_desc.BindFlags          = 0;

    hr = m_d3d_device->CreateTexture2D(&tex_desc, nullptr, &m_staging_texture);
    if (FAILED(hr)) {
        std::cerr << "DXGIScreenCaptor: CreateTexture2D (staging) failed" << std::endl;
        return false;
    }

    return true;
}

void DXGIScreenCaptor::release_dxgi() {
    m_staging_texture.Reset();
    m_desk_dup.Reset();
    m_d3d_context.Reset();
    m_d3d_device.Reset();
    m_output_width  = 0;
    m_output_height = 0;
    m_gdi_cursor_buf.reset();
    m_gdi_last_cursor_handle = nullptr;
}

void DXGIScreenCaptor::dxgi_capture_loop() {
    if (!init_dxgi()) {
        is_capturing.store(false);
        return;
    }

    // Allocate reusable buffer for frame data
    const int buf_size = m_output_width * m_output_height * 4;
    auto frame_buffer = std::make_unique<uint8_t[]>(buf_size);

    int64_t frame_index = 0;
    auto last_frame_time = std::chrono::steady_clock::now();
    int target_interval = 1000 / m_target_fps;

    while (is_capturing.load()) {
        if (is_paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Frame rate limiting
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_frame_time).count();
        if (elapsed < target_interval) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(target_interval - elapsed));
        }
        last_frame_time = std::chrono::steady_clock::now();

        ComPtr<IDXGIResource> desktop_resource;
        DXGI_OUTDUPL_FRAME_INFO frame_info = {};

        HRESULT hr = m_desk_dup->AcquireNextFrame(16, &frame_info, &desktop_resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;
        }
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            // Display mode changed, reinitialize
            std::cerr << "DXGIScreenCaptor: access lost, reinitializing..." << std::endl;
            release_dxgi();
            if (!init_dxgi()) {
                is_capturing.store(false);
                return;
            }
            continue;
        }
        if (FAILED(hr)) {
            std::cerr << "DXGIScreenCaptor: AcquireNextFrame failed, hr=0x"
                      << std::hex << hr << std::dec << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // If the frame didn't change, skip it
        // (frame_info.LastPresentTime == 0 means no update since last frame)
        // We still process it to keep the pipeline flowing

        // --- GDI-based cursor shape acquisition ---
        // DXGI GetFramePointerShape returns a black placeholder on some drivers.
        // Use GDI to get the actual cursor shape and cache it by handle.
        {
            CURSORINFO ci = { sizeof(CURSORINFO) };
            if (m_config.capture_cursor && GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) {
                if (ci.hCursor != m_gdi_last_cursor_handle) {
                    ICONINFO ii = {};
                    if (GetIconInfo(ci.hCursor, &ii)) {
                        GdiObjectGuard icon_color{ii.hbmColor};
                        GdiObjectGuard icon_mask{ii.hbmMask};

                        // Determine cursor dimensions
                        int w = 32, h = 32;
                        if (ii.hbmColor) {
                            BITMAP bm = {};
                            GetObject(ii.hbmColor, sizeof(bm), &bm);
                            w = bm.bmWidth;
                            h = bm.bmHeight;
                        } else if (ii.hbmMask) {
                            BITMAP bm = {};
                            GetObject(ii.hbmMask, sizeof(bm), &bm);
                            w = bm.bmWidth;
                            h = bm.bmHeight / 2;
                        }

                        // Render cursor into DIB section and cache BGRA pixel data
                        ScreenDcGuard screen_dc{GetDC(NULL)};
                        DcGuard mem_dc{CreateCompatibleDC(screen_dc.dc)};

                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = w;
                        bmi.bmiHeader.biHeight = -h;  // top-down
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        BYTE* bits = nullptr;
                        GdiObjectGuard hbm{CreateDIBSection(mem_dc.dc, &bmi, DIB_RGB_COLORS,
                                                            (void**)&bits, NULL, 0)};
                        if (hbm.obj) {
                            SelectObject(mem_dc.dc, hbm.obj);
                            DrawIconEx(mem_dc.dc, -ii.xHotspot, -ii.yHotspot,
                                       ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);

                            m_gdi_cursor_buf = std::make_unique<BYTE[]>(w * h * 4);
                            memcpy(m_gdi_cursor_buf.get(), bits, w * h * 4);
                            m_gdi_cursor_w = w;
                            m_gdi_cursor_h = h;
                            m_gdi_last_cursor_handle = ci.hCursor;
                        }

                    }
                }
            }
        }

        // Cache cursor position and visibility only when mouse actually updates (DXGI)
        if (m_config.capture_cursor && frame_info.LastMouseUpdateTime.QuadPart != 0) {
            m_cursor_visible_cache = (frame_info.PointerPosition.Visible != 0);
            if (m_cursor_visible_cache) {
                // PointerPosition is relative to the output, not virtual desktop
                m_cursor_x_cache = frame_info.PointerPosition.Position.x - (m_config.offset_x - m_monitor_left);
                m_cursor_y_cache = frame_info.PointerPosition.Position.y - (m_config.offset_y - m_monitor_top);
            }
        }

        // Use cached position/visibility every frame
        bool cursor_visible = m_config.capture_cursor && m_cursor_visible_cache && (m_gdi_cursor_buf != nullptr);
        int cursor_x = m_cursor_x_cache;
        int cursor_y = m_cursor_y_cache;
        // --- End cursor acquisition ---

        // Get the D3D11 texture
        ComPtr<ID3D11Texture2D> acquired_texture;
        hr = desktop_resource.As(&acquired_texture);
        if (FAILED(hr)) {
            m_desk_dup->ReleaseFrame();
            continue;
        }

        // Calculate relative offset within the monitor
        int rel_off_x = m_config.offset_x - m_monitor_left;
        int rel_off_y = m_config.offset_y - m_monitor_top;

        // Copy only the target sub-region from acquired texture to staging
        D3D11_BOX src_box = {};
        src_box.left   = static_cast<UINT>(rel_off_x);
        src_box.top    = static_cast<UINT>(rel_off_y);
        src_box.right  = static_cast<UINT>(rel_off_x + m_output_width);
        src_box.bottom = static_cast<UINT>(rel_off_y + m_output_height);
        src_box.front  = 0;
        src_box.back   = 1;

        m_d3d_context->CopySubresourceRegion(
            m_staging_texture.Get(), 0,
            0, 0, 0,
            acquired_texture.Get(), 0,
            &src_box
        );

        // Map the staging texture
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = m_d3d_context->Map(m_staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            m_desk_dup->ReleaseFrame();
            continue;
        }

        // Copy from staging to frame buffer
        if (mapped.RowPitch == static_cast<UINT>(m_output_width * 4)) {
            // Fast path: contiguous rows
            memcpy(frame_buffer.get(), mapped.pData, buf_size);
        } else {
            // RowPitch may have padding, copy line by line
            uint8_t* src = static_cast<uint8_t*>(mapped.pData);
            uint8_t* dst = frame_buffer.get();
            int row_bytes = m_output_width * 4;
            for (int y = 0; y < m_output_height; ++y) {
                memcpy(dst + y * row_bytes, src + y * mapped.RowPitch, row_bytes);
            }
        }

        m_d3d_context->Unmap(m_staging_texture.Get(), 0);

        // --- Draw cursor onto frame buffer (using GDI-cached shape) ---
        if (cursor_visible) {
            int src_pitch = m_gdi_cursor_w;  // pitch in uint32_t units (= pixel count per row)
            const auto* src = reinterpret_cast<const uint32_t*>(m_gdi_cursor_buf.get());
            for (int y = 0; y < m_gdi_cursor_h; ++y) {
                int dst_y = cursor_y + y;
                if (dst_y < 0 || dst_y >= m_output_height) continue;
                int row_offset = y * src_pitch;
                for (int x = 0; x < m_gdi_cursor_w; ++x) {
                    int dst_x = cursor_x + x;
                    if (dst_x < 0 || dst_x >= m_output_width) continue;
                    uint32_t pixel = src[row_offset + x];
                    uint32_t alpha = (pixel >> 24) & 0xFF;
                    if (alpha == 0) continue;
                    auto* dst = reinterpret_cast<uint32_t*>(
                        frame_buffer.get() + (dst_y * m_output_width + dst_x) * 4);
                    uint32_t bg = *dst;
                    uint32_t br = (bg >> 16) & 0xFF;
                    uint32_t bg_g = (bg >> 8) & 0xFF;
                    uint32_t bb = bg & 0xFF;
                    uint32_t sr = (pixel >> 16) & 0xFF;
                    uint32_t sg = (pixel >> 8) & 0xFF;
                    uint32_t sb = pixel & 0xFF;
                    uint32_t out_r = (sr * alpha + br * (255 - alpha)) / 255;
                    uint32_t out_g = (sg * alpha + bg_g * (255 - alpha)) / 255;
                    uint32_t out_b = (sb * alpha + bb * (255 - alpha)) / 255;
                    *dst = 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
        // --- End cursor drawing ---

        m_desk_dup->ReleaseFrame();

        const size_t frame_data_size = static_cast<size_t>(m_output_width) * m_output_height * 4;

        if (on_frame_captured) {
            // Pool mode: create ONE shared AVFrame and distribute to all consumers
            // via shared_ptr for zero-copy sharing (no per-consumer memcpy).
            AVFramePtr pool_frame(av_frame_alloc(), AVFrameDeleter());
            if (pool_frame) {
                pool_frame->format = AV_PIX_FMT_BGRA;
                pool_frame->width  = m_output_width;
                pool_frame->height = m_output_height;
                pool_frame->pts = frame_index++;
                if (av_frame_get_buffer(pool_frame.get(), 0) == 0) {
                    memcpy(pool_frame->data[0], frame_buffer.get(), frame_data_size);
                    on_frame_captured(std::move(pool_frame));
                }
            }
        } else {
            // Standalone mode: push frame to own queue for direct consumption.
            AVFramePtr av_frame(av_frame_alloc(), AVFrameDeleter());
            if (!av_frame) continue;

            av_frame->format = AV_PIX_FMT_BGRA;
            av_frame->width  = m_output_width;
            av_frame->height = m_output_height;
            av_frame->pts = frame_index++;

            if (av_frame_get_buffer(av_frame.get(), 0) != 0) continue;

            memcpy(av_frame->data[0], frame_buffer.get(), frame_data_size);

            push_frame(std::move(av_frame));
            notify_frame_ready();
        }
    }
}

#endif // _WIN32