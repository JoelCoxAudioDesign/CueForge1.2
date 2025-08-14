// src/core/CueManager.h - Core Cue Management System
#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QReadWriteLock>
#include <QStringList>
#include <memory>

#include "Cue.h"

// Forward declarations
class GroupCue;
class AudioCue;
class VideoCue;
class MIDICue;
class FadeCue;
class ControlCue;

/**
 * @brief Central management system for all cues in CueForge
 *
 * This class provides the core functionality for managing cues, including:
 * - Creating, organizing, and executing cues
 * - Managing selection and playhead state
 * - Handling workspace save/load operations
 * - Coordinating with the audio engine
 *
 * Direct translation from the original cue-manager.js implementation
 */
class CueManager : public QObject
{
    Q_OBJECT

        // Properties that match the original JS API
        Q_PROPERTY(int cueCount READ cueCount NOTIFY cueCountChanged)
        Q_PROPERTY(bool hasActiveCues READ hasActiveCues NOTIFY playbackStateChanged)
        Q_PROPERTY(bool isPaused READ isPaused NOTIFY playbackStateChanged)
        Q_PROPERTY(QString standByCueId READ standByCueId WRITE setStandByCue NOTIFY playheadChanged)
        Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY workspaceChanged)
        Q_PROPERTY(QString currentWorkspacePath READ currentWorkspacePath NOTIFY workspaceChanged)

public:
    explicit CueManager(QObject* parent = nullptr);
    ~CueManager();

    // Cue management (matching JS API)
    QString addCue(CueType type, const QVariantMap& options = {});
    QString addCueAfter(CueType type, const QString& afterCueId, const QVariantMap& options = {});
    QString addCueAt(CueType type, int index, const QVariantMap& options = {});
    bool removeCue(const QString& cueId);
    bool removeCues(const QStringList& cueIds);

    // Cue access
    Cue* getCue(const QString& cueId) const;
    QList<Cue*> getAllCues() const { return cues_; }
    QList<Cue*> getCuesOfType(CueType type) const;
    int cueCount() const { return cues_.size(); }
    int findCueIndex(const QString& cueId) const;

    // Cue organization (matching JS organization functions)
    bool moveCue(const QString& cueId, int newIndex);
    bool moveCues(const QStringList& cueIds, int newIndex);
    bool moveSelectedCues(int newIndex);
    QList<Cue*> getFlattenedCues() const; // Include group children when expanded
    QString getNextCueNumber() const;
    void resequenceCues(const QString& startNumber = "1", double increment = 1.0);

    // Selection management (matching JS selection model)
    void selectCue(const QString& cueId);
    void selectCues(const QStringList& cueIds);
    void clearSelection();
    void selectAll();
    void toggleCueSelection(const QString& cueId);
    void selectRange(const QString& startCueId, const QString& endCueId);
    QList<Cue*> getSelectedCues() const;
    QStringList getSelectedCueIds() const { return selectedCueIds_; }
    bool isCueSelected(const QString& cueId) const;
    bool hasSelection() const { return !selectedCueIds_.isEmpty(); }

    // Playhead and transport (matching JS transport)
    Cue* getStandByCue() const;
    QString standByCueId() const { return standByCueId_; }
    void setStandByCue(const QString& cueId);
    void advanceStandBy();                    // Move to next executable cue
    void go();                               // Execute standby cue
    void stop();                             // Stop all cues
    void pause();                            // Pause active cues
    void resume();                           // Resume paused cues  
    void panic();                            // Emergency stop all
    void stopSelectedCues();                 // Stop only selected cues
    void stopCue(const QString& cueId);      // Stop specific cue

    // Group management (matching JS group functionality)
    QString createGroupFromSelection();
    QString createGroupFromCues(const QStringList& cueIds);
    bool ungroupCues(const QString& groupId);
    bool isGroupExpanded(const QString& groupId) const;
    void toggleGroupExpansion(const QString& groupId);
    void expandAllGroups();
    void collapseAllGroups();
    QList<Cue*> getGroupChildren(const QString& groupId) const;

    // Clipboard operations (matching JS clipboard functions)
    void cutSelectedCues();
    void copySelectedCues();
    void pasteCues();
    void pasteCuesAt(int index);
    bool hasClipboard() const { return !clipboard_.isEmpty(); }
    void clearClipboard();

    // Workspace management
    void newWorkspace();
    bool openWorkspace(const QString& filePath);
    bool saveWorkspace(const QString& filePath = QString());
    bool saveWorkspaceAs(const QString& filePath);
    QString currentWorkspacePath() const { return workspacePath_; }
    bool hasUnsavedChanges() const { return hasUnsavedChanges_; }
    void markWorkspaceModified();
    QString getWorkspaceTitle() const;

    // Status and monitoring
    bool hasActiveCues() const;
    bool isPaused() const { return isPaused_; }
    int getBrokenCueCount() const;
    QList<Cue*> getActiveCues() const;
    QList<Cue*> getBrokenCues() const;

    // Cue validation and fixing
    bool validateCue(Cue* cue);
    void validateAllCues();
    QString getTargetDisplayText(Cue* cue) const;
    QStringList getTargetCueIds(Cue* cue) const;

    // Search and filtering
    QList<Cue*> findCues(const QString& searchText) const;
    QList<Cue*> findCuesByNumber(const QString& number) const;
    QList<Cue*> findCuesByName(const QString& name) const;
    QList<Cue*> filterCuesByType(CueType type) const;
    QList<Cue*> filterCuesByStatus(CueStatus status) const;

    // Statistics
    struct CueStats {
        int totalCues = 0;
        int audioCues = 0;
        int videoCues = 0;
        int midiCues = 0;
        int fadeCues = 0;
        int groupCues = 0;
        int controlCues = 0;
        int brokenCues = 0;
        double totalDuration = 0.0;
    };
    CueStats getCueStatistics() const;

