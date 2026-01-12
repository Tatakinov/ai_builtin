#include "font.h"

#include "logger.h"

WrapFont::WrapFont(const fontlist::fontfamily &family) : name_(family.name) {
    fontlist::font font = family.fonts[0];
    int threshold = std::abs(400 - font.weight);
    for (auto &f : family.fonts) {
        if (f.style != fontlist::fontstyle::normal) {
            continue;
        }
        if (std::abs(400 - f.weight) < threshold) {
            threshold = std::abs(400 - f.weight);
            font = f;
        }
    }
    font_ = TTF_OpenFont(font.file.string().c_str(), font.size);
    //TTF_SetFontSizeDPI(font_, font.size, 96, 96);
}

WrapFont::WrapFont(const std::filesystem::path &path) : name_("") {
    // FIXME font pt size
    font_ = TTF_OpenFont(path.string().c_str(), 12);
}

WrapFont::~WrapFont() {
    if (font_ != nullptr) {
        TTF_CloseFont(font_);
    }
}

TTF_Font *WrapFont::font() {
    return font_;
}
