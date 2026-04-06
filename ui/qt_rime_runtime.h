#ifndef QT_RIME_RUNTIME_H_
#define QT_RIME_RUNTIME_H_

#include "../common/rime_webdav_sync.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <rime_api.h>
#include <filesystem>
#include <fstream>

namespace rime_sync {

class QtRimeRuntime : public RimeRuntime {
public:
    QtRimeRuntime() {
        api_ = rime_get_api();
        if (api_) {
            RimeTraits traits = {};
            traits.data_size = sizeof(traits);
            traits.app_name = "rime.rime-webdav";
            traits.shared_data_dir = "/usr/share/rime";
            const char* home = std::getenv("HOME");
            std::string userDir = home ? std::string(home) + "/.config/fcitx-rime" : "/home/faywong/.config/fcitx-rime";
            traits.user_data_dir = userDir.c_str();
            if (api_->initialize) {
                api_->initialize(&traits);
            }
            fprintf(stderr, "DEBUG: Rime initialized, user_data_dir=%s\n", userDir.c_str());
        }
    }

    std::filesystem::path getUserDataDir() const {
        char buf[512] = {0};
        if (api_ && api_->get_user_data_dir_s) {
            api_->get_user_data_dir_s(buf, sizeof(buf) - 1);
        }
        return std::filesystem::path(buf);
    }

    std::filesystem::path getSyncDir() const override {
        auto userDirStr = getUserDataDirString();
        std::string path = userDirStr + "/sync";
        return std::filesystem::path(path);
    }

    std::string getUserDataDirString() const {
        auto p = getUserDataDir();
        if (p.string().empty() || p.string() == ".") {
            const char* home = std::getenv("HOME");
            return home ? std::string(home) + "/.config/fcitx-rime" : "/home/faywong/.config/fcitx-rime";
        }
        return p.string();
    }

    std::string getInstallationId() const override {
        auto userDir = getUserDataDir();
        std::string installFile = userDir.string() + "/installation.ini";
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
        auto userDir = getUserDataDir();
        auto syncDir = getSyncDir();
        fprintf(stderr, "DEBUG: sync_user_data: user_data_dir=%s sync_dir=%s\n", 
                userDir.string().c_str(), syncDir.string().c_str());
        fflush(stderr);
        Bool result = api_->sync_user_data();
        fprintf(stderr, "DEBUG: sync_user_data returned %d\n", result);
        return result != 0;
    }

    bool runInstallationUpdate() override {
        auto userDirStr = getUserDataDirString();
        fprintf(stderr, "DEBUG: runInstallationUpdate: user_data_dir=%s\n", userDirStr.c_str());
        if (api_ && api_->run_task) {
            if (api_->run_task("installation_update")) {
                return true;
            }
        }
        std::string installFile = userDirStr + "/installation.ini";

        std::string newId;
        {
            char hostname[256] = {0};
            gethostname(hostname, sizeof(hostname) - 1);
            unsigned int seed = static_cast<unsigned int>(time(nullptr));
            newId = std::string(hostname) + "-" + std::to_string(seed);
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
    }

private:
    void getUserDataDir(char* buf, size_t size) const {
        if (api_ && api_->get_user_data_dir_s) {
            api_->get_user_data_dir_s(buf, size - 1);
        }
    }

    RimeApi* api_ = nullptr;
};

}

#endif  // QT_RIME_RUNTIME_H_