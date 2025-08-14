// src/utils/Settings.cpp - Settings Manager Implementation
#include "Settings.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QMutexLocker>

Settings::Settings(QObject* parent)
    : QObject(parent)
    , settings_(nullptr)
    , settingsGroup_("CueForge")
{
    // Create QSettings instance with proper organization info
    settings_ = new QSettings(
        QSettings::IniFormat,
        QSettings::UserScope,
        QApplication::organizationName(),
        QApplication::applicationName(),
        this
    );

    // Initialize default values
    initializeDefaults();

    // Validate existing settings
    validateSettings();

    qDebug() << "Settings initialized. File location:" << settings_->fileName();
}

Settings::~Settings()
{
    sync();
}

// Core Settings Interface

QVariant Settings::value(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&mutex_);

    QString fullKey = getFullKey(key);

    // Check if we have a default value for this key
    QVariant result = settings_->value(fullKey, defaults_.value(key, defaultValue));

    return result;
}

void Settings::setValue(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&mutex_);

    QString fullKey = getFullKey(key);
    QVariant oldValue = settings_->value(fullKey);

    if (oldValue != value) {
        settings_->setValue(fullKey, value);

        locker.unlock();
        emit settingChanged(key, oldValue, value);
    }
}

void Settings::remove(const QString& key)
{
    QMutexLocker locker(&mutex_);

    QString fullKey = getFullKey(key);
    if (settings_->contains(fullKey)) {
        QVariant oldValue = settings_->value(fullKey);
        settings_->remove(fullKey);

        locker.unlock();
        emit settingChanged(key, oldValue, QVariant());
    }
}

bool Settings::contains(const QString& key) const
{
    QMutexLocker locker(&mutex_);
    return settings_->contains(getFullKey(key));
}

// Convenience Accessors

QString Settings::getString(const QString& key, const QString& defaultValue) const
{
    return value(key, defaultValue).toString();
}

int Settings::getInt(const QString& key, int defaultValue) const
{
    return value(key, defaultValue).toInt();
}

double Settings::getDouble(const QString& key, double defaultValue) const
{
    return value(key, defaultValue).toDouble();
}

bool Settings::getBool(const QString& key, bool defaultValue) const
{
    return value(key, defaultValue).toBool();
}

QStringList Settings::getStringList(const QString& key, const QStringList& defaultValue) const
{
    return value(key, defaultValue).toStringList();
}

// Convenience Setters

void Settings::setString(const QString& key, const QString& value)
{
    setValue(key, value);
}

void Settings::setInt(const QString& key, int value)
{
    setValue(key, value);
}

void Settings::setDouble(const QString& key, double value)
{
    setValue(key, value);
}

void Settings::setBool(const QString& key, bool value)
{
    setValue(key, value);
}

void Settings::setStringList(const QString& key, const QStringList& value)
{
    setValue(key, value);
}

// Settings Groups

void Settings::beginGroup(const QString& prefix)
{
    QMutexLocker locker(&mutex_);
    groupStack_.append(prefix);
    settings_->beginGroup(prefix);
}

void Settings::endGroup()
{
    QMutexLocker locker(&mutex_);
    if (!groupStack_.isEmpty()) {
        groupStack_.removeLast();
        settings_->endGroup();
    }
}

QStringList Settings::childGroups() const
{
    QMutexLocker locker(&mutex_);
    return settings_->childGroups();
}

QStringList Settings::childKeys() const
{
    QMutexLocker locker(&mutex_);
    return settings_->childKeys();
}

// Batch Operations

void Settings::sync()
{
    QMutexLocker locker(&mutex_);
    settings_->sync();
}

void Settings::clear()
{
    QMutexLocker locker(&mutex_);
    settings_->clear();

    locker.unlock();
    emit settingsReset();
}

// Default Values Management

void Settings::setDefaultValue(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&mutex_);
    defaults_[key] = value;
}

QVariant Settings::getDefaultValue(const QString& key) const
{
    QMutexLocker locker(&mutex_);
    return defaults_.value(key);
}

void Settings::resetToDefaults()
{
    QMutexLocker locker(&mutex_);

    settings_->clear();

    // Set all default values
    for (auto it = defaults_.constBegin(); it != defaults_.constEnd(); ++it) {
        settings_->setValue(getFullKey(it.key()), it.value());
    }

    settings_->sync();

    locker.unlock();
    emit settingsReset();

    qDebug() << "Settings reset to defaults";
}

