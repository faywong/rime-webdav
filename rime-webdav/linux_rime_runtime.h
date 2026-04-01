// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RIME_WEBDAV_LINUX_RUNTIME_H_
#define RIME_WEBDAV_LINUX_RUNTIME_H_

#include "../shared/rime_webdav_sync.h"

#include <fcitx-utils/log.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <rime_api.h>
#include <filesystem>
#include <fstream>

namespace rime_sync {

// ============================================================================
// LinuxRimeRuntime — rime_sync::RimeRuntime 实现，使用 rime_get_api()
// ============================================================================

class LinuxRimeRuntime : public RimeRuntime {
public:
    LinuxRimeRuntime() {
        api_ = rime_get_api();
    }

    void dumpDebugInfo() const {
        FCITX_INFO() << "[rime-webdav] ========== Rime Debug Info ==========";

        // Helper: resolve a possibly-relative path to absolute
        auto abs = [](const std::filesystem::path &p) {
            return std::filesystem::absolute(p);
        };

        // User data directory
        char userDataDir[512] = {0};
        getUserDataDir(userDataDir, sizeof(userDataDir));
        FCITX_INFO() << "[rime-webdav]   user_data_dir: " << abs(userDataDir);

        // Sync directory
        auto syncDir = abs(getSyncDir());
        FCITX_INFO() << "[rime-webdav]   sync_dir: " << syncDir.string();

        // Shared data directory
        char sharedDataDir[512] = {0};
        if (api_ && api_->get_shared_data_dir_s) {
            api_->get_shared_data_dir_s(sharedDataDir, sizeof(sharedDataDir) - 1);
        }
        FCITX_INFO() << "[rime-webdav]   shared_data_dir: " << abs(sharedDataDir);

        // User config directory (same as user_data_dir for fcitx5-rime)
        FCITX_INFO() << "[rime-webdav]   user_config_dir: "
                     << (strlen(userDataDir) > 0 ? abs(userDataDir).string() : "(unknown)")
                     << " (same as user_data_dir for fcitx5-rime)";

        // Installation ID
        std::string installId = getInstallationId();
        FCITX_INFO() << "[rime-webdav]   installation_id: " << installId;

        // Installation ID source file
        FCITX_INFO() << "[rime-webdav]   installation_ini: "
                     << abs(userDataDir) / "installation.ini";

        // Sync directory contents
        std::error_code ec;
        if (std::filesystem::exists(syncDir, ec) && !ec) {
            FCITX_INFO() << "[rime-webdav]   sync_dir contents:";
            for (const auto &entry : std::filesystem::directory_iterator(syncDir, ec)) {
                if (ec) break;
                auto rel = std::filesystem::relative(entry.path(), syncDir, ec);
                if (ec) rel = entry.path().filename();
                std::string type = entry.is_directory() ? "[DIR] " : "[FILE]";
                FCITX_INFO() << "[rime-webdav]     " << type << rel.string();
                // If it's a subdirectory (device dir), list its contents
                if (entry.is_directory()) {
                    std::error_code ec2;
                    for (const auto &sub : std::filesystem::directory_iterator(entry.path(), ec2)) {
                        if (ec2) break;
                        auto subRel = sub.path().filename().string();
                        FCITX_INFO() << "[rime-webdav]       " << subRel;
                    }
                }
            }
        } else {
            FCITX_INFO() << "[rime-webdav]   sync_dir does not exist or is inaccessible";
        }

        // User data directory key files
        std::filesystem::path udPath = abs(userDataDir);
        if (std::filesystem::exists(udPath, ec) && !ec) {
            FCITX_INFO() << "[rime-webdav]   user_data_dir key files:";
            static const char *keyFiles[] = {
                "default.yaml",
                "default.custom.yaml",
                "user.yaml",
                "installation.ini",
                "build",
            };
            for (const auto &name : keyFiles) {
                auto p = udPath / name;
                if (std::filesystem::exists(p, ec) && !ec) {
                    FCITX_INFO() << "[rime-webdav]     " << name << " ✓";
                }
            }
            // List schema files
            FCITX_INFO() << "[rime-webdav]   user schemas (*.schema.yaml):";
            int schemaCount = 0;
            for (const auto &entry : std::filesystem::directory_iterator(udPath, ec)) {
                if (ec) break;
                std::string fname = entry.path().filename().string();
                if (fname.size() >= 11 &&
                    fname.compare(fname.size() - 11, 11, ".schema.yaml") == 0) {
                    FCITX_INFO() << "[rime-webdav]     " << fname;
                    ++schemaCount;
                }
            }
            if (schemaCount == 0) {
                FCITX_INFO() << "[rime-webdav]     (none found in user_data_dir)";
            }

            // List userdb directories
            FCITX_INFO() << "[rime-webdav]   user databases (* userdb dirs):";
            int dbCount = 0;
            for (const auto &entry : std::filesystem::directory_iterator(udPath, ec)) {
                if (ec) break;
                if (entry.is_directory()) {
                    std::string fname = entry.path().filename().string();
                    if (fname.size() >= 7 &&
                        fname.compare(fname.size() - 7, 7, ".userdb") == 0) {
                        FCITX_INFO() << "[rime-webdav]     " << fname << "/";
                        ++dbCount;
                    }
                }
            }
            if (dbCount == 0) {
                FCITX_INFO() << "[rime-webdav]     (none found — may not have typed yet)";
            }
        }

        // Rime API availability
        FCITX_INFO() << "[rime-webdav]   rime_api available: " << (api_ ? "yes" : "no");
        if (api_) {
            FCITX_INFO() << "[rime-webdav]   api->get_user_data_dir_s: "
                         << (api_->get_user_data_dir_s ? "yes" : "no");
            FCITX_INFO() << "[rime-webdav]   api->get_sync_dir_s: "
                         << (api_->get_sync_dir_s ? "yes" : "no");
            FCITX_INFO() << "[rime-webdav]   api->get_shared_data_dir_s: "
                         << (api_->get_shared_data_dir_s ? "yes" : "no");
            FCITX_INFO() << "[rime-webdav]   api->sync_user_data: "
                         << (api_->sync_user_data ? "yes" : "no");
            FCITX_INFO() << "[rime-webdav]   api->run_task: "
                         << (api_->run_task ? "yes" : "no");
        }

        FCITX_INFO() << "[rime-webdav] ======================================";
    }

