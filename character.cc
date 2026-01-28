#include "character.h"

#include <cassert>

#include "ai.h"
#include "sstp.h"
#include "util.h"
#include "window.h"

Character::Character(Ai *parent, std::unique_ptr<ImageCache> &image_cache, std::unique_ptr<FontCache> &font_cache, int side, const std::string &name)
    : parent_(parent), image_cache_(image_cache), font_cache_(font_cache), side_(side), name_(name),
    rect_({0, 0, 0, 0}), offset_({0, 0}),
    current_cursor_type_(CursorType::Default),
    upconverted_(false), info_(this, side, font_cache, image_cache) {
}

Character::~Character() {
}

void Character::create(SDL_DisplayID id) {
    windows_.try_emplace(id, std::make_unique<Window>(this, id));
}

void Character::destroy(SDL_DisplayID id) {
    if (windows_.contains(id)) {
        windows_.erase(id);
    }
}

void Character::draw() {
    position_changed_ = false;
    current_surface_ = info_.getSurface();
    for (auto &[_, v] : windows_) {
        if (util::isWayland()) {
            v->draw({rect_.x + offset_.x, rect_.y + offset_.y}, info_, current_surface_);
        }
        else {
            v->draw({0, 0}, info_, current_surface_);
        }
    }
    info_.update();
    return;
}

bool Character::swapBuffers() {
    bool redrawn = false;
    for (auto &[_, v] : windows_) {
        redrawn = v->swapBuffers() || redrawn;
    }
    return redrawn;
}

void Character::setScale(int scale) {
    info_.setScale(scale);
}

void Character::show() {
    for (auto &[_, v] : windows_) {
        v->show();
    }
}

void Character::hide() {
    for (auto &[_, v] : windows_) {
        v->hide();
    }
}

void Character::clearText(bool initialize) {
    info_.clear(initialize);
}

void Character::setBalloonID(int id) {
    if (id == -1) {
        info_.setID(-1);
        info_.hide();
        return;
    }
    info_.setID(id);
}

void Character::clearCache() {
    for (auto &[_, v] : windows_) {
        v->clearCache();
    }
}

void Character::setSize(int w, int h) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (rect_.width == w && rect_.height == h) {
        return;
    }
    rect_.width = w;
    rect_.height = h;
    std::vector<std::string> args = {util::to_s(side_), util::to_s(rect_.x), util::to_s(rect_.y), util::to_s(rect_.width), util::to_s(rect_.height)};
    Request req = {"EXECUTE", "UpdateBalloonRect", args};
    enqueueDirectSSTP({req});
    args = {util::to_s(side_)};
    req = {"EXECUTE", "ResetBalloonPosition", args};
    enqueueDirectSSTP({req});
}

void Character::setBalloonPosition(int x, int y) {
    rect_.x = x - offset_.x;
    rect_.y = y - offset_.y;
    if (!util::isWayland()) {
        for (auto &[_, v] : windows_) {
            v->position(x, y);
        }
    }
}

void Character::setBalloonDirection(int direction) {
    info_.setDirection(direction == 1);
}

void Character::raiseOnTalk() {
    for (auto &[_, v] : windows_) {
        v->raiseOnTalk();
    }
}

void Character::resetDrag() {
    drag_ = std::nullopt;
    position_changed_ = true;
}

void Character::setOffset(int x, int y) {
    if (x != rect_.x + offset_.x || y != rect_.y + offset_.y) {
        position_changed_ = true;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            offset_.x = x - rect_.x;
            offset_.y = y - rect_.y;
        }
        std::vector<std::string> args = {util::to_s(side_), util::to_s(offset_.x), util::to_s(offset_.y)};
        Request req = {"EXECUTE", "UpdateBalloonOffset", args};
        enqueueDirectSSTP({req});
    }
}

std::string Character::sendDirectSSTP(std::string method, std::string command, std::vector<std::string> args) {
    return parent_->sendDirectSSTP(method, command, args);
}

void Character::enqueueDirectSSTP(std::vector<Request> list) {
    parent_->enqueueDirectSSTP(list);
}

Link Character::getLink() const {
    return info_.getLink();
}

void Character::motion(const SDL_MouseMotionEvent &event) {
    for (auto &[_, v] : windows_) {
        v->motion(event);
    }
}

void Character::button(const SDL_MouseButtonEvent &event) {
    for (auto &[_, v] : windows_) {
        v->button(event);
    }
}

void Character::wheel(const SDL_MouseWheelEvent &event) {
    for (auto &[_, v] : windows_) {
        v->wheel(event);
    }
}

void Character::scroll(int diff) {
    info_.scroll(diff);
}

void Character::maximized(const SDL_WindowEvent &event) {
    for (auto &[_, v] : windows_) {
        v->maximized(event);
    }
}

void Character::hit(int x, int y) {
    if (util::isWayland()) {
        x -= rect_.x;
        y -= rect_.y;
    }
    info_.hit(x, y);
}

std::string Character::getInfo(int id, std::string key, std::string default_) {
    return parent_->getInfo(id, key, default_);
}

void Character::appendText(const std::string &text) {
    info_.appendText(text);
    info_.show();
}

void Character::appendLinkBegin(bool is_anchor, const std::string &event, const std::vector<std::string> &args) {
    info_.appendLinkBegin(is_anchor, event, args);
}

void Character::appendLinkEnd() {
    info_.appendLinkEnd();
}

void Character::setCursorPosition(std::string axis, double value, bool is_absolute, MoveUnit unit) {
    info_.setCursorPosition(axis, value, is_absolute, unit);
}

void Character::newLine() {
    info_.newBuffer(true);
}
