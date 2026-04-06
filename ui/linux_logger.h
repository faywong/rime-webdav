// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RIME_WEBDAV_FCITX_LOGGER_H_
#define RIME_WEBDAV_FCITX_LOGGER_H_

#include "../common/rime_webdav_sync.h"

#include <cstdio>
#include <cstdarg>

namespace rime_sync {

// ============================================================================
// FcitxLogger — rime_sync::Logger 实现，写入 stderr（Fcitx 会捕获）
// ============================================================================

class FcitxLogger : public Logger {
public:
    void log(LogLevel level, const char *fmt, va_list ap) override {
        FILE *out = stderr;
        const char *prefix = "";
        switch (level) {
            case LogLevel::Debug:   prefix = "[rime-webdav D] "; break;
            case LogLevel::Info:    prefix = "[rime-webdav I] "; break;
            case LogLevel::Warn:    prefix = "[rime-webdav W] "; break;
            case LogLevel::Error:   prefix = "[rime-webdav E] "; break;
        }
        fprintf(out, "%s", prefix);
        vfprintf(out, fmt, ap);
        fprintf(out, "\n");
    }
};

}  // namespace rime_sync

#endif  // RIME_WEBDAV_FCITX_LOGGER_H_
