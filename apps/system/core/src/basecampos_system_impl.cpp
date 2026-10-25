#include "basecampos_system_impl.h"

#include <algorithm>
#include <ctime>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {

// The environment children get. logos_host clears its own after start, so a
// program spawned from `environ` finds no display, no Sway socket and no
// runtime dir: rebuild it from the process's original one, keeping only what
// desktop programs need.
std::vector<std::string>& childEnvStorage()
{
    static std::vector<std::string> env = [] {
        static const char* keep[] = { "PATH", "HOME", "USER", "LANG", "LC_ALL", "XDG_RUNTIME_DIR",
            "WAYLAND_DISPLAY", "DISPLAY", "SWAYSOCK", "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE",
            "DBUS_SESSION_BUS_ADDRESS", "XDG_DATA_DIRS", "XDG_CONFIG_DIRS", "NIX_LD", "NIX_LD_LIBRARY_PATH" };
        std::vector<std::string> out;
        std::ifstream f("/proc/self/environ", std::ios::binary);
        std::string kv;
        while (std::getline(f, kv, '\0')) {
            auto eq = kv.find('=');
            if (eq == std::string::npos)
                continue;
            std::string k = kv.substr(0, eq);
            for (const char* want : keep)
                if (k == want)
                    out.push_back(kv);
        }
        if (std::none_of(out.begin(), out.end(), [](const std::string& e) { return e.rfind("PATH=", 0) == 0; }))
            out.push_back("PATH=/run/wrappers/bin:/run/current-system/sw/bin");
        return out;
    }();
    return env;
}

char** childEnv()
{
    static std::vector<char*> ptrs = [] {
        std::vector<char*> p;
        for (auto& e : childEnvStorage())
            p.push_back(const_cast<char*>(e.c_str()));
        p.push_back(nullptr);
        return p;
    }();
    return ptrs.data();
}

struct Result {
    int code = -1;
    std::string out;
};

// Where the tools are. logos_host inherits Basecamp's environment, and a
// session started by a display manager can have a bare PATH, so NixOS's
// system profile is tried first.
std::string findTool(const std::string& name)
{
    const char* dirs[] = { "/run/current-system/sw/bin", "/run/wrappers/bin", "/usr/bin", "/bin" };
    for (const char* d : dirs) {
        std::string p = std::string(d) + "/" + name;
        if (access(p.c_str(), X_OK) == 0)
            return p;
    }
    return name; // let posix_spawnp search PATH
}

// Runs argv without a shell; stdout and stderr together, stdin closed.
Result run(std::vector<std::string> argv)
{
    Result r;
    argv[0] = findTool(argv[0]);
    int pipefd[2];
    if (pipe(pipefd) != 0)
        return r;

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, pipefd[1], 1);
    posix_spawn_file_actions_adddup2(&fa, pipefd[1], 2);
    posix_spawn_file_actions_addclose(&fa, pipefd[0]);

    std::vector<char*> args;
    for (auto& a : argv)
        args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);

    pid_t pid = 0;
    int rc = posix_spawnp(&pid, args[0], &fa, nullptr, args.data(), childEnv());
    posix_spawn_file_actions_destroy(&fa);
    close(pipefd[1]);
    if (rc != 0) {
        close(pipefd[0]);
        r.out = "cannot run " + argv[0];
        return r;
    }
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof buf)) > 0)
        r.out.append(buf, static_cast<size_t>(n));
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    r.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return r;
}

// Starts argv in its own session, detached: stdio on /dev/null and through
// `setsid -f`, so the app outlives logos_host and never holds our pipes.
Result detach(std::vector<std::string> argv)
{
    argv.insert(argv.begin(), { findTool("setsid"), "-f" });
    argv[2] = findTool(argv[2]);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    std::vector<char*> args;
    for (auto& a : argv)
        args.push_back(const_cast<char*>(a.c_str()));
    args.push_back(nullptr);
    Result r;
    pid_t pid = 0;
    if (posix_spawn(&pid, args[0], &fa, nullptr, args.data(), childEnv()) == 0) {
        int status = 0;
        waitpid(pid, &status, 0);
        r.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        r.out = "cannot start " + argv[2];
    }
    posix_spawn_file_actions_destroy(&fa);
    return r;
}

