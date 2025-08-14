// src/core/CueManager.cpp - Core Cue Management Implementation
#include "CueManager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QReadLocker>
#include <QWriteLocker>
#include <algorithm>

#include "AudioCue.h"
#include "GroupCue.h"
// Additional cue types will be included as they're implemented

CueManager::CueManager(QObject* parent)
    : QObject(parent)
    , selectedCueIds_()
    , standByCueId_()
    , workspacePath_()
    , hasUnsavedChanges_(false)
    , isPaused_(false)
    , executionTimer_(new QTimer(this))
    , activeCues_()
    , groupExpansionState_()
    , brokenCueCount_(0)
    , clipboard_()
    , statsValid_(false)
{
    // Setup execution timer for cue processing
    executionTimer_->setInterval(EXECUTION_TIMER_INTERVAL);
    executionTimer_->setSingleShot(false);
    connect(executionTimer_, &QTimer::timeout, this, &CueManager::processCueExecution);

    // Start execution processing
    executionTimer_->start();

    qDebug() << "CueManager initialized";
}

CueManager::~CueManager()
{
    // Stop execution timer
    executionTimer_->stop();

    // Clean up all cues
    clearWorkspace();

    qDebug() << "CueManager destroyed";
}

// Core Cue Management

QString CueManager::addCue(CueType type, const QVariantMap& options)
{
    return addCueAt(type, cues_.size(), options);
}

QString CueManager::addCueAfter(CueType type, const QString& afterCueId, const QVariantMap& options)
{
    int afterIndex = findCueIndex(afterCueId);
    if (afterIndex >= 0) {
        return addCueAt(type, afterIndex + 1, options);
    }
    return addCue(type, options); // Append if not found
}

QString CueManager::addCueAt(CueType type, int index, const QVariantMap& options)
{
    QWriteLocker locker(&cueListLock_);

    // Create the cue
    Cue* cue = createCueOfType(type);
    if (!cue) {
        qCritical() << "Failed to create cue of type" << static_cast<int>(type);
        return QString();
    }

    // Apply options
    if (options.contains("number")) {
        cue->setNumber(options["number"].toString());
    }
    else {
        cue->setNumber(getNextCueNumber());
    }

    if (options.contains("name")) {
        cue->setName(options["name"].toString());
    }

    if (options.contains("color")) {
        cue->setColor(QColor(options["color"].toString()));
    }

    if (options.contains("notes")) {
        cue->setNotes(options["notes"].toString());
    }

    if (options.contains("duration")) {
        cue->setDuration(options["duration"].toDouble());
    }

    // Audio cue specific options
    if (type == CueType::Audio && options.contains("filePath")) {
        AudioCue* audioCue = qobject_cast<AudioCue*>(cue);
        if (audioCue) {
            audioCue->setFilePath(options["filePath"].toString());
        }
    }

    // Insert at specified position
    insertCueAt(cue, qBound(0, index, cues_.size()));

    // Connect signals
    connectCueSignals(cue);

    // Mark workspace as modified
    markWorkspaceModified();

    QString cueId = cue->id();
    qDebug() << "Added cue" << cue->number() << "(" << cue->typeString() << ") at index" << index;

    emit cueAdded(cue, index);
    emit cueCountChanged();

    return cueId;
}

bool CueManager::removeCue(const QString& cueId)
{
    return removeCues(QStringList() << cueId);
}

bool CueManager::removeCues(const QStringList& cueIds)
{
    QWriteLocker locker(&cueListLock_);

    bool anyRemoved = false;

    // Remove in reverse order to maintain indices
    for (const QString& cueId : cueIds) {
        int index = findCueIndex(cueId);
        if (index >= 0) {
            Cue* cue = cues_[index];

            // Stop cue if it's playing
            if (cue->isExecuting()) {
                cue->stop();
            }

            // Remove from selection if selected
            selectedCueIds_.removeAll(cueId);

            // Clear standby if this cue is standby
            if (standByCueId_ == cueId) {
                standByCueId_.clear();
            }

            // Remove from active cues if present
            activeCues_.removeAll(cue);

            // Disconnect signals
            disconnectCueSignals(cue);

            // Remove from list
            cues_.removeAt(index);

            qDebug() << "Removed cue" << cue->number() << "at index" << index;

            emit cueRemoved(cueId, index);

            // Delete the cue
            cue->deleteLater();

            anyRemoved = true;
        }
    }

    if (anyRemoved) {
        markWorkspaceModified();
        updateStandByCue();
        ensureValidSelection();

        emit cueCountChanged();
        emit selectionChanged();
        emit playheadChanged();
    }

    return anyRemoved;
}

