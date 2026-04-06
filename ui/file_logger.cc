#include "file_logger.h"

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <iomanip>

namespace rime_sync {

FileLogger::FileLogger(const std::string &logPath)
    : out_(logPath, std::ios::app) {}

FileLogger::~FileLogger() = default;

void FileLogger::log(LogLevel level, const char *fmt, va_list ap) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::time(nullptr);
    char timeStr[32];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    
    const char *prefix = "";
    switch (level) {
        case LogLevel::Debug:   prefix = "[D] "; break;
        case LogLevel::Info:    prefix = "[I] "; break;
        case LogLevel::Warn:    prefix = "[W] "; break;
        case LogLevel::Error:   prefix = "[E] "; break;
    }
    
    out_ << timeStr << " " << prefix << buffer << "\n";
    out_.flush();
}

}  // namespace rime_sync