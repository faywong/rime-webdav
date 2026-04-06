// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RIME_WEBDAV_ADDON_H_
#define RIME_WEBDAV_ADDON_H_

#include "rime_webdav_config.h"

#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/action.h>
#include <fcitx/instance.h>
#include <fcitx/userinterfacemanager.h>

#include <filesystem>
#include <thread>

#include "../common/rime_webdav_sync.h"
#include "linux_rime_runtime.h"
#include "linux_logger.h"

// ============================================================================
// RimeWebdavAddon — Fcitx5 AddonInstance
// ============================================================================

class RimeWebdavAddon : public fcitx::AddonInstance {
public:
    RimeWebdavAddon();
    ~RimeWebdavAddon() override;

    // ===== fcitx::AddonInstance 接口 =====

    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &rawConfig) override;
    void reloadConfig() override;
    void save() override;
    /// 处理配置工具中的 ExternalOption 按钮点击（如"手工同步"按钮）
    void setSubConfig(const std::string &path,
                      const fcitx::RawConfig &config) override;

    // ===== Sync trigger =====

    void onSyncActionActivated();

    // ===== Register action to UI manager =====
    void registerAction(const char *name, fcitx::UserInterfaceManager *ui);

private:
    std::string formatCurrentTime() const;

private:
    RimeWebdavConfig config_;
    std::unique_ptr<rime_sync::RimeRuntime> runtime_;
    std::unique_ptr<rime_sync::Logger> logger_;
    std::unique_ptr<rime_sync::WebDavSyncEngine> engine_;
    fcitx::SimpleAction syncAction_;
};

// ============================================================================
// RimeWebdavAddonFactory — Fcitx5 AddonFactory
// ============================================================================

class RimeWebdavAddonFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        auto *addon = new RimeWebdavAddon();
        addon->registerAction(
            "rime-webdav-sync",
            &manager->instance()->userInterfaceManager());
        return addon;
    }
};

FCITX_ADDON_FACTORY(RimeWebdavAddonFactory)

#endif  // RIME_WEBDAV_ADDON_H_
