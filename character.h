#ifndef CHARACTER_H_
#define CHARACTER_H_

#include <memory>
#include <optional>
#include <vector>

#include <SDL3/SDL_video.h>

#include "image_cache.h"
#include "font_cache.h"
#include "misc.h"
#include "render_info.h"
#include "texture.h"

class Ai;
class Window;

class Character {
    private:
        Ai *parent_;
        std::unique_ptr<ImageCache> &image_cache_;
        std::unique_ptr<FontCache> &font_cache_;
        int side_;
        std::string name_;
        std::unordered_map<SDL_DisplayID, std::unique_ptr<Window>> windows_;
        Rect rect_;
        Offset offset_;
        std::optional<DragPosition> drag_;
        CursorType current_cursor_type_;
        std::mutex mutex_;
        bool position_changed_;
        bool upconverted_;
        std::unique_ptr<WrapSurface> current_surface_;
        RenderInfo info_;
        bool raise_on_talk_;

    public:
        Character(Ai *parent, std::unique_ptr<ImageCache> &image_cache, std::unique_ptr<FontCache> &font_cache, int side, const std::string &name);
        ~Character();
        std::string name() const {
            return name_;
        }
        void create(SDL_DisplayID display_id);
        void destroy(SDL_DisplayID display_id);
        void draw();
        bool swapBuffers();
        int side() const {
            return side_;
        }
        void setScale(int scale);
        void show();
        void hide();
        void clearText(bool initialize);
        void setBalloonID(int id);
        void clearCache();
        Rect getRect() {
            Rect r;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                r = rect_;
            }
            return r;
        }
        void setSize(int w, int h);
        void setBalloonPosition(int x, int y);
        void setBalloonDirection(int direction);
        void raiseOnTalk();
        std::optional<DragPosition> drag() {
            return drag_;
        }
        void resetDrag();
        void setDrag(double x, double y) {
            drag_ = {x, y, rect_.x, rect_.y};
        }
        bool isInDragging() const {
            return drag_.has_value();
        }
        void setOffset(int x, int y);
        Offset getOffset() {
            Offset offset;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                offset = offset_;
            }
            return offset;
        }
        void resetPosition();
        std::string sendDirectSSTP(std::string method, std::string command, std::vector<std::string> args);
        void enqueueDirectSSTP(std::vector<Request> list);
        Link getLink() const;
        void motion(const SDL_MouseMotionEvent &event);
        void button(const SDL_MouseButtonEvent &event);
        void wheel(const SDL_MouseWheelEvent &event);
        void scroll(int diff);
        void hit(int x, int y);
        std::string getInfo(int id, std::string key, std::string default_);
        void appendText(const std::string &text);
        void appendLinkBegin(bool is_anchor, const std::string &event, const std::vector<std::string> &args);
        void appendLinkEnd();
        void setCursorPosition(std::string axis, double value, bool is_absolute, MoveUnit unit);
        void newLine();
};

#endif // CHARACTER_H_
