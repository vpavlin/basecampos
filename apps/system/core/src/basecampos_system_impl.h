#pragma once

#include <cstdint>
#include <string>

#include "logos_module_context.h"

/**
 * @brief The machine under Basecamp, for the System view.
 *
 * A ui_qml app cannot start a program or open a socket, so power, network and
 * sound go through here: this module runs in its own logos_host process. Each
 * method runs a system tool (systemctl, nmcli, wpctl) with an explicit argv,
 * never a shell, so a Wi-Fi name or password is passed as one argument and
 * nothing in it is interpreted. The tools decide what is allowed: logind
 * lets the active local session power off, NetworkManager asks polkit.
 *
 * Every method returns a JSON object; failures carry an "error" field.
 */
class BasecamposSystemImpl : public LogosModuleContext
{
public:
    /// Hostname, OS release, kernel, uptime in seconds, and battery if any.
    std::string info();

    /// Runs a power action: "poweroff", "reboot" or "suspend".
    std::string power(const std::string& action);

    /// Network devices and their state, and whether networking is online.
    std::string network();

    /// Visible Wi-Fi networks: ssid, signal (0-100), security, inUse.
    std::string wifiList();

    /// Connects to a Wi-Fi network; password may be empty for open networks.
    std::string wifiConnect(const std::string& ssid, const std::string& password);

    /// Turns the Wi-Fi radio on ("on") or off ("off").
    std::string wifiRadio(const std::string& state);

    /// Default output's volume (0-150, percent) and whether it is muted.
    std::string volume();

    /// Sets the default output's volume, in percent (clamped to 0-150).
    std::string setVolume(int64_t percent);

    /// Toggles mute on the default output.
    std::string toggleMute();

    /// Native (non-Basecamp) apps the system offers, from /etc/basecampos/external-apps:
    /// id, name and whether a window of it is open.
    std::string externalApps();

    /// Shows a native app full screen: focuses its window, or starts it.
    std::string openExternal(const std::string& id);

    /// Opens an http(s) URL in the system browser (the external app "browser").
    std::string openUrl(const std::string& url);

    /// Brings Basecamp itself back to the front.
    std::string showBasecamp();

    /// Screen brightness in percent, or available:false without a backlight.
    std::string brightness();

    /// Sets the screen brightness in percent (clamped to 1-100).
    std::string setBrightness(int64_t percent);

    /// Bluetooth: powered, and the known devices (mac, name, paired, connected).
    std::string bluetooth();

    /// Turns the Bluetooth radio on ("on") or off ("off").
    std::string bluetoothPower(const std::string& state);

    /// Scans for nearby devices for a few seconds, then returns bluetooth().
    std::string bluetoothScan();

    /// Connects ("connect": pairs and trusts first if needed), disconnects or forgets a device.
    std::string bluetoothDevice(const std::string& mac, const std::string& action);

    /// The battery charge limit (UPPER percent), or available:false without the battery-limit service.
    std::string chargeLimit();

    /// Sets the charge limit: 50-100 in steps of 5; 100 turns it off.
    std::string setChargeLimit(int64_t upper);

    /// Tablet bits: whether auto-rotation is locked and whether an on-screen keyboard is running.
    std::string tablet();

    /// Locks or unlocks auto-rotation ("on" locks it).
    std::string setRotationLock(const std::string& state);

    /// Shows or hides the on-screen keyboard.
    std::string toggleKeyboard();

    /// Outputs: the internal one, an external one if connected, and whether it mirrors the internal.
    std::string displays();

    /// Mirrors the internal screen onto the external one ("on") or stops ("off").
    std::string setMirror(const std::string& state);

    /// The internal screen's scale factor (Sway), e.g. 1.2.
    std::string scale();

    /// Sets the internal screen's scale, in percent (100-250).
    std::string setScale(int64_t percent);

    /// Microphone (default source): volume in percent and whether it is muted.
    std::string microphone();

    /// Sets the microphone volume in percent (0-300; the Duet's DMIC is quiet).
    std::string setMicrophone(int64_t percent);

    /// Toggles the microphone mute.
    std::string toggleMicrophone();

    /// Keyboard layouts of the physical keyboard, and the active one.
    std::string keyboardLayout();

    /// Switches the physical keyboard to its next layout.
    std::string nextKeyboardLayout();

    /// Screen sharing over VNC (the session's screen-share unit): available, active.
    std::string screenShare();

    /// Starts ("on") or stops ("off") screen sharing.
    std::string setScreenShare(const std::string& state);

    /// Takes a screenshot of the whole screen into ~/Pictures; returns its path.
    std::string screenshot();
};
