// src/core/Cue.h - Base Cue Class for CueForge
#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <QDateTime>
#include <QVariantMap>
#include <QJsonObject>
#include <QColor>
#include <QTimer>

/**
 * @brief Enumeration of all supported cue types in CueForge
 *
 * These match the cue types from the original Electron implementation
 */
enum class CueType {
    Audio,      // Audio playback cue
    Video,      // Video playback cue  
    MIDI,       // MIDI output cue
    Wait,       // Time-based wait cue
    Start,      // Start target cue
    Stop,       // Stop target cue
    Goto,       // Jump to target cue
    Fade,       // Fade parameters cue
    Group,      // Group container cue
    Target,     // Abstract target reference
    Load,       // Load/prepare cue
    Script      // Custom script cue
};

/**
 * @brief Enumeration of cue execution states
 *
 * Matches the status system from the original implementation
 */
enum class CueStatus {
    Loaded,     // Cue is ready to execute
    Playing,    // Cue is currently executing
    Paused,     // Cue is paused mid-execution
    Stopped,    // Cue has finished or been stopped
    Loading,    // Cue is preparing to execute
    Broken,     // Cue has an error and cannot execute
    Armed       // Cue is prepared and waiting for trigger
};

/**
 * @brief Base class for all cue types in CueForge
 *
 * This class provides the common interface and properties that all cue types share.
 * It closely mirrors the JavaScript cue structure from the original Electron version.
 */
class Cue : public QObject
{
    Q_OBJECT

        // Properties that match the original JS implementation
        Q_PROPERTY(QString id READ id CONSTANT)
        Q_PROPERTY(QString number READ number WRITE setNumber NOTIFY numberChanged)
        Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
        Q_PROPERTY(CueType type READ type CONSTANT)
        Q_PROPERTY(CueStatus status READ status WRITE setStatus NOTIFY statusChanged)
        Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
        Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY notesChanged)
        Q_PROPERTY(bool armed READ isArmed WRITE setArmed NOTIFY armedChanged)
        Q_PROPERTY(bool flagged READ isFlagged WRITE setFlagged NOTIFY flaggedChanged)
        Q_PROPERTY(bool continueMode READ continueMode WRITE setContinueMode NOTIFY continueModeChanged)
        Q_PROPERTY(double duration READ duration WRITE setDuration NOTIFY durationChanged)
        Q_PROPERTY(double preWait READ preWait WRITE setPreWait NOTIFY preWaitChanged)
        Q_PROPERTY(double postWait READ postWait WRITE setPostWait NOTIFY postWaitChanged)

