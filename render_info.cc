#include "render_info.h"

#include <cassert>

#include "character.h"
#include "logger.h"
#include "util.h"

namespace {
    constexpr int kLineSpace = 1;
}

RenderInfo::RenderInfo(Character *parent, int side, std::unique_ptr<FontCache> &font_cache, std::unique_ptr<ImageCache> &image_cache) : parent_(parent), side_(side), balloon_id_(-1), direction_(false), scroll_(0), shown_(false), scale_(100), font_cache_(font_cache), image_cache_(image_cache), origin_x_(0), origin_y_(0), changed_(false) {
    clear(true);
}

RenderInfo::~RenderInfo() {}

void RenderInfo::reconfigure() {
    post::Post prev = post_;
    clear(true);
    // truly clear
    post_.data.clear();
    for (const auto &data : prev.data) {
        if (data.head.valid) {
            newBuffer(true);
            post_.data.back().head = data.head;
            calculatePosition();
        }
        else if (data.head.link_begin || data.head.link_end) {
            newBuffer(false);
            post_.data.back().head = data.head;
            calculatePosition();
        }
        else {
            bool continue_flag = (post_.data.back().content.type == post::ContentType::Text && data.content.type == post::ContentType::Text && post_.data.back().content.attr == data.content.attr);
            if (!continue_flag) {
                newBuffer(false);
                calculatePosition();
            }
        }
        post_.data.back().content.attr = data.content.attr;
        switch (data.content.type) {
            case post::ContentType::Text:
                for (auto s : util::UTF8Split(data.content.data)) {
                    appendText(s);
                }
                break;
            default:
                // TODO stub
                break;
        }
    }
}

void RenderInfo::calculatePosition() {
    if (post_.data.size() == 0) {
        return;
    }
    auto &last = post_.data.back();
    if (post_.data.size() == 1) {
        last.head.x.value = origin_x_;
        last.position.x = origin_x_;
    }
    else if (last.head.x.type == post::PointType::Absolute) {
        last.position.x = origin_x_ + last.head.x.value;
    }
    else {
        auto &pos = post_.data[post_.data.size() - 2].position;
        last.position.x = pos.x + pos.w + last.head.x.value;
    }
    if (post_.data.size() == 1) {
        last.head.y.value = origin_y_;
        last.position.y = origin_y_;
    }
    else if (last.head.y.type == post::PointType::Absolute) {
        last.position.y = origin_y_ + last.head.y.value;
    }
    else {
        auto &pos = post_.data[post_.data.size() - 2].position;
        last.position.y = pos.y + last.head.y.value;
    }
}

void RenderInfo::clear(bool initialize) {
    if (initialize) {
        post_.data.clear();
        newBuffer(true);
    }
    else {
        newBuffer(true);
        setCursorPosition("x", 0, true, MoveUnit::Px);
        setCursorPosition("y", 0, true, MoveUnit::Px);
        auto last = post_.data.back();
        post_.data.clear();
        post_.data.push_back(last);
    }
    link_ = {
        .hit_region_list = {},
        .content = {
            .is_anchor = false,
            .text = "",
            .event = "",
            .args = {},
        },
    };
}

void RenderInfo::setScale(int scale) {
    if (scale == scale_) {
        return;
    }
    scale_ = scale;
    change();
}

void RenderInfo::setOrigin(int x, int y) {
    if (origin_x_ == x && origin_y_ == y) {
        return;
    }
    origin_x_ = x;
    origin_y_ = y;
}

void RenderInfo::setValidRect(int x, int y, int w, int h) {
    if (valid_rect_.x == x && valid_rect_.y == y && valid_rect_.width == w && valid_rect_.height == h) {
        return;
    }
    valid_rect_ = {x, y, w, h};
}

void RenderInfo::setWrapPoint(int x) {
    if (wrap_width_ == x) {
        return;
    }
    wrap_width_ = x;
}