Cue* CueManager::getCue(const QString& cueId) const
{
    QReadLocker locker(&cueListLock_);

    for (Cue* cue : cues_) {
        if (cue->id() == cueId) {
            return cue;
        }
    }
    return nullptr;
}

QList<Cue*> CueManager::getCuesOfType(CueType type) const
{
    QReadLocker locker(&cueListLock_);

    QList<Cue*> result;
    for (Cue* cue : cues_) {
        if (cue->type() == type) {
            result.append(cue);
        }
    }
    return result;
}

int CueManager::findCueIndex(const QString& cueId) const
{
    for (int i = 0; i < cues_.size(); ++i) {
        if (cues_[i]->id() == cueId) {
            return i;
        }
    }
    return -1;
}

// Cue Organization

bool CueManager::moveCue(const QString& cueId, int newIndex)
{
    return moveCues(QStringList() << cueId, newIndex);
}

bool CueManager::moveCues(const QStringList& cueIds, int newIndex)
{
    QWriteLocker locker(&cueListLock_);

    if (cueIds.isEmpty() || newIndex < 0 || newIndex > cues_.size()) {
        return false;
    }

    // Collect cues to move and their current indices
    QList<QPair<Cue*, int>> cuesToMove;
    for (const QString& cueId : cueIds) {
        int index = findCueIndex(cueId);
        if (index >= 0) {
            cuesToMove.append(qMakePair(cues_[index], index));
        }
    }

    if (cuesToMove.isEmpty()) {
        return false;
    }

    // Sort by index (descending) to remove from back to front
    std::sort(cuesToMove.begin(), cuesToMove.end(),
        [](const QPair<Cue*, int>& a, const QPair<Cue*, int>& b) {
            return a.second > b.second;
        });

    // Remove cues from their current positions
    QList<Cue*> movedCues;
    for (const auto& pair : cuesToMove) {
        cues_.removeAt(pair.second);
        movedCues.prepend(pair.first); // Prepend to maintain order
    }

    // Adjust target index if cues were removed before it
    int adjustedIndex = newIndex;
    for (const auto& pair : cuesToMove) {
        if (pair.second < newIndex) {
            adjustedIndex--;
        }
    }

    // Insert cues at new position
    for (int i = 0; i < movedCues.size(); ++i) {
        cues_.insert(adjustedIndex + i, movedCues[i]);
    }

    markWorkspaceModified();

    for (Cue* cue : movedCues) {
        emit cueMoved(cue->id(), -1, adjustedIndex); // Old index not relevant for multiple moves
    }

    qDebug() << "Moved" << cuesToMove.size() << "cues to index" << adjustedIndex;

    return true;
}

bool CueManager::moveSelectedCues(int newIndex)
{
    QMutexLocker locker(&selectionMutex_);
    return moveCues(selectedCueIds_, newIndex);
}

QList<Cue*> CueManager::getFlattenedCues() const
{
    QReadLocker locker(&cueListLock_);

    QList<Cue*> flattened;

    for (Cue* cue : cues_) {
        flattened.append(cue);

        // Add group children if group is expanded
        if (cue->type() == CueType::Group) {
            GroupCue* group = qobject_cast<GroupCue*>(cue);
            if (group && isGroupExpanded(group->id())) {
                flattened.append(group->children());
            }
        }
    }

    return flattened;
}

