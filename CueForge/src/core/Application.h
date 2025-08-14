// src/core/Application.h - Core CueForge Application Class
#pragma once

#include <QObject>
#include <QApplication>
#include <QTimer>
#include <QSettings>
#include <memory>

// Forward declarations
class CueManager;
class MainWindow;
class AudioEngineManager;
class Settings;

/**
 * @brief The main CueForge application class that orchestrates all core components
 *
 * This class manages the application lifecycle, coordinates between the UI and
 * core systems, and handles global application state. It serves as the central
 * hub that connects the CueManager, AudioEngine, and UI components.
 */
class CueForgeApplication : public QObject
{
    Q_OBJECT

public:
    explicit CueForgeApplication(QObject* parent = nullptr);
    ~CueForgeApplication();

    /**
     * @brief Initialize the application
     * @return true if initialization succeeds, false otherwise
     */
    bool initialize();

    /**
     * @brief Run the application event loop
     * @return Application exit code
     */
    int exec();

    /**
     * @brief Shutdown the application gracefully
     */
    void shutdown();

    // Global access to core components
    CueManager* cueManager() const { return cueManager_.get(); }
    AudioEngineManager* audioEngine() const { return audioEngine_.get(); }
    MainWindow* mainWindow() const { return mainWindow_.get(); }

    // Application state
    bool isInitialized() const { return initialized_; }
    bool isShuttingDown() const { return shuttingDown_; }

    // Global settings access
    QVariant getSetting(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setSetting(const QString& key, const QVariant& value);

public slots:
    /**
     * @brief Handle application quit request (can be cancelled)
     */
    void requestQuit();

    /**
     * @brief Force application quit (cannot be cancelled)
     */
    void forceQuit();

    /**
     * @brief Show the preferences dialog
     */
    void showPreferences();

    /**
     * @brief Show the about dialog
     */
    void showAbout();

    /**
     * @brief Handle emergency panic stop
     */
    void emergencyStop();

signals:
    /**
     * @brief Emitted when the application is about to quit
     * @param canCancel Whether the quit can be cancelled
     */
    void aboutToQuit(bool canCancel);

    /**
     * @brief Emitted when application settings change
     * @param key The setting key that changed
     * @param value The new value
     */
    void settingChanged(const QString& key, const QVariant& value);

    /**
     * @brief Emitted when the application encounters a critical error
     * @param message Error message
     */
    void criticalError(const QString& message);

private slots:
    /**
     * @brief Handle periodic application updates
     */
    void onUpdateTimer();

    /**
     * @brief Handle auto-save timer
     */
    void onAutoSaveTimer();

    /**
     * @brief Handle window close event from main window
     */
    void onMainWindowCloseRequested();

    /**
     * @brief Handle audio engine errors
     */
    void onAudioEngineError(const QString& error);

    /**
     * @brief Handle workspace changes for auto-save
     */
    void onWorkspaceChanged();

private:
    /**
     * @brief Initialize the audio engine
     * @return true if successful
     */
    bool initializeAudio();

    /**
     * @brief Initialize the user interface
     * @return true if successful
     */
    bool initializeUI();

    /**
     * @brief Load application settings
     */
    void loadSettings();

    /**
     * @brief Save application settings
     */
    void saveSettings();

    /**
     * @brief Setup application timers
     */
    void setupTimers();

    /**
     * @brief Connect signals between components
     */
    void connectSignals();

    /**
     * @brief Check if there are unsaved changes
     * @return true if there are unsaved changes
     */
    bool hasUnsavedChanges() const;

    /**
     * @brief Prompt user to save unsaved changes
     * @return true if user chose to continue (saved or discarded), false to cancel
     */
    bool promptSaveChanges();

    /**
     * @brief Perform application cleanup
     */
    void cleanup();

    // Core components (ordered by initialization dependency)
    std::unique_ptr<Settings> settings_;
    std::unique_ptr<CueManager> cueManager_;
    std::unique_ptr<AudioEngineManager> audioEngine_;
    std::unique_ptr<MainWindow> mainWindow_;

    // Application state
    bool initialized_ = false;
    bool shuttingDown_ = false;

    // Timers
    QTimer* updateTimer_;          // General application updates
    QTimer* autoSaveTimer_;        // Automatic workspace saving

    // Settings cache for frequently accessed values
    mutable QSettings* qtSettings_;
    bool autoSaveEnabled_;
    int autoSaveInterval_;        // in minutes
    QString lastWorkspacePath_;

    // Error handling
    QString lastError_;

    // Constants
    static constexpr int UPDATE_INTERVAL_MS = 50;     // 20 FPS for UI updates
    static constexpr int DEFAULT_AUTOSAVE_MINUTES = 5;
};