void RenderInfo::newBuffer(bool new_line) {
    change();
    if (post_.data.size() == 0) {
        post_.data.push_back({
            .position = {origin_x_, origin_y_, 0, 0},
            .content = {
                .type = post::ContentType::Undefined,
                .data = "",
                .attr = post::Attribute{
                    .font = "default",
                    .height = std::nullopt,
                    .color = "default",
                    .bold = "default",
                    .italic = "default",
                    .strike = "default",
                    .underline = "default",
                    .sup = "default",
                    .sub = "default",
                    .inline_ = false,
                    .opaque = false,
                    .use_self_alpha = false,
                    .fixed = false,
                    .foreground = false,
                    .is_sstp_marker = false,
                    .clipping = std::nullopt,
                }
            },
            .head = {
                .valid = new_line,
                .x = {
                    .type = post::PointType::Absolute,
                    .value = origin_x_
                },
                .y = {
                    .type = post::PointType::Absolute,
                    .value = origin_y_
                }
            }
        });
    }
    else {
        auto &last = post_.data.back();
        post_.data.push_back({
            .position = {last.position.x + last.position.w, last.position.y, 0, 0},
            .content = {
                .type = post::ContentType::Undefined,
                .data = "",
                .attr = last.content.attr // FIXME init image option
            },
            .head = {
                .valid = new_line,
                .x = {
                    .type = post::PointType::Relative,
                    .value = 0
                },
                .y = {
                    .type = post::PointType::Relative,
                    .value = 0
                },
            }
        });
    }
}

