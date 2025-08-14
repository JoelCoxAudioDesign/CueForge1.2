// src/core/Cue.cpp - Base Cue Class Implementation
#include "Cue.h"

#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QUuid>

// Initialize static type string mapping
QHash<CueType, QString> Cue::typeStringMap_;

Cue::Cue(CueType type, QObject* parent)
    : QObject(parent)
    , id_(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , type_(type)
    , number_("1")
    , name_("Untitled Cue")
    , status_(CueStatus::Loaded)
    , armed_(false)
    , flagged_(false)
    , continueMode_(false)
    , color_(Qt::white)
    , notes_()
    , duration_(5.0)
    , preWait_(0.0)
    , postWait_(0.0)
    , currentPosition_(0.0)
    , targetId_()
    , createdTime_(QDateTime::currentDateTime())
    , modifiedTime_(QDateTime::currentDateTime())
    , lastExecutedTime_()
    , preWaitTimer_(new QTimer(this))
    , postWaitTimer_(new QTimer(this))
    , inPreWait_(false)
    , inPostWait_(false)
{
    // Initialize type string mapping if needed
    if (typeStringMap_.isEmpty()) {
        initializeTypeStringMap();
    }

    // Setup timing
    preWaitTimer_->setSingleShot(true);
    postWaitTimer_->setSingleShot(true);

    connect(preWaitTimer_, &QTimer::timeout, this, &Cue::onPreWaitFinished);
    connect(postWaitTimer_, &QTimer::timeout, this, &Cue::onPostWaitFinished);
}

// Property Setters

void Cue::setNumber(const QString& number)
{
    if (number_ != number) {
        number_ = number;
        markModified();
        emit numberChanged();
        emit cueUpdated();
    }
}

void Cue::setName(const QString& name)
{
    if (name_ != name) {
        name_ = name;
        markModified();
        emit nameChanged();
        emit cueUpdated();
    }
}

void Cue::setStatus(CueStatus status)
{
    if (status_ != status) {
        CueStatus oldStatus = status_;
        status_ = status;
        markModified();
        emit statusChanged();
        emit cueUpdated();

        qDebug() << "Cue" << number_ << "status changed from"
            << static_cast<int>(oldStatus) << "to" << static_cast<int>(status);
    }
}

void Cue::setArmed(bool armed)
{
    if (armed_ != armed) {
        armed_ = armed;
        markModified();
        emit armedChanged();
        emit cueUpdated();
    }
}

void Cue::setFlagged(bool flagged)
{
    if (flagged_ != flagged) {
        flagged_ = flagged;
        markModified();
        emit flaggedChanged();
        emit cueUpdated();
    }
}

void Cue::setContinueMode(bool continueMode)
{
    if (continueMode_ != continueMode) {
        continueMode_ = continueMode;
        markModified();
        emit continueModeChanged();
        emit cueUpdated();
    }
}

void Cue::setColor(const QColor& color)
{
    if (color_ != color) {
        color_ = color;
        markModified();
        emit colorChanged();
        emit cueUpdated();
    }
}

void Cue::setNotes(const QString& notes)
{
    if (notes_ != notes) {
        notes_ = notes;
        markModified();
        emit notesChanged();
        emit cueUpdated();
    }
}

void Cue::setDuration(double duration)
{
    if (qAbs(duration_ - duration) > 0.001) {
        duration_ = qMax(0.0, duration);
        markModified();
        emit durationChanged();
        emit cueUpdated();
    }
}

void Cue::setPreWait(double preWait)
{
    if (qAbs(preWait_ - preWait) > 0.001) {
        preWait_ = qMax(0.0, preWait);
        markModified();
        emit preWaitChanged();
        emit cueUpdated();
    }
}

void Cue::setPostWait(double postWait)
{
    if (qAbs(postWait_ - postWait) > 0.001) {
        postWait_ = qMax(0.0, postWait);
        markModified();
        emit postWaitChanged();
        emit cueUpdated();
    }
}

void Cue::setTargetId(const QString& targetId)
{
    if (targetId_ != targetId) {
        targetId_ = targetId;
        markModified();
        emit targetChanged();
        emit cueUpdated();
    }
}

// Custom Properties System

QVariant Cue::getCustomProperty(const QString& key, const QVariant& defaultValue) const
{
    return customProperties_.value(key, defaultValue);
}

void Cue::setCustomProperty(const QString& key, const QVariant& value)
{
    if (customProperties_.value(key) != value) {
        customProperties_[key] = value;
        markModified();
        emit customPropertyChanged(key, value);
        emit cueUpdated();
    }
}

// Display Helpers

QString Cue::displayName() const
{
    if (!name_.isEmpty()) {
        return QString("%1: %2").arg(number_, name_);
    }
    return number_;
}

QString Cue::statusString() const
{
    switch (status_) {
    case CueStatus::Loaded:     return "Loaded";
    case CueStatus::Playing:    return "Playing";
    case CueStatus::Paused:     return "Paused";
    case CueStatus::Stopped:    return "Stopped";
    case CueStatus::Loading:    return "Loading";
    case CueStatus::Broken:     return "Broken";
    case CueStatus::Armed:      return "Armed";
    default:                    return "Unknown";
    }
}

QString Cue::typeString() const
{
    return typeToString(type_);
}

QString Cue::typeToString(CueType type)
{
    if (typeStringMap_.isEmpty()) {
        const_cast<QHash<CueType, QString>&>(typeStringMap_) = QHash<CueType, QString>();
        initializeTypeStringMap();
    }
    return typeStringMap_.value(type, "Unknown");
}

CueType Cue::stringToType(const QString& typeStr)
{
    if (typeStringMap_.isEmpty()) {
        initializeTypeStringMap();
    }

    for (auto it = typeStringMap_.constBegin(); it != typeStringMap_.constEnd(); ++it) {
        if (it.value() == typeStr) {
            return it.key();
        }
    }
    return CueType::Audio; // Default fallback
}

void Cue::initializeTypeStringMap()
{
    typeStringMap_[CueType::Audio] = "Audio";
    typeStringMap_[CueType::Video] = "Video";
    typeStringMap_[CueType::MIDI] = "MIDI";
    typeStringMap_[CueType::Wait] = "Wait";
    typeStringMap_[CueType::Start] = "Start";
    typeStringMap_[CueType::Stop] = "Stop";
    typeStringMap_[CueType::Goto] = "Goto";
    typeStringMap_[CueType::Fade] = "Fade";
    typeStringMap_[CueType::Group] = "Group";
    typeStringMap_[CueType::Target] = "Target";
    typeStringMap_[CueType::Load] = "Load";
    typeStringMap_[CueType::Script] = "Script";
}

// Execution Interface

bool Cue::canExecute() const
{
    return status_ == CueStatus::Loaded || status_ == CueStatus::Armed;
}

void Cue::trigger()
{
    if (!canExecute()) {
        qWarning() << "Cannot execute cue" << number_ << "- status is" << statusString();
        return;
    }

    emit aboutToExecute();

    if (preWait_ > 0.0) {
        executePreWait();
    }
    else {
        execute();
    }
}

void Cue::stopCue()
{
    stop();
}

void Cue::pauseCue()
{
    pause();
}

void Cue::resumeCue()
{
    resume();
}

void Cue::reset()
{
    setStatus(CueStatus::Loaded);
    setCurrentPosition(0.0);

    if (preWaitTimer_->isActive()) {
        preWaitTimer_->stop();
    }
    if (postWaitTimer_->isActive()) {
        postWaitTimer_->stop();
    }

    inPreWait_ = false;
    inPostWait_ = false;
}

// Progress Management

void Cue::setProgress(double progress)
{
    setCurrentPosition(qBound(0.0, progress, 1.0));
}

// Serialization

QJsonObject Cue::toJson() const
{
    QJsonObject json;

    // Core properties
    json["id"] = id_;
    json["type"] = typeToString(type_);
    json["number"] = number_;
    json["name"] = name_;
    json["status"] = static_cast<int>(status_);

    // State flags
    json["armed"] = armed_;
    json["flagged"] = flagged_;
    json["continueMode"] = continueMode_;

    // Visual properties
    json["color"] = color_.name();
    json["notes"] = notes_;

    // Timing properties
    json["duration"] = duration_;
    json["preWait"] = preWait_;
    json["postWait"] = postWait_;

    // Target system
    if (!targetId_.isEmpty()) {
        json["targetId"] = targetId_;
    }

    // Timestamps
    json["createdTime"] = createdTime_.toString(Qt::ISODate);
    json["modifiedTime"] = modifiedTime_.toString(Qt::ISODate);
    if (lastExecutedTime_.isValid()) {
        json["lastExecutedTime"] = lastExecutedTime_.toString(Qt::ISODate);
    }

    // Custom properties
    if (!customProperties_.isEmpty()) {
        QJsonObject customProps;
        for (auto it = customProperties_.constBegin(); it != customProperties_.constEnd(); ++it) {
            customProps[it.key()] = QJsonValue::fromVariant(it.value());
        }
        json["customProperties"] = customProps;
    }

    return json;
}

bool Cue::fromJson(const QJsonObject& json)
{
    try {
        // Core properties
        if (json.contains("number")) {
            setNumber(json["number"].toString());
        }
        if (json.contains("name")) {
            setName(json["name"].toString());
        }
        if (json.contains("status")) {
            setStatus(static_cast<CueStatus>(json["status"].toInt()));
        }

        // State flags
        if (json.contains("armed")) {
            setArmed(json["armed"].toBool());
        }
        if (json.contains("flagged")) {
            setFlagged(json["flagged"].toBool());
        }
        if (json.contains("continueMode")) {
            setContinueMode(json["continueMode"].toBool());
        }

        // Visual properties
        if (json.contains("color")) {
            setColor(QColor(json["color"].toString()));
        }
        if (json.contains("notes")) {
            setNotes(json["notes"].toString());
        }

        // Timing properties
        if (json.contains("duration")) {
            setDuration(json["duration"].toDouble());
        }
        if (json.contains("preWait")) {
            setPreWait(json["preWait"].toDouble());
        }
        if (json.contains("postWait")) {
            setPostWait(json["postWait"].toDouble());
        }

        // Target system
        if (json.contains("targetId")) {
            setTargetId(json["targetId"].toString());
        }

        // Timestamps
        if (json.contains("createdTime")) {
            createdTime_ = QDateTime::fromString(json["createdTime"].toString(), Qt::ISODate);
        }
        if (json.contains("modifiedTime")) {
            modifiedTime_ = QDateTime::fromString(json["modifiedTime"].toString(), Qt::ISODate);
        }
        if (json.contains("lastExecutedTime")) {
            lastExecutedTime_ = QDateTime::fromString(json["lastExecutedTime"].toString(), Qt::ISODate);
        }

        // Custom properties
        if (json.contains("customProperties")) {
            QJsonObject customProps = json["customProperties"].toObject();
            customProperties_.clear();
            for (auto it = customProps.constBegin(); it != customProps.constEnd(); ++it) {
                customProperties_[it.key()] = it.value().toVariant();
            }
        }

        return true;
    }
    catch (...) {
        qCritical() << "Failed to deserialize cue from JSON";
        return false;
    }
}

// Protected Implementation

void Cue::markModified()
{
    modifiedTime_ = QDateTime::currentDateTime();
}

void Cue::setCurrentPosition(double position)
{
    double newPosition = qBound(0.0, position, 1.0);
    if (qAbs(currentPosition_ - newPosition) > 0.001) {
        currentPosition_ = newPosition;
        emit progressChanged(currentPosition_);
    }
}

void Cue::executePreWait()
{
    if (preWait_ <= 0.0) {
        execute();
        return;
    }

    inPreWait_ = true;
    setStatus(CueStatus::Loading);
    preWaitTimer_->start(static_cast<int>(preWait_ * 1000));

    qDebug() << "Cue" << number_ << "starting pre-wait of" << preWait_ << "seconds";
}

void Cue::executePostWait()
{
    if (postWait_ <= 0.0) {
        cleanupExecution();
        return;
    }

    inPostWait_ = true;
    postWaitTimer_->start(static_cast<int>(postWait_ * 1000));

    qDebug() << "Cue" << number_ << "starting post-wait of" << postWait_ << "seconds";
}

void Cue::cleanupExecution()
{
    lastExecutedTime_ = QDateTime::currentDateTime();
    setCurrentPosition(1.0);
    setStatus(CueStatus::Stopped);

    inPreWait_ = false;
    inPostWait_ = false;

    emit executionFinished();

    qDebug() << "Cue" << number_ << "execution completed";
}

// Private Slots

void Cue::onPreWaitFinished()
{
    inPreWait_ = false;
    qDebug() << "Cue" << number_ << "pre-wait finished, starting execution";
    execute();
}

void Cue::onPostWaitFinished()
{
    inPostWait_ = false;
    qDebug() << "Cue" << number_ << "post-wait finished";
    cleanupExecution();
}