QString CueManager::getNextCueNumber() const
{
    QReadLocker locker(&cueListLock_);

    // Find the highest numeric cue number
    double highestNumber = 0.0;
    for (Cue* cue : cues_) {
        bool ok;
        double number = cue->number().toDouble(&ok);
        if (ok && number > highestNumber) {
            highestNumber = number;
        }
    }

    return QString::number(highestNumber + 1.0, 'f', 0);
}

void CueManager::resequenceCues(const QString& startNumber, double increment)
{
    QWriteLocker locker(&cueListLock_);

    bool ok;
    double currentNumber = startNumber.toDouble(&ok);
    if (!ok) {
        currentNumber = 1.0;
    }

    for (Cue* cue : cues_) {
        cue->setNumber(QString::number(currentNumber, 'f', 0));
        currentNumber += increment;
    }

    markWorkspaceModified();
    qDebug() << "Resequenced" << cues_.size() << "cues starting from" << startNumber;
}

// Selection Management

void CueManager::selectCue(const QString& cueId)
{
    selectCues(QStringList() << cueId);
}

void CueManager::selectCues(const QStringList& cueIds)
{
    QMutexLocker locker(&selectionMutex_);

    QStringList validCueIds;
    for (const QString& cueId : cueIds) {
        if (getCue(cueId)) {
            validCueIds.append(cueId);
        }
    }

    if (selectedCueIds_ != validCueIds) {
        selectedCueIds_ = validCueIds;
        emit selectionChanged();
        emit selectedCuesChanged(selectedCueIds_);

        qDebug() << "Selected" << selectedCueIds_.size() << "cues";
    }
}

void CueManager::clearSelection()
{
    selectCues(QStringList());
}

void CueManager::selectAll()
{
    QReadLocker locker(&cueListLock_);

    QStringList allCueIds;
    for (Cue* cue : cues_) {
        allCueIds.append(cue->id());
    }

    selectCues(allCueIds);
}

void CueManager::toggleCueSelection(const QString& cueId)
{
    QMutexLocker locker(&selectionMutex_);

    QStringList newSelection = selectedCueIds_;
    if (newSelection.contains(cueId)) {
        newSelection.removeAll(cueId);
    }
    else {
        newSelection.append(cueId);
    }

    locker.unlock();
    selectCues(newSelection);
}

void CueManager::selectRange(const QString& startCueId, const QString& endCueId)
{
    QReadLocker locker(&cueListLock_);

    int startIndex = findCueIndex(startCueId);
    int endIndex = findCueIndex(endCueId);

    if (startIndex < 0 || endIndex < 0) {
        return;
    }

    if (startIndex > endIndex) {
        std::swap(startIndex, endIndex);
    }

    QStringList rangeCueIds;
    for (int i = startIndex; i <= endIndex; ++i) {
        rangeCueIds.append(cues_[i]->id());
    }

    locker.unlock();
    selectCues(rangeCueIds);
}

QList<Cue*> CueManager::getSelectedCues() const
{
    QMutexLocker locker(&selectionMutex_);

    QList<Cue*> selectedCues;
    for (const QString& cueId : selectedCueIds_) {
        Cue* cue = getCue(cueId);
        if (cue) {
            selectedCues.append(cue);
        }
    }
    return selectedCues;
}

bool CueManager::isCueSelected(const QString& cueId) const
{
    QMutexLocker locker(&selectionMutex_);
    return selectedCueIds_.contains(cueId);
}

// Playhead and Transport

Cue* CueManager::getStandByCue() const
{
    QMutexLocker locker(&playheadMutex_);
    return getCue(standByCueId_);
}

void CueManager::setStandByCue(const QString& cueId)
{
    QMutexLocker locker(&playheadMutex_);

    if (standByCueId_ != cueId) {
        standByCueId_ = cueId;
        locker.unlock();

        emit playheadChanged();
        emit standByCueChanged(cueId);

        qDebug() << "Standby cue set to" << cueId;
    }
}

void CueManager::advanceStandBy()
{
    QString nextCueId = findNextExecutableCue(standByCueId_);
    if (!nextCueId.isEmpty()) {
        setStandByCue(nextCueId);
    }
}