std::string esc(const std::string& s)
{
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                o += b;
            } else {
                o += static_cast<char>(c);
            }
        }
    }
    return o;
}

std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

std::string error(const std::string& msg)
{
    return "{\"error\":\"" + esc(trim(msg)) + "\"}";
}

std::string okOrError(const Result& r)
{
    if (r.code == 0)
        return "{\"ok\":true}";
    return error(r.out.empty() ? "failed" : r.out);
}

// nmcli -t separates fields with ':' and escapes literal ones as '\:'.
std::vector<std::string> splitTerse(const std::string& line)
{
    std::vector<std::string> f;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size()) {
            cur += line[++i];
        } else if (line[i] == ':') {
            f.push_back(cur);
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    f.push_back(cur);
    return f;
}

std::vector<std::string> lines(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string l;
    while (std::getline(in, l))
        if (!l.empty())
            out.push_back(l);
    return out;
}

std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return trim(ss.str());
}

std::string osRelease(const std::string& key)
{
    std::ifstream f("/etc/os-release");
    std::string l;
    while (std::getline(f, l)) {
        if (l.rfind(key + "=", 0) == 0) {
            std::string v = l.substr(key.size() + 1);
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            return v;
        }
    }
    return "";
}

} // namespace

std::string BasecamposSystemImpl::info()
{
    std::string uptime = readFile("/proc/uptime");
    long up = std::atol(uptime.c_str());

    // Any power supply of type Battery (laptops: BAT0; the Duet's ChromeOS EC
    // names its own), with a time estimate from the gauge where it has one.
    std::string battery;
    if (DIR* d = opendir("/sys/class/power_supply")) {
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.')
                continue;
            std::string base = std::string("/sys/class/power_supply/") + e->d_name;
            if (readFile(base + "/type") != "Battery" || readFile(base + "/capacity").empty())
                continue;
            std::string status = readFile(base + "/status");
            long secs = std::atol(readFile(base + (status == "Charging" ? "/time_to_full_avg" : "/time_to_empty_avg")).c_str());
            if (secs <= 0) {
                // energy/power or charge/current, whichever the gauge reports
                double now = std::atof(readFile(base + "/energy_now").c_str()), rate = std::atof(readFile(base + "/power_now").c_str());
                if (now <= 0 || rate <= 0) {
                    now = std::atof(readFile(base + "/charge_now").c_str());
                    rate = std::atof(readFile(base + "/current_now").c_str());
                }
                if (now > 0 && rate > 0 && status == "Discharging")
                    secs = static_cast<long>(now / std::abs(rate) * 3600);
            }
            battery = ",\"battery\":{\"percent\":" + std::to_string(std::atoi(readFile(base + "/capacity").c_str()))
                + ",\"status\":\"" + esc(status) + "\",\"minutes\":" + std::to_string(secs / 60) + "}";
            break;
        }
        closedir(d);
    }

    return "{\"hostname\":\"" + esc(readFile("/proc/sys/kernel/hostname"))
        + "\",\"os\":\"" + esc(osRelease("PRETTY_NAME"))
        + "\",\"kernel\":\"" + esc(readFile("/proc/sys/kernel/osrelease"))
        + "\",\"uptime\":" + std::to_string(up) + battery + "}";
}

std::string BasecamposSystemImpl::power(const std::string& action)
{
    if (action != "poweroff" && action != "reboot" && action != "suspend")
        return error("unknown power action: " + action);
    return okOrError(run({ "systemctl", action }));
}

