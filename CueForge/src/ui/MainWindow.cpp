// src/ui/MainWindow.cpp - Main Window Implementation
#include "MainWindow.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDockWidget>
#include <QActionGroup>
#include <QStyle>
#include <QStyleFactory>

#include "core/CueManager.h"
#include "CueListWidget.h"
#include "InspectorWidget.h"
#include "TransportWidget.h"
#include "MatrixMixerWidget.h"
#include "PreferencesDialog.h"

MainWindow::MainWindow(CueManager* cueManager, QWidget* parent)
    : QMainWindow(parent)
    , cueManager_(cueManager)
    , mainSplitter_(nullptr)
    , rightSplitter_(nullptr)
    , cueListWidget_(nullptr)
    , inspectorWidget_(nullptr)
    , transportWidget_(nullptr)
    , inspectorDock_(nullptr)
    , transportDock_(nullptr)
    , matrixMixerDock_(nullptr)
    , matrixMixerWidget_(nullptr)
    , menuBar_(nullptr)
    , mainToolBar_(nullptr)
    , transportToolBar_(nullptr)
    , cueToolBar_(nullptr)
    , settings_(new QSettings(this))
    , statusTimer_(new QTimer(this))
    , currentWorkspacePath_()
    , isFullScreen_(false)
    , currentZoom_(1.0)
    , inspectorVisible_(true)
    , transportVisible_(true)
    , matrixMixerVisible_(false)
    , spacePressed_(false)
    , shiftPressed_(false)
    , ctrlPressed_(false)
    , altPressed_(false)
    , updateThrottle_(new QTimer(this))
    , pendingUpdate_(false)
{
    // Set window properties
    setWindowTitle("CueForge 2.0");
    setWindowIcon(QIcon(":/icons/cueforge.ico"));
    setMinimumSize(800, 600);

    // Setup UI components
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();
    setupKeyboardShortcuts();

    // Connect signals
    connectSignals();

    // Setup timers
    statusTimer_->setInterval(STATUS_UPDATE_INTERVAL);
    statusTimer_->setSingleShot(false);
    connect(statusTimer_, &QTimer::timeout, this, &MainWindow::onStatusTimer);
    statusTimer_->start();

    // Setup update throttling
    updateThrottle_->setInterval(50); // 20 FPS max
    updateThrottle_->setSingleShot(true);
    connect(updateThrottle_, &QTimer::timeout, [this]() {
        if (pendingUpdate_) {
            updateActions();
            updateStatusBarInfo();
            pendingUpdate_ = false;
        }
        });

    // Load settings and restore window state
    loadSettings();

    qDebug() << "MainWindow initialized";
}

MainWindow::~MainWindow()
{
    saveSettings();
}

// Public Interface

void MainWindow::updateStatus()
{
    if (!updateThrottle_->isActive()) {
        pendingUpdate_ = true;
        updateThrottle_->start();
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = "CueForge 2.0";

    if (cueManager_) {
        QString workspaceTitle = cueManager_->getWorkspaceTitle();
        if (!workspaceTitle.isEmpty()) {
            title += " - " + workspaceTitle;
        }

        if (cueManager_->hasUnsavedChanges()) {
            title += " *";
        }
    }

    setWindowTitle(title);
}

bool MainWindow::isInspectorVisible() const
{
    return inspectorDock_ && inspectorDock_->isVisible();
}

bool MainWindow::isTransportVisible() const
{
    return transportDock_ && transportDock_->isVisible();
}

void MainWindow::restoreLayout()
{
    if (!defaultState_.isEmpty()) {
        restoreState(defaultState_);
    }
    if (!defaultGeometry_.isEmpty()) {
        restoreGeometry(defaultGeometry_);
    }
}

void MainWindow::resetLayout()
{
    // Reset to default layout
    if (inspectorDock_) {
        addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
        inspectorDock_->show();
    }

    if (transportDock_) {
        addDockWidget(Qt::BottomDockWidgetArea, transportDock_);
        transportDock_->show();
    }

    if (matrixMixerDock_) {
        addDockWidget(Qt::BottomDockWidgetArea, matrixMixerDock_);
        matrixMixerDock_->hide();
    }

    // Reset zoom
    currentZoom_ = 1.0;

    // Resize to reasonable default
    resize(1200, 800);

    qDebug() << "Layout reset to default";
}

// Event Handlers

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (cueManager_ && cueManager_->hasUnsavedChanges()) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            "Unsaved Changes",
            "There are unsaved changes in the current workspace.\n\n"
            "Do you want to save your changes before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        switch (result) {
        case QMessageBox::Save:
            saveWorkspace();
            if (cueManager_->hasUnsavedChanges()) {
                event->ignore(); // Save was cancelled
                return;
            }
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
        default:
            event->ignore();
            return;
        }
    }

    saveSettings();
    emit closeRequested();
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    // Track modifier keys
    spacePressed_ = (event->key() == Qt::Key_Space);
    shiftPressed_ = event->modifiers().testFlag(Qt::ShiftModifier);
    ctrlPressed_ = event->modifiers().testFlag(Qt::ControlModifier);
    altPressed_ = event->modifiers().testFlag(Qt::AltModifier);

    // Handle global shortcuts
    switch (event->key()) {
    case Qt::Key_Space:
        if (!event->isAutoRepeat()) {
            if (shiftPressed_) {
                stop();
            }
            else {
                go();
            }
        }
        event->accept();
        return;

    case Qt::Key_Escape:
        if (!event->isAutoRepeat()) {
            panic();
        }
        event->accept();
        return;

    case Qt::Key_P:
        if (!ctrlPressed_ && !event->isAutoRepeat()) {
            if (cueManager_->isPaused()) {
                resume();
            }
            else {
                pause();
            }
        }
        event->accept();
        return;

    case Qt::Key_F11:
        toggleFullScreen();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    // Update modifier key tracking
    if (event->key() == Qt::Key_Space) {
        spacePressed_ = false;
    }

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // Update status when window size changes
    updateStatus();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    // Save default state for reset
    if (defaultGeometry_.isEmpty()) {
        defaultGeometry_ = saveGeometry();
        defaultState_ = saveState();
    }
}