void CueManager::go()
{
    Cue* standbyCue = getStandByCue();
    if (!standbyCue) {
        qWarning() << "No standby cue to execute";
        return;
    }

    if (!standbyCue->canExecute()) {
        qWarning() << "Standby cue" << standbyCue->number() << "cannot be executed";
        return;
    }

    qDebug() << "Executing cue" << standbyCue->number();

    // Execute the cue
    standbyCue->trigger();

    // Add to active cues if it's now executing
    if (standbyCue->isExecuting()) {
        activeCues_.append(standbyCue);
        emit cueExecutionStarted(standbyCue->id());
    }

    // Advance standby if continue mode is enabled
    if (standbyCue->continueMode()) {
        advanceStandBy();
    }

    emit playbackStateChanged();
}

void CueManager::stop()
{
    qDebug() << "Stopping all active cues";

    QList<Cue*> cuesToStop = activeCues_;
    for (Cue* cue : cuesToStop) {
        cue->stop();
    }

    activeCues_.clear();
    isPaused_ = false;

    emit allCuesStopped();
    emit playbackStateChanged();
}

void CueManager::pause()
{
    if (activeCues_.isEmpty()) {
        return;
    }

    qDebug() << "Pausing" << activeCues_.size() << "active cues";

    for (Cue* cue : activeCues_) {
        if (cue->status() == CueStatus::Playing) {
            cue->pause();
        }
    }

    isPaused_ = true;
    emit playbackStateChanged();
}

void CueManager::resume()
{
    if (!isPaused_) {
        return;
    }

    qDebug() << "Resuming" << activeCues_.size() << "paused cues";

    for (Cue* cue : activeCues_) {
        if (cue->status() == CueStatus::Paused) {
            cue->resume();
        }
    }

    isPaused_ = false;
    emit playbackStateChanged();
}

void CueManager::panic()
{
    qWarning() << "PANIC STOP activated";

    // Immediately stop all cues
    for (Cue* cue : cues_) {
        if (cue->isExecuting()) {
            cue->stop(0.0); // No fade on panic
        }
    }

    activeCues_.clear();
    isPaused_ = false;

    emit allCuesStopped();
    emit playbackStateChanged();
}

void CueManager::stopSelectedCues()
{
    QList<Cue*> selectedCues = getSelectedCues();

    qDebug() << "Stopping" << selectedCues.size() << "selected cues";

    for (Cue* cue : selectedCues) {
        if (cue->isExecuting()) {
            cue->stop();
        }
    }

    emit playbackStateChanged();
}

void CueManager::stopCue(const QString& cueId)
{
    Cue* cue = getCue(cueId);
    if (cue && cue->isExecuting()) {
        qDebug() << "Stopping cue" << cue->number();
        cue->stop();
        emit playbackStateChanged();
    }
}

// Group Management

QString CueManager::createGroupFromSelection()
{
    QList<Cue*> selectedCues = getSelectedCues();
    if (selectedCues.isEmpty()) {
        return QString();
    }

    return createGroupFromCues(getSelectedCueIds());
}

QString CueManager::createGroupFromCues(const QStringList& cueIds)
{
    if (cueIds.isEmpty()) {
        return QString();
    }

    QWriteLocker locker(&cueListLock_);

    // Find the first cue's position for group placement
    int firstIndex = -1;
    QList<Cue*> cuesToGroup;

    for (const QString& cueId : cueIds) {
        Cue* cue = getCue(cueId);
        if (cue) {
            cuesToGroup.append(cue);
            int index = findCueIndex(cueId);
            if (firstIndex < 0 || index < firstIndex) {
                firstIndex = index;
            }
        }
    }

    if (cuesToGroup.isEmpty() || firstIndex < 0) {
        return QString();
    }

    // Create group cue
    GroupCue* group = new GroupCue(this);
    group->setNumber(getNextCueNumber());
    group->setName("Group");

    // Remove cues from main list and add to group
    for (Cue* cue : cuesToGroup) {
        int index = findCueIndex(cue->id());
        if (index >= 0) {
            cues_.removeAt(index);
            group->addChildCue(cue);

            // Adjust firstIndex if necessary
            if (index < firstIndex) {
                firstIndex--;
            }
        }
    }

    // Insert group at the first position
    cues_.insert(firstIndex, group);
    connectCueSignals(group);

    // Set group as expanded by default
    groupExpansionState_[group->id()] = true;

    markWorkspaceModified();

    QString groupId = group->id();
    qDebug() << "Created group" << group->number() << "with" << cuesToGroup.size() << "cues";

    emit groupCreated(groupId);
    emit cueAdded(group, firstIndex);
    emit cueCountChanged();

    return groupId;
}

