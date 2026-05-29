#ifndef OBSIM_DEVICEMANAGER_H
#define OBSIM_DEVICEMANAGER_H

#include "PCH.h"
#include <QMediaDevices>
#include <QCameraDevice>
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys.h>

/// @brief Information about a display monitor
struct DisplayInfo {
    int index;                          ///< Display index
    std::string name;                   ///< Display name
    std::string manufacturer;           ///< Manufacturer name
    std::string model;                  ///< Model name
    QRect geometry;                     ///< Screen geometry (position and size)
    QRect available_geometry;           ///< Available area (excluding taskbar etc.)
    bool is_primary;                    ///< Whether this is the primary display
    qreal dpi;                          ///< Dots per inch
    qreal refresh_rate;                 ///< Refresh rate in Hz
};

/// @brief Information about a camera device
struct CameraInfo {
    int index;                          ///< Camera index
    std::string name;                   ///< Camera name/description
    QByteArray id;                      ///< Camera unique identifier
    bool is_default;                    ///< Whether this is the system default camera
};

/// @brief Information about a WASAPI audio output device
struct AudioOutputInfo {
    QString id;                         ///< WASAPI device ID
    QString name;                       ///< Device friendly name
    bool is_default;                    ///< Whether this is the default playback device
};

/// @brief Information about a DShow audio input device
struct AudioInputInfo {
    QString name;                       ///< DShow device description (friendly name)
    QString raw_name;                   ///< DShow raw device name (for "audio=XXX" construction)
    bool is_default;                    ///< Whether this is the default recording device
};

/// @brief Manages enumeration of displays, cameras, and audio devices
class DeviceManager : public QObject {
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);

    /// @brief Run a quick demo enumerating all devices
    static void run();

    /// @brief Get all available displays
    QVector<DisplayInfo> get_all_displays() const;

    /// @brief Get all available cameras
    QVector<CameraInfo> get_all_cameras() const;

    /// @brief Get the number of displays
    int get_display_count() const;

    /// @brief Get the number of cameras
    int get_camera_count() const;

    /// @brief Get the primary display
    DisplayInfo get_primary_display() const;

    /// @brief Get display by index
    DisplayInfo get_display(int index) const;

    /// @brief Get camera by index
    CameraInfo get_camera(int index) const;

    /// @brief Check whether the screen at the given index is the primary screen
    bool is_primary_screen(int index) const;

    /// @brief Get all WASAPI audio output devices
    QVector<AudioOutputInfo> get_all_audio_outputs() const;

    /// @brief Get all DShow audio input devices (microphones)
    QVector<AudioInputInfo> get_all_audio_inputs() const;

    /// @brief Get the default audio output device
    AudioOutputInfo get_default_audio_output() const;

    /// @brief Get the default audio input device
    AudioInputInfo get_default_audio_input() const;

private:
    DisplayInfo convert_to_display_info(QScreen *screen, int index, bool is_primary) const;
};


#endif //OBSIM_DEVICEMANAGER_H