// File Menu Actions

void MainWindow::newWorkspace()
{
    if (cueManager_ && cueManager_->hasUnsavedChanges()) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            "Unsaved Changes",
            "There are unsaved changes in the current workspace.\n\n"
            "Do you want to save your changes before creating a new workspace?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        switch (result) {
        case QMessageBox::Save:
            saveWorkspace();
            if (cueManager_->hasUnsavedChanges()) {
                return; // Save was cancelled
            }
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
        default:
            return;
        }
    }

    if (cueManager_) {
        cueManager_->newWorkspace();
        currentWorkspacePath_.clear();
        updateWindowTitle();
        updateRecentWorkspaces();

        qDebug() << "New workspace created";
    }
}

void MainWindow::openWorkspace()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Workspace",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "CueForge Workspace (*.cfws);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        if (cueManager_ && cueManager_->openWorkspace(filePath)) {
            currentWorkspacePath_ = filePath;
            updateWindowTitle();
            updateRecentWorkspaces();

            emit workspaceOpenRequested(filePath);
            qDebug() << "Opened workspace:" << filePath;
        }
        else {
            QMessageBox::critical(
                this,
                "Open Workspace",
                "Failed to open workspace file:\n" + filePath
            );
        }
    }
}

void MainWindow::saveWorkspace()
{
    if (currentWorkspacePath_.isEmpty()) {
        saveWorkspaceAs();
        return;
    }

    if (cueManager_ && cueManager_->saveWorkspace(currentWorkspacePath_)) {
        updateWindowTitle();
        updateRecentWorkspaces();

        qDebug() << "Saved workspace:" << currentWorkspacePath_;
    }
    else {
        QMessageBox::critical(
            this,
            "Save Workspace",
            "Failed to save workspace file:\n" + currentWorkspacePath_
        );
    }
}

void MainWindow::saveWorkspaceAs()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save Workspace As",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "CueForge Workspace (*.cfws);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        if (!filePath.endsWith(".cfws", Qt::CaseInsensitive)) {
            filePath += ".cfws";
        }

        if (cueManager_ && cueManager_->saveWorkspace(filePath)) {
            currentWorkspacePath_ = filePath;
            updateWindowTitle();
            updateRecentWorkspaces();

            qDebug() << "Saved workspace as:" << filePath;
        }
        else {
            QMessageBox::critical(
                this,
                "Save Workspace As",
                "Failed to save workspace file:\n" + filePath
            );
        }
    }
}

void MainWindow::exportWorkspace()
{
    // TODO: Implement workspace export functionality
    QMessageBox::information(
        this,
        "Export Workspace",
        "Export functionality will be implemented in a future version."
    );
}

void MainWindow::recentWorkspace()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        QString filePath = action->data().toString();
        if (QFile::exists(filePath)) {
            if (cueManager_ && cueManager_->openWorkspace(filePath)) {
                currentWorkspacePath_ = filePath;
                updateWindowTitle();
                updateRecentWorkspaces();

                emit workspaceOpenRequested(filePath);
            }
            else {
                QMessageBox::critical(
                    this,
                    "Open Recent Workspace",
                    "Failed to open workspace file:\n" + filePath
                );
            }
        }
        else {
            QMessageBox::warning(
                this,
                "File Not Found",
                "The workspace file could not be found:\n" + filePath
            );
            // Remove from recent files
            recentWorkspaces_.removeAll(filePath);
            updateRecentWorkspaces();
        }
    }
}

void MainWindow::showPreferences()
{
    emit preferencesRequested();
}

void MainWindow::quitApplication()
{
    close();
}

// Edit Menu Actions

void MainWindow::undoAction()
{
    // TODO: Implement undo functionality
    qDebug() << "Undo requested";
}

void MainWindow::redoAction()
{
    // TODO: Implement redo functionality
    qDebug() << "Redo requested";
}

void MainWindow::cutCues()
{
    if (cueManager_) {
        cueManager_->cutSelectedCues();
        qDebug() << "Cut selected cues";
    }
}

void MainWindow::copyCues()
{
    if (cueManager_) {
        cueManager_->copySelectedCues();
        qDebug() << "Copied selected cues";
    }
}

void MainWindow::pasteCues()
{
    if (cueManager_) {
        cueManager_->pasteCues();
        qDebug() << "Pasted cues";
    }
}

