#ifndef RENDER_INFO_H_
#define RENDER_INFO_H_

#include <filesystem>
#include <memory>
#include <variant>
#include <vector>

#include "texture.h"
#include "misc.h"

class Character;

namespace post {
    struct Rect {
        int x, y, w, h;
        bool operator==(const Rect &l) const {
            return x == l.x && y == l.y && w == l.w && h == l.h;
        }
    };

    struct ColorInt {
        int r, g, b, a;
        bool operator==(const ColorInt &l) const {
            return r == l.r && g == l.g && b == l.b && a == l.a;
        }
    };

    using Color = std::variant<ColorInt, std::string>;

    using IntString = std::variant<int, std::string>;
    using BoolString = std::variant<bool, std::string>;

    struct Attribute {
        std::filesystem::path font;
        std::optional<int> height;
        Color color;
        BoolString bold, italic, strike, underline, sup, sub;
        bool inline_, opaque, use_self_alpha, fixed, foreground, is_sstp_marker;
        std::optional<Rect> clipping;
        bool operator==(const Attribute &l) const {
            return font == l.font && height == l.height && color == l.color &&
                bold == l.bold && italic == l.italic && strike == l.strike &&
                underline == l.underline && sup == l.sup && sub == l.sub &&
                inline_ == l.inline_ && opaque == l.opaque &&
                use_self_alpha == l.use_self_alpha && fixed == l.fixed &&
                foreground == l.foreground &&
                is_sstp_marker == l.is_sstp_marker && clipping == l.clipping;
        }
    };

    enum class ContentType {
        Undefined, Text, Image
    };

    struct Content {
        ContentType type;
        std::string data;
        Attribute attr;
        bool operator==(const Content &l) const {
            return type == l.type && data == l.data && attr == l.attr;
        }
    };

    enum class PointType {
        Absolute, Relative
    };

    struct Point {
        PointType type;
        int value;
        bool operator==(const Point &l) const {
            return type == l.type && value == l.value;
        }
    };

    struct LinkBegin {
        bool is_anchor;
        std::string event;
        std::vector<std::string> args;
        bool operator==(const LinkBegin &l) const {
            return is_anchor == l.is_anchor && event == l.event && args == l.args;
        }
    };

    struct LinkEnd {
        bool operator==(const LinkEnd &l) const {
            return true;
        }
    };

    struct Head {
        bool valid;
        Point x, y;
        std::optional<LinkBegin> link_begin;
        std::optional<LinkEnd> link_end;
        bool in_anchor;
        bool operator==(const Head &l) const {
            return valid == l.valid && x == l.x && y == l.y && link_begin == l.link_begin && link_end == l.link_end && in_anchor == l.in_anchor;
        }
    };

    struct Data {
        Rect position;
        Content content;
        Head head;
        bool operator==(const Data &l) const {
            return position == l.position && content == l.content && head == l.head;
        }
    };

    struct Post {
        std::vector<Data> data;
        bool operator==(const Post &l) const {
            return data == l.data;
        }
    };
}

struct LinkContent {
    bool is_anchor;
    std::string text;
    std::string event;
    std::vector<std::string> args;
    bool operator==(const LinkContent &l) const {
        return is_anchor == l.is_anchor && text == l.text && event == l.event && args == l.args;
    }
};

struct Link {
    std::vector<post::Rect> hit_region_list;
    LinkContent content;
    bool operator==(const Link &l) const {
        return hit_region_list == l.hit_region_list && content == l.content;
    }
};

class RenderInfo {
    private:
        Character *parent_;
        int side_;
        int balloon_id_;
        bool direction_;
        int scroll_;
        bool shown_;
        post::Post post_;

        std::unique_ptr<FontCache> &font_cache_;
        std::unique_ptr<ImageCache> &image_cache_;
        int origin_x_, origin_y_;
        Rect valid_rect_;
        int wrap_width_;
        bool changed_;
        Link link_;

        void reconfigure();
        void calculatePosition();
    public:
        RenderInfo(Character *parent, int side, std::unique_ptr<FontCache> &font_cache, std::unique_ptr<ImageCache> &image_cache);
        ~RenderInfo();
        bool operator==(const RenderInfo &l) const {
            return side_ == l.side_ && balloon_id_ == l.balloon_id_ &&
                direction_ == l.direction_ &&
                shown_ == l.shown_ && post_ == l.post_;
        }

        void setID(int id);
        void setDirection(bool direction);
        void scroll(int diff);
        void hit(int x, int y);
        std::vector<post::Rect> getHitRegion() const;
        Link getLink() const {
            return link_;
        }
        void show() {
            shown_ = true;
        }
        void hide() {
            shown_ = false;
        }
        void update() {
            changed_ = false;
        }
        void change() {
            changed_ = true;
        }
        bool changed() const {
            return changed_;
        }
        void clear(bool initialize);
        void setOrigin(int x, int y);
        void setValidRect(int x, int y, int w, int h);
        void setWrapPoint(int x);
        void newBuffer(bool new_line);
        std::unique_ptr<WrapSurface> getSurface();
        void appendText(const std::string &text);
        void appendLinkBegin(bool is_anchor, const std::string &event, const std::vector<std::string> &args);
        void appendLinkEnd();
        void setCursorPosition(std::string axis, double value, bool is_absolute, MoveUnit unit);
};

#endif // RENDER_INFO_H_
