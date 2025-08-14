// src/core/AudioCue.h - Audio Cue Class
#pragma once

#include "Cue.h"
#include <QUrl>
#include <QFileInfo>
#include <QVariantMap>

// Forward declaration for JUCE integration
class AudioEngineManager;

/**
 * @brief Audio cue class for audio file playback
 *
 * This class handles audio file playback with matrix mixer integration,
 * fade controls, and professional audio features. It interfaces with
 * the JUCE-based audio engine for high-quality, low-latency playback.
 */
class AudioCue : public Cue
{
    Q_OBJECT

        // Audio-specific properties
        Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
        Q_PROPERTY(bool isFileLoaded READ isFileLoaded NOTIFY fileLoadStateChanged)
        Q_PROPERTY(double startTime READ startTime WRITE setStartTime NOTIFY startTimeChanged)
        Q_PROPERTY(double fadeInTime READ fadeInTime WRITE setFadeInTime NOTIFY fadeInTimeChanged)
        Q_PROPERTY(double fadeOutTime READ fadeOutTime WRITE setFadeOutTime NOTIFY fadeOutTimeChanged)
        Q_PROPERTY(int sliceMarker READ sliceMarker WRITE setSliceMarker NOTIFY sliceMarkerChanged)
        Q_PROPERTY(bool loop READ isLooping WRITE setLooping NOTIFY loopingChanged)
        Q_PROPERTY(double playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
        Q_PROPERTY(QVariantMap matrixRouting READ matrixRouting WRITE setMatrixRouting NOTIFY matrixRoutingChanged)
        Q_PROPERTY(QVariantMap levels READ levels WRITE setLevels NOTIFY levelsChanged)

public:
    explicit AudioCue(QObject* parent = nullptr);
    ~AudioCue() override;

    // File management
    QString filePath() const { return filePath_; }
    void setFilePath(const QString& filePath);
    QFileInfo fileInfo() const { return QFileInfo(filePath_); }
    QString fileName() const { return fileInfo().fileName(); }
    bool isFileLoaded() const { return fileLoaded_; }

    // Playback properties
    double startTime() const { return startTime_; }
    void setStartTime(double startTime);

    double fadeInTime() const { return fadeInTime_; }
    void setFadeInTime(double fadeInTime);

    double fadeOutTime() const { return fadeOutTime_; }
    void setFadeOutTime(double fadeOutTime);

    int sliceMarker() const { return sliceMarker_; }
    void setSliceMarker(int marker);

    bool isLooping() const { return looping_; }
    void setLooping(bool looping);

    double playbackSpeed() const { return playbackSpeed_; }
    void setPlaybackSpeed(double speed);

    // Audio file information (available after loading)
    int numChannels() const { return numChannels_; }
    double sampleRate() const { return sampleRate_; }
    double fileDuration() const { return fileDuration_; }
    QString audioFormat() const { return audioFormat_; }
    qint64 fileSizeBytes() const { return fileSizeBytes_; }

    // Matrix routing and levels
    QVariantMap matrixRouting() const { return matrixRouting_; }
    void setMatrixRouting(const QVariantMap& routing);

    QVariantMap levels() const { return levels_; }
    void setLevels(const QVariantMap& levels);

    // Individual channel routing
    void setChannelRouting(int inputChannel, int outputChannel, double level = 1.0);
    void clearChannelRouting(int inputChannel);
    double getChannelLevel(int inputChannel, int outputChannel) const;

    // Ganging (group control)
    void setGang(const QString& gangId);
    QString currentGang() const { return gangId_; }
    void clearGang();

    // Volume and effects
    double mainLevel() const { return mainLevel_; }
    void setMainLevel(double level);

    bool isMuted() const { return muted_; }
    void setMuted(bool muted);

    bool isSoloed() const { return soloed_; }
    void setSoloed(bool soloed);

    // Cue interface implementation
    bool prepare() override;
    void execute() override;
    void stop(double fadeTime = 0.0) override;
    void pause() override;
    void resume() override;
    void reset() override;

    // Audio-specific execution
    bool loadFile();
    void unloadFile();

    // Progress and timing
    double getCurrentPlaybackTime() const;
    double getRemainingTime() const;
    double getProgress() const override;

    // Serialization
    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& json) override;

    // Validation
    bool isValid() const;
    QString getValidationError() const;

    // Static helpers for file format support
    static QStringList getSupportedFormats();
    static bool isFormatSupported(const QString& filePath);
    static QString getFormatDescription(const QString& extension);

public slots:
    void onAudioEngineStatusChanged();
    void onFileLoadCompleted(bool success);
    void onPlaybackPositionChanged(double position);

signals:
    // Audio-specific signals
    void filePathChanged();
    void fileLoadStateChanged();
    void startTimeChanged();
    void fadeInTimeChanged();
    void fadeOutTimeChanged();
    void sliceMarkerChanged();
    void loopingChanged();
    void playbackSpeedChanged();
    void matrixRoutingChanged();
    void levelsChanged();
    void mainLevelChanged();
    void mutedChanged();
    void soloedChanged();
    void gangChanged();

    // File loading signals
    void fileLoadStarted();
    void fileLoadProgress(int percentage);
    void fileLoadCompleted(bool success, const QString& error = QString());

    // Playback position signals
    void playbackPositionChanged(double seconds);
    void playbackTimeRemaining(double seconds);

protected:
    void executeImpl() override;
    void cleanupExecution() override;

private slots:
    void updatePlaybackPosition();
    void onFadeCompleted();

private:
    void connectToAudioEngine();
    void disconnectFromAudioEngine();
    void updateFileInfo();
    void applyMatrixRouting();
    void validateAudioFile();
    void setupDefaultRouting();

    // Audio file properties
    QString filePath_;
    bool fileLoaded_;
    int numChannels_;
    double sampleRate_;
    double fileDuration_;
    QString audioFormat_;
    qint64 fileSizeBytes_;
    QString validationError_;

    // Playback properties
    double startTime_;           // Start position in file (seconds)
    double fadeInTime_;          // Fade in duration (seconds)
    double fadeOutTime_;         // Fade out duration (seconds)
    int sliceMarker_;           // Slice marker position (samples)
    bool looping_;              // Loop playback
    double playbackSpeed_;      // Playback speed multiplier

    // Current playback state
    double currentPlaybackTime_;
    bool currentlyFading_;

    // Matrix and routing
    QVariantMap matrixRouting_;  // Input channel -> Output channel mappings
    QVariantMap levels_;         // Per-crosspoint levels
    QString gangId_;             // Gang identifier for grouped control

    // Volume control
    double mainLevel_;           // Main output level (0.0 to 1.0)
    bool muted_;                // Mute state
    bool soloed_;               // Solo state

    // Audio engine integration
    AudioEngineManager* audioEngine_;
    QString engineCueId_;        // ID used by audio engine

    // Update timer for position tracking
    QTimer* positionTimer_;

    // Constants
    static constexpr double MIN_FADE_TIME = 0.001;     // Minimum fade time (1ms)
    static constexpr double MAX_FADE_TIME = 60.0;      // Maximum fade time (60s)
    static constexpr double MIN_PLAYBACK_SPEED = 0.1;  // Minimum playback speed
    static constexpr double MAX_PLAYBACK_SPEED = 4.0;  // Maximum playback speed
    static constexpr int POSITION_UPDATE_INTERVAL = 50; // Position update interval (ms)
};