void MainWindow::deleteCues()
{
    if (!cueManager_) return;

    QList<class Cue*> selectedCues = cueManager_->getSelectedCues();
    if (selectedCues.isEmpty()) return;

    // Get confirmation setting
    bool confirmDelete = settings_->value("general/confirmDelete", true).toBool();

    if (confirmDelete) {
        QString message;
        if (selectedCues.size() == 1) {
            message = QString("Are you sure you want to delete cue \"%1\"?")
                .arg(selectedCues.first()->displayName());
        }
        else {
            message = QString("Are you sure you want to delete %1 selected cues?")
                .arg(selectedCues.size());
        }

        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            "Delete Cues",
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    QStringList cueIds = cueManager_->getSelectedCueIds();
    cueManager_->removeCues(cueIds);

    qDebug() << "Deleted" << cueIds.size() << "cues";
}

void MainWindow::selectAll()
{
    if (cueManager_) {
        cueManager_->selectAll();
    }
}

void MainWindow::selectNone()
{
    if (cueManager_) {
        cueManager_->clearSelection();
    }
}

void MainWindow::renumberCues()
{
    if (!cueManager_) return;

    // TODO: Implement renumber dialog
    cueManager_->resequenceCues("1", 1.0);
    qDebug() << "Renumbered cues";
}

// Cue Menu Actions

void MainWindow::addAudioCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Audio);
        qDebug() << "Added audio cue:" << cueId;
    }
}

void MainWindow::addVideoCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Video);
        qDebug() << "Added video cue:" << cueId;
    }
}

void MainWindow::addMIDICue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::MIDI);
        qDebug() << "Added MIDI cue:" << cueId;
    }
}

void MainWindow::addWaitCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Wait);
        qDebug() << "Added wait cue:" << cueId;
    }
}

void MainWindow::addFadeCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Fade);
        qDebug() << "Added fade cue:" << cueId;
    }
}

void MainWindow::addGroupCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Group);
        qDebug() << "Added group cue:" << cueId;
    }
}

void MainWindow::addStartCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Start);
        qDebug() << "Added start cue:" << cueId;
    }
}

void MainWindow::addStopCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Stop);
        qDebug() << "Added stop cue:" << cueId;
    }
}

void MainWindow::addGotoCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Goto);
        qDebug() << "Added goto cue:" << cueId;
    }
}

void MainWindow::addLoadCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Load);
        qDebug() << "Added load cue:" << cueId;
    }
}

void MainWindow::addScriptCue()
{
    if (cueManager_) {
        QString cueId = cueManager_->addCue(CueType::Script);
        qDebug() << "Added script cue:" << cueId;
    }
}

void MainWindow::duplicateSelectedCues()
{
    // TODO: Implement cue duplication
    qDebug() << "Duplicate selected cues requested";
}

void MainWindow::groupSelectedCues()
{
    if (cueManager_) {
        QString groupId = cueManager_->createGroupFromSelection();
        if (!groupId.isEmpty()) {
            qDebug() << "Created group:" << groupId;
        }
    }
}

void MainWindow::ungroupSelectedCues()
{
    if (!cueManager_) return;

    QList<class Cue*> selectedCues = cueManager_->getSelectedCues();
    for (class Cue* cue : selectedCues) {
        if (cue->type() == CueType::Group) {
            cueManager_->ungroupCues(cue->id());
            qDebug() << "Ungrouped cue:" << cue->id();
        }
    }
}

// Transport Actions

void MainWindow::go()
{
    if (cueManager_) {
        cueManager_->go();
        qDebug() << "GO executed";
    }
}

void MainWindow::stop()
{
    if (cueManager_) {
        cueManager_->stop();
        qDebug() << "STOP executed";
    }
}

void MainWindow::pause()
{
    if (cueManager_) {
        cueManager_->pause();
        qDebug() << "PAUSE executed";
    }
}

void MainWindow::resume()
{
    if (cueManager_) {
        cueManager_->resume();
        qDebug() << "RESUME executed";
    }
}

void MainWindow::panic()
{
    if (cueManager_) {
        cueManager_->panic();
        qDebug() << "PANIC executed";
    }
}

void MainWindow::setStandBy()
{
    // TODO: Implement standby cue selection dialog
    qDebug() << "Set standby requested";
}

void MainWindow::previousCue()
{
    // TODO: Implement previous cue navigation
    qDebug() << "Previous cue requested";
}

void MainWindow::nextCue()
{
    if (cueManager_) {
        cueManager_->advanceStandBy();
        qDebug() << "Advanced to next cue";
    }
}

// View Menu Actions

void MainWindow::toggleInspector()
{
    if (inspectorDock_) {
        bool visible = inspectorDock_->isVisible();
        inspectorDock_->setVisible(!visible);
        inspectorVisible_ = !visible;

        qDebug() << "Inspector" << (inspectorVisible_ ? "shown" : "hidden");
    }
}

void MainWindow::toggleTransport()
{
    if (transportDock_) {
        bool visible = transportDock_->isVisible();
        transportDock_->setVisible(!visible);
        transportVisible_ = !visible;

        qDebug() << "Transport" << (transportVisible_ ? "shown" : "hidden");
    }
}