bool CueManager::ungroupCues(const QString& groupId)
{
    Cue* cue = getCue(groupId);
    GroupCue* group = qobject_cast<GroupCue*>(cue);
    if (!group) {
        return false;
    }

    QWriteLocker locker(&cueListLock_);

    int groupIndex = findCueIndex(groupId);
    if (groupIndex < 0) {
        return false;
    }

    // Get group children
    QList<Cue*> children = group->children();

    // Remove group from main list
    cues_.removeAt(groupIndex);
    disconnectCueSignals(group);

    // Insert children at group position
    for (int i = 0; i < children.size(); ++i) {
        cues_.insert(groupIndex + i, children[i]);
    }

    // Remove group expansion state
    groupExpansionState_.remove(groupId);

    markWorkspaceModified();

    qDebug() << "Ungrouped" << children.size() << "cues from group" << group->number();

    emit groupRemoved(groupId);
    emit cueRemoved(groupId, groupIndex);

    // Add events for inserted children
    for (int i = 0; i < children.size(); ++i) {
        emit cueAdded(children[i], groupIndex + i);
    }

    emit cueCountChanged();

    // Delete the group
    group->deleteLater();

    return true;
}

bool CueManager::isGroupExpanded(const QString& groupId) const
{
    return groupExpansionState_.value(groupId, true); // Default to expanded
}

void CueManager::toggleGroupExpansion(const QString& groupId)
{
    bool expanded = isGroupExpanded(groupId);
    groupExpansionState_[groupId] = !expanded;

    emit groupExpansionChanged(groupId, !expanded);

    qDebug() << "Group" << groupId << (expanded ? "collapsed" : "expanded");
}

void CueManager::expandAllGroups()
{
    QReadLocker locker(&cueListLock_);

    for (Cue* cue : cues_) {
        if (cue->type() == CueType::Group) {
            groupExpansionState_[cue->id()] = true;
            emit groupExpansionChanged(cue->id(), true);
        }
    }
}

void CueManager::collapseAllGroups()
{
    QReadLocker locker(&cueListLock_);

    for (Cue* cue : cues_) {
        if (cue->type() == CueType::Group) {
            groupExpansionState_[cue->id()] = false;
            emit groupExpansionChanged(cue->id(), false);
        }
    }
}

QList<Cue*> CueManager::getGroupChildren(const QString& groupId) const
{
    Cue* cue = getCue(groupId);
    GroupCue* group = qobject_cast<GroupCue*>(cue);
    if (group) {
        return group->children();
    }
    return QList<Cue*>();
}

// Private Implementation

Cue* CueManager::createCueOfType(CueType type)
{
    switch (type) {
    case CueType::Audio:
        return new AudioCue(this);
    case CueType::Group:
        return new GroupCue(this);
        // Add other cue types as they're implemented
    default:
        qWarning() << "Unsupported cue type:" << static_cast<int>(type);
        return nullptr;
    }
}

void CueManager::insertCueAt(Cue* cue, int index)
{
    cues_.insert(qBound(0, index, cues_.size()), cue);
}

QString CueManager::generateUniqueCueId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void CueManager::connectCueSignals(Cue* cue)
{
    connect(cue, &Cue::cueUpdated, this, &CueManager::onCuePropertyChanged);
    connect(cue, &Cue::statusChanged, this, &CueManager::onCueStatusChanged);
    connect(cue, &Cue::executionFinished, this, &CueManager::onCueExecutionFinished);
}

