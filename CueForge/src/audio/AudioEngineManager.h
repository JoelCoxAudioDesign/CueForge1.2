// src/audio/AudioEngineManager.h - Qt6 Audio Engine Manager
#pragma once

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <QString>
#include <QStringList>
#include <memory>

// Forward declarations to avoid including JUCE headers in Qt code
class AudioEngine;  // Your existing JUCE AudioEngine class
class JuceAudioBridge;

// Forward declarations for Qt6 classes
class CueManager;
class AudioCue;

/**
 * @brief Qt6 wrapper and manager for the JUCE-based audio engine
 *
 * This class provides a Qt-native interface to the existing JUCE audio engine,
 * handling thread safety, signal conversion, and lifecycle management. It serves
 * as the bridge between the Qt6 cue management system and the professional
 * JUCE audio backend.
 */
class AudioEngineManager : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngineManager(CueManager* cueManager, QObject* parent = nullptr);
    ~AudioEngineManager();

    // Lifecycle management
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // Device management
    QStringList getAvailableDevices() const;
    QString getCurrentDevice() const;
    bool setAudioDevice(const QString& deviceName);

    // Audio settings
    QList<int> getAvailableSampleRates() const;
    QList<int> getAvailableBufferSizes() const;
    int getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    bool setSampleRate(int sampleRate);
    bool setBufferSize(int bufferSize);

    // Cue management integration with your existing JUCE engine
    bool registerAudioCue(AudioCue* cue);
    bool unregisterAudioCue(const QString& cueId);
    bool loadAudioFile(const QString& cueId, const QString& filePath);

    // Playback control (delegates to JUCE AudioEngine)
    bool playCue(const QString& cueId, double startTime = 0.0, double fadeInTime = 0.0);
    bool stopCue(const QString& cueId, double fadeOutTime = 0.0);
    bool pauseCue(const QString& cueId);
    bool resumeCue(const QString& cueId);
    void stopAllCues();
    void emergencyStop();

    // Matrix routing (interfaces with your JUCE MatrixMixer)
    bool setCrosspoint(const QString& cueId, int input, int output, float level);
    float getCrosspoint(const QString& cueId, int input, int output) const;
    bool setInputLevel(const QString& cueId, int input, float level);
    bool setOutputLevel(int output, float level);
    bool muteOutput(int output, bool mute);
    bool soloOutput(int output, bool solo);

    // Output patch routing (interfaces with your JUCE OutputPatch)
    bool setPatchRouting(int cueOutput, int deviceOutput, float level);
    float getPatchRouting(int cueOutput, int deviceOutput) const;

    // Status and monitoring
    struct EngineStatus {
        bool isRunning = false;
        double sampleRate = 0.0;
        int bufferSize = 0;
        double cpuUsage = 0.0;
        int dropoutCount = 0;
        QString currentDevice;
        int activeCues = 0;
        QString lastError;
    };

    EngineStatus getStatus() const;
    void updateStatus();

    // Performance monitoring
    double getCpuUsage() const;
    int getDropoutCount() const;
    void resetDropoutCount();

    // Cue state tracking
    bool isCuePlaying(const QString& cueId) const;
    double getCuePosition(const QString& cueId) const;
    double getCueDuration(const QString& cueId) const;

public slots:
    // CueManager integration slots
    void onCueAdded(class Cue* cue);
    void onCueRemoved(const QString& cueId);
    void onCueUpdated(class Cue* cue);

    // Audio cue specific slots
    void onAudioCueFileChanged(const QString& cueId, const QString& newFilePath);
    void onAudioCueMatrixChanged(const QString& cueId);
    void onAudioCueLevelsChanged(const QString& cueId);

    // Device management slots
    void refreshAudioDevices();
    void handleDeviceChange();

signals:
    // Engine status signals
    void initialized();
    void shutdownComplete();
    void statusChanged();
    void criticalError(const QString& error);
    void warningMessage(const QString& warning);

    // Device signals
    void audioDeviceChanged(const QString& deviceName);
    void audioDeviceError(const QString& error);
    void availableDevicesChanged();

    // Playback signals
    void cueStarted(const QString& cueId);
    void cueFinished(const QString& cueId);
    void cuePaused(const QString& cueId);
    void cueResumed(const QString& cueId);
    void cueStopped(const QString& cueId);
    void cueError(const QString& cueId, const QString& error);

    // Position tracking signals
    void cuePositionChanged(const QString& cueId, double position);
    void cueTimeRemaining(const QString& cueId, double timeRemaining);

    // Performance signals
    void cpuUsageChanged(double usage);
    void audioDropout();
    void bufferUnderrun();

private slots:
    void onStatusTimer();
    void onAudioEngineCallback();
    void handleJuceError(const QString& error);

private:
    // Initialization helpers
    bool initializeJuceEngine();
    bool setupAudioDevice();
    void setupCallbacks();
    void loadAudioSettings();
    void saveAudioSettings();

    // Cue registration helpers
    void registerCueWithJuce(AudioCue* cue);
    void unregisterCueFromJuce(const QString& cueId);
    void updateCueInJuce(AudioCue* cue);

    // Status monitoring helpers
    void updateEngineStatus();
    void checkForErrors();
    void monitorPerformance();

    // Thread safety helpers
    void executeOnAudioThread(std::function<void()> callback);
    void executeOnMainThread(std::function<void()> callback);

    // Error handling
    void handleEngineError(const QString& error);
    void reportCriticalError(const QString& error);

    // Core components
    CueManager* cueManager_;
    std::unique_ptr<JuceAudioBridge> juceBridge_;

    // Status monitoring
    QTimer* statusTimer_;
    mutable QMutex statusMutex_;
    EngineStatus currentStatus_;
    EngineStatus lastStatus_;

    // Cue tracking
    QHash<QString, AudioCue*> registeredCues_;
    QHash<QString, QString> cueIdToJuceId_;  // Qt cue ID -> JUCE engine ID mapping
    mutable QMutex cueRegistryMutex_;

    // Device management
    QStringList availableDevices_;
    QString currentDevice_;
    QTimer* deviceRefreshTimer_;

    // Performance monitoring
    double lastCpuUsage_;
    int lastDropoutCount_;
    QTimer* performanceTimer_;

    // Settings
    QString settingsGroup_;

    // State flags
    bool initialized_;
    bool shutdownRequested_;
    bool emergencyStopActive_;

    // Constants
    static constexpr int STATUS_UPDATE_INTERVAL = 100;      // 100ms status updates
    static constexpr int PERFORMANCE_UPDATE_INTERVAL = 250; // 250ms performance updates
    static constexpr int DEVICE_REFRESH_INTERVAL = 5000;    // 5s device refresh
    static constexpr double CPU_WARNING_THRESHOLD = 80.0;   // 80% CPU warning
    static constexpr double CPU_CRITICAL_THRESHOLD = 95.0;  // 95% CPU critical
};