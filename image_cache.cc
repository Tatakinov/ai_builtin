#include "image_cache.h"

#include <cassert>
#include <cmath>

#include <SDL3_image/SDL_image.h>

#include "logger.h"
#include "texture.h"

#if defined(USE_ONNX)
ImageCache::ImageCache(const std::filesystem::path &balloon_dir, const std::filesystem::path &exe_dir, bool use_self_alpha)
    : balloon_dir_(balloon_dir), alive_(true), use_self_alpha_(use_self_alpha), scale_(100), session_(nullptr) {
    std::filesystem::path model_path = exe_dir / "model.onnx";
    try {
        Ort::SessionOptions session_options;
        session_ = {env_, model_path.string().c_str(), session_options};
        th_ = std::make_unique<std::thread>([&]() {
            while (true) {
                std::filesystem::path p;
                int scale;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cond_.wait(lock, [&]() { return !queue_.empty(); });
                    p = queue_.front();
                    queue_.pop();
                    scale = scale_;
                }
                int num_resize = std::ceil(std::log2(scale_ / 100.0));
                auto &info = cache_orig_.at(p);
                int w = info->width();
                int h = info->height();
                std::vector<unsigned char> src;
                std::vector<unsigned char> dest = info->get();
                for (int i = 0; i < num_resize; i++, w <<= 1, h <<= 1) {
                    src = dest;
                    dest.resize(src.size() * 4);
                    std::array<int64_t, 4> input_shape = {4, 1, h, w};
                    std::array<int64_t, 4> output_shape = {4, 1, 2 * h, 2 * w};
                    std::vector<float> input;
                    input.resize(src.size());
                    std::vector<float> output;
                    output.resize(dest.size());
                    for (int i = 0; i < w * h; i++) {
                        for (int c = 0; c < 4; c++) {
                            input[c * w * h + i] = src[4 * i + c] / 255.0;
                        }
                    }
                    auto mem_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
                    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(mem_info, input.data(), input.size(), input_shape.data(), input_shape.size());
                    Ort::Value output_tensor = Ort::Value::CreateTensor<float>(mem_info, output.data(), output.size(), output_shape.data(), output_shape.size());
                    const char *input_names[] = {"input"};
                    const char *output_names[] = {"output"};
                    Ort::RunOptions run_options;
                    try {
                        session_.Run(run_options, input_names, &input_tensor, 1, output_names, &output_tensor, 1);
                    }
                    catch (Ort::Exception &e) {
                        Logger::log(e.what());
                        auto &tmp = cache_.at(p);
                        cache_[p] = {tmp->get(), tmp->width(), tmp->height(), true};
                    }
                    for (int i = 0; i < (2 * w) * (2 * h); i++) {
                        for (int c = 0; c < 4; c++) {
                            int byte = std::round(output[c * (2 * w) * (2 * h) + i] * 255);
                            dest[4 * i + c] = std::max(0, std::min(255, byte));
                        }
                    }
                }
                if (info->width() * scale_ / 100.0 != w) {
                    int w_resize = std::round(info->width() * scale_ / 100.0);
                    int h_resize = std::round(info->height() * scale_ / 100.0);
                    std::vector<unsigned char> resize;
                    resize.resize(w_resize * h_resize * 4);

                    SDL_Surface *in = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, dest.data(), w * 4);
                    SDL_Surface *out = SDL_CreateSurface(w_resize, h_resize, SDL_PIXELFORMAT_ABGR8888);
                    SDL_ClearSurface(out, 0, 0, 0, 0);
                    SDL_BlitSurfaceScaled(in, nullptr, out, nullptr, SDL_SCALEMODE_LINEAR);
                    SDL_LockSurface(out);
                    memcpy(resize.data(), out->pixels, sizeof(unsigned char) * w_resize * h_resize * 4);
                    SDL_UnlockSurface(out);
                    SDL_DestroySurface(in);
                    SDL_DestroySurface(out);

                    dest = resize;
                    w = w_resize;
                    h = h_resize;
                }
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    if (scale == scale_) {
                        cache_[p] = {dest, w, h, true};
                    }
                }
                Logger::log("upconverted!");
            }
        });
    }
    catch (Ort::Exception &e) {
        Logger::log(e.what());
    }
}
#endif // USE_ONNX

ImageCache::~ImageCache() {
    alive_ = false;
    if (th_) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push("");
            cond_.notify_one();
        }
        th_->join();
    }
}

void ImageCache::setScale(int scale) {
    std::unique_lock<std::mutex> lock(mutex_);
    scale_ = scale;
    cache_.clear();
}

