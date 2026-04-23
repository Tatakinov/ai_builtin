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
        static void log_internal() {
            *ofs_ << std::endl;
        }
        template<typename Head, typename... Remain>
        static void log_internal(Head&& h, Remain&&... remain) {
            *ofs_ << h << " ";
            log_internal(std::forward<Remain>(remain)...);
        }
    public:
        static void configure(std::filesystem::path p) {
            ofs_ = std::make_unique<std::ofstream>(p);
        }
        template<typename... Args>
        static void log(Args&&... args) {
            if (ofs_) {
                std::unique_lock<std::mutex> lock(mutex_);
                log_internal(std::forward<Args>(args)...);
            }
        }
};

#endif // LOGGER_H_