public slots:
    // Cue property update slots
    void onCuePropertyChanged();
    void onCueStatusChanged();
    void onCueExecutionFinished();

    // Group expansion slots
    void onGroupExpansionChanged(const QString& groupId, bool expanded);

signals:
    // UI update signals (matching JS events)
    void cueAdded(Cue* cue, int index);
    void cueRemoved(const QString& cueId, int index);
    void cueUpdated(Cue* cue);
    void cueMoved(const QString& cueId, int oldIndex, int newIndex);
    void cueCountChanged();

    // Selection signals
    void selectionChanged();
    void selectedCuesChanged(const QStringList& cueIds);

    // Playhead signals
    void playheadChanged();
    void standByCueChanged(const QString& cueId);

    // Playback state signals
    void playbackStateChanged();
    void cueExecutionStarted(const QString& cueId);
    void cueExecutionFinished(const QString& cueId);
    void cueExecutionFailed(const QString& cueId, const QString& error);
    void allCuesStopped();

    // Group signals
    void groupExpansionChanged(const QString& groupId, bool expanded);
    void groupCreated(const QString& groupId);
    void groupRemoved(const QString& groupId);

    // Workspace signals
    void workspaceChanged();
    void workspaceOpened(const QString& filePath);
    void workspaceSaved(const QString& filePath);
    void workspaceModified(bool hasChanges);

    // Status signals
    void cueValidationChanged(const QString& cueId, bool isValid);
    void brokenCueCountChanged(int count);

private slots:
    void processCueExecution();
    void updateActiveCues();
    void onExecutionTimer();

private:
    // Core cue management
    Cue* createCueOfType(CueType type);
    void insertCueAt(Cue* cue, int index);
    void removeCueAt(int index);
    QString generateUniqueCueId() const;
    void connectCueSignals(Cue* cue);
    void disconnectCueSignals(Cue* cue);

    // Selection helpers
    void updateSelection(const QStringList& newSelection);
    void ensureValidSelection();

    // Playhead helpers
    void updateStandByCue();
    QString findNextExecutableCue(const QString& fromCueId) const;
    bool isCueExecutable(Cue* cue) const;

    // Group management helpers
    GroupCue* findParentGroup(const QString& cueId) const;
    void updateGroupChildren(GroupCue* group);

    // Workspace serialization
    QJsonObject serializeWorkspace() const;
    bool deserializeWorkspace(const QJsonObject& json);
    void clearWorkspace();

    // Validation helpers
    void validateCueTargets();
    void updateBrokenCueCount();

    // Core data (matching JS structure)
    QList<Cue*> cues_;                          // Main cue list
    QStringList selectedCueIds_;                // Selected cue IDs
    QString standByCueId_;                      // Current standby cue
    QString workspacePath_;                     // Current workspace file
    bool hasUnsavedChanges_;                    // Modification flag
    bool isPaused_;                             // Global pause state

    // Execution management
    QTimer* executionTimer_;                    // Cue execution processing timer
    QList<Cue*> activeCues_;                   // Currently executing cues
    QMap<QString, bool> groupExpansionState_;  // Group expansion states
    int brokenCueCount_;                       // Cached broken cue count

    // Clipboard system
    QJsonArray clipboard_;                      // Clipboard data (JSON format)

    // Thread safety
    mutable QReadWriteLock cueListLock_;       // Protects cue list access
    mutable QMutex selectionMutex_;            // Protects selection changes
    mutable QMutex playheadMutex_;             // Protects playhead changes

    // Statistics cache
    mutable CueStats cachedStats_;
    mutable bool statsValid_;

    // Constants
    static constexpr int EXECUTION_TIMER_INTERVAL = 50;  // 20 FPS execution updates
    static constexpr double DEFAULT_CUE_DURATION = 5.0;   // Default cue duration
};