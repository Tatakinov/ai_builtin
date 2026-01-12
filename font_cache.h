#ifndef FONT_CACHE_H_
#define FONT_CACHE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "font.h"

class FontCache {
    private:
        std::unordered_map<std::string, std::unique_ptr<WrapFont>> cache_;
    public:
        FontCache();
        ~FontCache();
        void setDefaultFont(const fontlist::fontfamily &family);
        std::unique_ptr<WrapFont> &getDefaultFont();
        std::unique_ptr<WrapFont> &get(const std::filesystem::path &path);
};

#endif // FONT_CACHE_H_
