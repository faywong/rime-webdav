// SPDX-FileCopyrightText: 2024 Rime community
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "file_logger.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QTime>

#include <chrono>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

std::string formatCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[64] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

}  // namespace

ConfigData ConfigData::load(const std::filesystem::path& path) {
    ConfigData cfg;
    if (!std::filesystem::exists(path)) {
        cfg.lastSyncTime = "从未同步";
        return cfg;
    }
    
    std::ifstream fin(path.string());
    if (!fin) {
        cfg.lastSyncTime = "从未同步";
        return cfg;
    }
    
    std::string line;
    while (std::getline(fin, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key == "server_url") cfg.serverUrl = value;
        else if (key == "username") cfg.username = value;
        else if (key == "password") cfg.password = value;
        else if (key == "last_sync_time") cfg.lastSyncTime = value;
    }
    
    if (cfg.lastSyncTime.empty()) {
        cfg.lastSyncTime = "从未同步";
    }
    
    return cfg;
}

void ConfigData::save(const std::filesystem::path& path) const {
    std::ofstream fout(path.string());
    if (!fout) return;
    fout << "# rime-webdav Qt configuration\n";
    fout << "server_url=" << serverUrl << "\n";
    fout << "username=" << username << "\n";
    fout << "password=" << password << "\n";
    fout << "last_sync_time=" << lastSyncTime << "\n";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , workerThread_(nullptr)
    , syncWorker_(nullptr) {
    setupUi();
    loadConfig();
}

MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait();
        delete workerThread_;
    }
}

void MainWindow::setupUi() {
    setWindowTitle("Rime WebDAV 同步");
    setMinimumSize(600, 500);
    
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    
    settingsGroup_ = new QGroupBox("WebDAV 服务器设置", this);
    auto *settingsLayout = new QGridLayout(settingsGroup_);
    settingsLayout->setSpacing(12);
    
    serverUrlLabel_ = new QLabel("服务器地址:", this);
    serverUrlEdit_ = new QLineEdit(this);
    serverUrlEdit_->setPlaceholderText("https://dav.jianguoyun.com/dav/");
    settingsLayout->addWidget(serverUrlLabel_, 0, 0);
    settingsLayout->addWidget(serverUrlEdit_, 0, 1);
    
    usernameLabel_ = new QLabel("用户名:", this);
    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setPlaceholderText("您的用户名");
    settingsLayout->addWidget(usernameLabel_, 1, 0);
    settingsLayout->addWidget(usernameEdit_, 1, 1);
    
    passwordLabel_ = new QLabel("密码:", this);
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setPlaceholderText("您的密码");
    passwordEdit_->setEchoMode(QLineEdit::Password);
    settingsLayout->addWidget(passwordLabel_, 2, 0);
    settingsLayout->addWidget(passwordEdit_, 2, 1);
    
    mainLayout->addWidget(settingsGroup_);
    
    auto *statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(12);
    
    lastSyncLabel_ = new QLabel("上次同步时间:", this);
    lastSyncValueLabel_ = new QLabel(config_.lastSyncTime.c_str(), this);
    lastSyncValueLabel_->setStyleSheet("font-weight: bold; color: #666;");
    
    statusLayout->addWidget(lastSyncLabel_);
    statusLayout->addWidget(lastSyncValueLabel_);
    statusLayout->addStretch();
    
    mainLayout->addLayout(statusLayout);
    
    logGroup_ = new QGroupBox("同步日志", this);
    auto *logLayout = new QVBoxLayout(logGroup_);
    
    logTextEdit_ = new QTextEdit(this);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    logTextEdit_->setStyleSheet(
        "QTextEdit { "
        "  background-color: #1e1e1e; "
        "  color: #d4d4d4; "
        "  border: 1px solid #3c3c3c; "
        "  padding: 8px; "
        "}"
    );
    logLayout->addWidget(logTextEdit_);
    
    mainLayout->addWidget(logGroup_);
    
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    syncNowButton_ = new QPushButton("立即同步", this);
    syncNowButton_->setMinimumWidth(120);
    syncNowButton_->setStyleSheet(
        "QPushButton { "
        "  background-color: #0078d4; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: #106ebe; }"
        "QPushButton:pressed { background-color: #005a9e; }"
        "QPushButton:disabled { background-color: #666; }"
    );
    
    saveButton_ = new QPushButton("保存", this);
    saveButton_->setMinimumWidth(120);
    saveButton_->setStyleSheet(
        "QPushButton { "
        "  background-color: #4caf50; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: #43a047; }"
        "QPushButton:pressed { background-color: #388e3c; }"
    );
    
    buttonLayout->addWidget(syncNowButton_);
    buttonLayout->addWidget(saveButton_);
    
    mainLayout->addLayout(buttonLayout);
    
    mainLayout->setStretchFactor(logGroup_, 1);
    mainLayout->setStretchFactor(buttonLayout, 0);
    
    connect(syncNowButton_, &QPushButton::clicked, this, &MainWindow::onSyncNowClicked);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::onSaveClicked);
}

