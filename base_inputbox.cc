#include "base_inputbox.h"

#include <cassert>
#include <numeric>

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>

#include "ai.h"
#include "logger.h"
#include "util.h"

#define MOUSE_BUTTON_LEFT 1
#define MOUSE_BUTTON_MIDDLE 2
#define MOUSE_BUTTON_RIGHT 3

BaseInputBox::BaseInputBox(Ai *parent, std::unique_ptr<FontCache> &font_cache) : alive_(true), changed_(true), cursor_index_(0), font_cache_(font_cache), parent_(parent) {
    int r, g, b, a = 0xff;
    std::string s;
    s = parent_->getInfo(-1, "communicatebox.font.color.r", "0");
    util::to_x(s, r);
    s = parent_->getInfo(-1, "communicatebox.font.color.g", "0");
    util::to_x(s, g);
    s = parent_->getInfo(-1, "communicatebox.font.color.b", "0");
    util::to_x(s, b);
    color_ = {static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b), static_cast<Uint8>(a)};
}

BaseInputBox::~BaseInputBox() {
    if (window_ != nullptr) {
        SDL_StopTextInput(window_);
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
    }
}

void BaseInputBox::init(std::unique_ptr<ImageCache> &image_cache) {
    auto &info = image_cache->getRelative(filename());
    if (info) {
        window_ = SDL_CreateWindow(name().c_str(), info->width(), info->height(), SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
        SDL_GetWindowPosition(window_, &x_, &y_);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        SDL_SetRenderVSync(renderer_, 1);
        WrapSurface surface(info.value());
        texture_ = std::make_unique<WrapTexture>(renderer_, surface.surface(), surface.isUpconverted());
        std::string s;
        int x, y, w, h;
        s = parent_->getInfo(-1, "communicatebox.x", "0");
        util::to_x(s, x);
        s = parent_->getInfo(-1, "communicatebox.y", "0");
        util::to_x(s, y);
        s = parent_->getInfo(-1, "communicatebox.w", "200");
        util::to_x(s, w);
        s = parent_->getInfo(-1, "communicatebox.h", "20");
        util::to_x(s, h);
        r_ = {x, y, w, h};
        SDL_SetTextInputArea(window_, &r_, 0);
    }
    else {
        window_ = SDL_CreateWindow(name().c_str(), 200, 20, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
        SDL_GetWindowPosition(window_, &x_, &y_);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        SDL_SetRenderVSync(renderer_, 1);
        texture_ = std::make_unique<WrapTexture>(renderer_, 200, 20);
        SDL_SetRenderTarget(renderer_, texture_->texture());
        SDL_SetRenderDrawColor(renderer_, 0xff, 0xff, 0xff, 0xff);
        SDL_RenderClear(renderer_);
        r_ = {0, 0, 200, 20};
        SDL_SetTextInputArea(window_, &r_, 0);
    }
    SDL_PropertiesID p = SDL_CreateProperties();
    // FIXME password, number, etc
    SDL_SetNumberProperty(p, SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT);
    SDL_SetBooleanProperty(p, SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, false);
    SDL_StartTextInputWithProperties(window_, p);

    cursor_texture_ = std::make_unique<WrapTexture>(renderer_, 1, 1);
    SDL_SetRenderTarget(renderer_, cursor_texture_->texture());
    SDL_SetRenderDrawColor(renderer_, color_.r, color_.g, color_.b, color_.a);
    SDL_RenderClear(renderer_);

    editing_texture_ = std::make_unique<WrapTexture>(renderer_, 1, 1);
    SDL_SetRenderTarget(renderer_, editing_texture_->texture());
    SDL_SetRenderDrawColor(renderer_, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(renderer_);
    Logger::log("inputbox.init:", SDL_GetError(), window_);
}

bool BaseInputBox::alive() const {
    return alive_;
}

void BaseInputBox::kill() {
    alive_ = false;
}

void BaseInputBox::draw() {
    if (!changed()) {
        return;
    }
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_SetRenderDrawColor(renderer_, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_->texture(), nullptr, nullptr);
    auto &font = font_cache_->get("default");
    int width = 0;
    {
        std::string str;
        for (int i = 0; i < text_.size(); i++) {
            if (i >= cursor_index_) {
                break;
            }
            str += text_[i];
        }
        if (str.length() > 0) {
            TTF_MeasureString(font->font(), str.data(), str.length(), 0, &width, nullptr);
        }
    }
    {
        SDL_FRect r = {r_.x + width, r_.y, 1, TTF_GetFontHeight(font->font())};
        SDL_RenderTexture(renderer_, cursor_texture_->texture(), nullptr, &r);
    }
    if (!text_.empty()) {
        std::string str;
        for (auto &s : text_) {
            str += s;
        }
        SDL_Surface *surface = TTF_RenderText_Blended(font->font(), str.data(), str.length(), color_);
        WrapTexture t(renderer_, surface, true);
        SDL_FRect r = {r_.x, r_.y, surface->w, surface->h};
        SDL_RenderTexture(renderer_, t.texture(), nullptr, &r);
        SDL_DestroySurface(surface);
    }
    if (!editing_.empty()) {
        SDL_Color c = {0x00, 0x00, 0x00, 0xff};
        SDL_Surface *surface = TTF_RenderText_Blended(font->font(), editing_.data(), editing_.length(), c);
        WrapTexture t(renderer_, surface, true);
        SDL_FRect r = {r_.x + width, r_.y, surface->w, surface->h};
        SDL_RenderTexture(renderer_, editing_texture_->texture(), nullptr, &r);
        SDL_RenderTexture(renderer_, t.texture(), nullptr, &r);
        SDL_DestroySurface(surface);
    }
    return;
}

bool BaseInputBox::swapBuffers() {
    if (changed()) {
        SDL_SetRenderTarget(renderer_, nullptr);
        SDL_RenderPresent(renderer_);
    }
    bool ret = changed();
    update();
    return ret;
}

void BaseInputBox::activate(std::string text) {
    kill();
}

void BaseInputBox::cancel() {
    kill();
}

void BaseInputBox::key(const SDL_KeyboardEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    if (!event.down) {
        return;
    }
    switch (event.key) {
        case SDLK_RETURN:
            activate(std::accumulate(text_.begin(), text_.end(), std::string()));
            change();
            break;
        case SDLK_ESCAPE:
            cancel();
            break;
        case SDLK_BACKSPACE:
            if (text_.size() > 0 && cursor_index_ > 0) {
                text_.erase(std::next(text_.begin(), --cursor_index_));
                change();
            }
            break;
        case SDLK_RIGHT:
            cursor_index_ = std::min(cursor_index_ + 1, static_cast<int>(text_.size()));
            change();
            break;
        case SDLK_LEFT:
            cursor_index_ = std::max(cursor_index_ - 1, 0);
            change();
            break;
        default:
            break;
    }
}

void BaseInputBox::input(const SDL_TextInputEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    Logger::log("event.input", event.text);
    for (auto &s : util::UTF8Split(event.text)) {
        text_.insert(std::next(text_.begin(), cursor_index_++), s);
    }
    change();
}

void BaseInputBox::edit(const SDL_TextEditingEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    Logger::log("event.editing", event.text);
    editing_ = event.text;
    change();
}

void BaseInputBox::motion(const SDL_MouseMotionEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    if (util::isWayland()) {
        return;
    }
    if (drag_) {
        if (event.x == drag_->x && event.y == drag_->y) {
            return;
        }
        x_ += event.x - drag_->x;
        y_ += event.y - drag_->y;
        SDL_SetWindowPosition(window_, x_, y_);
    }
}

void BaseInputBox::button(const SDL_MouseButtonEvent &event) {
    if (event.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    if (event.button == MOUSE_BUTTON_LEFT) {
        if (event.down) {
            drag_ = std::make_optional<Offset>(event.x, event.y);
        }
        else {
            drag_ = std::nullopt;
        }
    }
}