void MainWindow::toggleMatrixMixer()
{
    if (matrixMixerDock_) {
        bool visible = matrixMixerDock_->isVisible();
        matrixMixerDock_->setVisible(!visible);
        matrixMixerVisible_ = !visible;

        qDebug() << "Matrix mixer" << (matrixMixerVisible_ ? "shown" : "hidden");
    }
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen_) {
        showNormal();
        isFullScreen_ = false;
    }
    else {
        showFullScreen();
        isFullScreen_ = true;
    }

    qDebug() << "Full screen" << (isFullScreen_ ? "enabled" : "disabled");
}

void MainWindow::zoomIn()
{
    currentZoom_ = qMin(currentZoom_ + ZOOM_STEP, MAX_ZOOM);
    applyZoom();
}

void MainWindow::zoomOut()
{
    currentZoom_ = qMax(currentZoom_ - ZOOM_STEP, MIN_ZOOM);
    applyZoom();
}

void MainWindow::resetZoom()
{
    currentZoom_ = 1.0;
    applyZoom();
}

void MainWindow::showCueList()
{
    if (cueListWidget_) {
        cueListWidget_->setFocus();
    }
}

void MainWindow::showWorkspaceOverview()
{
    // TODO: Implement workspace overview
    qDebug() << "Workspace overview requested";
}

// Tools Menu Actions

void MainWindow::validateWorkspace()
{
    if (cueManager_) {
        cueManager_->validateAllCues();

        int brokenCount = cueManager_->getBrokenCueCount();
        QString message;
        if (brokenCount == 0) {
            message = "Workspace validation completed successfully.\nNo issues found.";
        }
        else {
            message = QString("Workspace validation completed.\n%1 cue(s) have issues that need attention.")
                .arg(brokenCount);
        }

        QMessageBox::information(this, "Workspace Validation", message);
        qDebug() << "Workspace validation completed. Broken cues:" << brokenCount;
    }
}

void MainWindow::optimizeWorkspace()
{
    // TODO: Implement workspace optimization
    QMessageBox::information(
        this,
        "Optimize Workspace",
        "Workspace optimization will be implemented in a future version."
    );
}

void MainWindow::showAudioSettings()
{
    // TODO: Implement audio settings dialog
    qDebug() << "Audio settings requested";
}

void MainWindow::showMIDISettings()
{
    // TODO: Implement MIDI settings dialog
    qDebug() << "MIDI settings requested";
}

void MainWindow::showNetworkSettings()
{
    // TODO: Implement network settings dialog
    qDebug() << "Network settings requested";
}

void MainWindow::showKeyboardShortcuts()
{
    // TODO: Implement keyboard shortcuts reference
    qDebug() << "Keyboard shortcuts reference requested";
}

// Help Menu Actions

void MainWindow::showUserManual()
{
    // TODO: Implement user manual
    QMessageBox::information(
        this,
        "User Manual",
        "User manual will be available in a future version."
    );
}

void MainWindow::showKeyboardReference()
{
    QString shortcuts =
        "<h3>CueForge Keyboard Shortcuts</h3>"
        "<table>"
        "<tr><td><b>Space</b></td><td>GO (Execute standby cue)</td></tr>"
        "<tr><td><b>Shift+Space</b></td><td>STOP (Stop all cues)</td></tr>"
        "<tr><td><b>P</b></td><td>PAUSE/RESUME</td></tr>"
        "<tr><td><b>Escape</b></td><td>PANIC (Emergency stop)</td></tr>"
        "<tr><td><b>Ctrl+N</b></td><td>New workspace</td></tr>"
        "<tr><td><b>Ctrl+O</b></td><td>Open workspace</td></tr>"
        "<tr><td><b>Ctrl+S</b></td><td>Save workspace</td></tr>"
        "<tr><td><b>Ctrl+A</b></td><td>Select all cues</td></tr>"
        "<tr><td><b>Delete</b></td><td>Delete selected cues</td></tr>"
        "<tr><td><b>F11</b></td><td>Toggle full screen</td></tr>"
        "</table>";

    QMessageBox::information(this, "Keyboard Reference", shortcuts);
}

void MainWindow::reportBug()
{
    // TODO: Implement bug reporting
    QMessageBox::information(
        this,
        "Report Bug",
        "Bug reporting will be available in a future version."
    );
}

void MainWindow::checkForUpdates()
{
    // TODO: Implement update checking
    QMessageBox::information(
        this,
        "Check for Updates",
        "Update checking will be implemented in a future version."
    );
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this,
        "About CueForge",
        "<h2>CueForge 2.0</h2>"
        "<p>Professional cue-based show control application</p>"
        "<p>Built with Qt6 and JUCE/Tracktion Engine</p>"
        "<p>Copyright © 2025 CueForge</p>"
        "<p><a href='https://cueforge.app'>https://cueforge.app</a></p>"
    );
}

// Status Update Slots

void MainWindow::onCueCountChanged()
{
    updateStatus();
}

void MainWindow::onPlaybackStateChanged()
{
    updateStatus();
}

void MainWindow::onSelectionChanged()
{
    updateStatus();
}

