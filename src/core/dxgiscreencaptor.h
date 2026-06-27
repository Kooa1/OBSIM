#ifndef OBSIM_DXGI_SCREEN_CAPTOR_H
#define OBSIM_DXGI_SCREEN_CAPTOR_H

#ifdef _WIN32

#include "base/videocaptor.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

/// @brief Screen capture using DXGI Desktop Duplication API directly.
/// Bypasses FFmpeg gdigrab/dxgigrab entirely to avoid cursor flickering.
class DXGIScreenCaptor : public VideoCaptor {
public:
    DXGIScreenCaptor();
    ~DXGIScreenCaptor() override;

    void start() override;
    void stop() override;
    void apply_config(const CaptorConfig &config) override;

protected:
    const char* get_input_format_name() const override { return "dxgi_direct"; }
    const char* get_device_name() const override { return "desktop"; }

private:
    /// @brief DXGI capture loop running on the capture thread.
    void dxgi_capture_loop();
    /// @brief Initialize DXGI Desktop Duplication resources.
    bool init_dxgi();
    /// @brief Release all DXGI resources.
    void release_dxgi();

    // DXGI / D3D11 resources
    ComPtr<ID3D11Device>        m_d3d_device;
    ComPtr<ID3D11DeviceContext> m_d3d_context;
    ComPtr<IDXGIOutputDuplication> m_desk_dup;
    ComPtr<ID3D11Texture2D>     m_staging_texture;

    int m_output_width  = 0;
    int m_output_height = 0;

    // Selected monitor's position in virtual desktop coordinates
    int m_monitor_left   = 0;
    int m_monitor_top    = 0;
    int m_monitor_width  = 0;
    int m_monitor_height = 0;

    // GDI-based cursor shape cache (replaces unreliable DXGI GetFramePointerShape)
    HCURSOR m_gdi_last_cursor_handle = nullptr;
    std::unique_ptr<BYTE[]> m_gdi_cursor_buf;
    int m_gdi_cursor_w = 0;
    int m_gdi_cursor_h = 0;

    // Cursor position/visibility cache (only updated on mouse move via DXGI)
    bool m_cursor_visible_cache = false;
    int m_cursor_x_cache = 0;
    int m_cursor_y_cache = 0;
};

#endif // _WIN32
#endif // OBSIM_DXGI_SCREEN_CAPTOR_H