std::optional<ImageInfo> &ImageCache::getOriginal(const std::filesystem::path &path) {
    Logger::log("scale => ", scale_);
    Logger::log("file: ", path.string());
    if (cache_orig_.contains(path)) {
        return cache_orig_.at(path);
    }
    SDL_Surface *in = IMG_Load(path.string().c_str());
    if (in == nullptr) {
        cache_orig_[path] = std::nullopt;
        return cache_orig_.at(path);
    }
    SDL_Surface *abgr = SDL_ConvertSurface(in, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(in);
    int w = abgr->w, h = abgr->h;
    std::vector<unsigned char> data;
    data.resize(w * h * 4);
    SDL_LockSurface(abgr);
    unsigned char *p = static_cast<unsigned char *>(abgr->pixels);
    for (int i = 0; i < w * h; i++) {
        // alpha: 0なら全て0にする
        if (p[4 * i + 3] == 0) {
            data[4 * i + 0] = 0;
            data[4 * i + 1] = 0;
            data[4 * i + 2] = 0;
            data[4 * i + 3] = 0;
        }
        else {
            data[4 * i + 0] = p[4 * i + 0];
            data[4 * i + 1] = p[4 * i + 1];
            data[4 * i + 2] = p[4 * i + 2];
            data[4 * i + 3] = p[4 * i + 3];
        }
    }
    SDL_UnlockSurface(abgr);
    SDL_DestroySurface(abgr);
    if (!use_self_alpha_) {
        auto pna_filename = path.parent_path() / path.stem();
        pna_filename += ".pna";
        SDL_Surface *pna_in = IMG_Load(pna_filename.string().c_str());
        if (pna_in != nullptr && w == pna_in->w && h == pna_in->h) {
            SDL_Surface *pna_abgr = SDL_ConvertSurface(pna_in, SDL_PIXELFORMAT_ABGR8888);
            SDL_DestroySurface(pna_in);
            SDL_LockSurface(pna_abgr);
            unsigned char *pna = static_cast<unsigned char *>(pna_abgr->pixels);
            for (int i = 0; i < w * h; i++) {
                int alpha = pna[4 * i];
                data[4 * i + 3] = alpha;
            }
            SDL_UnlockSurface(pna_abgr);
            SDL_DestroySurface(pna_abgr);
        }
        else {
            int r = data[0 + 0];
            int g = data[0 + 1];
            int b = data[0 + 2];
            for (int i = 0; i < w * h; i++) {
                if (data[4 * i + 0] == r && data[4 * i + 1] == g && data[4 * i + 2] == b) {
                    data[4 * i + 0] = 0;
                    data[4 * i + 1] = 0;
                    data[4 * i + 2] = 0;
                    data[4 * i + 3] = 0;
                }
            }
        }
    }
    // そのままだとalphaが0とそうでない部分の境界で
    // alpha-blendがうまくいかなくなるので
    // alpha>0なピクセルの値をalpha=0なピクセルに伝播させる
    {
        std::vector<unsigned char> blurred;
        blurred.resize(data.size());
        const int blur[3][3] = {
            {1, 2, 1},
            {2, 4, 2},
            {1, 2, 1},
        };
        auto in = [](int x, int y, int w, int h) {
            if (x < 0) {
                return false;
            }
            if (y < 0) {
                return false;
            }
            if (x >= w) {
                return false;
            }
            if (y >= h) {
                return false;
            }
            return true;
        };
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                double n = 0.0;
                int sum[4] = {};
                for (int j = -1; j <= 1; j++) {
                    for (int i = -1; i <= 1; i++) {
                        if (in(x + i, y + j, w, h)) {
                            int index = 4 * ((y + j) * w + (x + i));
                            // alpha>0なピクセルを伝播させるため
                            // alpha=0なら無視する
                            if (data[index + 3] == 0) {
                                continue;
                            }
                            auto factor = blur[1 + j][1 + i];
                            n += factor;
                            sum[0] += data[index + 0] * factor;
                            sum[1] += data[index + 1] * factor;
                            sum[2] += data[index + 2] * factor;
                            sum[3] += data[index + 3] * factor;
                        }
                    }
                }
                int index = 4 * (y * w + x);
                blurred[index + 0] = std::min(255.0, std::ceil(sum[0] / n));
                blurred[index + 1] = std::min(255.0, std::ceil(sum[1] / n));
                blurred[index + 2] = std::min(255.0, std::ceil(sum[2] / n));
                blurred[index + 3] = std::min(255.0, std::ceil(sum[3] / n));
            }
        }
        for (int i = 0; i < w * h; i++) {
            // alpha以外を伝播
            if (blurred[4 * i + 3] > 0 && data[4 * i + 3] == 0) {
                data[4 * i + 0] = blurred[4 * i + 0];
                data[4 * i + 1] = blurred[4 * i + 1];
                data[4 * i + 2] = blurred[4 * i + 2];
            }
        }
    }
    cache_orig_[path] = {data, w, h, true};
    return cache_orig_.at(path);
}

std::optional<ImageInfo> &ImageCache::get(const std::filesystem::path &path) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cache_.contains(path)) {
            return cache_.at(path);
        }
    }
    auto &info = getOriginal(path);
    if (info == std::nullopt || scale_ == 100) {
        cache_[path] = info;
        return cache_.at(path);
    }
    int w = std::round(info->width() * scale_ / 100.0);
    int h = std::round(info->height() * scale_ / 100.0);
    std::vector<unsigned char> resize;
    resize.resize(w * h * 4);

    SDL_Surface *in = SDL_CreateSurfaceFrom(info->width(), info->height(), SDL_PIXELFORMAT_ABGR8888, info->get().data(), info->width() * 4);
    SDL_Surface *out = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ABGR8888);
    SDL_ClearSurface(out, 0, 0, 0, 0);
    SDL_BlitSurfaceScaled(in, nullptr, out, nullptr, SDL_SCALEMODE_LINEAR);
    SDL_LockSurface(out);
    memcpy(resize.data(), out->pixels, sizeof(unsigned char) * w * h * 4);
    SDL_UnlockSurface(out);
    SDL_DestroySurface(in);
    SDL_DestroySurface(out);

    if (scale_ <= 100 || !th_) {
        cache_[path] = {resize, w, h, true};
    }
    else {
        cache_[path] = {resize, w, h, false};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push(path);
        }
        cond_.notify_one();
    }

    return cache_.at(path);
}

void ImageCache::clearCache() {
    cache_.clear();
    cache_orig_.clear();
}
