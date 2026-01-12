#include "font_cache.h"

FontCache::FontCache() {
    std::unique_ptr<WrapFont> invalid;
    cache_["invalid"] = std::move(invalid);
}

FontCache::~FontCache() {
    cache_.clear();
}

void FontCache::setDefaultFont(const fontlist::fontfamily &family) {
    cache_["default"] = std::make_unique<WrapFont>(family);
}

std::unique_ptr<WrapFont> &FontCache::getDefaultFont() {
    if (!cache_.contains("default")) {
        return cache_.at("invalid");
    }
    return cache_.at("default");
}

std::unique_ptr<WrapFont> &FontCache::get(const std::filesystem::path &path) {
    if (cache_.contains(path.string())) {
        return cache_.at(path.string());
    }
    cache_[path.string()] = std::make_unique<WrapFont>(path);
    return cache_[path.string()];
}
