// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H_
#define MAINWINDOW_H_

#include <QMainWindow>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollBar>
#include <QGroupBox>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#include <memory>
#include <string>
#include <filesystem>

#include "../common/rime_webdav_sync.h"
#include "qt_logger.h"
#include "qt_rime_runtime.h"

class SyncWorker;

// ============================================================================
// ConfigData — Configuration data structure (saved to/loaded from file)
// ============================================================================

struct ConfigData {
    std::string serverUrl;
    std::string username;
    std::string password;
    std::string lastSyncTime;
    
    static ConfigData load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
};

// ============================================================================
// MainWindow — Standalone Qt configuration UI for rime-webdav
// ============================================================================

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onSyncNowClicked();
    void onSaveClicked();
    void onSyncFinished(const QString &message);
    void appendLog(const QString &line);

private:
    void setupUi();
    void loadConfig();
    void saveConfig();
    std::filesystem::path getConfigPath() const;

private:
    // WebDAV Settings
    QGroupBox *settingsGroup_;
    QLabel *serverUrlLabel_;
    QLineEdit *serverUrlEdit_;
    QLabel *usernameLabel_;
    QLineEdit *usernameEdit_;
    QLabel *passwordLabel_;
    QLineEdit *passwordEdit_;
    
    // Status
    QLabel *lastSyncLabel_;
    QLabel *lastSyncValueLabel_;
    
    // Debug Log
    QGroupBox *logGroup_;
    QTextEdit *logTextEdit_;
    
    // Buttons
    QPushButton *syncNowButton_;
    QPushButton *saveButton_;
    
    // Sync worker thread
    QThread *workerThread_;
    SyncWorker *syncWorker_;
    
    ConfigData config_;
};

// ============================================================================
// SyncWorker — Worker thread for running sync operation
// ============================================================================

class SyncWorker : public QObject {
    Q_OBJECT

public:
    explicit SyncWorker(const QString &serverUrl,
                       const QString &username,
                       const QString &password,
                       QObject *parent = nullptr);
    ~SyncWorker() override;

public slots:
    void doSync();

signals:
    void finished(const QString &message);
    void logReady(const QString &line);

private:
    QString serverUrl_;
    QString username_;
    QString password_;
};

#endif  // MAINWINDOW_H_