std::string BasecamposSystemImpl::network()
{
    Result st = run({ "nmcli", "-t", "-f", "STATE,CONNECTIVITY,WIFI", "general" });
    if (st.code != 0)
        return error(st.out.empty() ? "NetworkManager is not available" : st.out);
    auto g = splitTerse(trim(st.out));

    Result dev = run({ "nmcli", "-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device" });
    std::string devices;
    for (const auto& l : lines(dev.out)) {
        auto f = splitTerse(l);
        if (f.size() < 4 || f[1] == "loopback" || f[1] == "wireguard" || f[1] == "tun")
            continue;
        if (!devices.empty())
            devices += ",";
        devices += "{\"device\":\"" + esc(f[0]) + "\",\"type\":\"" + esc(f[1])
            + "\",\"state\":\"" + esc(f[2]) + "\",\"connection\":\"" + esc(f[3]) + "\"}";
    }
    return "{\"state\":\"" + esc(g.size() > 0 ? g[0] : "") + "\",\"connectivity\":\""
        + esc(g.size() > 1 ? g[1] : "") + "\",\"wifi\":\"" + esc(g.size() > 2 ? g[2] : "")
        + "\",\"devices\":[" + devices + "]}";
}

std::string BasecamposSystemImpl::wifiList()
{
    Result r = run({ "nmcli", "-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "device", "wifi", "list", "--rescan", "auto" });
    if (r.code != 0)
        return error(r.out);
    std::string nets;
    std::vector<std::string> seen;
    for (const auto& l : lines(r.out)) {
        auto f = splitTerse(l);
        if (f.size() < 4 || f[1].empty())
            continue;
        if (std::find(seen.begin(), seen.end(), f[1]) != seen.end())
            continue; // one row per name: the strongest comes first
        seen.push_back(f[1]);
        if (!nets.empty())
            nets += ",";
        nets += "{\"ssid\":\"" + esc(f[1]) + "\",\"signal\":" + std::to_string(std::atoi(f[2].c_str()))
            + ",\"security\":\"" + esc(f[3]) + "\",\"inUse\":" + (f[0] == "*" ? "true" : "false") + "}";
    }
    return "{\"networks\":[" + nets + "]}";
}

std::string BasecamposSystemImpl::wifiConnect(const std::string& ssid, const std::string& password)
{
    if (ssid.empty())
        return error("no network name");
    std::vector<std::string> argv = { "nmcli", "device", "wifi", "connect", ssid };
    if (!password.empty()) {
        argv.push_back("password");
        argv.push_back(password);
    }
    return okOrError(run(argv));
}

std::string BasecamposSystemImpl::wifiRadio(const std::string& state)
{
    if (state != "on" && state != "off")
        return error("state must be on or off");
    return okOrError(run({ "nmcli", "radio", "wifi", state }));
}

// PipeWire (wpctl) where it runs the sound, PulseAudio (pactl) otherwise: the
// Duet keeps PulseAudio for sound and runs PipeWire only for its cameras.
std::string BasecamposSystemImpl::volume()
{
    // "Volume: 0.40" or "Volume: 0.40 [MUTED]"
    Result r = run({ "wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@" });
    if (r.code == 0) {
        double v = 0;
        std::sscanf(r.out.c_str(), "Volume: %lf", &v);
        bool muted = r.out.find("MUTED") != std::string::npos;
        return "{\"percent\":" + std::to_string(static_cast<int>(v * 100 + 0.5))
            + ",\"muted\":" + (muted ? "true" : "false") + "}";
    }
    // "Volume: front-left: 26214 /  40% / ..." and "Mute: no"
    Result p = run({ "pactl", "get-sink-volume", "@DEFAULT_SINK@" });
    if (p.code != 0)
        return error(r.out.empty() ? "no audio output" : r.out);
    int pct = 0;
    auto slash = p.out.find('/');
    if (slash != std::string::npos)
        pct = std::atoi(p.out.c_str() + slash + 1);
    bool muted = run({ "pactl", "get-sink-mute", "@DEFAULT_SINK@" }).out.find("yes") != std::string::npos;
    return "{\"percent\":" + std::to_string(pct) + ",\"muted\":" + (muted ? "true" : "false") + "}";
}

std::string BasecamposSystemImpl::setVolume(int64_t percent)
{
    percent = std::clamp<int64_t>(percent, 0, 150);
    std::string v = std::to_string(percent) + "%";
    Result r = run({ "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", v });
    if (r.code == 0)
        return "{\"ok\":true}";
    return okOrError(run({ "pactl", "set-sink-volume", "@DEFAULT_SINK@", v }));
}

std::string BasecamposSystemImpl::toggleMute()
{
    Result r = run({ "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle" });
    if (r.code == 0)
        return "{\"ok\":true}";
    return okOrError(run({ "pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle" }));
}

// ---- native apps -----------------------------------------------------------

namespace {

struct ExternalApp {
    std::string id, appId, name;
    std::vector<std::string> argv;
};

// /etc/basecampos/external-apps, written by the NixOS module (root-owned):
// one app per line, tab-separated: id, sway app_id, display name, argv...
std::vector<ExternalApp> externalConfig()
{
    std::vector<ExternalApp> apps;
    std::ifstream f("/etc/basecampos/external-apps");
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '\t'))
            fields.push_back(field);
        if (fields.size() < 4)
            continue;
        apps.push_back({ fields[0], fields[1], fields[2], { fields.begin() + 3, fields.end() } });
    }
    return apps;
}