void MainWindow::onWorkspaceChanged()
{
    updateWindowTitle();
    updateStatus();
}

void MainWindow::onBrokenCueCountChanged()
{
    updateStatus();
}

void MainWindow::onCueExecutionStarted(const QString& cueId)
{
    Q_UNUSED(cueId)
        updateStatus();
}

void MainWindow::onCueExecutionFinished(const QString& cueId)
{
    Q_UNUSED(cueId)
        updateStatus();
}

// Private Slots

void MainWindow::onRecentWorkspaceAction()
{
    recentWorkspace();
}

void MainWindow::onStatusTimer()
{
    updateStatus();
}

void MainWindow::onAutoSaveIndicator()
{
    // TODO: Implement auto-save indicator animation
}

// Private Implementation

void MainWindow::setupUI()
{
    // Create central widget with splitter layout
    QWidget* centralWidget = new QWidget;
    setCentralWidget(centralWidget);

    QHBoxLayout* centralLayout = new QHBoxLayout(centralWidget);
    centralLayout->setContentsMargins(4, 4, 4, 4);
    centralLayout->setSpacing(4);

    // Create main horizontal splitter
    mainSplitter_ = new QSplitter(Qt::Horizontal);
    centralLayout->addWidget(mainSplitter_);

    // Create cue list widget (placeholder for now)
    cueListWidget_ = new QWidget; // Will be replaced with CueListWidget
    cueListWidget_->setMinimumWidth(300);
    cueListWidget_->setStyleSheet("background-color: #2b2b2b; border: 1px solid #555;");

    QLabel* cueListLabel = new QLabel("Cue List Widget\n(To be implemented)");
    cueListLabel->setAlignment(Qt::AlignCenter);
    cueListLabel->setStyleSheet("color: #888; font-size: 14px;");

    QVBoxLayout* cueListLayout = new QVBoxLayout(cueListWidget_);
    cueListLayout->addWidget(cueListLabel);

    mainSplitter_->addWidget(cueListWidget_);

    // Set initial splitter proportions
    mainSplitter_->setSizes({ 700, 500 });
    mainSplitter_->setStretchFactor(0, 1); // Cue list gets priority
    mainSplitter_->setStretchFactor(1, 0); // Inspector is fixed-ish

    qDebug() << "Main UI layout created";
}

void MainWindow::setupMenuBar()
{
    menuBar_ = menuBar();

    // File Menu
    QMenu* fileMenu = createFileMenu();
    menuBar_->addMenu(fileMenu);

    // Edit Menu
    QMenu* editMenu = createEditMenu();
    menuBar_->addMenu(editMenu);

    // Cue Menu
    QMenu* cueMenu = createCueMenu();
    menuBar_->addMenu(cueMenu);

    // Transport Menu
    QMenu* transportMenu = createTransportMenu();
    menuBar_->addMenu(transportMenu);

    // View Menu
    QMenu* viewMenu = createViewMenu();
    menuBar_->addMenu(viewMenu);

    // Tools Menu
    QMenu* toolsMenu = createToolsMenu();
    menuBar_->addMenu(toolsMenu);

    // Help Menu
    QMenu* helpMenu = createHelpMenu();
    menuBar_->addMenu(helpMenu);

    qDebug() << "Menu bar created";
}

void MainWindow::setupToolBar()
{
    // Main toolbar
    mainToolBar_ = createMainToolBar();
    addToolBar(Qt::TopToolBarArea, mainToolBar_);

    // Transport toolbar
    transportToolBar_ = createTransportToolBar();
    addToolBar(Qt::TopToolBarArea, transportToolBar_);

    // Cue toolbar
    cueToolBar_ = createCueToolBar();
    addToolBar(Qt::TopToolBarArea, cueToolBar_);

    qDebug() << "Toolbars created";
}

void MainWindow::setupStatusBar()
{
    createStatusBarWidgets();

    QStatusBar* status = statusBar();

    // Add widgets to status bar
    status->addWidget(cueCountLabel_);
    status->addWidget(selectionLabel_);
    status->addPermanentWidget(brokenCueLabel_);
    status->addPermanentWidget(playbackStatusLabel_);
    status->addPermanentWidget(currentCueLabel_);
    status->addPermanentWidget(executionProgress_);
    status->addPermanentWidget(autoSaveIndicator_);
    status->addPermanentWidget(audioStatusLabel_);

    // Initial update
    updateStatusBarInfo();

    qDebug() << "Status bar created";
}

