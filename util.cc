#include "util.h"
#include "misc.h"

#include <cassert>
#include <cstdlib>
#include <random>
#include <sstream>

#if defined(IS__NIX)
#include <wayland-client-protocol.h>

#include "xdg-shell-client-protocol.h"
#endif // Linux/Unix

#include "logger.h"

#if defined(IS__NIX)
struct display_info {
    int width, height, refresh;
};

struct wl_thing {
    wl_display *display;
    wl_compositor *compositor;
    xdg_wm_base *wm_base;
};

struct free_required_wl_thing {
    wl_registry *reg;
    wl_surface *wl_s;
    xdg_surface *xdg_s;
    xdg_toplevel *toplevel;
    int w, h;

    free_required_wl_thing() : reg(nullptr), wl_s(nullptr), xdg_s(nullptr), toplevel(nullptr) {}
    ~free_required_wl_thing() {
        if (toplevel != nullptr) {
            xdg_toplevel_destroy(toplevel);
        }
        if (xdg_s != nullptr) {
            xdg_surface_destroy(xdg_s);
        }
        if (wl_s != nullptr) {
            wl_surface_destroy(wl_s);
        }
        if (reg != nullptr) {
            wl_registry_destroy(reg);
        }
    }
};
#endif // Linux/Unix


namespace {
    std::random_device rd;
    std::mt19937 mt(rd());
#if defined(IS__NIX)
    xdg_toplevel_listener t_listener = {
        .configure = [](void *data, xdg_toplevel *toplevel, int32_t width, int32_t height, wl_array *states) {
            Logger::log("toplevel.configure: ", width, height);
            auto *info = static_cast<display_info *>(data);
            info->width = width;
            info->height = height;
        }
    };

    xdg_wm_base_listener wm_listener = {
        .ping = [](void *data, xdg_wm_base *wm_base, uint32_t serial) {
            Logger::log("ping: ", serial);
            xdg_wm_base_pong(wm_base, serial);
        }
    };
    wl_output_listener o_listener = {
        .mode = [](void *data, wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
            if (flags == WL_OUTPUT_MODE_CURRENT) {
                auto *info = static_cast<display_info *>(data);
                info->width = width;
                info->height = height;
                info->refresh = refresh;
            }
        }
    };
    wl_surface_listener s_listener = {
        .enter = [](void *data, wl_surface *wl_surface, wl_output *output) {
            auto ret = wl_output_add_listener(output, &o_listener, data);
            Logger::log("add_o: ", ret);
        }
    };
#endif // Linux/Unix
}

namespace util {
    double random() {
        std::uniform_real_distribution<> dist(0, 1);
        return dist(mt);
    }

    int random(int a, int b) {
        std::uniform_int_distribution<> dist(a, b);
        return dist(mt);
    }

    std::string side2str(int side) {
        if (side == 0) {
            return "sakura";
        }
        else if (side == 1) {
            return "kero";
        }
        else if (side >= 2) {
            std::ostringstream oss;
            oss << "char" << side;
            return oss.str();
        }
        // unreachable
        assert(false);
    }

    int balloon2id(int balloon_id, bool direction) {
        if (balloon_id == -1) {
            return -1;
        }
        return (direction) ? (balloon_id + 1) : (balloon_id);
    }

    std::string balloonSide2str(int side, int balloon_id, bool direction) {
        std::ostringstream oss;
        oss << "balloon";
        if (side == 0) {
            oss << "s";
        }
        else {
            oss << "k";
        }
        oss << balloon2id(balloon_id, direction) << ".png";
        return oss.str();
    }

    bool isWayland() {
        std::string wayland = "wayland";
        return (SDL_GetCurrentVideoDriver() != nullptr && wayland == SDL_GetCurrentVideoDriver());
    }

    SDL_DisplayID getNearestDisplay(int x, int y) {
        SDL_DisplayID id = 0;
        int count = 0;
        double distance = -1;
        auto *displays = SDL_GetDisplays(&count);
        for (int i = 0; i < count; i++) {
            SDL_Rect r;
            SDL_GetDisplayBounds(displays[i], &r);
            double d;
            if (r.x <= x && r.x + r.w >= x &&
                    r.y <= y && r.y + r.h >= y) {
                d = 0;
            }
            else if (r.x <= x && r.x + r.w >= x) {
                d = std::min(std::abs(r.x - x), std::abs(r.x + r.w - x));
            }
            else if (r.y <= y && r.y + r.h >= y) {
                d = std::min(std::abs(r.y - y), std::abs(r.y + r.h - y));
            }
            else {
                {
                    int dx = r.x - x;
                    int dy = r.y - y;
                    d = sqrt(dx * dx + dy * dy);
                }
                {
                    int dx = r.x + r.w - x;
                    int dy = r.y - y;
                    d = std::min(d, sqrt(dx * dx + dy * dy));
                }
                {
                    int dx = r.x + r.w - x;
                    int dy = r.y + r.h - y;
                    d = std::min(d, sqrt(dx * dx + dy * dy));
                }
                {
                    int dx = r.x - x;
                    int dy = r.y + r.h - y;
                    d = std::min(d, sqrt(dx * dx + dy * dy));
                }
            }
            if (distance == -1 || distance > d) {
                distance = d;
                id = displays[i];
            }
        }
        SDL_free(displays);
        return id;
    }

