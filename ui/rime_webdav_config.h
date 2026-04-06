// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RIME_WEBDAV_CONFIG_H_
#define RIME_WEBDAV_CONFIG_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>

class RimeWebdavConfig : public fcitx::Configuration {
public:
    RimeWebdavConfig()
        : manualSyncTrigger(this, "ManualSyncTrigger",
                           "手工同步",
                           "rime-webdav://trigger-sync") {}

    RimeWebdavConfig(const RimeWebdavConfig &other) : RimeWebdavConfig() {
        copyHelper(other);
    }
    RimeWebdavConfig &operator=(const RimeWebdavConfig &other) {
        copyHelper(other);
        return *this;
    }
    bool operator==(const RimeWebdavConfig &other) const {
        return compareHelper(other);
    }
    const char *typeName() const override { return "RimeWebdavConfig"; }

public:
    fcitx::Option<std::string> serverUrl{
        this, "ServerURL",
        "服务器地址", ""};
    fcitx::Option<std::string> username{
        this, "Username",
        "用户名", ""};
    fcitx::Option<std::string> password{
        this, "Password",
        "密码", ""};
    fcitx::Option<bool> syncEnabled{
        this, "SyncEnabled",
        "启用自动同步", true};
    fcitx::Option<int> autoSyncInterval{
        this, "AutoSyncInterval",
        "自动同步间隔(分钟)",
        30};
    fcitx::ExternalOption manualSyncTrigger;
    fcitx::Option<std::string> lastSyncTime{
        this, "LastSyncTime",
        "上次同步时间(无需输入)", "从未同步"};
};

#endif  // RIME_WEBDAV_CONFIG_H_