void MainWindow::setupDockWidgets()
{
    // Create inspector dock widget
    inspectorDock_ = new QDockWidget("Inspector", this);
    inspectorDock_->setObjectName("InspectorDock");
    inspectorDock_->setFeatures(QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetFloatable);

    // Create inspector widget (placeholder for now)
    inspectorWidget_ = new QWidget;
    inspectorWidget_->setMinimumWidth(300);
    inspectorWidget_->setStyleSheet("background-color: #2b2b2b; border: 1px solid #555;");

    QLabel* inspectorLabel = new QLabel("Inspector Widget\n(To be implemented)");
    inspectorLabel->setAlignment(Qt::AlignCenter);
    inspectorLabel->setStyleSheet("color: #888; font-size: 14px;");

    QVBoxLayout* inspectorLayout = new QVBoxLayout(inspectorWidget_);
    inspectorLayout->addWidget(inspectorLabel);

    inspectorDock_->setWidget(inspectorWidget_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);

    // Create transport dock widget
    transportDock_ = new QDockWidget("Transport", this);
    transportDock_->setObjectName("TransportDock");
    transportDock_->setFeatures(QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetFloatable);

    // Create transport widget (placeholder for now)
    transportWidget_ = new QWidget;
    transportWidget_->setMaximumHeight(120);
    transportWidget_->setStyleSheet("background-color: #2b2b2b; border: 1px solid #555;");

    QLabel* transportLabel = new QLabel("Transport Widget (To be implemented)");
    transportLabel->setAlignment(Qt::AlignCenter);
    transportLabel->setStyleSheet("color: #888; font-size: 14px;");

    QVBoxLayout* transportLayout = new QVBoxLayout(transportWidget_);
    transportLayout->addWidget(transportLabel);

    transportDock_->setWidget(transportWidget_);
    addDockWidget(Qt::BottomDockWidgetArea, transportDock_);

    // Create matrix mixer dock widget (hidden by default)
    matrixMixerDock_ = new QDockWidget("Matrix Mixer", this);
    matrixMixerDock_->setObjectName("MatrixMixerDock");
    matrixMixerDock_->setFeatures(QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetFloatable);

    // Create matrix mixer widget (placeholder for now)
    matrixMixerWidget_ = new QWidget;
    matrixMixerWidget_->setMinimumHeight(200);
    matrixMixerWidget_->setStyleSheet("background-color: #2b2b2b; border: 1px solid #555;");

    QLabel* matrixLabel = new QLabel("Matrix Mixer Widget\n(To be implemented)");
    matrixLabel->setAlignment(Qt::AlignCenter);
    matrixLabel->setStyleSheet("color: #888; font-size: 14px;");

    QVBoxLayout* matrixLayout = new QVBoxLayout(matrixMixerWidget_);
    matrixLayout->addWidget(matrixLabel);

    matrixMixerDock_->setWidget(matrixMixerWidget_);
    addDockWidget(Qt::BottomDockWidgetArea, matrixMixerDock_);
    matrixMixerDock_->hide(); // Hidden by default

    // Tabify transport and matrix mixer docks
    tabifyDockWidget(transportDock_, matrixMixerDock_);
    transportDock_->raise(); // Show transport tab by default

    qDebug() << "Dock widgets created";
}

void MainWindow::setupKeyboardShortcuts()
{
    setupFileShortcuts();
    setupEditShortcuts();
    setupCueShortcuts();
    setupTransportShortcuts();
    setupViewShortcuts();

    qDebug() << "Keyboard shortcuts configured";
}

void MainWindow::connectSignals()
{
    if (cueManager_) {
        // Connect cue manager signals
        connect(cueManager_, &CueManager::cueCountChanged,
            this, &MainWindow::onCueCountChanged);
        connect(cueManager_, &CueManager::playbackStateChanged,
            this, &MainWindow::onPlaybackStateChanged);
        connect(cueManager_, &CueManager::selectionChanged,
            this, &MainWindow::onSelectionChanged);
        connect(cueManager_, &CueManager::workspaceChanged,
            this, &MainWindow::onWorkspaceChanged);
        connect(cueManager_, &CueManager::brokenCueCountChanged,
            this, &MainWindow::onBrokenCueCountChanged);
        connect(cueManager_, &CueManager::cueExecutionStarted,
            this, &MainWindow::onCueExecutionStarted);
        connect(cueManager_, &CueManager::cueExecutionFinished,
            this, &MainWindow::onCueExecutionFinished);
    }

    qDebug() << "Signals connected";
}

void MainWindow::loadSettings()
{
    // Restore window geometry and state
    QByteArray geometry = settings_->value("window/geometry").toByteArray();
    QByteArray state = settings_->value("window/state").toByteArray();

    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (!state.isEmpty()) {
        restoreState(state);
    }

    // Restore window properties
    bool maximized = settings_->value("window/maximized", false).toBool();
    if (maximized) {
        showMaximized();
    }

    currentZoom_ = settings_->value("window/zoom", 1.0).toDouble();

    // Restore dock visibility
    inspectorVisible_ = settings_->value("window/inspectorVisible", true).toBool();
    transportVisible_ = settings_->value("window/transportVisible", true).toBool();
    matrixMixerVisible_ = settings_->value("window/matrixVisible", false).toBool();

    if (inspectorDock_) inspectorDock_->setVisible(inspectorVisible_);
    if (transportDock_) transportDock_->setVisible(transportVisible_);
    if (matrixMixerDock_) matrixMixerDock_->setVisible(matrixMixerVisible_);

    // Load recent workspaces
    recentWorkspaces_ = settings_->value("workspace/recentFiles").toStringList();
    updateRecentWorkspaces();

    qDebug() << "Settings loaded";
}

