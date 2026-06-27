#ifdef _WIN32

#include "dxgiscreencaptor.h"
#include <iostream>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

DXGIScreenCaptor::DXGIScreenCaptor() = default;

DXGIScreenCaptor::~DXGIScreenCaptor() {
    stop();
}

void DXGIScreenCaptor::apply_config(const CaptorConfig &config) {
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
}

void DXGIScreenCaptor::dxgi_capture_loop() {
    if (!init_dxgi()) {
        is_capturing.store(false);
        return;
    }

    // Allocate reusable buffer for frame data
    const int buf_size = m_output_width * m_output_height * 4;
    auto frame_buffer = std::make_unique<uint8_t[]>(buf_size);

    while (is_capturing.load()) {
        if (is_paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        ComPtr<IDXGIResource> desktop_resource;
        DXGI_OUTDUPL_FRAME_INFO frame_info = {};

        HRESULT hr = m_desk_dup->AcquireNextFrame(100, &frame_info, &desktop_resource);
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
        m_desk_dup->ReleaseFrame();

        // Build AVFrame
        AVFramePtr av_frame(av_frame_alloc(), AVFrameDeleter());
        if (!av_frame) continue;

        av_frame->format = AV_PIX_FMT_BGRA;
        av_frame->width  = m_output_width;
        av_frame->height = m_output_height;

        int ret = av_frame_get_buffer(av_frame.get(), 0);
        if (ret != 0) continue;

        // Copy pixel data into AVFrame's internal buffer
        memcpy(av_frame->data[0], frame_buffer.get(),
               static_cast<size_t>(m_output_width) * m_output_height * 4);

        push_frame(std::move(av_frame));
        notify_frame_ready();
    }

    release_dxgi();
}

#endif // _WIN32