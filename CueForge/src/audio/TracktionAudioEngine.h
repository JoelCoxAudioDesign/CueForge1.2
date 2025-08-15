// src/audio/TracktionAudioEngine.h - Tracktion Engine Integration
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QMutex>
#include <QMap>
#include <memory>

// Conditional include for Tracktion Engine
#if CUEFORGE_USE_TRACKTION_ENGINE
#include <tracktion_engine/tracktion_engine.h>
namespace te = tracktion_engine;
#endif

/**
 * @brief Qt6 wrapper for Tracktion Engine providing professional audio capabilities
 *
 * This class integrates the powerful Tracktion Engine with our Qt6 application,
 * providing professional DAW-quality audio processing, matrix mixing, and
 * multi-format audio file support.
 */
class TracktionAudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit TracktionAudioEngine(QObject* parent = nullptr);
    ~TracktionAudioEngine() override;

    // Engine lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // Device management
    QStringList getAvailableDevices() const;
    QString getCurrentDevice() const;
    bool setAudioDevice(const QString& deviceName);

    // Audio device properties
    QList<int> getAvailableSampleRates() const;
    QList<int> getAvailableBufferSizes() const;
    int getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    bool setSampleRate(int sampleRate);
    bool setBufferSize(int bufferSize);

    // Cue management
    bool createAudioCue(const QString& cueId, const QString& filePath);
    bool loadAudioFile(const QString& cueId, const QString& filePath);
    bool removeAudioCue(const QString& cueId);

    // Playback control
    bool playCue(const QString& cueId, double startTime = 0.0, double fadeInTime = 0.0);
    bool stopCue(const QString& cueId, double fadeOutTime = 0.0);
    bool pauseCue(const QString& cueId);
    bool resumeCue(const QString& cueId);
    void stopAllCues();
    void emergencyStop();

    // Matrix mixer integration
    struct MatrixRoute {
        int inputChannel = -1;
        int outputChannel = -1;
        double level = 1.0;
        bool muted = false;
        bool soloed = false;
    };

    bool setMatrixRouting(const QString& cueId, const QList<MatrixRoute>& routes);
    QList<MatrixRoute> getMatrixRouting(const QString& cueId) const;
    bool setCrosspoint(const QString& cueId, int input, int output, double level);
    double getCrosspoint(const QString& cueId, int input, int output) const;

    // Level control
    bool setInputLevel(const QString& cueId, int input, double level);
    bool setOutputLevel(int output, double level);
    bool muteOutput(int output, bool mute);
    bool soloOutput(int output, bool solo);

signals:
    // Engine status
    void initialized();
    void shutdownComplete();
    void statusChanged();
    void errorOccurred(const QString& error);
    void warningOccurred(const QString& warning);

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

    // Position tracking
    void cuePositionChanged(const QString& cueId, double position);
    void cueTimeRemaining(const QString& cueId, double timeRemaining);

    // Performance monitoring
    void cpuUsageChanged(double usage);
    void audioDropout();
    void bufferUnderrun();

private slots:
    void onStatusTimer();
    void onPositionTimer();
    void processTracktionCallbacks();

private:
    // Initialization helpers
    bool initializeTracktionEngine();
    bool setupAudioDevice();
    void setupCallbacks();
    void cleanupTracktionEngine();

    // Cue management helpers
    void registerCueWithTracktion(const QString& cueId);
    void unregisterCueFromTracktion(const QString& cueId);

    // String conversion helpers
    QString tracktionToQt(const std::string& str) const;
    std::string qtToTracktion(const QString& str) const;

    // Error handling
    void handleTracktionError(const QString& error);
    void reportError(const QString& context, const QString& error);

#if CUEFORGE_USE_TRACKTION_ENGINE
    // Tracktion Engine components
    std::unique_ptr<te::Engine> tracktionEngine_;
    std::unique_ptr<te::Project> currentProject_;
    std::unique_ptr<te::Edit> currentEdit_;

    // Audio device management
    te::DeviceManager* deviceManager_;
    te::WaveAudioDevice* audioDevice_;

    // Cue tracking
    struct TracktionCue {
        QString cueId;
        std::unique_ptr<te::AudioTrack> track;
        std::unique_ptr<te::WaveAudioClip> clip;
        te::TransportControl* transport = nullptr;
        bool isPlaying = false;
        bool isPaused = false;
        double startTime = 0.0;
        double duration = 0.0;
    };

    QMap<QString, std::unique_ptr<TracktionCue>> tracktionCues_;
    QMutex cuesMutex_;

    // Matrix mixer (using Tracktion's sends/returns)
    struct MatrixConnection {
        te::Send* send = nullptr;
        double level = 1.0;
        bool muted = false;
        bool soloed = false;
    };

    QMap<QString, QMap<int, QMap<int, MatrixConnection>>> matrixConnections_;
    QMutex matrixMutex_;

#else
    // Placeholder structures when Tracktion Engine is not available
    struct DummyCue {
        QString cueId;
        QString filePath;
        bool isPlaying = false;
        bool isPaused = false;
        double position = 0.0;
        double duration = 0.0;
    };

    QMap<QString, DummyCue> dummyCues_;
#endif

    // Status monitoring
    QTimer* statusTimer_;
    QTimer* positionTimer_;
    mutable QMutex statusMutex_;
    EngineStatus currentStatus_;

    // Performance tracking
    double lastCpuUsage_;
    int lastDropoutCount_;
    QStringList availableDevices_;
    QString currentDeviceName_;

    // State flags
    bool initialized_;
    bool shutdownInProgress_;
    QString lastError_;

    // Constants
    static constexpr int STATUS_UPDATE_INTERVAL = 100;    // 100ms
    static constexpr int POSITION_UPDATE_INTERVAL = 50;   // 50ms for smooth position updates
    static constexpr double FADE_STEP_SIZE = 0.001;       // Fade resolution
    static constexpr int MAX_MATRIX_INPUTS = 64;          // Maximum matrix inputs
    static constexpr int MAX_MATRIX_OUTPUTS = 64;         // Maximum matrix outputs
};