public:
    explicit Cue(CueType type, QObject* parent = nullptr);
    virtual ~Cue() = default;

    // Core identification properties
    QString id() const { return id_; }
    QString number() const { return number_; }
    QString name() const { return name_; }
    CueType type() const { return type_; }

    // Status and state
    CueStatus status() const { return status_; }
    bool isArmed() const { return armed_; }
    bool isFlagged() const { return flagged_; }
    bool continueMode() const { return continueMode_; }

    // Visual properties
    QColor color() const { return color_; }
    QString notes() const { return notes_; }

    // Timing properties
    double duration() const { return duration_; }
    double preWait() const { return preWait_; }
    double postWait() const { return postWait_; }

    // Execution state tracking
    QDateTime createdTime() const { return createdTime_; }
    QDateTime modifiedTime() const { return modifiedTime_; }
    QDateTime lastExecutedTime() const { return lastExecutedTime_; }
    double currentPosition() const { return currentPosition_; }
    bool isExecuting() const { return status_ == CueStatus::Playing || status_ == CueStatus::Loading; }
    bool canExecute() const;

    // Target system (for control cues)
    QString targetId() const { return targetId_; }
    void setTargetId(const QString& targetId);

    // Custom properties system (extensible like JS version)
    QVariant getCustomProperty(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setCustomProperty(const QString& key, const QVariant& value);
    QVariantMap getAllCustomProperties() const { return customProperties_; }

    // Serialization (matching JS workspace format)
    virtual QJsonObject toJson() const;
    virtual bool fromJson(const QJsonObject& json);

    // Display helpers
    QString displayName() const;
    QString statusString() const;
    QString typeString() const;
    static QString typeToString(CueType type);
    static CueType stringToType(const QString& typeStr);

    // Execution interface (pure virtual - implemented by subclasses)
    virtual bool prepare() = 0;
    virtual void execute() = 0;
    virtual void stop(double fadeTime = 0.0) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void reset();

    // Progress tracking
    virtual double getProgress() const { return currentPosition_; }
    virtual void setProgress(double progress);

public slots:
    // Property setters (emit signals)
    void setNumber(const QString& number);
    void setName(const QString& name);
    void setStatus(CueStatus status);
    void setArmed(bool armed);
    void setFlagged(bool flagged);
    void setContinueMode(bool continueMode);
    void setColor(const QColor& color);
    void setNotes(const QString& notes);
    void setDuration(double duration);
    void setPreWait(double preWait);
    void setPostWait(double postWait);

    // Execution control
    void trigger();         // Public interface for execution
    void stopCue();         // Public interface for stopping
    void pauseCue();        // Public interface for pausing
    void resumeCue();       // Public interface for resuming

signals:
    // Property change signals
    void numberChanged();
    void nameChanged();
    void statusChanged();
    void armedChanged();
    void flaggedChanged();
    void continueModeChanged();
    void colorChanged();
    void notesChanged();
    void durationChanged();
    void preWaitChanged();
    void postWaitChanged();
    void targetChanged();
    void customPropertyChanged(const QString& key, const QVariant& value);

    // Execution signals
    void aboutToExecute();
    void executionStarted();
    void executionFinished();
    void executionPaused();
    void executionResumed();
    void executionStopped();
    void executionFailed(const QString& error);
    void progressChanged(double progress);

    // General state signals
    void cueUpdated();      // Emitted when any property changes

protected:
    /**
     * @brief Update the modified timestamp
     */
    void markModified();

    /**
     * @brief Set current execution position
     * @param position Position as ratio (0.0 to 1.0)
     */
    void setCurrentPosition(double position);

    /**
     * @brief Handle pre-wait timing
     */
    virtual void executePreWait();

    /**
     * @brief Handle post-wait timing
     */
    virtual void executePostWait();

    /**
     * @brief The actual execution implementation (called after pre-wait)
     */
    virtual void executeImpl() = 0;

    /**
     * @brief Cleanup after execution completes
     */
    virtual void cleanupExecution();

private slots:
    void onPreWaitFinished();
    void onPostWaitFinished();

private:
    // Core properties
    const QString id_;              // Unique identifier (UUID)
    const CueType type_;            // Cue type (immutable)
    QString number_;                // User-visible cue number
    QString name_;                  // Display name
    CueStatus status_;              // Current execution status

    // State flags
    bool armed_;                    // Armed for execution
    bool flagged_;                  // Flagged for attention
    bool continueMode_;             // Auto-continue to next cue

    // Visual properties
    QColor color_;                  // Display color
    QString notes_;                 // User notes

    // Timing properties
    double duration_;               // Expected duration in seconds
    double preWait_;                // Pre-execution wait in seconds
    double postWait_;               // Post-execution wait in seconds
    double currentPosition_;        // Current position (0.0 to 1.0)

    // Target system
    QString targetId_;              // Target cue ID for control cues

    // Timestamps
    QDateTime createdTime_;
    QDateTime modifiedTime_;
    QDateTime lastExecutedTime_;

    // Custom properties (extensible)
    QVariantMap customProperties_;

    // Execution timing
    QTimer* preWaitTimer_;
    QTimer* postWaitTimer_;
    bool inPreWait_;
    bool inPostWait_;

    // Static helpers
    static QHash<CueType, QString> typeStringMap_;
    static void initializeTypeStringMap();
};