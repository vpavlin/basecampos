#include "basecampos_system_impl.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

    std::string battery;
    for (const char* bat : { "BAT0", "BAT1" }) {
        std::string base = std::string("/sys/class/power_supply/") + bat;
        std::string cap = readFile(base + "/capacity");
        if (!cap.empty()) {
            battery = ",\"battery\":{\"percent\":" + std::to_string(std::atoi(cap.c_str()))
                + ",\"status\":\"" + esc(readFile(base + "/status")) + "\"}";
            break;
        }
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

std::string BasecamposSystemImpl::volume()
{
    // "Volume: 0.40" or "Volume: 0.40 [MUTED]"
    Result r = run({ "wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@" });
    if (r.code != 0)
        return error(r.out.empty() ? "no audio output" : r.out);
    double v = 0;
    std::sscanf(r.out.c_str(), "Volume: %lf", &v);
    bool muted = r.out.find("MUTED") != std::string::npos;
    return "{\"percent\":" + std::to_string(static_cast<int>(v * 100 + 0.5))
        + ",\"muted\":" + (muted ? "true" : "false") + "}";
}

std::string BasecamposSystemImpl::setVolume(int64_t percent)
{
    percent = std::clamp<int64_t>(percent, 0, 150);
    return okOrError(run({ "wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", std::to_string(percent) + "%" }));
}

std::string BasecamposSystemImpl::toggleMute()
{
    return okOrError(run({ "wpctl", "set-mute", "@DEFAULT_AUDIO_SINK@", "toggle" }));
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