    std::filesystem::path getSyncDir() const override {
        char buf[512] = {0};
        if (api_ && api_->get_sync_dir_s) {
            api_->get_sync_dir_s(buf, sizeof(buf) - 1);
        }
        std::string path(buf);
        if (path.empty()) {
            // fallback: user_data_dir/sync
            getUserDataDir(buf, sizeof(buf));
            path = std::string(buf) + "/sync";
        }
        return std::filesystem::path(path);
    }

    std::string getInstallationId() const override {
        char buf[512] = {0};
        char dirBuf[512] = {0};
        getUserDataDir(dirBuf, sizeof(dirBuf));
        std::string installFile = std::string(dirBuf) + "/installation.ini";
        std::ifstream fin(installFile);
        if (!fin) {
            return "unknown";
        }
        std::string line;
        while (std::getline(fin, line)) {
            if (line.rfind("installation_id", 0) == 0) {
                size_t eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string id = line.substr(eq + 1);
                    // trim whitespace
                    size_t start = id.find_first_not_of(" \t\r\n");
                    size_t end = id.find_last_not_of(" \t\r\n");
                    if (start != std::string::npos) {
                        return id.substr(start, end - start + 1);
                    }
                }
            }
        }
        return "unknown";
    }

    bool runSyncUserDataBlocking() override {
        if (!api_ || !api_->sync_user_data) {
            return false;
        }
        return api_->sync_user_data() != 0;
    }

    bool runInstallationUpdate() override {
        if (api_ && api_->run_task) {
            if (api_->run_task("installation_update")) {
                return true;
            }
        }
        // fallback: generate installation_id and write installation.ini
        char dirBuf[512] = {0};
        getUserDataDir(dirBuf, sizeof(dirBuf));
        std::string installFile = std::string(dirBuf) + "/installation.ini";

        std::string newId;
        {
            // generate UUID-like id: hostname + random
            char hostname[256] = {0};
            gethostname(hostname, sizeof(hostname) - 1);
            unsigned int seed = static_cast<unsigned int>(time(nullptr));
            newId = std::string(hostname) + "-" +
                    std::to_string(seed);
        }

        std::ofstream fout(installFile);
        if (!fout) {
            return false;
        }
        fout << "[info]\n"
             << "installation_id=" << newId << "\n"
             << "rime_version=1.0\n";
        return true;
    }

    void resetSession() override {
        // Fcitx5 rime module handles session reset automatically
        // when user_data syncs complete
    }

private:
    void getUserDataDir(char* buf, size_t size) const {
        if (api_ && api_->get_user_data_dir_s) {
            api_->get_user_data_dir_s(buf, size - 1);
        }
    }

    RimeApi* api_ = nullptr;
};

}  // namespace rime_sync

#endif  // RIME_WEBDAV_LINUX_RUNTIME_H_