void Settings::resetKeyToDefault(const QString& key)
{
    QMutexLocker locker(&mutex_);

    if (defaults_.contains(key)) {
        QVariant oldValue = settings_->value(getFullKey(key));
        QVariant defaultValue = defaults_[key];

        settings_->setValue(getFullKey(key), defaultValue);

        locker.unlock();
        emit settingChanged(key, oldValue, defaultValue);
    }
    else {
        // Remove key if no default exists
        locker.unlock();
        remove(key);
    }
}

// Settings Validation

bool Settings::isValidKey(const QString& key) const
{
    // Basic validation - key should not be empty and should not contain certain characters
    return !key.isEmpty() &&
        !key.contains("//") &&
        !key.startsWith("/") &&
        !key.endsWith("/");
}

bool Settings::isValidValue(const QString& key, const QVariant& value) const
{
    Q_UNUSED(key)

        // Basic validation - value should be serializable
        return value.isValid() && !value.isNull();
}

// Import/Export

bool Settings::exportSettings(const QString& filePath) const
{
    QMutexLocker locker(&mutex_);

    QJsonObject json;

    // Export all settings
    QStringList keys = settings_->allKeys();
    for (const QString& key : keys) {
        QVariant value = settings_->value(key);
        json[key] = QJsonValue::fromVariant(value);
    }

    // Write to file
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for export:" << filePath;
        return false;
    }

    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();

    locker.unlock();
    emit settingsExported();

    qDebug() << "Settings exported to:" << filePath;
    return true;
}

bool Settings::importSettings(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for import:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse settings JSON:" << error.errorString();
        return false;
    }

    QJsonObject json = doc.object();

    QMutexLocker locker(&mutex_);

    // Import settings
    for (auto it = json.constBegin(); it != json.constEnd(); ++it) {
        QString key = it.key();
        QVariant value = it.value().toVariant();

        if (isValidKey(key) && isValidValue(key, value)) {
            settings_->setValue(key, value);
        }
    }

    settings_->sync();

    locker.unlock();
    emit settingsImported();

    qDebug() << "Settings imported from:" << filePath;
    return true;
}

// Private Implementation

void Settings::initializeDefaults()
{
    // General application settings
    defaults_[Keys::General::Theme] = "dark";
    defaults_[Keys::General::Language] = "en";
    defaults_[Keys::General::AutoSave] = true;
    defaults_[Keys::General::AutoSaveInterval] = 5; // minutes
    defaults_[Keys::General::LoadLastWorkspace] = true;
    defaults_[Keys::General::ConfirmDelete] = true;
    defaults_[Keys::General::ShowSplashScreen] = true;

    // Window and UI settings
    defaults_[Keys::Window::Maximized] = false;
    defaults_[Keys::Window::FullScreen] = false;
    defaults_[Keys::Window::InspectorVisible] = true;
    defaults_[Keys::Window::TransportVisible] = true;
    defaults_[Keys::Window::MatrixVisible] = false;
    defaults_[Keys::Window::Zoom] = 1.0;

    // Workspace settings
    defaults_[Keys::Workspace::DefaultSaveLocation] = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    defaults_[Keys::Workspace::AutoBackup] = true;
    defaults_[Keys::Workspace::BackupInterval] = 10; // minutes
    defaults_[Keys::Workspace::MaxBackups] = 5;

    // Audio engine settings
    defaults_[Keys::Audio::SampleRate] = 48000;
    defaults_[Keys::Audio::BufferSize] = 256;
    defaults_[Keys::Audio::InputChannels] = 2;
    defaults_[Keys::Audio::OutputChannels] = 2;
    defaults_[Keys::Audio::MasterVolume] = 0.8;
    defaults_[Keys::Audio::EnableExclusive] = false;

    // MIDI settings
    defaults_[Keys::MIDI::EnableMTC] = false;
    defaults_[Keys::MIDI::EnableMMC] = false;
    defaults_[Keys::MIDI::MTCOffset] = 0.0;

    // Network settings
    defaults_[Keys::Network::OSCEnabled] = false;
    defaults_[Keys::Network::OSCPort] = 53000;
    defaults_[Keys::Network::ArtNetEnabled] = false;
    defaults_[Keys::Network::ArtNetUniverse] = 0;
    defaults_[Keys::Network::TCPPort] = 53001;

    // Keyboard shortcuts (using Qt key sequences)
    defaults_[Keys::Shortcuts::Go] = "Space";
    defaults_[Keys::Shortcuts::Stop] = "Shift+Space";
    defaults_[Keys::Shortcuts::Pause] = "P";
    defaults_[Keys::Shortcuts::Panic] = "Esc";
    defaults_[Keys::Shortcuts::Save] = "Ctrl+S";
    defaults_[Keys::Shortcuts::Open] = "Ctrl+O";
    defaults_[Keys::Shortcuts::New] = "Ctrl+N";

    // Advanced settings
    defaults_[Keys::Advanced::LogLevel] = "Info";
    defaults_[Keys::Advanced::EnableLogging] = true;
    defaults_[Keys::Advanced::TempDirectory] = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    defaults_[Keys::Advanced::CacheSize] = 100; // MB
    defaults_[Keys::Advanced::ThreadPoolSize] = QThread::idealThreadCount();

    qDebug() << "Initialized" << defaults_.size() << "default settings";
}

