// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rime_webdav_addon.h"

#include <fcitx-config/iniparser.h>

#include <chrono>
#include <ctime>

// ============================================================================
// RimeWebdavAddon implementation
// ============================================================================

RimeWebdavAddon::RimeWebdavAddon() {
    // Load saved config from disk
    reloadConfig();

    auto runtime = std::make_unique<rime_sync::LinuxRimeRuntime>();
    runtime->dumpDebugInfo();
    runtime_ = std::move(runtime);
    logger_ = std::make_unique<rime_sync::FcitxLogger>();
    engine_ = std::make_unique<rime_sync::WebDavSyncEngine>(
        std::move(runtime_), std::move(logger_));

    // Create "manual sync" action (shown in toolbar/menu)
    syncAction_.setIcon("fcitx-cloud-sync");
    syncAction_.setShortText("手动同步");
    syncAction_.setLongText("立即将 Rime 用户词典同步到 WebDAV 服务器");

    // Connect action click signal
    syncAction_.connect<fcitx::SimpleAction::Activated>(
        [this](fcitx::InputContext *) {
            onSyncActionActivated();
        });
}

RimeWebdavAddon::~RimeWebdavAddon() = default;

const fcitx::Configuration *RimeWebdavAddon::getConfig() const {
    return &config_;
}

void RimeWebdavAddon::setConfig(const fcitx::RawConfig &rawConfig) {
    auto items = rawConfig.subItems();
    bool hasTrigger = false;
    for (const auto &k : items) {
        if (k == "ManualSyncTrigger") {
            hasTrigger = true;
            break;
        }
    }

    config_.load(rawConfig);
    safeSaveAsIni(config_, "conf/rime-webdav.config");

    // ExternalOption buttons arrive as sub-items in setConfig(), not setSubConfig()
    if (hasTrigger) {
        FCITX_INFO() << "[rime-webdav] ManualSyncTrigger detected, starting sync";
        onSyncActionActivated();
    }
}

void RimeWebdavAddon::reloadConfig() {
    readAsIni(config_, "conf/rime-webdav.config");
}

void RimeWebdavAddon::save() {
}

void RimeWebdavAddon::setSubConfig(const std::string &path,
                                   const fcitx::RawConfig &config) {
    if (path == "rime-webdav://trigger-sync") {
        onSyncActionActivated();
        return;
    }
    if (path == "rime-webdav://display-last-sync") {
        return;
    }
    fcitx::AddonInstance::setSubConfig(path, config);
}

void RimeWebdavAddon::registerAction(const char *name,
                                     fcitx::UserInterfaceManager *ui) {
    ui->registerAction(name, &syncAction_);
}

std::string RimeWebdavAddon::formatCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[64] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

void RimeWebdavAddon::onSyncActionActivated() {
    FCITX_INFO() << "[rime-webdav] onSyncActionActivated() called, serverUrl=" << config_.serverUrl.value();

    if (engine_->isRunning()) {
        FCITX_WARN() << "[rime-webdav] Sync already in progress, skipping";
        return;
    }

    // Dump debug info at sync time — rime is guaranteed initialized here
    rime_sync::LinuxRimeRuntime rtForDebug;
    rtForDebug.dumpDebugInfo();

    if (config_.serverUrl.value().empty()) {
        FCITX_WARN() << "[rime-webdav] Server URL is not configured, skipping sync";
        return;
    }

    // Parse serverUrl: split into hostname and remote path
    std::string serverUrl = config_.serverUrl.value();
    rime_sync::WebDavEndpoint endpoint =
        rime_sync::splitHostnameAndRoot(serverUrl);

    std::string hostname;
    std::string remoteRoot;

    if (!endpoint.root.empty() && endpoint.root != "/") {
        hostname = endpoint.hostname_base;
        remoteRoot = endpoint.root;
    } else {
        hostname = serverUrl;
        remoteRoot = "/RimeSync";
    }

    FCITX_INFO() << "[rime-webdav] Starting sync: host=" << hostname
                 << " remote=" << remoteRoot;

    // Update action state
    syncAction_.update(nullptr);

    std::thread([this, hostname, remoteRoot]() {
        auto result = engine_->sync(
            hostname,
            config_.username.value(),
            config_.password.value(),
            remoteRoot);

        std::string ts = formatCurrentTime();
        if (result.success) {
            FCITX_INFO() << "[rime-webdav] Sync succeeded: type="
                         << result.syncType
                         << ", downloaded " << result.downloadedDevices
                         << " device(s), " << result.downloadedSchemas
                         << " schema(s)";
            // Update last success sync time
            config_.lastSyncTime.setValue(ts);
        } else {
            FCITX_ERROR() << "[rime-webdav] Sync failed: "
                             << result.errorMessage;
            // Show failure time
            config_.lastSyncTime.setValue("同步失败 " + ts);
        }
        syncAction_.update(nullptr);
    }).detach();
}
