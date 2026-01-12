#ifndef IMAGE_CACHE_H_
#define IMAGE_CACHE_H_

#include <condition_variable>
#include <filesystem>
#include <mutex>
#if defined(USE_ONNX)
#include <onnxruntime_cxx_api.h>
#endif // USE_ONNX
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

class ImageInfo {
    private:
        std::vector<unsigned char> data_;
        int width_, height_;
        bool is_upconverted_;
    public:
        ImageInfo(const std::vector<unsigned char> &data, int width, int height, bool is_upconverted) : data_(data), width_(width), height_(height), is_upconverted_(is_upconverted) {}
        ~ImageInfo() {}
        std::vector<unsigned char> &get() {
            return data_;
        }
        int width() const {
            return width_;
        }
        int height() const {
            return height_;
        }
        bool isUpconverted() const {
            return is_upconverted_;
        }
};

class ImageCache {
    private:
        std::filesystem::path balloon_dir_;
        bool alive_;
        bool use_self_alpha_;
        int scale_;
        std::mutex mutex_;
        std::condition_variable cond_;
        std::unique_ptr<std::thread> th_;
        std::queue<std::filesystem::path> queue_;
        std::unordered_map<std::filesystem::path, std::optional<ImageInfo>> cache_orig_;
        std::unordered_map<std::filesystem::path, std::optional<ImageInfo>> cache_;
#if defined(USE_ONNX)
        Ort::Env env_;
        Ort::Session session_;
#endif // USE_ONNX

        std::optional<ImageInfo> &getOriginal(const std::filesystem::path &path);

    public:
#if defined(USE_ONNX)
        ImageCache(const std::filesystem::path &balloon_dir, const std::filesystem::path &exe_dir, bool use_self_alpha);
#else
        ImageCache(const std::filesystem::path &balloon_dir, const std::filesystem::path &exe_dir, bool use_self_alpha)
        : balloon_dir_(balloon_dir), alive_(true), use_self_alpha_(use_self_alpha), scale_(100) {}
#endif // USE_ONNX
        ~ImageCache();
        void setScale(int scale);
        std::optional<ImageInfo> &getRelative(const std::filesystem::path &relative_path) {
            return get(balloon_dir_ / relative_path);
        }
        std::optional<ImageInfo> &get(const std::filesystem::path &path);
        void clearCache();
};

#endif // IMAGE_CACHE_H_