void Settings::validateSettings()
{
    QMutexLocker locker(&mutex_);

    // Validate critical settings and reset to defaults if invalid

    // Audio settings validation
    QList<int> validSampleRates = { 22050, 44100, 48000, 88200, 96000, 192000 };
    int sampleRate = settings_->value(Keys::Audio::SampleRate, defaults_[Keys::Audio::SampleRate]).toInt();
    if (!validSampleRates.contains(sampleRate)) {
        settings_->setValue(Keys::Audio::SampleRate, defaults_[Keys::Audio::SampleRate]);
        qWarning() << "Invalid sample rate, reset to default";
    }

    QList<int> validBufferSizes = { 64, 128, 256, 512, 1024, 2048 };
    int bufferSize = settings_->value(Keys::Audio::BufferSize, defaults_[Keys::Audio::BufferSize]).toInt();
    if (!validBufferSizes.contains(bufferSize)) {
        settings_->setValue(Keys::Audio::BufferSize, defaults_[Keys::Audio::BufferSize]);
        qWarning() << "Invalid buffer size, reset to default";
    }

    // Volume validation (0.0 to 1.0)
    double masterVolume = settings_->value(Keys::Audio::MasterVolume, defaults_[Keys::Audio::MasterVolume]).toDouble();
    if (masterVolume < 0.0 || masterVolume > 1.0) {
        settings_->setValue(Keys::Audio::MasterVolume, defaults_[Keys::Audio::MasterVolume]);
        qWarning() << "Invalid master volume, reset to default";
    }

    // Validate theme setting
    QStringList validThemes = { "dark", "light", "auto" };
    QString theme = settings_->value(Keys::General::Theme, defaults_[Keys::General::Theme]).toString();
    if (!validThemes.contains(theme)) {
        settings_->setValue(Keys::General::Theme, defaults_[Keys::General::Theme]);
        qWarning() << "Invalid theme, reset to default";
    }

    // Validate zoom level
    double zoom = settings_->value(Keys::Window::Zoom, defaults_[Keys::Window::Zoom]).toDouble();
    if (zoom < 0.5 || zoom > 3.0) {
        settings_->setValue(Keys::Window::Zoom, defaults_[Keys::Window::Zoom]);
        qWarning() << "Invalid zoom level, reset to default";
    }

    // Validate directory paths
    QString tempDir = settings_->value(Keys::Advanced::TempDirectory, defaults_[Keys::Advanced::TempDirectory]).toString();
    if (!QDir(tempDir).exists()) {
        QString defaultTempDir = defaults_[Keys::Advanced::TempDirectory].toString();
        settings_->setValue(Keys::Advanced::TempDirectory, defaultTempDir);
        qWarning() << "Invalid temp directory, reset to default";
    }

    // Validate auto-save interval (1-60 minutes)
    int autoSaveInterval = settings_->value(Keys::General::AutoSaveInterval, defaults_[Keys::General::AutoSaveInterval]).toInt();
    if (autoSaveInterval < 1 || autoSaveInterval > 60) {
        settings_->setValue(Keys::General::AutoSaveInterval, defaults_[Keys::General::AutoSaveInterval]);
        qWarning() << "Invalid auto-save interval, reset to default";
    }

    // Validate network ports
    int oscPort = settings_->value(Keys::Network::OSCPort, defaults_[Keys::Network::OSCPort]).toInt();
    if (oscPort < 1024 || oscPort > 65535) {
        settings_->setValue(Keys::Network::OSCPort, defaults_[Keys::Network::OSCPort]);
        qWarning() << "Invalid OSC port, reset to default";
    }

    int tcpPort = settings_->value(Keys::Network::TCPPort, defaults_[Keys::Network::TCPPort]).toInt();
    if (tcpPort < 1024 || tcpPort > 65535) {
        settings_->setValue(Keys::Network::TCPPort, defaults_[Keys::Network::TCPPort]);
        qWarning() << "Invalid TCP port, reset to default";
    }

    settings_->sync();

    qDebug() << "Settings validation completed";
}

