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
};