std::filesystem::path MainWindow::getConfigPath() const {
    const char *xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig) {
        return std::filesystem::path(xdgConfig) / "rime-webdav" / "config";
    }
    const char *home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "rime-webdav" / "config";
    }
    return std::filesystem::path("/etc/rime-webdav.conf");
}

void MainWindow::loadConfig() {
    config_ = ConfigData::load(getConfigPath());
    serverUrlEdit_->setText(config_.serverUrl.c_str());
    usernameEdit_->setText(config_.username.c_str());
    passwordEdit_->setText(config_.password.c_str());
    lastSyncValueLabel_->setText(config_.lastSyncTime.c_str());
}

void MainWindow::saveConfig() {
    config_.serverUrl = serverUrlEdit_->text().toStdString();
    config_.username = usernameEdit_->text().toStdString();
    config_.password = passwordEdit_->text().toStdString();
    
    auto path = getConfigPath();
    std::filesystem::create_directories(path.parent_path());
    config_.save(path);
}

void MainWindow::onSyncNowClicked() {
    if (serverUrlEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "提示", "请先填写服务器地址");
        return;
    }
    
    syncNowButton_->setEnabled(false);
    saveButton_->setEnabled(false);
    logTextEdit_->clear();
    
    appendLog("开始同步...");
    appendLog("服务器: " + serverUrlEdit_->text());
    
    workerThread_ = new QThread(this);
    syncWorker_ = new SyncWorker(
        serverUrlEdit_->text(),
        usernameEdit_->text(),
        passwordEdit_->text()
    );
    syncWorker_->moveToThread(workerThread_);
    
    connect(workerThread_, &QThread::started, syncWorker_, &SyncWorker::doSync);
    connect(syncWorker_, &SyncWorker::finished, this, &MainWindow::onSyncFinished);
    connect(syncWorker_, &SyncWorker::logReady, this, &MainWindow::appendLog);
    connect(syncWorker_, &SyncWorker::finished, workerThread_, &QThread::quit);
    
    workerThread_->start();
}

void MainWindow::onSyncFinished(const QString &message) {
    syncNowButton_->setEnabled(true);
    saveButton_->setEnabled(true);
    
    if (message.startsWith("成功")) {
        std::string ts = formatCurrentTime();
        config_.lastSyncTime = ts;
        lastSyncValueLabel_->setText(ts.c_str());
        
        saveConfig();
        
        appendLog("同步完成!");
        QMessageBox::information(this, "同步完成", 
            "词库已下载到 Rime 用户目录。请在输入法设置中点击「同步」按钮使新词库生效。");
    } else {
        config_.lastSyncTime = "同步失败 " + formatCurrentTime();
        lastSyncValueLabel_->setText(config_.lastSyncTime.c_str());
        
        appendLog("同步失败: " + message);
        QMessageBox::critical(this, "同步失败", message);
    }
}

void MainWindow::appendLog(const QString &line) {
    QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    logTextEdit_->append(QString("[%1] %2").arg(timestamp, line));
    logTextEdit_->verticalScrollBar()->setValue(logTextEdit_->verticalScrollBar()->maximum());
}

void MainWindow::onSaveClicked() {
    saveConfig();
    QMessageBox::information(this, "保存成功", "配置已保存");
}

SyncWorker::SyncWorker(const QString &serverUrl,
                       const QString &username,
                       const QString &password,
                       QObject *parent)
    : QObject(parent)
    , serverUrl_(serverUrl)
    , username_(username)
    , password_(password) {
}

SyncWorker::~SyncWorker() = default;

void SyncWorker::doSync() {
    // Create Qt logger that emits signals for UI updates
    auto logger = std::make_unique<rime_sync::QtLogger>();
    rime_sync::QtLogger* loggerPtr = logger.get();
    connect(loggerPtr, &rime_sync::QtLogger::logReady, this, &SyncWorker::logReady);
    
    auto runtime = std::make_unique<rime_sync::QtRimeRuntime>();
    emit logReady("Rime: user_data_dir=" + QString::fromStdString(runtime->getUserDataDirString()));
    auto engine = std::make_unique<rime_sync::WebDavSyncEngine>(
        std::move(runtime), std::move(logger));
    
    std::string serverUrl = serverUrl_.toStdString();
    rime_sync::WebDavEndpoint endpoint = rime_sync::splitHostnameAndRoot(serverUrl);
    
    std::string hostname;
    std::string remoteRoot;
    
    if (!endpoint.root.empty() && endpoint.root != "/") {
        hostname = endpoint.hostname_base;
        remoteRoot = endpoint.root + "RimeSync";
    } else {
        hostname = serverUrl;
        remoteRoot = "/RimeSync";
    }
    
    auto result = engine->sync(
        hostname,
        username_.toStdString(),
        password_.toStdString(),
        remoteRoot);
    
    if (result.success) {
        QString msg = QString("同步成功! 下载了 %1 个设备, %2 个词库")
                         .arg(result.downloadedDevices)
                         .arg(result.downloadedSchemas);
        emit finished(msg);
    } else {
        emit finished(QString("失败: ") + QString::fromStdString(result.errorMessage));
    }
}