void MainWindow::saveSettings()
{
    // Save window geometry and state
    settings_->setValue("window/geometry", saveGeometry());
    settings_->setValue("window/state", saveState());
    settings_->setValue("window/maximized", isMaximized());
    settings_->setValue("window/zoom", currentZoom_);

    // Save dock visibility
    settings_->setValue("window/inspectorVisible", isInspectorVisible());
    settings_->setValue("window/transportVisible", isTransportVisible());
    settings_->setValue("window/matrixVisible", matrixMixerDock_ && matrixMixerDock_->isVisible());

    // Save recent workspaces
    settings_->setValue("workspace/recentFiles", recentWorkspaces_);

    // Save current workspace path
    if (!currentWorkspacePath_.isEmpty()) {
        settings_->setValue("workspace/lastOpened", currentWorkspacePath_);
    }

    settings_->sync();

    qDebug() << "Settings saved";
}

void MainWindow::updateRecentWorkspaces()
{
    // Update recent files list
    if (!currentWorkspacePath_.isEmpty()) {
        recentWorkspaces_.removeAll(currentWorkspacePath_);
        recentWorkspaces_.prepend(currentWorkspacePath_);

        // Limit to MaxRecentFiles
        while (recentWorkspaces_.size() > MaxRecentFiles) {
            recentWorkspaces_.removeLast();
        }
    }

    // Update recent files actions
    for (int i = 0; i < recentActions_.size(); ++i) {
        if (i < recentWorkspaces_.size()) {
            QString filePath = recentWorkspaces_[i];
            QString fileName = QFileInfo(filePath).baseName();
            recentActions_[i]->setText(QString("&%1 %2").arg(i + 1).arg(fileName));
            recentActions_[i]->setData(filePath);
            recentActions_[i]->setVisible(true);
        }
        else {
            recentActions_[i]->setVisible(false);
        }
    }
}

void MainWindow::updateActions()
{
    if (!cueManager_) return;

    bool hasSelection = cueManager_->hasSelection();
    bool hasClipboard = cueManager_->hasClipboard();
    bool hasActiveCues = cueManager_->hasActiveCues();
    bool isPaused = cueManager_->isPaused();

    // Update edit actions
    if (cutAction_) cutAction_->setEnabled(hasSelection);
    if (copyAction_) copyAction_->setEnabled(hasSelection);
    if (pasteAction_) pasteAction_->setEnabled(hasClipboard);
    if (deleteAction_) deleteAction_->setEnabled(hasSelection);
    if (groupAction_) groupAction_->setEnabled(hasSelection);
    if (ungroupAction_) ungroupAction_->setEnabled(hasSelection);
    if (duplicateAction_) duplicateAction_->setEnabled(hasSelection);

    // Update transport actions
    if (goAction_) goAction_->setEnabled(cueManager_->getStandByCue() != nullptr);
    if (stopAction_) stopAction_->setEnabled(hasActiveCues);
    if (pauseAction_) {
        pauseAction_->setEnabled(hasActiveCues);
        pauseAction_->setText(isPaused ? "Resume" : "Pause");
        pauseAction_->setIcon(style()->standardIcon(isPaused ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
    }
    if (resumeAction_) resumeAction_->setEnabled(isPaused);
    if (panicAction_) panicAction_->setEnabled(hasActiveCues);
}

void MainWindow::updateStatusBarInfo()
{
    if (!cueManager_) return;

    // Update cue count
    if (cueCountLabel_) {
        int count = cueManager_->cueCount();
        cueCountLabel_->setText(QString("Cues: %1").arg(count));
    }

    // Update selection
    if (selectionLabel_) {
        int selected = cueManager_->getSelectedCueIds().size();
        if (selected > 0) {
            selectionLabel_->setText(QString("Selected: %1").arg(selected));
            selectionLabel_->show();
        }
        else {
            selectionLabel_->hide();
        }
    }

    // Update broken cue count
    if (brokenCueLabel_) {
        int broken = cueManager_->getBrokenCueCount();
        if (broken > 0) {
            brokenCueLabel_->setText(QString("Issues: %1").arg(broken));
            brokenCueLabel_->setStyleSheet("color: #ff6b6b; font-weight: bold;");
            brokenCueLabel_->show();
        }
        else {
            brokenCueLabel_->hide();
        }
    }

    // Update playback status
    if (playbackStatusLabel_) {
        if (cueManager_->hasActiveCues()) {
            if (cueManager_->isPaused()) {
                playbackStatusLabel_->setText("PAUSED");
                playbackStatusLabel_->setStyleSheet("color: #ffa500; font-weight: bold;");
            }
            else {
                playbackStatusLabel_->setText("PLAYING");
                playbackStatusLabel_->setStyleSheet("color: #4ade80; font-weight: bold;");
            }
            playbackStatusLabel_->show();
        }
        else {
            playbackStatusLabel_->hide();
        }
    }

    // Update current cue
    if (currentCueLabel_) {
        class Cue* standby = cueManager_->getStandByCue();
        if (standby) {
            currentCueLabel_->setText(QString("Next: %1").arg(standby->displayName()));
            currentCueLabel_->show();
        }
        else {
            currentCueLabel_->hide();
        }
    }

    // Update execution progress (placeholder)
    if (executionProgress_) {
        executionProgress_->hide(); // Will be implemented with actual progress tracking
    }

    // Update auto-save indicator (placeholder)
    if (autoSaveIndicator_) {
        if (cueManager_->hasUnsavedChanges()) {
            autoSaveIndicator_->setText("●");
            autoSaveIndicator_->setStyleSheet("color: #ffa500;");
            autoSaveIndicator_->setToolTip("Unsaved changes");
        }
        else {
            autoSaveIndicator_->setText("●");
            autoSaveIndicator_->setStyleSheet("color: #4ade80;");
            autoSaveIndicator_->setToolTip("All changes saved");
        }
    }

    // Update audio status (placeholder)
    if (audioStatusLabel_) {
        audioStatusLabel_->setText("Audio: OK");
        audioStatusLabel_->setStyleSheet("color: #4ade80;");
    }
}

void MainWindow::applyZoom()
{
    // Apply zoom to relevant widgets
    QString zoomStyle = QString("font-size: %1px;").arg(static_cast<int>(12 * currentZoom_));

    if (cueListWidget_) {
        // Will apply to actual cue list widget when implemented
    }

    qDebug() << "Zoom applied:" << currentZoom_;
}

// Menu Creation Methods

QMenu* MainWindow::createFileMenu()
{
    QMenu* menu = new QMenu("&File", this);

    newAction_ = menu->addAction("&New Workspace", this, &MainWindow::newWorkspace);
    newAction_->setShortcut(QKeySequence::New);
    newAction_->setIcon(style()->standardIcon(QStyle::SP_FileIcon));

    openAction_ = menu->addAction("&Open Workspace...", this, &MainWindow::openWorkspace);
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));

    menu->addSeparator();

    saveAction_ = menu->addAction("&Save Workspace", this, &MainWindow::saveWorkspace);
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));

    saveAsAction_ = menu->addAction("Save Workspace &As...", this, &MainWindow::saveWorkspaceAs);
    saveAsAction_->setShortcut(QKeySequence::SaveAs);

    menu->addSeparator();

    exportAction_ = menu->addAction("&Export Workspace...", this, &MainWindow::exportWorkspace);

    menu->addSeparator();

    // Recent files submenu
    QMenu* recentMenu = menu->addMenu("Recent Workspaces");
    for (int i = 0; i < MaxRecentFiles; ++i) {
        QAction* action = new QAction(this);
        action->setVisible(false);
        connect(action, &QAction::triggered, this, &MainWindow::onRecentWorkspaceAction);
        recentMenu->addAction(action);
        recentActions_.append(action);
    }

    menu->addSeparator();

    preferencesAction_ = menu->addAction("&Preferences...", this, &MainWindow::showPreferences);
    preferencesAction_->setShortcut(QKeySequence::Preferences);
    preferencesAction_->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));

    menu->addSeparator();

    quitAction_ = menu->addAction("&Quit", this, &MainWindow::quitApplication);
    quitAction_->setShortcut(QKeySequence::Quit);
    quitAction_->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));

    return menu;
}

