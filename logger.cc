#include "logger.h"

std::unique_ptr<std::ofstream> Logger::ofs_;
std::mutex Logger::mutex_;