const ExternalApp* findExternal(const std::vector<ExternalApp>& apps, const std::string& id)
{
    for (const auto& a : apps)
        if (a.id == id)
            return &a;
    return nullptr;
}

// Only these characters reach a sway criteria string.
bool safeCriterion(const std::string& s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-' || c == '_';
    });
}

bool windowOpen(const std::string& tree, const std::string& appId)
{
    return tree.find("\"app_id\": \"" + appId + "\"") != std::string::npos;
}

} // namespace

std::string BasecamposSystemImpl::externalApps()
{
    std::string tree = run({ "swaymsg", "-t", "get_tree" }).out;
    std::string out;
    for (const auto& a : externalConfig()) {
        if (!out.empty())
            out += ",";
        out += "{\"id\":\"" + esc(a.id) + "\",\"name\":\"" + esc(a.name)
            + "\",\"running\":" + (windowOpen(tree, a.appId) ? "true" : "false") + "}";
    }
    return "{\"apps\":[" + out + "]}";
}

std::string BasecamposSystemImpl::openExternal(const std::string& id)
{
    auto apps = externalConfig();
    const ExternalApp* a = findExternal(apps, id);
    if (!a)
        return error("no such app: " + id);
    if (!safeCriterion(a->appId))
        return error("bad app_id in the system config");
    std::string tree = run({ "swaymsg", "-t", "get_tree" }).out;
    if (windowOpen(tree, a->appId))
        return okOrError(run({ "swaymsg", "[app_id=\"" + a->appId + "\"] focus" }));
    return okOrError(detach(a->argv));
}

std::string BasecamposSystemImpl::openUrl(const std::string& url)
{
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
        return error("only http(s) links open in the browser");
    auto apps = externalConfig();
    const ExternalApp* a = findExternal(apps, "browser");
    if (!a)
        return error("no browser configured");
    std::vector<std::string> argv = a->argv;
    argv.push_back(url); // one argument: never parsed by a shell
    return okOrError(detach(argv));
}

std::string BasecamposSystemImpl::showBasecamp()
{
    // The session marks Basecamp's window "basecamp" (see the sway config).
    return okOrError(run({ "swaymsg", "[con_mark=\"basecamp\"] focus" }));
}

// ---- display, bluetooth, battery, tablet ----------------------------------

namespace {

bool validMac(const std::string& m)
{
    if (m.size() != 17)
        return false;
    for (size_t i = 0; i < m.size(); ++i) {
        if (i % 3 == 2) {
            if (m[i] != ':')
                return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(m[i]))) {
            return false;
        }
    }
    return true;
}

std::string home()
{
    for (const auto& kv : childEnvStorage())
        if (kv.rfind("HOME=", 0) == 0)
            return kv.substr(5);
    return "/tmp";
}

const char* kChargeConf = "/etc/default/battery-limit";

// Sway's outputs in order; the first is taken as the internal panel.
std::vector<std::string> outputs()
{
    std::vector<std::string> names;
    std::string j = run({ "swaymsg", "-t", "get_outputs", "-r" }).out;
    const std::string key = "\"name\": \"";
    size_t pos = 0;
    while ((pos = j.find(key, pos)) != std::string::npos) {
        pos += key.size();
        size_t end = j.find('"', pos);
        std::string n = j.substr(pos, end - pos);
        if (safeCriterion(n))
            names.push_back(n);
        pos = end;
    }
    return names;
}

} // namespace

