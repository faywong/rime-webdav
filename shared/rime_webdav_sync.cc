// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rime_webdav_sync.h"

#include <webdav/client.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <sys/time.h>

#if defined(__ANDROID__) || defined(__linux__)
extern "C" time_t timegm(struct tm *tm);
#endif

namespace rime_sync {

// ============================================================================
// Logger inline implementations
// ============================================================================

void Logger::info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log(LogLevel::Info, fmt, ap);
    va_end(ap);
}

void Logger::warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log(LogLevel::Warn, fmt, ap);
    va_end(ap);
}

void Logger::error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log(LogLevel::Error, fmt, ap);
    va_end(ap);
}

// ============================================================================
// Utility functions (adapted from rime_sync_jni.cc)
// ============================================================================

static std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static std::string ensureTrailingSlash(std::string p) {
    if (p.empty()) return std::string("/");
    if (p.back() != '/') p.push_back('/');
    return p;
}

static std::string ensureLeadingSlash(std::string p) {
    if (p.empty()) return std::string("/");
    if (p.front() != '/') p.insert(p.begin(), '/');
    return p;
}

static std::string removeTrailingSlashes(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

static std::string normalizeRemoteForLog(std::string p) {
    p = ensureLeadingSlash(trim(std::move(p)));
    while (p.find("//") != std::string::npos) {
        p.replace(p.find("//"), 2, "/");
    }
    return p;
}

static std::string joinRemote(const std::string &dirWithSlash, const std::string &child) {
    if (child.empty()) return dirWithSlash;
    if (!child.empty() && child.front() == '/') return child;
    return dirWithSlash + child;
}

static std::string basename(std::string path) {
    while (!path.empty() && path.back() == '/') path.pop_back();
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

static bool endsWith(const std::string &value, const std::string &suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

WebDavEndpoint splitHostnameAndRoot(const std::string &raw) {
    WebDavEndpoint out;
    std::string s = trim(raw);
    if (s.empty()) return out;

    if (s.find("://") == std::string::npos) {
        s = std::string("https://") + s;
    }

    size_t q = s.find_first_of("?#");
    if (q != std::string::npos) s.resize(q);

    size_t scheme_pos = s.find("://");
    if (scheme_pos == std::string::npos) return out;

    size_t authority_start = scheme_pos + 3;
    size_t path_start = s.find('/', authority_start);

    if (path_start == std::string::npos) {
        out.hostname_base = removeTrailingSlashes(s);
        out.root = std::string("/");
        return out;
    }

    out.hostname_base = removeTrailingSlashes(s.substr(0, path_start));
    std::string path = s.substr(path_start);
    path = ensureLeadingSlash(std::move(path));
    path = ensureTrailingSlash(std::move(path));
    while (path.find("//") != std::string::npos) {
        path.replace(path.find("//"), 2, "/");
    }
    out.root = std::move(path);
    return out;
}

std::string computeRemoteRoot(const std::string &webdavRoot, const std::string &userDir) {
    std::string userDirClean = trim(userDir);
    if (userDirClean.empty()) userDirClean = "/RimeSync";
    userDirClean = ensureLeadingSlash(userDirClean);
    userDirClean = removeTrailingSlashes(userDirClean);
    return ensureTrailingSlash(userDirClean);
}

// ============================================================================
// Private implementation
// ============================================================================

namespace {

constexpr char kUserdbSnapshotSuffix[] = ".userdb.txt";

bool isUserdbSnapshotFile(const std::string &name) {
    return endsWith(name, kUserdbSnapshotSuffix);
}

bool directoryHasUserdbSnapshotFiles(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) return false;
    if (!std::filesystem::is_directory(dir, ec) || ec) return false;
    for (std::filesystem::recursive_directory_iterator it(dir, ec), end;
         it != end; it.increment(ec)) {
        if (ec) return false;
        if (!it->is_regular_file()) continue;
        if (isUserdbSnapshotFile(it->path().filename().string())) return true;
    }
    return false;
}

// 是否为已知文件模式（永远不是目录）
bool isKnownFilePattern(const std::string &path) {
    static const char *knownFilePatterns[] = {
        ".userdb.txt", ".yaml", ".custom.yaml", ".schema.yaml",
        ".dict.yaml", ".txt", ".json",
    };
    for (const auto &pattern : knownFilePatterns) {
        if (endsWith(path, pattern)) return true;
    }
    return false;
}

std::unique_ptr<WebDAV::Client> makeClient(const std::string &hostname,
                                            const std::string &username,
                                            const std::string &password,
                                            const std::string &root) {
    std::map<std::string, std::string> options{
        {"webdav_hostname", hostname},
        {"webdav_username", username},
        {"webdav_password", password},
        {"webdav_root", root},
    };
    return std::unique_ptr<WebDAV::Client>(new WebDAV::Client{options});
}

// 安全的远程文件/目录判断
bool isRemoteDirectory(WebDAV::Client &client, const std::string &remotePath) {
    if (!remotePath.empty() && remotePath.back() == '/') return true;
    if (isKnownFilePattern(remotePath)) return false;
    std::string probe = ensureTrailingSlash(remotePath);
    if (client.check(remotePath)) return client.is_directory(remotePath);
    if (client.check(probe)) return client.is_directory(probe);
    return false;
}

// 确保远程目录存在
bool ensureRemoteDirectory(WebDAV::Client &client, std::string remoteDirWithSlash) {
    remoteDirWithSlash = ensureTrailingSlash(std::move(remoteDirWithSlash));
    std::string noSlash = removeTrailingSlashes(remoteDirWithSlash);
    if (client.check(noSlash) && client.is_directory(noSlash)) return true;
    if (client.check(remoteDirWithSlash) && client.is_directory(remoteDirWithSlash)) return true;
    return client.create_directory(remoteDirWithSlash, true);
}

// 递归删除远程目录
bool cleanRemoteDirectory(WebDAV::Client &client, const std::string &remoteDirWithSlash) {
    std::string dirPath = removeTrailingSlashes(remoteDirWithSlash);
    auto items = client.list(remoteDirWithSlash);
    for (const auto &item : items) {
        std::string itemPath = joinRemote(remoteDirWithSlash, item);
        if (isRemoteDirectory(client, itemPath)) {
            cleanRemoteDirectory(client, itemPath + "/");
        } else {
            client.clean(itemPath);
        }
    }
    return client.clean(dirPath);
}

std::optional<std::time_t> parseRfc1123Utc(const std::string &s) {
    std::tm tm{};
    std::istringstream iss(s);
    iss.imbue(std::locale::classic());
    iss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");
    if (iss.fail()) return std::nullopt;
#if defined(__ANDROID__) || defined(__linux__)
    return static_cast<time_t>(timegm(&tm));
#else
    return static_cast<time_t>(std::mktime(&tm));
#endif
}

std::optional<std::time_t> remoteModifiedTime(WebDAV::Client &client, const std::string &remotePath) {
    auto info = client.info(remotePath);
    auto it = info.find("modified");
    if (it == info.end()) return std::nullopt;
    return parseRfc1123Utc(it->second);
}

std::optional<std::time_t> localModifiedTime(const std::filesystem::path &p) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(p, ec);
    if (ec) return std::nullopt;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

// 下载单个文件
bool downloadFile(WebDAV::Client &client,
                 Logger &logger,
                 const std::string &remotePath,
                 const std::filesystem::path &localPath,
                 bool respectMtime) {
    std::error_code ec;
    bool shouldDownload = true;
    if (respectMtime && std::filesystem::exists(localPath, ec) && !ec) {
        auto remoteTime = remoteModifiedTime(client, remotePath);
        auto localTime = localModifiedTime(localPath);
        if (!remoteTime || !localTime) {
            logger.warn("mtime unavailable, download anyway: %s", remotePath.c_str());
            shouldDownload = true;
        } else if (*remoteTime <= *localTime) {
            shouldDownload = false;
        }
    }
    if (!shouldDownload) {
        logger.info("webdav skipping up-to-date file: %s", remotePath.c_str());
        return true;
    }
    if (!client.download(remotePath, localPath.string())) {
        logger.error("webdav download failed: remote=%s local=%s",
                     remotePath.c_str(), localPath.string().c_str());
        return false;
    }
    logger.info("webdav downloaded: %s -> %s", remotePath.c_str(), localPath.string().c_str());
    return true;
}

// 递归下载目录树
bool downloadTree(WebDAV::Client &client,
                  Logger &logger,
                  const std::string &remoteDirWithSlash,
                  const std::filesystem::path &localDir,
                  const std::string &webdavRootWithSlash,
                  int depth,
                  bool respectMtime) {
    std::error_code ec;
    std::filesystem::create_directories(localDir, ec);
    if (ec) {
        logger.error("create_directories failed: %s", ec.message().c_str());
        return false;
    }
    if (depth == 0) {
        logger.info("webdav list: remote=%s local=%s",
                    remoteDirWithSlash.c_str(), localDir.string().c_str());
    }
    auto items = client.list(remoteDirWithSlash);
    for (const auto &item : items) {
        std::string remotePath = joinRemote(remoteDirWithSlash, item);
        std::string name = basename(remotePath);
        if (name.empty()) continue;
        std::filesystem::path localPath = localDir / name;
        bool isDir = isRemoteDirectory(client, remotePath);
        if (isDir) {
            std::string subdir = ensureTrailingSlash(remotePath);
            logger.info("webdav downloading dir: %s -> %s/",
                        remotePath.c_str(), localPath.string().c_str());
            if (!downloadTree(client, logger, subdir, localPath, webdavRootWithSlash,
                             depth + 1, respectMtime)) {
                return false;
            }
            continue;
        }
        if (!downloadFile(client, logger, remotePath, localPath, respectMtime)) {
            return false;
        }
    }
    return true;
}

// 上传单个文件
bool uploadFile(WebDAV::Client &client,
                Logger &logger,
                const std::string &remoteFile,
                const std::filesystem::path &localPath,
                bool respectMtime) {
    bool shouldUpload = true;
    if (respectMtime && client.check(remoteFile)) {
        auto remoteTime = remoteModifiedTime(client, remoteFile);
        auto localTime = localModifiedTime(localPath);
        if (!remoteTime || !localTime) {
            logger.warn("mtime unavailable, upload anyway: %s", remoteFile.c_str());
            shouldUpload = true;
        } else if (*remoteTime > *localTime) {
            shouldUpload = false;
        }
    }
    if (!shouldUpload) {
        logger.info("webdav skipping up-to-date file: %s", remoteFile.c_str());
        return true;
    }
    if (client.upload(remoteFile, localPath.string())) {
        logger.info("webdav uploaded: %s <- %s",
                    remoteFile.c_str(), localPath.string().c_str());
        return true;
    }
    // 上传失败，清理后重试
    logger.warn("webdav upload failed, retrying: %s", remoteFile.c_str());
    client.clean(remoteFile);
    client.clean(ensureTrailingSlash(remoteFile));
    if (!client.upload(remoteFile, localPath.string())) {
        logger.error("webdav upload failed: remote=%s local=%s",
                      remoteFile.c_str(), localPath.string().c_str());
        return false;
    }
    return true;
}

// 递归上传目录树
bool uploadTree(WebDAV::Client &client,
                Logger &logger,
                const std::filesystem::path &localDir,
                const std::string &remoteDirWithSlash,
                int depth,
                bool respectMtime,
                bool allowErrors,
                int *errorCount) {
    std::error_code ec;
    if (!std::filesystem::exists(localDir, ec)) return true;
    if (ec) return false;
    if (depth == 0 && !ensureRemoteDirectory(client, remoteDirWithSlash)) {
        logger.error("webdav mkdir failed: remote=%s", remoteDirWithSlash.c_str());
        if (allowErrors) {
            if (errorCount) ++(*errorCount);
            return true;
        }
        return false;
    }
    for (const auto &entry : std::filesystem::directory_iterator(localDir, ec)) {
        if (ec) return false;
        std::string name = entry.path().filename().string();
        if (name.empty()) continue;
        if (entry.is_directory()) {
            std::string subdir = remoteDirWithSlash + name + "/";
            if (!ensureRemoteDirectory(client, subdir)) {
                logger.error("webdav mkdir failed: remote=%s", subdir.c_str());
                if (allowErrors) {
                    if (errorCount) ++(*errorCount);
                    continue;
                }
                return false;
            }
            if (!uploadTree(client, logger, entry.path(), subdir, depth + 1,
                            respectMtime, allowErrors, errorCount)) {
                if (allowErrors) {
                    if (errorCount) ++(*errorCount);
                    continue;
                }
                return false;
            }
            continue;
        }
        if (!entry.is_regular_file()) continue;
        std::string remoteFile = remoteDirWithSlash + name;
        if (!uploadFile(client, logger, remoteFile, entry.path(), respectMtime)) {
            if (allowErrors) {
                if (errorCount) ++(*errorCount);
                continue;
            }
            return false;
        }
    }
    if (depth == 0 && allowErrors && errorCount && *errorCount > 0) {
        logger.warn("webdav upload completed with %d errors: local=%s remote=%s",
                    *errorCount, localDir.string().c_str(), remoteDirWithSlash.c_str());
    }
    return true;
}

// 检查远程设备安装目录是否存在
bool remoteInstallationDirExists(WebDAV::Client &client,
                                 const std::string &remoteRoot,
                                 const std::string &installationId) {
    std::string remoteDir = ensureTrailingSlash(remoteRoot) + installationId;
    if (client.check(remoteDir)) return true;
    if (client.check(remoteDir + "/")) return true;
    return false;
}

// 列出远程同步目录中的所有设备安装目录
std::vector<std::string> listRemoteInstallationDirs(WebDAV::Client &client,
                                                   const std::string &remoteBase) {
    std::vector<std::string> result;
    auto items = client.list(remoteBase);
    for (const auto &item : items) {
        std::string remotePath = joinRemote(remoteBase, item);
        if (isRemoteDirectory(client, remotePath)) {
            std::string name = basename(remotePath);
            if (!name.empty()) {
                result.push_back(name);
            }
        }
    }
    return result;
}

// 列出远程目录中的 schema 文件数量（用于日志）
int countRemoteSchemas(WebDAV::Client &client,
                       const std::string &remoteDirPath) {
    int count = 0;
    auto remoteFiles = client.list(remoteDirPath);
    for (const auto &file : remoteFiles) {
        std::string filePath = joinRemote(remoteDirPath, file);
        if (!isRemoteDirectory(client, filePath) && isUserdbSnapshotFile(file)) {
            ++count;
        }
    }
    return count;
}

}  // anonymous namespace

// ============================================================================
// WebDavSyncEngine::Impl
// ============================================================================

class WebDavSyncEngine::Impl {
public:
    Impl(std::unique_ptr<RimeRuntime> runtime,
         std::unique_ptr<Logger> logger)
        : runtime_(std::move(runtime)), logger_(std::move(logger)) {}

    SyncResult sync(const std::string &hostname,
                    const std::string &username,
                    const std::string &password,
                    const std::string &userRemoteDir) {
        SyncResult result;

        // 解析 hostname
        WebDavEndpoint endpoint = splitHostnameAndRoot(hostname);
        if (endpoint.hostname_base.empty()) {
            result.errorMessage = "invalid hostname: " + hostname;
            return result;
        }

        // 计算远程根路径
        std::string remoteRoot = computeRemoteRoot(endpoint.root, userRemoteDir);

        logger_->info("webdav sync config: host=%s root=%s user_dir=%s -> remote_root=%s",
                      endpoint.hostname_base.c_str(), endpoint.root.c_str(),
                      userRemoteDir.c_str(), remoteRoot.c_str());

        // 更新安装信息
        if (!runtime_->runInstallationUpdate()) {
            result.errorMessage = "installation_update failed";
            return result;
        }

        std::filesystem::path localSync = runtime_->getSyncDir();
        std::error_code ec;
        std::filesystem::create_directories(localSync, ec);
        if (ec) {
            result.errorMessage = "cannot create local sync dir: " + ec.message();
            return result;
        }

        std::string installationId = runtime_->getInstallationId();
        if (installationId.empty() || installationId == "unknown") {
            result.errorMessage = "deployer.user_id is empty/unknown";
            return result;
        }

        std::filesystem::path localOurDir = localSync / installationId;
        std::string remoteOurDir = remoteRoot + installationId + "/";
        const std::string kLastSelfSyncDir = "last_self_sync";

        enum SyncType { SYNC_TYPE_NORMAL, SYNC_TYPE_REINSTALL, SYNC_TYPE_NEW };

        try {
            auto client = makeClient(endpoint.hostname_base, username, password,
                                     endpoint.root);

            // 确保远程根目录存在
            if (!ensureRemoteDirectory(*client, remoteRoot)) {
                if (!client->check(remoteRoot) || !client->is_directory(remoteRoot)) {
                    result.errorMessage = "cannot create or access remote root directory";
                    return result;
                }
            }

            // 判断同步类型
            bool localHasSnapshots = directoryHasUserdbSnapshotFiles(localOurDir);
            bool remoteHasSnapshots = remoteInstallationDirExists(*client, remoteRoot,
                                                                 installationId);

            SyncType syncType;
            if (!localHasSnapshots && remoteHasSnapshots) {
                syncType = SYNC_TYPE_REINSTALL;
                logger_->info("webdav sync type: REINSTALL (local=no, remote=yes)");
            } else if (!localHasSnapshots && !remoteHasSnapshots) {
                syncType = SYNC_TYPE_NEW;
                logger_->info("webdav sync type: NEW (local=no, remote=no)");
            } else {
                syncType = SYNC_TYPE_NORMAL;
                logger_->info("webdav sync type: NORMAL (local=yes, remote=yes)");
            }

            logger_->info("webdav sync starting: local_sync=%s remote_root=%s installation_id=%s",
                          localSync.string().c_str(), remoteRoot.c_str(),
                          installationId.c_str());

            // ========== 步骤 0: 重装恢复 ==========
            int reinstallSchemasMerged = 0;
            if (syncType == SYNC_TYPE_REINSTALL) {
                logger_->info("webdav step 0: REINSTALL recovery");
                std::filesystem::path localLastSelfSync = localSync / kLastSelfSyncDir;

                if (downloadTree(*client, *logger_, remoteOurDir, localLastSelfSync,
                                 endpoint.root, 0, false)) {
                    std::error_code ec_count;
                    for (const auto &entry :
                         std::filesystem::directory_iterator(localLastSelfSync, ec_count)) {
                        if (entry.is_regular_file() &&
                            isUserdbSnapshotFile(entry.path().filename().string())) {
                            ++reinstallSchemasMerged;
                        }
                    }
                    logger_->info("webdav downloaded %d schema snapshots for reinstall",
                                  reinstallSchemasMerged);
                }

                // 合并到本地用户词典
                logger_->info("webdav rime_sync_user_data(reinstall-merge) start");
                runtime_->runSyncUserDataBlocking();
                logger_->info("webdav rime_sync_user_data(reinstall-merge) ok");

                // 删除临时目录
                if (std::filesystem::exists(localLastSelfSync, ec) && !ec) {
                    std::filesystem::remove_all(localLastSelfSync, ec);
                    if (!ec) logger_->info("webdav cleaned up last_self_sync/");
                }

                // 导出合并后的快照
                logger_->info("webdav rime_sync_user_data(export after reinstall) start");
                if (!runtime_->runSyncUserDataBlocking()) {
                    result.errorMessage = "export after reinstall failed";
                    return result;
                }
                logger_->info("webdav rime_sync_user_data(export after reinstall) ok");

                std::filesystem::create_directories(localOurDir, ec);
                if (ec) {
                    result.errorMessage = "cannot create local our dir: " + ec.message();
                    return result;
                }
            }

            // ========== 步骤 1: 预上传（仅正常增量同步）==========
            if (syncType == SYNC_TYPE_NORMAL) {
                logger_->info("webdav step 1: pre-upload local snapshot");
                logger_->info("webdav rime_sync_user_data(pre-upload) start");
                if (!runtime_->runSyncUserDataBlocking()) {
                    result.errorMessage = "pre-upload sync failed";
                    return result;
                }
                logger_->info("webdav rime_sync_user_data(pre-upload) ok");

                std::filesystem::create_directories(localOurDir, ec);
                if (ec) {
                    result.errorMessage = "cannot create local our dir: " + ec.message();
                    return result;
                }

                if (!ensureRemoteDirectory(*client, remoteOurDir)) {
                    result.errorMessage = "cannot create remote installation directory";
                    return result;
                }

                logger_->info("webdav upload(own) start: local=%s remote=%s",
                              localOurDir.string().c_str(), remoteOurDir.c_str());
                int uploadErrors = 0;
                uploadTree(*client, *logger_, localOurDir, remoteOurDir, 0,
                           false, true, &uploadErrors);
                logger_->info("webdav upload(own) ok: errors=%d", uploadErrors);
            }

            // ========== 步骤 2: 下载其他设备的快照 ==========
            logger_->info("webdav step 2: download other devices' snapshots");
            auto remoteDirs = listRemoteInstallationDirs(*client, remoteRoot);
            logger_->info("webdav found %zu remote installation dirs", remoteDirs.size());

            int downloadedDevices = 0;
            int downloadedSchemas = 0;
            for (const auto &remoteDirName : remoteDirs) {
                if (remoteDirName == installationId) {
                    logger_->info("webdav skipping own installation dir: %s",
                                  remoteDirName.c_str());
                    continue;
                }
                std::string remoteDirPath = remoteRoot + remoteDirName + "/";
                std::filesystem::path localDirPath = localSync / remoteDirName;

                int schemaCount = countRemoteSchemas(*client, remoteDirPath);
                logger_->info("webdav device[%s] has %d schema snapshots to download",
                               remoteDirName.c_str(), schemaCount);

                logger_->info("webdav downloading installation dir: %s -> %s",
                              remoteDirPath.c_str(), localDirPath.string().c_str());
                if (downloadTree(*client, *logger_, remoteDirPath, localDirPath,
                                endpoint.root, 0, false)) {
                    ++downloadedDevices;
                    downloadedSchemas += schemaCount;
                }
            }
            logger_->info("webdav download(others) ok: %d devices, %d schemas",
                          downloadedDevices, downloadedSchemas);

            // ========== 步骤 3: 合并所有快照到本地用户词典 ==========
            logger_->info("webdav step 3: merge all snapshots to local user dict");
            logger_->info("webdav rime_sync_user_data(post-download) start");
            if (!runtime_->runSyncUserDataBlocking()) {
                result.errorMessage = "post-download merge failed";
                return result;
            }
            logger_->info("webdav rime_sync_user_data(post-download) ok");

            // ========== 步骤 4: 上传本设备的快照 ==========
            logger_->info("webdav step 4: upload own snapshot");

            // 清理远程设备目录
            if (client->check(remoteOurDir)) {
                cleanRemoteDirectory(*client, remoteOurDir);
            }
            if (!ensureRemoteDirectory(*client, remoteOurDir)) {
                result.errorMessage = "cannot create remote installation directory";
                return result;
            }

            if (syncType == SYNC_TYPE_NEW || syncType == SYNC_TYPE_REINSTALL) {
                // 全新或重装：重新导出
                logger_->info("webdav rime_sync_user_data(new-sync-export) start");
                if (!runtime_->runSyncUserDataBlocking()) {
                    result.errorMessage = "new-sync export failed";
                    return result;
                }
                logger_->info("webdav rime_sync_user_data(new-sync-export) ok");
                std::filesystem::create_directories(localOurDir, ec);
                if (ec) {
                    result.errorMessage = "cannot create local our dir: " + ec.message();
                    return result;
                }
            }

            logger_->info("webdav upload(own-after-merge) start: local=%s remote=%s",
                          localOurDir.string().c_str(), remoteOurDir.c_str());
            int uploadErrors = 0;
            uploadTree(*client, *logger_, localOurDir, remoteOurDir, 0,
                       false, true, &uploadErrors);
            logger_->info("webdav upload(own-after-merge) ok: errors=%d", uploadErrors);

            // ========== 步骤 5: 重置 Rime 会话 ==========
            logger_->info("webdav step 5: reset Rime session");
            runtime_->resetSession();

            // 填充结果
            result.success = true;
            result.downloadedDevices = downloadedDevices;
            result.downloadedSchemas = downloadedSchemas;
            result.reinstallSchemasMerged = reinstallSchemasMerged;
            switch (syncType) {
                case SYNC_TYPE_REINSTALL: result.syncType = "reinstall_recovery"; break;
                case SYNC_TYPE_NEW:       result.syncType = "new_sync"; break;
                default:                   result.syncType = "incremental_sync"; break;
            }
            logger_->info("webdav ====== Sync Completed Successfully ======");
            logger_->info("webdav   - sync_type: %s", result.syncType.c_str());
            logger_->info("webdav   - downloaded: %d device(s), %d schema(s)",
                          downloadedDevices, downloadedSchemas);
            if (reinstallSchemasMerged > 0) {
                logger_->info("webdav   - reinstall_recovery: merged %d schemas from remote",
                              reinstallSchemasMerged);
            }
            return result;

        } catch (const std::exception &e) {
            result.errorMessage = std::string("exception: ") + e.what();
            logger_->error("webdav sync exception: %s", e.what());
            return result;
        }
    }

private:
    std::unique_ptr<RimeRuntime> runtime_;
    std::unique_ptr<Logger> logger_;
};

// ============================================================================
// WebDavSyncEngine public API
// ============================================================================

WebDavSyncEngine::WebDavSyncEngine(std::unique_ptr<RimeRuntime> runtime,
                                   std::unique_ptr<Logger> logger)
    : impl_(std::make_unique<Impl>(std::move(runtime), std::move(logger))) {}

WebDavSyncEngine::~WebDavSyncEngine() = default;

SyncResult WebDavSyncEngine::sync(const std::string &hostname,
                                   const std::string &username,
                                   const std::string &password,
                                   const std::string &userRemoteDir) {
    running_.store(true, std::memory_order_release);
    auto result = impl_->sync(hostname, username, password, userRemoteDir);
    running_.store(false, std::memory_order_release);
    return result;
}

}  // namespace rime_sync
