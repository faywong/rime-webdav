#ifndef QT_LOGGER_BRIDGE_H_
#define QT_LOGGER_BRIDGE_H_

#include "../common/rime_webdav_sync.h"
#include "qt_logger.h"

#include <QObject>

namespace rime_sync {

class QtLoggerBridge : public Logger, public QObject {
    Q_OBJECT

public:
    explicit QtLoggerBridge(rime_sync::QtLogger *qtLogger, QObject *parent = nullptr);
    void log(LogLevel level, const char *fmt, va_list ap) override;

private:
    rime_sync::QtLogger *qtLogger_;
};

}

#endif  // QT_LOGGER_BRIDGE_H_