std::string BasecamposSystemImpl::brightness()
{
    // brightnessctl -m: device,class,current,percent%,max
    Result r = run({ "brightnessctl", "-m", "-c", "backlight", "info" });
    if (r.code != 0 || r.out.empty())
        return "{\"available\":false}";
    std::vector<std::string> f;
    std::stringstream ss(trim(r.out));
    std::string x;
    while (std::getline(ss, x, ','))
        f.push_back(x);
    int pct = f.size() > 3 ? std::atoi(f[3].c_str()) : 0;
    return "{\"available\":true,\"percent\":" + std::to_string(pct) + "}";
}

std::string BasecamposSystemImpl::setBrightness(int64_t percent)
{
    percent = std::clamp<int64_t>(percent, 1, 100);
    return okOrError(run({ "brightnessctl", "-q", "-c", "backlight", "set", std::to_string(percent) + "%" }));
}

std::string BasecamposSystemImpl::bluetooth()
{
    Result show = run({ "bluetoothctl", "show" });
    if (show.code != 0 || show.out.find("Controller") == std::string::npos)
        return "{\"available\":false}";
    bool powered = show.out.find("Powered: yes") != std::string::npos;
    std::string devs;
    for (const auto& l : lines(run({ "bluetoothctl", "devices" }).out)) {
        // "Device AA:BB:CC:DD:EE:FF Name"
        if (l.rfind("Device ", 0) != 0 || l.size() < 24)
            continue;
        std::string mac = l.substr(7, 17);
        if (!validMac(mac))
            continue;
        std::string name = l.size() > 25 ? l.substr(25) : mac;
        std::string info = run({ "bluetoothctl", "info", mac }).out;
        if (!devs.empty())
            devs += ",";
        devs += "{\"mac\":\"" + mac + "\",\"name\":\"" + esc(name)
            + "\",\"paired\":" + (info.find("Paired: yes") != std::string::npos ? "true" : "false")
            + ",\"connected\":" + (info.find("Connected: yes") != std::string::npos ? "true" : "false") + "}";
    }
    return std::string("{\"available\":true,\"powered\":") + (powered ? "true" : "false")
        + ",\"devices\":[" + devs + "]}";
}

std::string BasecamposSystemImpl::bluetoothPower(const std::string& state)
{
    if (state != "on" && state != "off")
        return error("state must be on or off");
    return okOrError(run({ "bluetoothctl", "power", state }));
}

std::string BasecamposSystemImpl::bluetoothScan()
{
    run({ "bluetoothctl", "--timeout", "8", "scan", "on" });
    return bluetooth();
}

std::string BasecamposSystemImpl::bluetoothDevice(const std::string& mac, const std::string& action)
{
    if (!validMac(mac))
        return error("not a Bluetooth address");
    if (action == "connect") {
        std::string info = run({ "bluetoothctl", "info", mac }).out;
        if (info.find("Paired: yes") == std::string::npos) {
            Result p = run({ "bluetoothctl", "--timeout", "20", "pair", mac });
            if (p.code != 0)
                return error(p.out.empty() ? "pairing failed" : p.out);
        }
        run({ "bluetoothctl", "trust", mac });
        return okOrError(run({ "bluetoothctl", "--timeout", "20", "connect", mac }));
    }
    if (action == "disconnect")
        return okOrError(run({ "bluetoothctl", "disconnect", mac }));
    if (action == "forget")
        return okOrError(run({ "bluetoothctl", "remove", mac }));
    return error("unknown action: " + action);
}

std::string BasecamposSystemImpl::chargeLimit()
{
    std::ifstream f(kChargeConf);
    if (!f)
        return "{\"available\":false}";
    int upper = 100;
    std::string l;
    while (std::getline(f, l))
        if (l.rfind("UPPER=", 0) == 0)
            upper = std::atoi(l.c_str() + 6);
    return "{\"available\":true,\"upper\":" + std::to_string(upper) + "}";
}

