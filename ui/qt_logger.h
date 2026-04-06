#ifndef QT_LOGGER_H_
#define QT_LOGGER_H_

#include <QObject>
#include <QString>

#include "../common/rime_webdav_sync.h"

namespace rime_sync {

class QtLogger : public QObject, public Logger {
    Q_OBJECT

public:
    explicit QtLogger(QObject *parent = nullptr);

    void log(LogLevel level, const char *fmt, va_list ap) override;

    void info(const char *fmt, ...);
    void warn(const char *fmt, ...);
    void error(const char *fmt, ...);

signals:
    void logReady(const QString &line);
};

}

#endif  // QT_LOGGER_H_