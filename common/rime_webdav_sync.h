// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RIME_WEBDAV_SYNC_H_
#define RIME_WEBDAV_SYNC_H_

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace rime_sync {

// ===== 日志接口 =====

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(LogLevel level, const char *fmt, va_list ap) = 0;

    void info(const char *fmt, ...);
    void warn(const char *fmt, ...);
    void error(const char *fmt, ...);
};

// ===== Rime 运行时抽象（平台相关实现注入） =====

class RimeRuntime {
public:
    virtual ~RimeRuntime() = default;

    /// 返回同步目录路径，如 ~/.config/fcitx/rime/sync/
    virtual std::filesystem::path getSyncDir() const = 0;

    /// 返回设备唯一 ID
    virtual std::string getInstallationId() const = 0;

    /// 触发 Rime 用户数据同步（导出/导入快照），阻塞直到完成
    virtual bool runSyncUserDataBlocking() = 0;

    /// 更新安装信息（重新生成 installation_id 等）
    virtual bool runInstallationUpdate() = 0;

    /// 重置 Rime 会话，使词典变更生效
    virtual void resetSession() = 0;
};

// ===== 同步结果 =====

struct SyncResult {
    bool success = false;
    std::string errorMessage;
    int downloadedDevices = 0;
    int downloadedSchemas = 0;
    int reinstallSchemasMerged = 0;
    /// "incremental_sync" | "reinstall_recovery" | "new_sync"
    std::string syncType;
};

// ===== WebDAV 端点 =====

struct WebDavEndpoint {
    std::string hostname_base;  // e.g. "https://dav.jianguoyun.com"
    std::string root;            // e.g. "/dav/"
};

// ===== WebDAV 同步引擎 =====

class WebDavSyncEngine {
public:
    WebDavSyncEngine(std::unique_ptr<RimeRuntime> runtime,
                     std::unique_ptr<Logger> logger);
    ~WebDavSyncEngine();

    // 主同步入口（阻塞直到完成）
    // hostname: 原始 URL，如 "https://dav.jianguoyun.com"
    // userRemoteDir: 用户指定的远程子目录，如 "/RimeSync"
    SyncResult sync(const std::string &hostname,
                    const std::string &username,
                    const std::string &password,
                    const std::string &userRemoteDir);

    bool isRunning() const { return running_.load(std::memory_order_acquire); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
};

// ===== 工具函数（供平台层使用） =====

/// 解析 hostname，拆分为服务器基础 URL 和根路径
WebDavEndpoint splitHostnameAndRoot(const std::string &raw);

/// 计算远程根路径（相对于 webdav_root 的路径）
std::string computeRemoteRoot(const std::string &webdavRoot,
                              const std::string &userDir);

}  // namespace rime_sync

#endif  // RIME_WEBDAV_SYNC_H_