std::string BasecamposSystemImpl::setChargeLimit(int64_t upper)
{
    // The battery-limit service reads this file (and clamps what it reads);
    // it is owned by the session user, so no privilege is needed.
    upper = std::clamp<int64_t>(upper / 5 * 5, 50, 100);
    std::ofstream f(kChargeConf, std::ios::trunc);
    if (!f)
        return error("cannot write the charge limit");
    f << "UPPER=" << upper << "\nLOWER=" << std::max<int64_t>(0, upper - 5) << "\n";
    return f ? "{\"ok\":true}" : error("cannot write the charge limit");
}

std::string BasecamposSystemImpl::tablet()
{
    std::string lock = home() + "/.cache/rotation-lock";
    bool locked = access(lock.c_str(), F_OK) == 0;
    bool osk = run({ "pgrep", "-x", "wvkbd-mobintl" }).code == 0;
    bool sensor = access("/sys/bus/iio/devices/iio:device0", F_OK) == 0;
    return std::string("{\"rotationLocked\":") + (locked ? "true" : "false")
        + ",\"keyboard\":" + (osk ? "true" : "false")
        + ",\"orientationSensor\":" + (sensor ? "true" : "false") + "}";
}

std::string BasecamposSystemImpl::setRotationLock(const std::string& state)
{
    // The autorotate daemon (duet-kit) honours this file.
    std::string dir = home() + "/.cache";
    std::string lock = dir + "/rotation-lock";
    if (state == "on") {
        run({ "mkdir", "-p", dir });
        std::ofstream(lock).flush();
    } else if (state == "off") {
        std::remove(lock.c_str());
    } else {
        return error("state must be on or off");
    }
    return "{\"ok\":true}";
}

std::string BasecamposSystemImpl::toggleKeyboard()
{
    // wvkbd toggles its visibility on SIGRTMIN (the session starts it hidden).
    Result r = run({ "pkill", "-RTMIN", "-x", "wvkbd-mobintl" });
    if (r.code != 0)
        return error("no on-screen keyboard is running");
    return "{\"ok\":true}";
}

std::string BasecamposSystemImpl::displays()
{
    auto outs = outputs();
    if (outs.empty())
        return "{\"available\":false}";
    bool mirroring = run({ "systemctl", "--user", "is-active", "--quiet", "display-mirror.service" }).code == 0;
    std::string ext = outs.size() > 1 ? outs[1] : "";
    return "{\"available\":true,\"internal\":\"" + outs[0] + "\",\"external\":\"" + ext
        + "\",\"mirroring\":" + (mirroring ? "true" : "false") + "}";
}

std::string BasecamposSystemImpl::setMirror(const std::string& state)
{
    run({ "systemctl", "--user", "stop", "display-mirror.service" });
    if (state == "off")
        return "{\"ok\":true}";
    if (state != "on")
        return error("state must be on or off");
    auto outs = outputs();
    if (outs.size() < 2)
        return error("no external display connected");
    // A transient user unit, so the mirror outlives this call and stops cleanly.
    return okOrError(run({ "systemd-run", "--user", "--quiet", "--unit=display-mirror", "--collect",
        findTool("wl-mirror"), "--fullscreen-output", outs[1], "--scaling", "fit", "--no-show-cursor", outs[0] }));
}

// ---- scale, microphone, keyboard layout, screen sharing, screenshot --------

std::string BasecamposSystemImpl::scale()
{
    std::string j = run({ "swaymsg", "-t", "get_outputs", "-r" }).out;
    auto pos = j.find("\"scale\": ");
    if (pos == std::string::npos)
        return "{\"available\":false}";
    double sc = std::atof(j.c_str() + pos + 9);
    return "{\"available\":true,\"percent\":" + std::to_string(static_cast<int>(sc * 100 + 0.5)) + "}";
}

std::string BasecamposSystemImpl::setScale(int64_t percent)
{
    auto outs = outputs();
    if (outs.empty())
        return error("no display");
    percent = std::clamp<int64_t>(percent, 100, 250);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", percent / 100.0);
    return okOrError(run({ "swaymsg", "output " + outs[0] + " scale " + buf }));
}