QString Settings::getFullKey(const QString& key) const
{
    if (groupStack_.isEmpty()) {
        return key;
    }

    return groupStack_.join("/") + "/" + key;
}

// Slot Implementations

void Settings::onSettingChanged(const QString& key, const QVariant& value)
{
    setValue(key, value);
}

// Constants for Settings Keys (implementation)
namespace Settings::Keys {
    namespace General {
        const QString Theme = "general/theme";
        const QString Language = "general/language";
        const QString AutoSave = "general/autoSave";
        const QString AutoSaveInterval = "general/autoSaveInterval";
        const QString LoadLastWorkspace = "general/loadLastWorkspace";
        const QString ConfirmDelete = "general/confirmDelete";
        const QString ShowSplashScreen = "general/showSplashScreen";
    }

    namespace Window {
        const QString Geometry = "window/geometry";
        const QString State = "window/state";
        const QString Maximized = "window/maximized";
        const QString FullScreen = "window/fullScreen";
        const QString InspectorVisible = "window/inspectorVisible";
        const QString TransportVisible = "window/transportVisible";
        const QString MatrixVisible = "window/matrixVisible";
        const QString Zoom = "window/zoom";
    }

    namespace Workspace {
        const QString LastOpened = "workspace/lastOpened";
        const QString RecentFiles = "workspace/recentFiles";
        const QString DefaultSaveLocation = "workspace/defaultSaveLocation";
        const QString AutoBackup = "workspace/autoBackup";
        const QString BackupInterval = "workspace/backupInterval";
        const QString MaxBackups = "workspace/maxBackups";
    }

    namespace Audio {
        const QString DeviceName = "audio/deviceName";
        const QString SampleRate = "audio/sampleRate";
        const QString BufferSize = "audio/bufferSize";
        const QString InputChannels = "audio/inputChannels";
        const QString OutputChannels = "audio/outputChannels";
        const QString MasterVolume = "audio/masterVolume";
        const QString EnableExclusive = "audio/enableExclusive";
    }

    namespace MIDI {
        const QString InputDevice = "midi/inputDevice";
        const QString OutputDevice = "midi/outputDevice";
        const QString EnableMTC = "midi/enableMTC";
        const QString EnableMMC = "midi/enableMMC";
        const QString MTCOffset = "midi/mtcOffset";
    }

    namespace Network {
        const QString OSCEnabled = "network/oscEnabled";
        const QString OSCPort = "network/oscPort";
        const QString ArtNetEnabled = "network/artnetEnabled";
        const QString ArtNetUniverse = "network/artnetUniverse";
        const QString TCPPort = "network/tcpPort";
    }

    namespace Shortcuts {
        const QString Go = "shortcuts/go";
        const QString Stop = "shortcuts/stop";
        const QString Pause = "shortcuts/pause";
        const QString Panic = "shortcuts/panic";
        const QString Save = "shortcuts/save";
        const QString Open = "shortcuts/open";
        const QString New = "shortcuts/new";
    }

    namespace Advanced {
        const QString LogLevel = "advanced/logLevel";
        const QString EnableLogging = "advanced/enableLogging";
        const QString LogFilePath = "advanced/logFilePath";
        const QString TempDirectory = "advanced/tempDirectory";
        const QString CacheSize = "advanced/cacheSize";
        const QString ThreadPoolSize = "advanced/threadPoolSize";
    }
}