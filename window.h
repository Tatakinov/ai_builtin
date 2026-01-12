#ifndef WINDOW_H_
#define WINDOW_H_

#include "misc.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#if defined(IS__NIX)
#include <wayland-client.h>
#endif // Linux/Unix

#include "image_cache.h"
#include "logger.h"
#include "misc.h"
#include "render_info.h"
#include "texture.h"
#include "util.h"

class Character;

class Window {
    struct State {
        bool press;
        bool drag;
        std::chrono::system_clock::time_point prev;
    };
    private:
        std::mutex mutex_;
        SDL_Window *window_;
        SDL_DisplayID id_;
        std::unordered_map<Uint8, State> mouse_state_;
        Position<double> cursor_position_;
        Character *parent_;
        Rect monitor_rect_;
        //int counter_;
        Offset offset_;
        std::optional<std::vector<int>> shape_;
        SDL_Renderer *renderer_;
        std::unique_ptr<TextureCache> texture_cache_;
        std::unique_ptr<WrapTexture> current_texture_;
        std::unique_ptr<WrapTexture> link_texture_;
        bool redrawn_;
        Link prev_link_;
#if defined(IS__NIX)
        wl_registry *reg_;
        wl_compositor *compositor_;
#endif // Linux/Unix

    public:
        Window(Character *parent, SDL_DisplayID id);
        virtual ~Window();

        operator bool() {
            return true;
        }

#if defined(IS__NIX)
        void setCompositor(wl_compositor *compositor) {
            compositor_ = compositor;
        }

        wl_compositor *compositor() {
            return compositor_;
        }
#endif // Linux/Unix

        void position(int x, int y);

        void draw(Offset offset, const RenderInfo &info, std::unique_ptr<WrapSurface> &surface);
        bool swapBuffers();

        void show() {
            SDL_ShowWindow(window_);
        }

        void hide() {
            SDL_HideWindow(window_);
        }

        Rect getMonitorRect() const;

        double distance(int x, int y) const;

        void clearCache();

        void motion(const SDL_MouseMotionEvent &event);
        void button(const SDL_MouseButtonEvent &event);
        void wheel(const SDL_MouseWheelEvent &event);
};

#endif // WINDOW_H_
