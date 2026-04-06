#ifndef FILE_LOGGER_H_
#define FILE_LOGGER_H_

#include "../common/rime_webdav_sync.h"

#include <fstream>
#include <mutex>

namespace rime_sync {

class FileLogger : public Logger {
public:
    FileLogger(const std::string &logPath);
    ~FileLogger() override;
    void log(LogLevel level, const char *fmt, va_list ap) override;

private:
    std::ofstream out_;
    std::mutex mutex_;
};

}

#endif  // FILE_LOGGER_H_