std::string BasecamposSystemImpl::microphone()
{
    Result r = run({ "wpctl", "get-volume", "@DEFAULT_AUDIO_SOURCE@" });
    if (r.code == 0) {
        double v = 0;
        std::sscanf(r.out.c_str(), "Volume: %lf", &v);
        return "{\"available\":true,\"percent\":" + std::to_string(static_cast<int>(v * 100 + 0.5))
            + ",\"muted\":" + (r.out.find("MUTED") != std::string::npos ? "true" : "false") + "}";
    }
    Result p = run({ "pactl", "get-source-volume", "@DEFAULT_SOURCE@" });
    if (p.code != 0)
        return "{\"available\":false}";
    int pct = 0;
    auto slash = p.out.find('/');
    if (slash != std::string::npos)
        pct = std::atoi(p.out.c_str() + slash + 1);
    bool muted = run({ "pactl", "get-source-mute", "@DEFAULT_SOURCE@" }).out.find("yes") != std::string::npos;
    return "{\"available\":true,\"percent\":" + std::to_string(pct) + ",\"muted\":" + (muted ? "true" : "false") + "}";
}

std::string BasecamposSystemImpl::setMicrophone(int64_t percent)
{
    std::string v = std::to_string(std::clamp<int64_t>(percent, 0, 300)) + "%";
    if (run({ "wpctl", "set-volume", "@DEFAULT_AUDIO_SOURCE@", v }).code == 0)
        return "{\"ok\":true}";
    return okOrError(run({ "pactl", "set-source-volume", "@DEFAULT_SOURCE@", v }));
}

std::string BasecamposSystemImpl::toggleMicrophone()
{
    if (run({ "wpctl", "set-mute", "@DEFAULT_AUDIO_SOURCE@", "toggle" }).code == 0)
        return "{\"ok\":true}";
    return okOrError(run({ "pactl", "set-source-mute", "@DEFAULT_SOURCE@", "toggle" }));
}

std::string BasecamposSystemImpl::keyboardLayout()
{
    // The first physical keyboard's layouts ("xkb_layout_names") and active one.
    std::string j = run({ "swaymsg", "-t", "get_inputs", "-r" }).out;
    auto kb = j.find("\"type\": \"keyboard\"");
    if (kb == std::string::npos)
        return "{\"available\":false}";
    auto act = j.find("\"xkb_active_layout_name\": \"", kb);
    auto names = j.find("\"xkb_layout_names\": [", kb);
    if (act == std::string::npos || names == std::string::npos)
        return "{\"available\":false}";
    act += 27;
    std::string active = j.substr(act, j.find('"', act) - act);
    std::string list = j.substr(names + 21, j.find(']', names) - names - 21);
    int count = 0;
    for (char c : list)
        if (c == ',')
            ++count;
    return "{\"available\":true,\"active\":\"" + esc(active) + "\",\"layouts\":" + std::to_string(count + 1) + "}";
}

std::string BasecamposSystemImpl::nextKeyboardLayout()
{
    return okOrError(run({ "swaymsg", "input type:keyboard xkb_switch_layout next" }));
}

std::string BasecamposSystemImpl::screenShare()
{
    bool exists = run({ "systemctl", "--user", "cat", "screen-share.service" }).code == 0;
    if (!exists)
        return "{\"available\":false}";
    bool active = run({ "systemctl", "--user", "is-active", "--quiet", "screen-share.service" }).code == 0;
    return std::string("{\"available\":true,\"active\":") + (active ? "true" : "false") + "}";
}

std::string BasecamposSystemImpl::setScreenShare(const std::string& state)
{
    if (state != "on" && state != "off")
        return error("state must be on or off");
    return okOrError(run({ "systemctl", "--user", state == "on" ? "start" : "stop", "screen-share.service" }));
}

std::string BasecamposSystemImpl::screenshot()
{
    std::string dir = home() + "/Pictures";
    run({ "mkdir", "-p", dir });
    char name[64];
    std::time_t t = std::time(nullptr);
    std::strftime(name, sizeof name, "/screenshot-%Y%m%d-%H%M%S.png", std::localtime(&t));
    std::string path = dir + name;
    Result r = run({ "grim", path });
    if (r.code != 0)
        return error(r.out.empty() ? "screenshot failed" : r.out);
    return "{\"ok\":true,\"path\":\"" + esc(path) + "\"}";
}
