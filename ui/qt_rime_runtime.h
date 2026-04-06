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

// Resolve the active Rime user data directory by probing known locations
// in priority order.  Fcitx5-rime stores its data under
// $XDG_DATA_HOME/fcitx5/rime, while legacy fcitx4 used
// ~/.config/fcitx-rime.
static std::string detect_rime_user_data_dir() {
    namespace fs = std::filesystem;

    // 1. XDG_DATA_HOME/fcitx5/rime  (Fcitx5 standard)
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::string xdg_data = xdg ? std::string(xdg) : std::string(std::getenv("HOME")) + "/.local/share";
    std::string fcitx5_dir = xdg_data + "/fcitx5/rime";
    if (fs::exists(fs::path(fcitx5_dir) / "installation.yaml")) {
        return fcitx5_dir;
    }

    // 2. ~/.config/fcitx-rime  (legacy fcitx4)
    const char* home = std::getenv("HOME");
    std::string fcitx4_dir = home ? std::string(home) + "/.config/fcitx-rime" : std::string();
    if (!fcitx4_dir.empty() && fs::exists(fs::path(fcitx4_dir) / "installation.yaml")) {
        return fcitx4_dir;
    }

    // 3. Fallback: prefer Fcitx5 path if the directory exists at all,
    //    otherwise legacy path.
    if (fs::exists(fcitx5_dir)) return fcitx5_dir;
    return fcitx4_dir;
}

class QtRimeRuntime : public RimeRuntime {
public:
    QtRimeRuntime() {
        api_ = rime_get_api();
        if (api_) {
            RimeTraits traits = {};
            traits.data_size = sizeof(traits);
            traits.app_name = "rime.rime-webdav";
            traits.shared_data_dir = "/usr/share/rime";
            std::string userDir = detect_rime_user_data_dir();
            traits.user_data_dir = userDir.c_str();
            if (api_->initialize) {
                api_->initialize(&traits);
            }
            // deployer_initialize() registers deployment tasks
            // (installation_update, user_dict_sync, etc.) required by
            // sync_user_data() and run_task().  Without this, all
            // deployment tasks are "unknown" and sync_user_data() fails.
            if (api_->deployer_initialize) {
                api_->deployer_initialize(&traits);
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
        if (!p.string().empty() && p.string() != ".") {
            return p.string();
        }
        // Fallback: re-detect using the same logic as initialization
        return detect_rime_user_data_dir();
    }

    std::string getInstallationId() const override {
        auto userDir = getUserDataDir();

        // Try installation.yaml first (Fcitx5-rime format):
        //   installation_id: "abe792d8-8b5f-4b68-9909-3772ff9a92a3"
        std::string yamlFile = userDir.string() + "/installation.yaml";
        {
            std::ifstream fin(yamlFile);
            std::string line;
            while (std::getline(fin, line)) {
                if (line.rfind("installation_id", 0) == 0) {
                    size_t colon = line.find(':');
                    if (colon != std::string::npos) {
                        std::string val = line.substr(colon + 1);
                        size_t start = val.find_first_not_of(" \t\r\n");
                        size_t end = val.find_last_not_of(" \t\r\n");
                        if (start != std::string::npos && end != std::string::npos && end >= start) {
                            std::string id = val.substr(start, end - start + 1);
                            // installation_id may be quoted: "uuid"
                            if (id.front() == '"' && id.back() == '"' && id.size() >= 2) {
                                id = id.substr(1, id.size() - 2);
                            }
                            return id;
                        }
                    }
                }
            }
        }

        // Fallback: installation.ini (legacy format):
        //   installation_id=cachyos-1775483636
        std::string iniFile = userDir.string() + "/installation.ini";
        {
            std::ifstream fin(iniFile);
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

        // Ensure directory exists before writing
        std::filesystem::create_directories(std::filesystem::path(userDirStr));

        std::string newId;
        {
            char hostname[256] = {0};
            gethostname(hostname, sizeof(hostname) - 1);
            unsigned int seed = static_cast<unsigned int>(time(nullptr));
            newId = std::string(hostname) + "-" + std::to_string(seed);
        }

        std::ofstream fout(installFile);
        if (!fout) {
            fprintf(stderr, "ERROR: runInstallationUpdate: cannot write %s\n", installFile.c_str());
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