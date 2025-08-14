// src/ui/MainWindow.h - Main Application Window
#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QSettings>

// Forward declarations
QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QDockWidget;
QT_END_NAMESPACE

class CueManager;
class CueListWidget;
class InspectorWidget;
class TransportWidget;
class MatrixMixerWidget;

/**
 * @brief Main application window for CueForge
 *
 * This class provides the main user interface layout, matching the structure
 * of the original Electron implementation. It coordinates between the various
 * UI components and handles menu/toolbar actions.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(CueManager* cueManager, QWidget* parent = nullptr);
    ~MainWindow();

    // Public interface for application integration
    void updateStatus();
    void updateWindowTitle();

    // UI state management  
    bool isInspectorVisible() const;
    bool isTransportVisible() const;
    void restoreLayout();
    void resetLayout();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

public slots:
    // File menu actions (matching Electron menu structure)
    void newWorkspace();
    void openWorkspace();
    void saveWorkspace();
    void saveWorkspaceAs();
    void exportWorkspace();
    void recentWorkspace();
    void showPreferences();
    void quitApplication();

    // Edit menu actions (matching JS edit functions)
    void undoAction();
    void redoAction();
    void cutCues();
    void copyCues();
    void pasteCues();
    void deleteCues();
    void selectAll();
    void selectNone();
    void renumberCues();

    // Cue menu actions (matching JS add cue functions)
    void addAudioCue();
    void addVideoCue();
    void addMIDICue();
    void addWaitCue();
    void addFadeCue();
    void addGroupCue();
    void addStartCue();
    void addStopCue();
    void addGotoCue();
    void addLoadCue();
    void addScriptCue();
    void duplicateSelectedCues();
    void groupSelectedCues();
    void ungroupSelectedCues();

    // Transport actions (matching JS transport controls)
    void go();
    void stop();
    void pause();
    void resume();
    void panic();
    void setStandBy();
    void previousCue();
    void nextCue();

    // View menu actions
    void toggleInspector();
    void toggleTransport();
    void toggleMatrixMixer();
    void toggleFullScreen();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void showCueList();
    void showWorkspaceOverview();

    // Tools menu actions
    void validateWorkspace();
    void optimizeWorkspace();
    void showAudioSettings();
    void showMIDISettings();
    void showNetworkSettings();
    void showKeyboardShortcuts();

    // Help menu actions
    void showUserManual();
    void showKeyboardReference();
    void reportBug();
    void checkForUpdates();
    void showAbout();

    // Status update slots
    void onCueCountChanged();
    void onPlaybackStateChanged();
    void onSelectionChanged();
    void onWorkspaceChanged();
    void onBrokenCueCountChanged();
    void onCueExecutionStarted(const QString& cueId);
    void onCueExecutionFinished(const QString& cueId);

signals:
    void closeRequested();
    void workspaceOpenRequested(const QString& filePath);
    void preferencesRequested();

private slots:
    void onRecentWorkspaceAction();
    void onStatusTimer();
    void onAutoSaveIndicator();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDockWidgets();
    void setupKeyboardShortcuts();
    void setupSplitters();
    void connectSignals();

    void loadSettings();
    void saveSettings();
    void updateRecentWorkspaces();
    void updateActions();
    void updateStatusBarInfo();

    // UI creation helpers
    QMenu* createFileMenu();
    QMenu* createEditMenu();
    QMenu* createCueMenu();
    QMenu* createTransportMenu();
    QMenu* createViewMenu();
    QMenu* createToolsMenu();
    QMenu* createHelpMenu();

    QToolBar* createMainToolBar();
    QToolBar* createTransportToolBar();
    QToolBar* createCueToolBar();

    void createStatusBarWidgets();

    // Keyboard shortcut helpers
    void setupFileShortcuts();
    void setupEditShortcuts();
    void setupCueShortcuts();
    void setupTransportShortcuts();
    void setupViewShortcuts();

    // Core components
    CueManager* cueManager_;

    // UI Components (matching current Electron layout)
    QSplitter* mainSplitter_;           // Main horizontal splitter
    QSplitter* rightSplitter_;          // Right vertical splitter

    // Primary UI widgets
    CueListWidget* cueListWidget_;
    InspectorWidget* inspectorWidget_;
    TransportWidget* transportWidget_;

    // Dock widgets for flexible layout
    QDockWidget* inspectorDock_;
    QDockWidget* transportDock_;
    QDockWidget* matrixMixerDock_;

    // Optional widgets
    MatrixMixerWidget* matrixMixerWidget_;

    // Menu and toolbar components
    QMenuBar* menuBar_;
    QToolBar* mainToolBar_;
    QToolBar* transportToolBar_;
    QToolBar* cueToolBar_;

    // File menu actions
    QAction* newAction_;
    QAction* openAction_;
    QAction* saveAction_;
    QAction* saveAsAction_;
    QAction* exportAction_;
    QAction* preferencesAction_;
    QAction* quitAction_;
    QList<QAction*> recentActions_;

    // Edit menu actions
    QAction* undoAction_;
    QAction* redoAction_;
    QAction* cutAction_;
    QAction* copyAction_;
    QAction* pasteAction_;
    QAction* deleteAction_;
    QAction* selectAllAction_;
    QAction* selectNoneAction_;
    QAction* renumberAction_;

    // Cue menu actions
    QAction* addAudioAction_;
    QAction* addVideoAction_;
    QAction* addMIDIAction_;
    QAction* addWaitAction_;
    QAction* addFadeAction_;
    QAction* addGroupAction_;
    QAction* addStartAction_;
    QAction* addStopAction_;
    QAction* addGotoAction_;
    QAction* addLoadAction_;
    QAction* addScriptAction_;
    QAction* duplicateAction_;
    QAction* groupAction_;
    QAction* ungroupAction_;

    // Transport menu actions
    QAction* goAction_;
    QAction* stopAction_;
    QAction* pauseAction_;
    QAction* resumeAction_;
    QAction* panicAction_;
    QAction* setStandByAction_;
    QAction* previousCueAction_;
    QAction* nextCueAction_;

    // View menu actions
    QAction* toggleInspectorAction_;
    QAction* toggleTransportAction_;
    QAction* toggleMatrixMixerAction_;
    QAction* toggleFullScreenAction_;
    QAction* zoomInAction_;
    QAction* zoomOutAction_;
    QAction* resetZoomAction_;

    // Tools menu actions
    QAction* validateWorkspaceAction_;
    QAction* optimizeWorkspaceAction_;
    QAction* audioSettingsAction_;
    QAction* midiSettingsAction_;
    QAction* networkSettingsAction_;
    QAction* keyboardShortcutsAction_;

    // Help menu actions
    QAction* userManualAction_;
    QAction* keyboardReferenceAction_;
    QAction* reportBugAction_;
    QAction* checkUpdatesAction_;
    QAction* aboutAction_;

    // Status bar widgets
    QLabel* cueCountLabel_;
    QLabel* selectionLabel_;
    QLabel* playbackStatusLabel_;
    QLabel* brokenCueLabel_;
    QLabel* currentCueLabel_;
    QProgressBar* executionProgress_;
    QLabel* autoSaveIndicator_;
    QLabel* audioStatusLabel_;

    // Settings and state
    QSettings* settings_;
    QTimer* statusTimer_;           // Regular status updates
    QString currentWorkspacePath_;
    bool isFullScreen_;
    double currentZoom_;

    // Layout state
    bool inspectorVisible_;
    bool transportVisible_;
    bool matrixMixerVisible_;
    QByteArray defaultGeometry_;
    QByteArray defaultState_;

    // Recent workspaces
    QStringList recentWorkspaces_;
    static constexpr int MaxRecentFiles = 10;

    // Keyboard state tracking
    bool spacePressed_;
    bool shiftPressed_;
    bool ctrlPressed_;
    bool altPressed_;

    // Update throttling
    QTimer* updateThrottle_;
    bool pendingUpdate_;

    // Constants
    static constexpr int STATUS_UPDATE_INTERVAL = 100;  // 10 FPS for status updates
    static constexpr double MIN_ZOOM = 0.5;
    static constexpr double MAX_ZOOM = 3.0;
    static constexpr double ZOOM_STEP = 0.1;
};