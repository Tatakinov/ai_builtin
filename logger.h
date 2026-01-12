#ifndef LOGGER_H_
#define LOGGER_H_

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>

class Logger {
    private:
        static std::unique_ptr<std::ofstream> ofs_;
        static std::mutex mutex_;
    public:
        static void configure(std::filesystem::path p) {
            ofs_ = std::make_unique<std::ofstream>(p);
        }
        static void log() {
            if (ofs_) {
                std::unique_lock<std::mutex> lock(mutex_);
                *ofs_ << std::endl;
            }
        }
        template<typename Head, typename... Remain>
        static void log(Head&& h, Remain&&... remain) {
            if (ofs_) {
                std::unique_lock<std::mutex> lock(mutex_);
                *ofs_ << h << " ";
            }
            log(std::forward<Remain>(remain)...);
        }
};

#endif // LOGGER_H_
