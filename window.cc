#include "window.h"

#include <cassert>
#include <cmath>
#include <string>
#include <unordered_map>

#include "character.h"
#include "logger.h"
#include "misc.h"
#include "sstp.h"

#define MOUSE_BUTTON_LEFT 1
#define MOUSE_BUTTON_MIDDLE 2
#define MOUSE_BUTTON_RIGHT 3

Window::Window(Character *parent, SDL_DisplayID id)
    : window_(nullptr), parent_(parent), offset_({0, 0}),
    renderer_(nullptr), redrawn_(false), changed_(false),
    raise_on_talk_(false) {
    if (util::isWayland() && id > 0) {
        SDL_Rect r;
        SDL_GetDisplayBounds(id, &r);
        monitor_rect_ = { r.x, r.y, r.w, r.h };
    }
    else {
        monitor_rect_ = { 0, 0, 1, 1 };
    }
    if (util::isWayland()) {
        int width = -1, height = -1;
        int count = 0;
        auto *displays = SDL_GetDisplays(&count);
        for (int i = 0; i < count; i++) {
            SDL_Rect r;
            SDL_GetDisplayBounds(displays[i], &r);
            if (width == -1 || width > r.w) {
                width = r.w;
            }
            if (height == -1 || height > r.h) {
                height = r.h;
            }
        }
        monitor_rect_.width = width;
        monitor_rect_.height = height;
        SDL_free(displays);
        window_ = SDL_CreateWindow(parent_->name().c_str(), width, height, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_RESIZABLE);
    }
    else {
        window_ = SDL_CreateWindow(parent_->name().c_str(), 200, 200, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
    }
    renderer_ = SDL_CreateRenderer(window_, nullptr);
    SDL_SetRenderVSync(renderer_, 1);
    texture_cache_ = std::make_unique<TextureCache>();
    link_texture_ = std::make_unique<WrapTexture>(renderer_, 1, 1);
    SDL_SetRenderTarget(renderer_, link_texture_->texture());
    SDL_SetRenderDrawColor(renderer_, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(renderer_);
    SDL_BlendMode mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
    SDL_SetTextureBlendMode(link_texture_->texture(), mode);
    SDL_SetRenderTarget(renderer_, nullptr);
#if defined(IS__NIX)
    if (util::isWayland()) {
        const wl_registry_listener listener = {
            .global = [](void *data, wl_registry *reg, uint32_t id, const char *interface, uint32_t version) {
                Window *window = static_cast<Window *>(data);
                std::string s = interface;
                if (s == "wl_compositor") {
                    window->setCompositor(static_cast<wl_compositor *>(wl_registry_bind(reg, id, &wl_compositor_interface, 1)));
                }
            },
            .global_remove = [](void *data, wl_registry *reg, uint32_t id) {
            }
        };
        wl_display *display = static_cast<wl_display *>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
        reg_ = wl_display_get_registry(display);
        wl_registry_add_listener(reg_, &listener, this);
        wl_display_roundtrip(display);
    }
#endif // Linux/Unix
}

Window::~Window() {
    if (window_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
    }
}

void Window::position(int x, int y) {
    SDL_SetWindowPosition(window_, x, y);
}

void Window::draw(Offset offset, const RenderInfo &info, std::unique_ptr<WrapSurface> &surface) {
    if (offset_ == offset && !info.changed() && !changed_) {
        redrawn_ = false;
        return;
    }
    changed_ = false;
    if (raise_on_talk_) {
        raise_on_talk_ = false;
        SDL_RaiseWindow(window_);
    }
    auto m = getMonitorRect();
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetRenderDrawColor(renderer_, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer_);
    if (surface) {
        current_texture_ = std::make_unique<WrapTexture>(renderer_, surface->surface(), surface->isUpconverted());
    }
    else {
        current_texture_.reset();
    }
    if (current_texture_) {
        if (!util::isWayland()) {
            SDL_SetWindowSize(window_, current_texture_->width(), current_texture_->height());
        }
        parent_->setSize(current_texture_->width(), current_texture_->height());

        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_BlendMode mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
        SDL_SetTextureBlendMode(current_texture_->texture(), mode);
        SDL_FRect r = { offset.x - m.x, offset.y - m.y, current_texture_->width(), current_texture_->height() };
        SDL_RenderTexture(renderer_, current_texture_->texture(), nullptr, &r);

        auto region_list = info.getHitRegion();
        for (auto &region : region_list) {
            SDL_FRect r = { offset.x - m.x + region.x, offset.y - m.y + region.y, region.w, region.h };
            SDL_RenderTexture(renderer_, link_texture_->texture(), nullptr, &r);
        }
    }
    if (surface) {
        std::vector<int> shape;
#if defined(IS__NIX)
        bool is_wayland = util::isWayland();
        int x_begin = -1;
        wl_region *region = nullptr;
        if (is_wayland) {
            region = wl_compositor_create_region(compositor_);
        }
#endif // Linux/Unix
        {
            SDL_LockSurface(surface->surface());
            for (int y = 0; y < surface->height(); y++) {
                for (int x = 0; x < surface->width(); x++) {
                    unsigned char *p = static_cast<unsigned char *>(surface->surface()->pixels);
                    int index = y * surface->width() + x;
                    if (p[4 * index + 3]) {
                        shape.push_back(index);
#if defined(IS__NIX)
                        if (x_begin == -1 && is_wayland) {
                            x_begin = x;
                        }
                    }
                    else {
                        if (x_begin != -1) {
                            wl_region_add(region, offset.x - m.x + x_begin, offset.y - m.y + y, x - x_begin, 1);
                            x_begin = -1;
                        }
#endif // Linux/Unix
                    }
                }
#if defined(IS__NIX)
                if (x_begin != -1 && is_wayland) {
                    wl_region_add(region, offset.x - m.x + x_begin, offset.y - m.y + y, surface->width() - x_begin, 1);
                    x_begin = -1;
                }
#endif // Linux/Unix
            }
            SDL_UnlockSurface(surface->surface());
        }
        if (!shape_ || shape_ != shape || offset_ != offset) {
#if defined(IS__NIX)
            if (is_wayland) {
                wl_surface *surface = static_cast<wl_surface *>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
                wl_surface_set_input_region(surface, region);
            }
            else
#endif // Linux/Unix
            {
                SDL_SetWindowShape(window_, surface->surface());
            }
            shape_ = shape;
        }
    }
    else if (!shape_ || shape_->size() > 0) {
        shape_ = std::make_optional<std::vector<int>>();
#if defined(IS__NIX)
        if (util::isWayland()) {
            wl_region *region = wl_compositor_create_region(compositor_);
            wl_surface *surface = static_cast<wl_surface *>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
            wl_surface_set_input_region(surface, region);
        }
        else
#endif // Linux/Unix
        {
            int w, h;
            SDL_GetWindowSize(window_, &w, &h);
            auto s = std::make_unique<WrapSurface>(w, h);
            SDL_ClearSurface(s->surface(), 0, 0, 0, 0);
            SDL_SetWindowShape(window_, s->surface());
        }
    }
    offset_ = offset;
    redrawn_ = true;
    return;
}

bool Window::swapBuffers() {
    if (redrawn_) {
        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_RenderPresent(renderer_);
    }
    return redrawn_;
}

void Window::raiseOnTalk() {
    raise_on_talk_ = true;
}

double Window::distance(int x, int y) const {
    if (monitor_rect_.x <= x && monitor_rect_.x + monitor_rect_.width >= x &&
            monitor_rect_.y <= y && monitor_rect_.y + monitor_rect_.height >= y) {
        return 0;
    }
    if (monitor_rect_.x <= x && monitor_rect_.x + monitor_rect_.width >= x) {
        return std::min(std::abs(monitor_rect_.x - x), std::abs(monitor_rect_.x + monitor_rect_.width - x));
    }
    if (monitor_rect_.y <= y && monitor_rect_.y + monitor_rect_.height >= y) {
        return std::min(std::abs(monitor_rect_.y - y), std::abs(monitor_rect_.y + monitor_rect_.height - y));
    }
    double d;
    {
        int dx = monitor_rect_.x - x;
        int dy = monitor_rect_.y - y;
        d = sqrt(dx * dx + dy * dy);
    }
    {
        int dx = monitor_rect_.x + monitor_rect_.width - x;
        int dy = monitor_rect_.y - y;
        d = std::min(d, sqrt(dx * dx + dy * dy));
    }
    {
        int dx = monitor_rect_.x + monitor_rect_.width - x;
        int dy = monitor_rect_.y + monitor_rect_.height - y;
        d = std::min(d, sqrt(dx * dx + dy * dy));
    }
    {
        int dx = monitor_rect_.x - x;
        int dy = monitor_rect_.y + monitor_rect_.height - y;
        d = std::min(d, sqrt(dx * dx + dy * dy));
    }
    return d;
}

void Window::clearCache() {
    texture_cache_->clear();
}

void Window::motion(const SDL_MouseMotionEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    if (!parent_->drag().has_value() && mouse_state_[MOUSE_BUTTON_LEFT].press) {
        if (util::isWayland()) {
            parent_->setDrag(cursor_position_.x + monitor_rect_.x, cursor_position_.y + monitor_rect_.y);
        }
        else {
            parent_->setDrag(cursor_position_.x, cursor_position_.y);
        }
    }
    if (!parent_->drag().has_value()) {
        int xi = event.x, yi = event.y;
        if (util::isWayland()) {
            auto r = getMonitorRect();
            xi = xi + r.x;
            yi = yi + r.y;
        }
        parent_->hit(xi, yi);
        auto link = parent_->getLink();
        if (prev_link_ != link) {
            prev_link_ = link;
            if (link.content.event.empty()) {
                if (link.content.is_anchor) {
                    Request anchor = {"NOTIFY", "OnAnchorEnter", {}};
                    parent_->enqueueDirectSSTP({anchor});
                }
                else {
                    Request choice = {"NOTIFY", "OnChoiceEnter", {}};
                    parent_->enqueueDirectSSTP({choice});
                }
            }
            else {
                auto args = link.content.args;
                args.insert(args.begin(), link.content.event);
                args.insert(args.begin(), link.content.text);
                if (link.content.is_anchor) {
                    Request anchor = {"NOTIFY", "OnAnchorEnter", args};
                    parent_->enqueueDirectSSTP({anchor});
                }
                else {
                    Request choice = {"NOTIFY", "OnChoiceEnter", args};
                    parent_->enqueueDirectSSTP({choice});
                }
            }
        }
    }
    cursor_position_ = {event.x, event.y};
    if (parent_->drag().has_value()) {
        auto [dx, dy, px, py] = parent_->drag().value();
        auto x = event.x;
        auto y = event.y;
        if (util::isWayland()) {
            x = x + monitor_rect_.x;
            y = y + monitor_rect_.y;
        }
        parent_->setOffset(px + x - dx, py + y - dy);
        if (!util::isWayland()) {
            parent_->setDrag(dx, dy);
        }
    }
    for (auto &[k, v] : mouse_state_) {
        if (v.press) {
            v.drag = true;
        }
    }
}

void Window::button(const SDL_MouseButtonEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    mouse_state_[event.button].press = event.down;
    if (event.button == MOUSE_BUTTON_LEFT && !mouse_state_[event.button].press) {
        parent_->resetDrag();
    }
    if (event.button == MOUSE_BUTTON_LEFT && !mouse_state_[event.button].press && !mouse_state_[event.button].drag) {
        auto link = parent_->getLink().content;
        if (!link.event.empty()) {
            if (link.event.starts_with("On")) {
                Request req = {"NOTIFY", link.event, link.args};
                parent_->enqueueDirectSSTP({req});
            }
            else if (link.event.starts_with("script:")) {
                // TODO stub
            }
            else {
                auto args = link.args;
                args.insert(args.begin(), link.event);
                args.insert(args.begin(), link.text);
                if (link.is_anchor) {
                    Request anchor_ex = {"NOTIFY", "OnAnchorSelectEx", args};
                    Request anchor = {"NOTIFY", "OnAnchorSelect", {link.event}};
                    parent_->enqueueDirectSSTP({anchor_ex, anchor});
                }
                else {
                    Request choice_ex = {"NOTIFY", "OnChoiceSelectEx", args};
                    Request choice = {"NOTIFY", "OnChoiceSelect", {link.event}};
                    parent_->enqueueDirectSSTP({choice_ex, choice});
                }
            }
        }
    }
    for (auto &[k, v] : mouse_state_) {
        if (!v.press) {
            v.drag = false;
        }
    }
}

void Window::wheel(const SDL_MouseWheelEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    parent_->scroll(event.y);
}

void Window::maximized(const SDL_WindowEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    if (util::isWayland()) {
        int w, h;
        SDL_GetWindowSize(window_, &w, &h);
        monitor_rect_.width = w;
        monitor_rect_.height = h;
        changed_ = true;
    }
}

Rect Window::getMonitorRect() const {
    if (util::isWayland()) {
        return monitor_rect_;
    }
    else {
        auto cr = parent_->getRect();
        auto id = util::getNearestDisplay(cr.x + cr.width / 2, cr.y + cr.height / 2);
        SDL_Rect r;
        SDL_GetDisplayBounds(id, &r);
        return {r.x, r.y, r.w, r.h};
    }
}
