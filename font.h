#ifndef FONT_H_
#define FONT_H_

#include <filesystem>
#include <string>

#include <SDL3_ttf/SDL_ttf.h>

#include "fontlist.hpp"

class WrapFont {
    private:
        TTF_Font *font_;
        std::string name_;
        float size_;
    public:
        WrapFont(const fontlist::fontfamily &family);
        WrapFont(const std::filesystem::path &path);
        ~WrapFont();
        TTF_Font *font();
        std::string name() const {
            return name_;
        }
};

#endif // FONT_H_