    std::string readDescript(std::filesystem::path path) {
        std::ifstream ifs(path, std::ios_base::binary);
        if (!ifs) {
            return "";
        }
        std::ostringstream oss;
        std::string buffer;
        std::string charset = "Shift_JIS";
        while (std::getline(ifs, buffer, '\n')) {
            if (buffer.ends_with('\r')) {
                oss << buffer.substr(0, buffer.length() - 1) << '\n';
            }
            else {
                oss << buffer << '\n';
            }
            if (buffer.starts_with("charset,")) {
                int begin = 8;
                int end = buffer.length() - 1;
                for (; begin < buffer.length(); begin++) {
                    if (buffer[begin] == ' ') {
                        continue;
                    }
                    break;
                }
                for (; end > 0; end--) {
                    if (buffer[begin] == ' ') {
                        continue;
                    }
                    break;
                }
                charset = buffer.substr(begin, end - begin);
                Logger::log("charset in ", path, ": ", charset);
            }
        }
        buffer = oss.str();
        if (charset == "UTF-8" || buffer.empty()) {
            return buffer;
        }
        std::string converted;
        converted.resize(buffer.length());
        do {
            converted.resize(converted.length() * 2);
            const char *in = buffer.data();
            size_t in_length = buffer.length();
            char *out = converted.data();
            size_t out_length = converted.length();
            SDL_iconv_t cd = SDL_iconv_open("UTF-8", charset.c_str());
            if (reinterpret_cast<size_t>(cd) == SDL_ICONV_ERROR) {
                Logger::log("iconv_open error");
                return buffer;
            }
            auto err = SDL_iconv(cd, &in, &in_length, &out, &out_length);
            SDL_iconv_close(cd);
            if (err == -2) {
                continue;
            }
            else if (err < 0) {
                Logger::log("iconv error");
                return buffer;
            }
            else {
                converted.resize(out_length);
                break;
            }
        } while (true);
        return converted;
    }

    SDL_DisplayID getCurrentDisplayID() {
#if defined(IS__NIX)
        if (!isWayland()) {
            return 0;
        }
        int w = -1, h = -1;
        int count = 0;
        auto *displays = SDL_GetDisplays(&count);
        for (int i = 0; i < count; i++) {
            SDL_Rect r;
            SDL_GetDisplayBounds(displays[i], &r);
            if (w == -1 || w > r.w) {
                w = r.w;
            }
            if (h == -1 || h > r.h) {
                h = r.h;
            }
        }
        assert(w > 0);
        assert(h > 0);

        wl_thing thing;
        thing.display = wl_display_connect(nullptr);
        assert(thing.display);

        wl_registry_listener r_listener = {
            .global = [](void *data, wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
                wl_thing *thing = static_cast<wl_thing *>(data);
                std::string s = interface;
                if (s == "wl_compositor") {
                    thing->compositor = static_cast<wl_compositor *>(wl_registry_bind(reg, name, &wl_compositor_interface, 1));
                }
                else if (s == "xdg_wm_base") {
                    Logger::log("xdg_wm_base");
                    thing->wm_base = static_cast<xdg_wm_base *>(wl_registry_bind(reg, name, &xdg_wm_base_interface, 1));
                    auto ret = xdg_wm_base_add_listener(thing->wm_base, &wm_listener, nullptr);
                    Logger::log("ret:", ret);
                }
            }
        };

        auto free_thing = std::make_unique<free_required_wl_thing>();
        free_thing->w = w;
        free_thing->h = h;
        free_thing->reg = wl_display_get_registry(thing.display);
        wl_registry_add_listener(free_thing->reg, &r_listener, &thing);
        wl_display_roundtrip(thing.display);

        assert(thing.compositor);
        assert(thing.wm_base);

        free_thing->wl_s = wl_compositor_create_surface(thing.compositor);
        assert(free_thing->wl_s);

        display_info info = {0, 0, 0};

        free_thing->xdg_s = xdg_wm_base_get_xdg_surface(thing.wm_base, free_thing->wl_s);
        assert(free_thing->xdg_s);

        free_thing->toplevel = xdg_surface_get_toplevel(free_thing->xdg_s);
        Logger::log(free_thing->toplevel);
        assert(free_thing->toplevel);

        xdg_toplevel_add_listener(free_thing->toplevel, &t_listener, &info);
        xdg_toplevel_set_fullscreen(free_thing->toplevel, nullptr);

        wl_surface_commit(free_thing->wl_s);

        wl_display_roundtrip(thing.display);

        free_thing.reset();

        xdg_wm_base_destroy(thing.wm_base);
        wl_compositor_destroy(thing.compositor);
        wl_display_disconnect(thing.display);

        SDL_DisplayID id = 0;
        for (int i = 0; i < count; i++) {
            SDL_Rect r;
            SDL_GetDisplayBounds(displays[i], &r);
            if (info.width == r.w && info.height == r.h) {
                id = displays[i];
                break;
            }
        }
        SDL_free(displays);

        Logger::log("display: ", id);
        return id;
#else
        return 0;
#endif // Linux/Unix
    }

    std::vector<std::string> UTF8Split(const std::string str) {
        std::vector<std::string> ret;
        std::string buffer;
        int remain = 0;
        for (int i = 0; i < str.length(); i++, remain--) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (c >= 0x00 && c < 0x80 && remain == 0) {
                remain = 1;
                buffer = c;
                ret.push_back(buffer);
                buffer.clear();
            }
            else if (c >= 0xc2 && c < 0xe0 && remain == 0) {
                remain = 2;
                buffer += c;
            }
            else if (c >= 0xe0 && c < 0xf0 && remain == 0) {
                remain = 3;
                buffer += c;
            }
            else if (c >= 0xf0 && c < 0xf5 && remain == 0) {
                remain = 4;
                buffer += c;
            }
            else if (c >= 0x80 && c < 0xc0 && remain > 0) {
                buffer += c;
                if (remain == 1) {
                    ret.push_back(buffer);
                    buffer.clear();
                }
            }
            else {
                remain = 1;
            }
        }
        return ret;
    }
}
