// src/utils/Settings.h - Application Settings Manager
#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QMutex>

/**
 * @brief Centralized settings management for CueForge
 *
 * This class provides a unified interface for accessing and storing application
 * settings, with proper defaults and validation. It wraps QSettings with
 * CueForge-specific functionality.
 */
class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(QObject* parent = nullptr);
    ~Settings();

    // Core settings interface
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);
    void remove(const QString& key);
    bool contains(const QString& key) const;

    // Convenience accessors for common settings
    QString getString(const QString& key, const QString& defaultValue = QString()) const;
    int getInt(const QString& key, int defaultValue = 0) const;
    double getDouble(const QString& key, double defaultValue = 0.0) const;
    bool getBool(const QString& key, bool defaultValue = false) const;
    QStringList getStringList(const QString& key, const QStringList& defaultValue = QStringList()) const;

    // Convenience setters
    void setString(const QString& key, const QString& value);
    void setInt(const QString& key, int value);
    void setDouble(const QString& key, double value);
    void setBool(const QString& key, bool value);
    void setStringList(const QString& key, const QStringList& value);

    // Settings groups for organization
    void beginGroup(const QString& prefix);
    void endGroup();
    QStringList childGroups() const;
    QStringList childKeys() const;

    // Batch operations
    void sync();
    void clear();

    // Default values management
    void setDefaultValue(const QString& key, const QVariant& value);
    QVariant getDefaultValue(const QString& key) const;
    void resetToDefaults();
    void resetKeyToDefault(const QString& key);

    // Settings validation
    bool isValidKey(const QString& key) const;
    bool isValidValue(const QString& key, const QVariant& value) const;

    // Import/Export
    bool exportSettings(const QString& filePath) const;
    bool importSettings(const QString& filePath);

    // Common settings keys (for type safety and autocompletion)
    namespace Keys {
        // General application settings
        namespace General {
            static const QString Theme = "general/theme";
            static const QString Language = "general/language";
            static const QString AutoSave = "general/autoSave";
            static const QString AutoSaveInterval = "general/autoSaveInterval";
            static const QString LoadLastWorkspace = "general/loadLastWorkspace";
            static const QString ConfirmDelete = "general/confirmDelete";
            static const QString ShowSplashScreen = "general/showSplashScreen";
        }

        // Window and UI settings
        namespace Window {
            static const QString Geometry = "window/geometry";
            static const QString State = "window/state";
            static const QString Maximized = "window/maximized";
            static const QString FullScreen = "window/fullScreen";
            static const QString InspectorVisible = "window/inspectorVisible";
            static const QString TransportVisible = "window/transportVisible";
            static const QString MatrixVisible = "window/matrixVisible";
            static const QString Zoom = "window/zoom";
        }

        // Workspace settings
        namespace Workspace {
            static const QString LastOpened = "workspace/lastOpened";
            static const QString RecentFiles = "workspace/recentFiles";
            static const QString DefaultSaveLocation = "workspace/defaultSaveLocation";
            static const QString AutoBackup = "workspace/autoBackup";
            static const QString BackupInterval = "workspace/backupInterval";
            static const QString MaxBackups = "workspace/maxBackups";
        }

        // Audio engine settings
        namespace Audio {
            static const QString DeviceName = "audio/deviceName";
            static const QString SampleRate = "audio/sampleRate";
            static const QString BufferSize = "audio/bufferSize";
            static const QString InputChannels = "audio/inputChannels";
            static const QString OutputChannels = "audio/outputChannels";
            static const QString MasterVolume = "audio/masterVolume";
            static const QString EnableExclusive = "audio/enableExclusive";
        }

        // MIDI settings
        namespace MIDI {
            static const QString InputDevice = "midi/inputDevice";
            static const QString OutputDevice = "midi/outputDevice";
            static const QString EnableMTC = "midi/enableMTC";
            static const QString EnableMMC = "midi/enableMMC";
            static const QString MTCOffset = "midi/mtcOffset";
        }

        // Network settings
        namespace Network {
            static const QString OSCEnabled = "network/oscEnabled";
            static const QString OSCPort = "network/oscPort";
            static const QString ArtNetEnabled = "network/artnetEnabled";
            static const QString ArtNetUniverse = "network/artnetUniverse";
            static const QString TCPPort = "network/tcpPort";
        }

        // Keyboard shortcuts
        namespace Shortcuts {
            static const QString Go = "shortcuts/go";
            static const QString Stop = "shortcuts/stop";
            static const QString Pause = "shortcuts/pause";
            static const QString Panic = "shortcuts/panic";
            static const QString Save = "shortcuts/save";
            static const QString Open = "shortcuts/open";
            static const QString New = "shortcuts/new";
        }

        // Advanced settings
        namespace Advanced {
            static const QString LogLevel = "advanced/logLevel";
            static const QString EnableLogging = "advanced/enableLogging";
            static const QString LogFilePath = "advanced/logFilePath";
            static const QString TempDirectory = "advanced/tempDirectory";
            static const QString CacheSize = "advanced/cacheSize";
            static const QString ThreadPoolSize = "advanced/threadPoolSize";
        }
    }

public slots:
    void onSettingChanged(const QString& key, const QVariant& value);

signals:
    void settingChanged(const QString& key, const QVariant& oldValue, const QVariant& newValue);
    void settingsReset();
    void settingsImported();
    void settingsExported();

private:
    void initializeDefaults();
    void validateSettings();
    QString getFullKey(const QString& key) const;

    QSettings* settings_;
    QMap<QString, QVariant> defaults_;
    mutable QMutex mutex_;

    // Current group stack for nested groups
    QStringList groupStack_;
};