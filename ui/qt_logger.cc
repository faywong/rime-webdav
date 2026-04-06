#include "qt_logger.h"

#include <cstdio>
#include <cstdarg>

namespace rime_sync {

QtLogger::QtLogger(QObject *parent) : QObject(parent) {}

void QtLogger::log(LogLevel level, const char *fmt, va_list ap) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    QString line = QString::fromUtf8(buffer);
    // Also emit to stderr for CLI capture (e.g. nohup runs)
    fprintf(stderr, "%s\n", buffer);
    fflush(stderr);
    emit logReady(line);
}

void QtLogger::info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log(LogLevel::Info, fmt, ap);
    va_end(ap);
}

void QtLogger::warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log(LogLevel::Warn, fmt, ap);
    va_end(ap);
}

void QtLogger::error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log(LogLevel::Error, fmt, ap);
    va_end(ap);
}

}  // namespace rime_sync