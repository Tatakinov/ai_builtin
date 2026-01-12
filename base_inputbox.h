#ifndef BASE_INPUTBOX_H_
#define BASE_INPUTBOX_H_

#include "misc.h"

#include <filesystem>
#include <memory>
#include <optional>

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include "logger.h"
#include "texture.h"

class Ai;

class BaseInputBox {
    private:
        SDL_Window *window_;
        SDL_Renderer *renderer_;
        SDL_Rect r_;
        bool alive_, changed_;
        int x_, y_;
        SDL_Color color_;
        std::vector<std::string> text_;
        int cursor_index_;
        std::string editing_;
        std::optional<Offset> drag_;
        std::unique_ptr<WrapTexture> cursor_texture_;
        std::unique_ptr<WrapTexture> editing_texture_;
        std::unique_ptr<WrapTexture> texture_;
        std::unique_ptr<FontCache> &font_cache_;
    protected:
        Ai *parent_;
    public:
        BaseInputBox(Ai *parent, std::unique_ptr<FontCache> &font_cache);
        virtual ~BaseInputBox();
        void init(std::unique_ptr<ImageCache> &image_cache);

        virtual std::string name() = 0;
        virtual std::filesystem::path filename() = 0;

        bool alive() const;
        void kill();

        void change() {
            changed_ = true;
        }
        bool changed() const {
            return changed_;
        }
        void update() {
            changed_ = false;
        }

        void draw();
        bool swapBuffers();

        virtual void activate(std::string text);
        virtual void cancel();

        void key(const SDL_KeyboardEvent &event);
        void input(const SDL_TextInputEvent &event);
        void edit(const SDL_TextEditingEvent &event);
        void motion(const SDL_MouseMotionEvent &event);
        void button(const SDL_MouseButtonEvent &event);
};

#endif // BASE_INPUTBOX_H_