std::unique_ptr<WrapSurface> RenderInfo::getSurface() {
    if (balloon_id_ == -1 || !shown_) {
        std::unique_ptr<WrapSurface> invalid;
        return invalid;
    }
    auto filename = util::balloonSide2str(side_, balloon_id_, direction_);
    auto &info = image_cache_->getRelative(filename);
    if (!info) {
        Logger::log("not found: ", filename);
        std::unique_ptr<WrapSurface> invalid;
        return invalid;
    }
    WrapSurface balloon(info.value());
    auto dst = std::make_unique<WrapSurface>(balloon.width(), balloon.height());
    SDL_ClearSurface(dst->surface(), 0, 0, 0, 0);
    SDL_SetSurfaceBlendMode(balloon.surface(), SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(balloon.surface(), nullptr, dst->surface(), nullptr);

    for (const auto &data : post_.data) {
        if (data.content.data.length() == 0) {
            continue;
        }
        auto &font = (font_cache_->get(data.content.attr.font)->font() != nullptr) ? font_cache_->get(data.content.attr.font) : font_cache_->get("default");
        auto old_size = TTF_GetFontSize(font->font());
        TTF_SetFontSize(font->font(), old_size * scale_ / 100.0);
        auto &color = data.content.attr.color;
        post::ColorInt c = {0, 0, 0, 0};
        if (std::holds_alternative<post::ColorInt>(color)) {
            c = std::get<post::ColorInt>(color);
        }
        else {
            auto s = std::get<std::string>(color);
            int id = util::balloon2id(balloon_id_, direction_);
            if (s == "default") {
                util::to_x(parent_->getInfo(id, "font.color.r", "0"), c.r);
                util::to_x(parent_->getInfo(id, "font.color.g", "0"), c.g);
                util::to_x(parent_->getInfo(id, "font.color.b", "0"), c.b);
            }
            else if (s == "disable") {
                util::to_x(parent_->getInfo(id, "disable.font.color.r", "0"), c.r);
                util::to_x(parent_->getInfo(id, "disable.font.color.g", "0"), c.g);
                util::to_x(parent_->getInfo(id, "disable.font.color.b", "0"), c.b);
            }
            c.a = 0xff;
        }
        SDL_Surface *text = TTF_RenderText_Blended(font->font(), data.content.data.data(), data.content.data.length(), {static_cast<Uint8>(c.r), static_cast<Uint8>(c.g), static_cast<Uint8>(c.b), static_cast<Uint8>(c.a)});
        SDL_Rect r = {data.position.x * scale_ / 100, (data.position.y - scroll_) * scale_ / 100, text->w * scale_ / 100, text->h * scale_ / 100};
        SDL_BlitSurface(text, nullptr, dst->surface(), &r);
        SDL_DestroySurface(text);
        TTF_SetFontSize(font->font(), old_size);
    }
    return dst;
}

void RenderInfo::setID(int id) {
    int tmp_id = (id / 2) * 2;
    auto filename = util::balloonSide2str(side_, tmp_id, direction_);
    auto &info = image_cache_->getRelative(filename);
    if (!info) {
        Logger::log("balloon.set:", filename, "not found");
        return;
    }
    balloon_id_ = tmp_id;
    id = util::balloon2id(balloon_id_, direction_);
    int x = 0, y = 0, w = 0, h = 0;
    std::string value = parent_->getInfo(id, "origin.x", "0");
    util::to_x(value, x);
    if (x < 0) {
        x += info->width();
    }
    value = parent_->getInfo(id, "origin.y", "0");
    util::to_x(value, y);
    if (y < 0) {
        y += info->height();
    }
    setOrigin(x, y);

    value = parent_->getInfo(id, "validrect.left", "0");
    util::to_x(value, x);
    if (x < 0) {
        x += info->width();
    }
    value = parent_->getInfo(id, "validrect.top", "0");
    util::to_x(value, y);
    if (y < 0) {
        y += info->height();
    }
    value = parent_->getInfo(id, "validrect.right", "0");
    util::to_x(value, w);
    if (w <= 0) {
        w += info->width();
    }
    w -= x;
    value = parent_->getInfo(id, "validrect.bottom", "0");
    util::to_x(value, h);
    if (h <= 0) {
        h += info->height();
    }
    h -= y;
    setValidRect(x, y, w, h);

    value = parent_->getInfo(id, "wordwrappoint.x", "0");
    util::to_x(value, x);
    if (x <= 0) {
        x += info->width();
    }
    setWrapPoint(x);
    reconfigure();
    change();
}

void RenderInfo::setDirection(bool direction) {
    if (direction_ != direction) {
        direction_ = direction;
        setID(balloon_id_);
        change();
    }
}

void RenderInfo::scroll(int diff) {
    auto filename = util::balloonSide2str(side_, balloon_id_, direction_);
    auto &info = image_cache_->getRelative(filename);
    if (!info) {
        return;
    }
    int h_max = 0;
    for (auto &data : post_.data) {
        h_max = std::max(h_max, data.position.y + data.position.h);
    }
    auto &font = font_cache_->get("default");
    scroll_ -= diff * (TTF_GetFontHeight(font->font()) + kLineSpace);
    scroll_ = std::min(scroll_, h_max - info->height());
    scroll_ = std::max(scroll_, 0);
    change();
}

void RenderInfo::hit(int x, int y) {
    std::vector<post::Rect> list;
    LinkContent content;
    bool hit = false;
    bool in_link = false;
    for (auto &data : post_.data) {
        if (data.head.link_begin) {
            auto begin = data.head.link_begin.value();
            content.is_anchor = begin.is_anchor;
            content.event = begin.event;
            content.args = begin.args;
            in_link = true;
        }
        if (in_link) {
            auto &p = data.position;
            list.push_back(p);
            if (data.content.type == post::ContentType::Text) {
                content.text += data.content.data;
            }
            hit = hit || (x >= p.x && x < p.x + p.w && y >= (p.y - scroll_) && y < (p.y - scroll_) + p.h);
        }
        if (data.head.link_end) {
            in_link = false;
            if (hit) {
                break;
            }
            list.clear();
            content = {
                .is_anchor = false,
                .text = "",
                .event = "",
                .args = {},
            };
        }
    }
    if (list != link_.hit_region_list) {
        link_.hit_region_list = list;
        link_.content = content;
        change();
    }
}

std::vector<post::Rect> RenderInfo::getHitRegion() const {
    auto ret = link_.hit_region_list;
    for (auto &r : ret) {
        r.y -= scroll_;
    }
    return ret;
}

void RenderInfo::appendText(const std::string &text) {
    if (text.empty()) {
        return;
    }
    if (balloon_id_ == -1) {
        setID(0);
    }
    change();
    auto &last = post_.data.back();
    auto &font = font_cache_->get(last.content.attr.font) ? font_cache_->get(last.content.attr.font) : font_cache_->get("default");
    std::string tmp;
    int width;
    switch (last.content.type) {
        case post::ContentType::Image:
            newBuffer(false);
        case post::ContentType::Undefined:
            post_.data.back().content.type = post::ContentType::Text;
            post_.data.back().content.data = text;
            calculatePosition();
            TTF_MeasureString(font->font(), text.data(), text.length(), 0, &width, nullptr);
            post_.data.back().position.w = width;
            post_.data.back().position.h = TTF_GetFontHeight(font->font());
            break;
        case post::ContentType::Text:
            tmp = last.content.data + text;
            TTF_MeasureString(font->font(), tmp.data(), tmp.length(), 0, &width, nullptr);
            if (width < wrap_width_ - origin_x_) {
                last.content.data = tmp;
                last.position.w = width;
            }
            else {
                newBuffer(false);
                setCursorPosition("x", 0, true, MoveUnit::Px);
                setCursorPosition("y", 1, false, MoveUnit::Lh);
                post_.data.back().content.type = post::ContentType::Text;
                post_.data.back().content.data = text;
                TTF_MeasureString(font->font(), text.data(), text.length(), 0, &width, nullptr);
                post_.data.back().position.w = width;
                post_.data.back().position.h = TTF_GetFontHeight(font->font());
            }
            break;
        default:
            break;
    }
    // scroll
    auto filename = util::balloonSide2str(side_, balloon_id_, direction_);
    auto &info = image_cache_->getRelative(filename);
    if (!info) {
        Logger::log("not found: ", filename);
        std::unique_ptr<WrapSurface> invalid;
        return;
    }
    int h_max = 0;
    for (auto &data : post_.data) {
        h_max = std::max(h_max, data.position.y + data.position.h);
    }
    scroll_ = std::max(0, h_max - info->height());
}

void RenderInfo::appendLinkBegin(bool is_anchor, const std::string &event, const std::vector<std::string> &args) {
    newBuffer(false);
    auto &last = post_.data.back();
    post::LinkBegin link = {
        .is_anchor = is_anchor,
        .event = event,
        .args = args,
    };
    last.head.link_begin = std::make_optional<post::LinkBegin>(link);
}

void RenderInfo::appendLinkEnd() {
    auto &last = post_.data.back();
    post::LinkEnd link;
    last.head.link_end = std::make_optional<post::LinkEnd>(link);
}

void RenderInfo::setCursorPosition(std::string axis, double value, bool is_absolute, MoveUnit unit) {
    auto &last = post_.data.back();
    auto &font = font_cache_->get(last.content.attr.font) ? font_cache_->get(last.content.attr.font) : font_cache_->get("default");
    int width;
    TTF_MeasureString(font->font(), "0", 1, 0, &width, nullptr);
    switch (unit) {
        case MoveUnit::Px:
            value = static_cast<int>(value);
            break;
        case MoveUnit::Em:
            if (axis == "x") {
                value *= width;
            }
            else if (axis == "y") {
                value *= TTF_GetFontHeight(font->font());
            }
            break;
        case MoveUnit::Lh:
            if (axis == "x") {
                value *= width;
            }
            else if (axis == "y") {
                value *= TTF_GetFontHeight(font->font()) + kLineSpace;
            }
            break;
    }
    if (axis == "x") {
        last.head.x.value = value;
        if (is_absolute) {
            last.head.x.type = post::PointType::Absolute;
        }
        else {
            last.head.x.type = post::PointType::Relative;
        }
    }
    else if (axis == "y") {
        last.head.y.value = value;
        if (is_absolute) {
            last.head.y.type = post::PointType::Absolute;
        }
        else {
            last.head.y.type = post::PointType::Relative;
        }
    }
    calculatePosition();
}