QMenu* MainWindow::createEditMenu()
{
    QMenu* menu = new QMenu("&Edit", this);

    undoAction_ = menu->addAction("&Undo", this, &MainWindow::undoAction);
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    undoAction_->setEnabled(false); // TODO: Enable when undo is implemented

    redoAction_ = menu->addAction("&Redo", this, &MainWindow::redoAction);
    redoAction_->setShortcut(QKeySequence::Redo);
    redoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    redoAction_->setEnabled(false); // TODO: Enable when redo is implemented

    menu->addSeparator();

    cutAction_ = menu->addAction("Cu&t", this, &MainWindow::cutCues);
    cutAction_->setShortcut(QKeySequence::Cut);
    cutAction_->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));

    copyAction_ = menu->addAction("&Copy", this, &MainWindow::copyCues);
    copyAction_->setShortcut(QKeySequence::Copy);
    copyAction_->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));

    pasteAction_ = menu->addAction("&Paste", this, &MainWindow::pasteCues);
    pasteAction_->setShortcut(QKeySequence::Paste);
    pasteAction_->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));

    menu->addSeparator();

    deleteAction_ = menu->addAction("&Delete", this, &MainWindow::deleteCues);
    deleteAction_->setShortcut(QKeySequence::Delete);
    deleteAction_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));

    menu->addSeparator();

    selectAllAction_ = menu->addAction("Select &All", this, &MainWindow::selectAll);
    selectAllAction_->setShortcut(QKeySequence::SelectAll);

    selectNoneAction_ = menu->addAction("Select &None", this, &MainWindow::selectNone);
    selectNoneAction_->setShortcut(QKeySequence("Ctrl+D"));

    menu->addSeparator();

    duplicateAction_ = menu->addAction("D&uplicate", this, &MainWindow::duplicateSelectedCues);
    duplicateAction_->setShortcut(QKeySequence("Ctrl+Shift+D"));

    groupAction_ = menu->addAction("&Group", this, &MainWindow::groupSelectedCues);
    groupAction_->setShortcut(QKeySequence("Ctrl+G"));

    ungroupAction_ = menu->addAction("&Ungroup", this, &MainWindow::ungroupSelectedCues);
    ungroupAction_->setShortcut(QKeySequence("Ctrl+Shift+G"));

    menu->addSeparator();

    renumberAction_ = menu->addAction("&Renumber Cues...", this, &MainWindow::renumberCues);

    return menu;
}