void CueManager::disconnectCueSignals(Cue* cue)
{
    disconnect(cue, nullptr, this, nullptr);
}

// Private Slots

void CueManager::onCuePropertyChanged()
{
    Cue* cue = qobject_cast<Cue*>(sender());
    if (cue) {
        markWorkspaceModified();
        emit cueUpdated(cue);
    }
}

void CueManager::onCueStatusChanged()
{
    Cue* cue = qobject_cast<Cue*>(sender());
    if (cue) {
        emit playbackStateChanged();
    }
}

void CueManager::onCueExecutionFinished()
{
    Cue* cue = qobject_cast<Cue*>(sender());
    if (cue) {
        activeCues_.removeAll(cue);
        emit cueExecutionFinished(cue->id());
        emit playbackStateChanged();

        qDebug() << "Cue" << cue->number() << "finished execution";
    }
}

void CueManager::processCueExecution()
{
    // Remove finished cues from active list
    for (auto it = activeCues_.begin(); it != activeCues_.end();) {
        if (!(*it)->isExecuting()) {
            it = activeCues_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void CueManager::updateStandByCue()
{
    // If current standby cue is invalid, find the next valid one
    if (!getCue(standByCueId_)) {
        QString nextCueId = findNextExecutableCue(QString());
        setStandByCue(nextCueId);
    }
}

QString CueManager::findNextExecutableCue(const QString& fromCueId) const
{
    QReadLocker locker(&cueListLock_);

    int startIndex = 0;
    if (!fromCueId.isEmpty()) {
        startIndex = findCueIndex(fromCueId) + 1;
    }

    for (int i = startIndex; i < cues_.size(); ++i) {
        if (isCueExecutable(cues_[i])) {
            return cues_[i]->id();
        }
    }

    return QString(); // No executable cue found
}

bool CueManager::isCueExecutable(Cue* cue) const
{
    return cue && cue->canExecute();
}

void CueManager::ensureValidSelection()
{
    QMutexLocker locker(&selectionMutex_);

    QStringList validSelection;
    for (const QString& cueId : selectedCueIds_) {
        if (getCue(cueId)) {
            validSelection.append(cueId);
        }
    }

    if (validSelection.size() != selectedCueIds_.size()) {
        selectedCueIds_ = validSelection;
        locker.unlock();
        emit selectionChanged();
    }
}

void CueManager::markWorkspaceModified()
{
    if (!hasUnsavedChanges_) {
        hasUnsavedChanges_ = true;
        emit workspaceChanged();
        emit workspaceModified(true);
    }
}

// Workspace Management Stubs (to be fully implemented)

void CueManager::newWorkspace()
{
    clearWorkspace();
    workspacePath_.clear();
    hasUnsavedChanges_ = false;

    emit workspaceChanged();
    qDebug() << "New workspace created";
}

void CueManager::clearWorkspace()
{
    QWriteLocker locker(&cueListLock_);

    // Stop all cues
    stop();

    // Clear selection and playhead
    selectedCueIds_.clear();
    standByCueId_.clear();

    // Delete all cues
    qDeleteAll(cues_);
    cues_.clear();

    // Clear state
    activeCues_.clear();
    groupExpansionState_.clear();
    clipboard_ = QJsonArray();

    hasUnsavedChanges_ = false;
    statsValid_ = false;

    emit cueCountChanged();
    emit selectionChanged();
    emit playheadChanged();
}

// Status and Monitoring

bool CueManager::hasActiveCues() const
{
    return !activeCues_.isEmpty();
}

int CueManager::getBrokenCueCount() const
{
    return brokenCueCount_;
}

QList<Cue*> CueManager::getActiveCues() const
{
    return activeCues_;
}

QList<Cue*> CueManager::getBrokenCues() const
{
    QReadLocker locker(&cueListLock_);

    QList<Cue*> brokenCues;
    for (Cue* cue : cues_) {
        if (cue->status() == CueStatus::Broken) {
            brokenCues.append(cue);
        }
    }
    return brokenCues;
}