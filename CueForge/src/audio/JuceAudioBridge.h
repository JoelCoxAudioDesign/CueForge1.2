// src/audio/JuceAudioBridge.h - JUCE Integration Bridge
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QMutex>
#include <memory>
#include <functional>

// Forward declare your existing JUCE classes to avoid header dependencies
class AudioEngine;      // Your existing JUCE AudioEngine from native/include/AudioEngine.h
class MatrixMixer;      // Your existing JUCE MatrixMixer
class OutputPatch;      // Your existing JUCE OutputPatch

/**
 * @brief Low-level bridge between Qt6 and your existing JUCE audio engine
 *
 * This class handles the direct integration with your existing JUCE classes,
 * providing string conversion, thread safety, and lifecycle management.
 * It isolates JUCE dependencies from the rest of the Qt6 application.
 */
class JuceAudioBridge : public QObject
{
    Q_OBJECT

public:
    explicit JuceAudioBridge(QObject* parent = nullptr);
    ~JuceAudioBridge();

    // Lifecycle management
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // Direct interface to your existing JUCE AudioEngine
    AudioEngine* getJuceEngine() const { return juceEngine_.get(); }

    // Device management (wrapping your JUCE AudioEngine methods)
    QStringList getAvailableDevices() const;
    QString getCurrentDevice() const;
    bool setAudioDevice(const QString& deviceName);

    // Audio cue operations (converting Qt strings to JUCE strings)
    bool createAudioCue(const QString& cueId, const QString& filePath);
    bool loadAudioFile(const QString& cueId, const QString& filePath);
    bool playCue(const QString& cueId, double startTime = 0.0, double fadeInTime = 0.0);
    bool stopCue(const QString& cueId, double fadeOutTime = 0.0);
    bool pauseCue(const QString& cueId);
    bool resumeCue(const QString& cueId);
    void stopAllCues();

    // Matrix routing (wrapping your JUCE MatrixMixer)
    bool setCrosspoint(const QString& cueId, int input, int output, float level);
    float getCrosspoint(const QString& cueId, int input, int output) const;
    bool setInputLevel(const QString& cueId, int input, float level);
    bool setOutputLevel(int output, float level);
    bool muteOutput(int output, bool mute);
    bool soloOutput(int output, bool solo);

    // Output patch routing (wrapping your JUCE OutputPatch)
    bool setPatchRouting(int cueOutput, int deviceOutput, float level);
    float getPatchRouting(int cueOutput, int deviceOutput) const;

    // Status information from your JUCE engine
    struct JuceStatus {
        bool isRunning = false;
        double sampleRate = 0.0;
        int bufferSize = 0;
        double cpuUsage = 0.0;
        int dropoutCount = 0;
        QString currentDevice;
        QString lastError;
    };
    JuceStatus getStatus() const;

    // Cue state queries
    bool isCuePlaying(const QString& cueId) const;
    double getCuePosition(const QString& cueId) const;
    double getCueDuration(const QString& cueId) const;

    // Performance monitoring
    double getCpuUsage() const;
    int getDropoutCount() const;
    void resetDropoutCount();

    // Thread-safe execution helpers
    void executeOnAudioThread(std::function<void()> callback);
    void executeOnMainThread(std::function<void()> callback);

public slots:
    void updateStatus();
    void handleJuceMessage();

signals:
    // Error and status signals
    void juceError(const QString& error);
    void juceWarning(const QString& warning);
    void statusUpdated();

    // Playback signals from JUCE engine
    void cueStarted(const QString& cueId);
    void cueFinished(const QString& cueId);
    void cuePaused(const QString& cueId);
    void cueResumed(const QString& cueId);
    void cueStopped(const QString& cueId);
    void cueError(const QString& cueId, const QString& error);

    // Position tracking
    void cuePositionChanged(const QString& cueId, double position);

    // Performance signals
    void cpuUsageChanged(double usage);
    void audioDropout();

private slots:
    void onStatusTimer();
    void processJuceCallbacks();

private:
    // JUCE initialization helpers
    bool initializeJuceFramework();
    bool createAudioEngine();
    bool setupAudioDevice();
    void shutdownJuceFramework();

    // String conversion helpers
    QString juceToQt(const class juce::String& juceString) const;
    class juce::String qtToJuce(const QString& qtString) const;

    // Error handling
    void handleJuceError(const QString& error);
    void reportError(const QString& context, const QString& error);

    // Callback setup for your JUCE engine
    void setupJuceCallbacks();
    void cleanupJuceCallbacks();

    // Thread synchronization
    void ensureAudioThread();
    void ensureMainThread();

    // Your existing JUCE components
    std::unique_ptr<AudioEngine> juceEngine_;    // Your main AudioEngine class

    // Status monitoring
    QTimer* statusTimer_;
    mutable QMutex statusMutex_;
    JuceStatus currentStatus_;

    // Error handling
    QString lastError_;
    QTimer* errorTimer_;

    // Thread management
    QMutex audioThreadMutex_;
    std::queue<std::function<void()>> audioThreadQueue_;
    std::queue<std::function<void()>> mainThreadQueue_;
    QTimer* callbackTimer_;

    // State tracking
    bool initialized_;
    bool shutdownInProgress_;

    // Performance monitoring
    double lastCpuUsage_;
    int lastDropoutCount_;

    // Constants
    static constexpr int STATUS_UPDATE_INTERVAL = 50;   // 50ms for responsive UI updates
    static constexpr int CALLBACK_PROCESS_INTERVAL = 16; // ~60 FPS for smooth updates

    // JUCE-specific helpers (implementation will include JUCE headers)
    class JuceCallbackHandler;
    std::unique_ptr<JuceCallbackHandler> callbackHandler_;
};