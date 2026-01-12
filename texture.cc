#include "texture.h"

#include <cassert>

#include "image_cache.h"

namespace {
    std::unique_ptr<WrapTexture> invalid_texture;
}

WrapSurface::WrapSurface(int w, int h) : is_upconverted_(false) {
    surface_ = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ABGR8888);
}

WrapSurface::WrapSurface(ImageInfo &info) : is_upconverted_(info.isUpconverted()) {
    surface_ = SDL_CreateSurfaceFrom(info.width(), info.height(), SDL_PIXELFORMAT_ABGR8888, info.get().data(), info.width() * 4);
}

WrapSurface::~WrapSurface() {
    if (surface_ != nullptr) {
        SDL_DestroySurface(surface_);
    }
}


WrapTexture::WrapTexture(SDL_Renderer *renderer, int w, int h) : is_upconverted_(false) {
    texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h);
}

WrapTexture::WrapTexture(SDL_Renderer *renderer, SDL_Surface *surface, bool is_upconverted) : is_upconverted_(is_upconverted) {
    texture_ = SDL_CreateTextureFromSurface(renderer, surface);
}

WrapTexture::~WrapTexture() {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
    }
}

TextureCache::TextureCache() {}

TextureCache::~TextureCache() {
    cache_.clear();
}

std::unique_ptr<WrapTexture> &TextureCache::get(const std::filesystem::path &path, SDL_Renderer *renderer, std::unique_ptr<ImageCache> &image_cache) {
    auto &info = image_cache->get(path);
    if (!info) {
        return invalid_texture;
    }
    if (cache_.contains(path)) {
        if (cache_.at(path)->isUpconverted() || cache_.at(path)->isUpconverted() == info->isUpconverted()) {
            return cache_.at(path);
        }
    }
    WrapSurface surface(info.value());
    cache_[path] = std::make_unique<WrapTexture>(renderer, surface.surface(), surface.isUpconverted());
    return cache_.at(path);
}
