#include "DCLinkMissionEditor.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>
#include <QtCore/QStandardPaths>
#include <QtCore/QUuid>

#include <algorithm>
#include <cmath>

#include "DCLinkMissionClient.h"
#include "DCLinkMissionJson.h"
#include "DigiviewManager.h"

namespace {
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool validId(const QString& id)
{
    const QUuid uuid(id);
    return uuid.version() == QUuid::Random && uuid.variant() == QUuid::DCE && uuid.toString(QUuid::WithoutBraces) == id;
}

bool validMetadata(const QVariantMap& metadata)
{
    const QJsonValue revision = QJsonObject::fromVariantMap(metadata).value(QStringLiteral("revision"));
    const double number = revision.toDouble();
    return validId(metadata.value(QStringLiteral("mission_id")).toString()) && revision.isDouble() &&
           std::isfinite(number) && number >= 1 && number <= 4294967295.0 && std::floor(number) == number;
}

QString draftsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/dclink-mission-drafts.json");
}

QString endpointText(const QVariantMap& endpoint)
{
    if (endpoint.isEmpty()) {
        return DCLinkMissionEditor::tr("Local draft · no destination");
    }
    return QStringLiteral("%1:%2 · %3/%4")
        .arg(endpoint.value(QStringLiteral("host")).toString())
        .arg(endpoint.value(QStringLiteral("port")).toInt())
        .arg(endpoint.value(QStringLiteral("peer_system_id")).toInt())
        .arg(endpoint.value(QStringLiteral("peer_component_id")).toInt());
}

QVariantMap emptyMission(const QString& name)
{
    return {{QStringLiteral("schema_version"), DCLinkMissionJson::SCHEMA_VERSION},
            {QStringLiteral("name"), name},
            {QStringLiteral("altitude_reference"), QStringLiteral("relative_home")},
            {QStringLiteral("waypoints"), QVariantList{}},
            {QStringLiteral("rules"), QVariantList{}},
            {QStringLiteral("areas"), QVariantList{}}};
}
}  // namespace

DCLinkMissionEditor::DCLinkMissionEditor(DCLinkMissionClient* client, DigiviewManager* digiview, QObject* parent)
    : QObject(parent), _client(client), _digiview(digiview), _processId(newId())
{
    _restore();
    connect(_client, &DCLinkMissionClient::finished, this, &DCLinkMissionEditor::_finished);
    connect(_client, &DCLinkMissionClient::stateChanged, this, [this]() {
        if (!_client->available() || _verifiedEndpoint != _endpoint() ||
            _verifiedSession != _client->serviceSession()) {
            _verifiedSession.clear();
            _invalidateCatalog();
        }
        emit changed();
    });
    connect(&_runPoll, &QTimer::timeout, this, &DCLinkMissionEditor::_pollRunStatus);
    _runPoll.start(1000);
}

QVariantMap DCLinkMissionEditor::_endpoint() const
{
    return {{QStringLiteral("host"), _digiview->host()},
            {QStringLiteral("port"), _digiview->port()},
            {QStringLiteral("listen_port"), _digiview->listenPort()},
            {QStringLiteral("peer_system_id"), _client->peerSystemId()},
            {QStringLiteral("peer_component_id"), _client->peerComponentId()}};
}

QVariantMap DCLinkMissionEditor::_draft() const
{
    return _drafts.value(_draftId);
}

QVariantMap DCLinkMissionEditor::mission() const
{
    return _draft().value(QStringLiteral("mission")).toMap();
}

QVariantList DCLinkMissionEditor::waypoints() const
{
    return mission().value(QStringLiteral("waypoints")).toList();
}

QString DCLinkMissionEditor::missionId() const
{
    return _draft().value(QStringLiteral("mission_id")).toString();
}

quint32 DCLinkMissionEditor::revision() const
{
    return _draft().value(QStringLiteral("revision")).toUInt();
}

QString DCLinkMissionEditor::currentEntry() const
{
    return QStringLiteral("d:") + _draftId;
}

bool DCLinkMissionEditor::dirty() const
{
    return !mission().isEmpty() &&
           (missionId().isEmpty() || mission() != _draft().value(QStringLiteral("base")).toMap());
}

bool DCLinkMissionEditor::readOnly() const
{
    return _draft().value(QStringLiteral("read_only")).toBool();
}

bool DCLinkMissionEditor::busy() const
{
    return !_requestId.isEmpty() || (_client->busy() && !_client->readingRunStatus());
}

bool DCLinkMissionEditor::canEdit() const
{
    return !readOnly() && !busy() && !reviewRequired() && !mission().isEmpty();
}

bool DCLinkMissionEditor::fresh() const
{
    return _client->available() && !_verifiedSession.isEmpty() && _verifiedSession == _client->serviceSession() &&
           _verifiedEndpoint == _endpoint();
}

QString DCLinkMissionEditor::source() const
{
    return endpointText(_endpoint());
}

bool DCLinkMissionEditor::destinationMatches() const
{
    const QVariantMap bound = _draft().value(QStringLiteral("endpoint")).toMap();
    return bound.isEmpty() || bound == _endpoint();
}

QString DCLinkMissionEditor::draftSource() const
{
    return endpointText(_draft().value(QStringLiteral("endpoint")).toMap());
}

bool DCLinkMissionEditor::canSave() const
{
    return saveReason().isEmpty();
}

bool DCLinkMissionEditor::conflict() const
{
    if (_conflict) {
        return true;
    }
    if (missionId().isEmpty() || !_reviewedCatalog || !fresh() ||
        _draft().value(QStringLiteral("endpoint")).toMap() != _catalogEndpoint) {
        return false;
    }
    for (const QVariant& entry : _catalog) {
        const QVariantMap record = entry.toMap();
        if (record.value(QStringLiteral("mission_id")).toString() == missionId()) {
            return record.value(QStringLiteral("revision")).toUInt() > revision();
        }
    }
    return true;
}

QString DCLinkMissionEditor::saveReason() const
{
    if (!_storageError.isEmpty()) {
        return _storageError;
    }
    if (busy()) {
        return tr("Waiting for DCLink…");
    }
    if (reviewRequired()) {
        if (executionReviewRequired()) {
            return tr("Review the mission request in Fly View.");
        }
        return tr(
            "The previous change may have been saved. Refresh and review the library before making another change.");
    }
    if (readOnly()) {
        return tr("Unsupported mission features. This document is read only.");
    }
    if (!dirty()) {
        return tr("No unsaved changes.");
    }
    const QJsonObject content = QJsonObject::fromVariantMap(mission());
    const QString validation = DCLinkMissionJson::validateMission(content);
    if (!validation.isEmpty()) {
        return validation;
    }
    if (!fresh()) {
        return tr("Connect DigiView, then refresh the mission library.");
    }
    if (!content.value(QStringLiteral("rules")).toArray().isEmpty() &&
        !_client->capabilities().value(QStringLiteral("features")).toStringList().contains(
            QStringLiteral("waypoint_rules"))) {
        return tr("This DCLink does not support waypoint rules.");
    }
    if (!destinationMatches()) {
        return tr("This draft belongs to another DCLink. Return to it, or make a copy for this destination.");
    }
    if (conflict()) {
        return tr("The saved mission changed. Reload it or make a copy to preserve your edits.");
    }
    const QString action =
        missionId().isEmpty() ? QStringLiteral("missions.create") : QStringLiteral("missions.update");
    if (!_client->capabilities().value(QStringLiteral("operations")).toStringList().contains(action)) {
        return tr("This DCLink does not support saving this mission.");
    }
    return {};
}

QString DCLinkMissionEditor::status() const
{
    if (!hasDraft()) {
        return tr("Choose a saved mission or select New.");
    }
    if (reviewRequired() && _pendingMutation.value(QStringLiteral("draft_id")).toString() == _draftId) {
        return tr("Change outcome unknown · review required");
    }
    if (readOnly()) {
        return tr("Read only");
    }
    if (conflict()) {
        return tr("Revision conflict · local changes kept");
    }
    if (missionId().isEmpty()) {
        return tr("Local draft");
    }
    return dirty() ? tr("Unsaved changes · based on revision %1").arg(revision())
                   : tr("Saved revision %1%2")
                         .arg(revision())
                         .arg(fresh() && destinationMatches() ? QString() : tr(" · last known"));
}

QVariantList DCLinkMissionEditor::entries() const
{
    QVariantList result;
    QSet<QString> localMissions;
    QMap<QString, int> names;
    for (const QVariantMap& draft : _drafts) {
        ++names[draft.value(QStringLiteral("mission")).toMap().value(QStringLiteral("name")).toString()];
        const QString id = draft.value(QStringLiteral("mission_id")).toString();
        if (draft.value(QStringLiteral("endpoint")).toMap() == _endpoint() && !id.isEmpty()) {
            localMissions.insert(id);
        }
    }
    if (_catalogEndpoint == _endpoint()) {
        for (const QVariant& entry : _catalog) {
            const QVariantMap record = entry.toMap();
            if (!localMissions.contains(record.value(QStringLiteral("mission_id")).toString())) {
                ++names[record.value(QStringLiteral("name")).toString()];
            }
        }
    }
    for (auto it = _drafts.cbegin(); it != _drafts.cend(); ++it) {
        const QVariantMap draft = it.value();
        const QVariantMap content = draft.value(QStringLiteral("mission")).toMap();
        const QString id = draft.value(QStringLiteral("mission_id")).toString();
        QString label = content.value(QStringLiteral("name")).toString();
        if (names.value(label) > 1) {
            label += QStringLiteral(" · %1").arg((id.isEmpty() ? it.key() : id).left(8));
        }
        label += id.isEmpty() ? tr(" · local draft")
                              : (content != draft.value(QStringLiteral("base")).toMap()
                                     ? tr(" · unsaved changes")
                                     : tr(" · revision %1").arg(draft.value(QStringLiteral("revision")).toUInt()));
        const QVariantMap endpoint = draft.value(QStringLiteral("endpoint")).toMap();
        if (!endpoint.isEmpty() && endpoint != _endpoint()) {
            label += tr(" · other DCLink");
        }
        result.append(
            QVariantMap{{QStringLiteral("key"), QStringLiteral("d:") + it.key()}, {QStringLiteral("label"), label}});
    }
    if (_catalogEndpoint == _endpoint()) {
        for (const QVariant& entry : _catalog) {
            const QVariantMap record = entry.toMap();
            const QString id = record.value(QStringLiteral("mission_id")).toString();
            if (localMissions.contains(id)) {
                continue;
            }
            QString label = record.value(QStringLiteral("name")).toString();
            if (names.value(label) > 1) {
                label += QStringLiteral(" · %1").arg(id.left(8));
            }
            label += tr(" · revision %1").arg(record.value(QStringLiteral("revision")).toUInt());
            result.append(
                QVariantMap{{QStringLiteral("key"), QStringLiteral("s:") + id}, {QStringLiteral("label"), label}});
        }
    }
    return result;
}

void DCLinkMissionEditor::_setDraft(const QVariantMap& draft)
{
    _drafts.insert(_draftId, draft);
    _persist();
    emit changed();
}

void DCLinkMissionEditor::_editMission(const QVariantMap& content)
{
    QVariantMap draft = _draft();
    draft.insert(QStringLiteral("mission"), content);
    _setDraft(draft);
}

void DCLinkMissionEditor::_createDraft(const QVariantMap& content)
{
    if (_drafts.size() >= 50) {
        _error = tr("Keep at most 50 local drafts. Remove an unused local draft first.");
        emit changed();
        return;
    }
    _draftId = newId();
    _selectedWaypoint = -1;
    _conflict = false;
    _error = _storageError;
    _setDraft({{QStringLiteral("mission"), content},
               {QStringLiteral("base"), QVariantMap{}},
               {QStringLiteral("mission_id"), QString()},
               {QStringLiteral("revision"), 0},
               {QStringLiteral("endpoint"), QVariantMap{}},
               {QStringLiteral("read_only"), false}});
}

void DCLinkMissionEditor::newDraft()
{
    if (!busy() && !reviewRequired()) {
        _createDraft(emptyMission(tr("New mission")));
    }
}

void DCLinkMissionEditor::duplicateDraft()
{
    if (busy() || reviewRequired() || readOnly() || !hasDraft()) {
        return;
    }
    QVariantMap copy = mission();
    const auto name = tr("%1 copy").arg(copy.value(QStringLiteral("name")).toString()).toStdU32String();
    copy.insert(QStringLiteral("name"), QString::fromStdU32String(name.substr(0, 80)));
    _createDraft(copy);
}

void DCLinkMissionEditor::discardDraft()
{
    if (busy() || reviewRequired()) {
        return;
    }
    _drafts.remove(_draftId);
    _draftId.clear();
    _persist();
    emit changed();
}

void DCLinkMissionEditor::selectEntry(const QString& key)
{
    if (busy() || key == currentEntry()) {
        return;
    }
    if (key.startsWith(QStringLiteral("d:")) && _drafts.contains(key.mid(2))) {
        _draftId = key.mid(2);
        _conflict = false;
        _error = _storageError;
        _selectedWaypoint = -1;
        _persist();
        emit changed();
        emit routeLoaded();
    } else if (key.startsWith(QStringLiteral("s:")) && fresh()) {
        _send(QStringLiteral("missions.get"), {{QStringLiteral("mission_id"), key.mid(2)}}, QStringLiteral("load"));
    }
}

void DCLinkMissionEditor::setName(const QString& name)
{
    if (canEdit()) {
        QVariantMap content = mission();
        content.insert(QStringLiteral("name"), name);
        _editMission(content);
    }
}

void DCLinkMissionEditor::setSelectedWaypoint(int index)
{
    _selectedWaypoint = index >= 0 && index < waypoints().size() ? index : -1;
    emit changed();
}

void DCLinkMissionEditor::addWaypoint(double latitude, double longitude)
{
    if (!canEdit() || !std::isfinite(latitude) || !std::isfinite(longitude) || std::abs(latitude) > 90 ||
        std::abs(longitude) > 180 || waypoints().size() >= 1000) {
        return;
    }
    QVariantList points = waypoints();
    const double altitude =
        points.isEmpty() ? 30.0 : points.last().toMap().value(QStringLiteral("altitude_m")).toDouble();
    points.append(QVariantMap{{QStringLiteral("id"), newId()},
                              {QStringLiteral("latitude_deg"), latitude},
                              {QStringLiteral("longitude_deg"), longitude},
                              {QStringLiteral("altitude_m"), altitude},
                              {QStringLiteral("acceptance_radius_m"), 30.0}});
    _selectedWaypoint = points.size() - 1;
    QVariantMap content = mission();
    content.insert(QStringLiteral("waypoints"), points);
    _editMission(content);
}

void DCLinkMissionEditor::updateWaypoint(int index, const QString& field, double value)
{
    if (!canEdit() || !std::isfinite(value) || index < 0 || index >= waypoints().size() ||
        !QStringList{QStringLiteral("latitude_deg"), QStringLiteral("longitude_deg"), QStringLiteral("altitude_m"),
                     QStringLiteral("acceptance_radius_m")}
             .contains(field) ||
        (field == QStringLiteral("latitude_deg") && std::abs(value) > 90) ||
        (field == QStringLiteral("longitude_deg") && std::abs(value) > 180) ||
        (field == QStringLiteral("acceptance_radius_m") && value <= 0)) {
        return;
    }
    QVariantList points = waypoints();
    QVariantMap point = points[index].toMap();
    point.insert(field, value);
    points[index] = point;
    QVariantMap content = mission();
    content.insert(QStringLiteral("waypoints"), points);
    _editMission(content);
}

void DCLinkMissionEditor::moveWaypoint(int from, int to)
{
    QVariantList points = waypoints();
    if (!canEdit() || from < 0 || to < 0 || from >= points.size() || to >= points.size()) {
        return;
    }
    points.move(from, to);
    _selectedWaypoint = to;
    QVariantMap content = mission();
    content.insert(QStringLiteral("waypoints"), points);
    _editMission(content);
}

void DCLinkMissionEditor::setWaypointCoordinate(int index, double latitude, double longitude)
{
    if (!canEdit() || index < 0 || index >= waypoints().size() || !std::isfinite(latitude) ||
        !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180) {
        return;
    }
    QVariantList points = waypoints();
    QVariantMap point = points[index].toMap();
    point.insert(QStringLiteral("latitude_deg"), latitude);
    point.insert(QStringLiteral("longitude_deg"), longitude);
    points[index] = point;
    QVariantMap content = mission();
    content.insert(QStringLiteral("waypoints"), points);
    _editMission(content);
}

void DCLinkMissionEditor::removeWaypoint(int index)
{
    QVariantList points = waypoints();
    if (!canEdit() || index < 0 || index >= points.size()) {
        return;
    }
    points.removeAt(index);
    _selectedWaypoint = std::min(index, int(points.size()) - 1);
    QVariantMap content = mission();
    content.insert(QStringLiteral("waypoints"), points);
    _editMission(content);
}

bool DCLinkMissionEditor::_send(const QString& action, const QVariantMap& payload, const QString& purpose,
                                const QString& requestId)
{
    if (busy()) {
        return false;
    }
    _requestId = requestId.isEmpty() ? newId() : requestId;
    _requestedMissionId = payload.value(QStringLiteral("mission_id")).toString();
    _purpose = purpose;
    _error.clear();
    const QString sent = _client->request(action, payload, _requestId);
    if (sent.isEmpty()) {
        _requestId.clear();
        _purpose.clear();
        _error = _client->error();
    }
    emit changed();
    return !sent.isEmpty();
}

void DCLinkMissionEditor::refresh()
{
    if (!busy()) {
        _verifiedSession.clear();
        _invalidateCatalog();
        _listRetries = 0;
        _send(QStringLiteral("missions.capabilities"), {}, QStringLiteral("discover"));
    }
}

void DCLinkMissionEditor::_invalidateCatalog()
{
    if (_reviewedCatalog) {
        _reviewedCatalog = false;
        emit catalogChanged();
    }
}

void DCLinkMissionEditor::_listPage(const QString& cursor)
{
    QVariantMap payload{{QStringLiteral("limit"), 50}};
    if (!cursor.isEmpty()) {
        payload.insert(QStringLiteral("cursor"), cursor);
    }
    _send(QStringLiteral("missions.list"), payload, QStringLiteral("list"));
}

void DCLinkMissionEditor::_mutate(const QString& action, const QVariantMap& payload)
{
    const QString id = newId();
    _pendingMutation = {
        {QStringLiteral("action"), action},        {QStringLiteral("payload"), payload},
        {QStringLiteral("request_id"), id},        {QStringLiteral("draft_id"), _draftId},
        {QStringLiteral("endpoint"), _endpoint()}, {QStringLiteral("session"), _client->serviceSession()},
        {QStringLiteral("process"), _processId}};
    const QString purpose = executionReviewRequired() ? QStringLiteral("execution") : QStringLiteral("mutation");
    if (purpose == QStringLiteral("execution")) {
        _pendingMutation.remove(QStringLiteral("draft_id"));
    } else {
        _invalidateCatalog();
    }
    if (!_persist()) {
        _pendingMutation.clear();
        emit changed();
        return;
    }
    if (!_send(action, payload, purpose, id)) {
        _pendingMutation.clear();
        _persist();
    }
}

QString DCLinkMissionEditor::applyRule(const QString& id, const QVariantMap& trigger, const QVariantList& actions)
{
    if (!canEdit()) {
        return tr("This mission cannot be edited right now.");
    }
    QVariantMap content = mission();
    QVariantList rules = content.value(QStringLiteral("rules")).toList();
    qsizetype index = rules.size();
    if (id.isEmpty()) {
        if (rules.size() >= DCLinkMissionJson::MAX_RULES) {
            return tr("A mission can contain at most %1 rules.").arg(DCLinkMissionJson::MAX_RULES);
        }
    } else {
        for (qsizetype i = 0; i < rules.size(); ++i) {
            if (rules.at(i).toMap().value(QStringLiteral("id")).toString() == id) {
                index = i;
                break;
            }
        }
        if (index == rules.size()) {
            return tr("This rule has changed. Open it again to edit.");
        }
    }
    const QVariantMap rule{{QStringLiteral("id"), id.isEmpty() ? newId() : id},
                           {QStringLiteral("trigger"), trigger},
                           {QStringLiteral("actions"), actions}};
    const QString error = DCLinkMissionJson::validateRule(
        QJsonObject::fromVariantMap(rule), QJsonArray::fromVariantList(waypoints()));
    if (!error.isEmpty()) {
        return error;
    }
    if (id.isEmpty()) {
        rules.append(rule);
    } else {
        rules[index] = rule;
    }
    content.insert(QStringLiteral("rules"), rules);
    _editMission(content);
    return {};
}

void DCLinkMissionEditor::removeRule(int index)
{
    QVariantMap content = mission();
    QVariantList rules = content.value(QStringLiteral("rules")).toList();
    if (!canEdit() || index < 0 || index >= rules.size()) {
        return;
    }
    rules.removeAt(index);
    content.insert(QStringLiteral("rules"), rules);
    _editMission(content);
}

void DCLinkMissionEditor::moveRule(int from, int to)
{
    QVariantMap content = mission();
    QVariantList rules = content.value(QStringLiteral("rules")).toList();
    if (!canEdit() || from < 0 || to < 0 || from >= rules.size() || to >= rules.size() || from == to) {
        return;
    }
    rules.move(from, to);
    content.insert(QStringLiteral("rules"), rules);
    _editMission(content);
}

void DCLinkMissionEditor::save()
{
    if (!canSave()) {
        _error = saveReason();
        emit changed();
        return;
    }
    if (missionId().isEmpty()) {
        QVariantMap draft = _draft();
        draft.insert(QStringLiteral("endpoint"), _endpoint());
        _setDraft(draft);
        _mutate(QStringLiteral("missions.create"), {{QStringLiteral("mission"), mission()}});
    } else {
        // Refresh the restored draft's base before uploading; the service checks again at commit.
        _send(QStringLiteral("missions.get"), {{QStringLiteral("mission_id"), missionId()}},
              QStringLiteral("check_save"));
    }
}

void DCLinkMissionEditor::reload()
{
    if (!busy() && fresh() && !missionId().isEmpty() &&
        _draft().value(QStringLiteral("endpoint")).toMap() == _endpoint()) {
        _send(QStringLiteral("missions.get"), {{QStringLiteral("mission_id"), missionId()}}, QStringLiteral("reload"));
    }
}

void DCLinkMissionEditor::deleteMission()
{
    if (!busy() && !reviewRequired() && fresh() && !missionId().isEmpty() &&
        _draft().value(QStringLiteral("endpoint")).toMap() == _endpoint() && _storageError.isEmpty()) {
        _mutate(QStringLiteral("missions.delete"),
                {{QStringLiteral("mission_id"), missionId()}, {QStringLiteral("expected_revision"), revision()}});
    }
}

bool DCLinkMissionEditor::canRetry() const
{
    return !busy() && fresh() && reviewRequired() && !executionReviewRequired() &&
           _pendingMutation.value(QStringLiteral("process")).toString() == _processId &&
           _pendingMutation.value(QStringLiteral("session")).toString() == _client->serviceSession() &&
           _pendingMutation.value(QStringLiteral("endpoint")).toMap() == _endpoint();
}

void DCLinkMissionEditor::retryChange()
{
    if (canRetry()) {
        _send(_pendingMutation.value(QStringLiteral("action")).toString(),
              _pendingMutation.value(QStringLiteral("payload")).toMap(), QStringLiteral("mutation"),
              _pendingMutation.value(QStringLiteral("request_id")).toString());
    }
}

void DCLinkMissionEditor::acknowledgeReview()
{
    if (canAcknowledgeReview()) {
        _client->acknowledgeOutcome();
        _pendingMutation.clear();
        _persist();
        emit changed();
    }
}

bool DCLinkMissionEditor::canAcknowledgeReview() const
{
    return !busy() && fresh() && reviewRequired() &&
           (executionReviewRequired() ? _executionStatusFresh() : _reviewedCatalog) &&
           _pendingMutation.value(QStringLiteral("endpoint")).toMap() == _endpoint();
}

QString DCLinkMissionEditor::reviewDescription() const
{
    if (!reviewRequired()) {
        return {};
    }
    if (executionReviewRequired()) {
        return tr("Mission command outcome unknown at %1. Check the aircraft and current run before continuing.")
            .arg(endpointText(_pendingMutation.value(QStringLiteral("endpoint")).toMap()));
    }
    return tr("Unconfirmed change at %1. Refresh that library and inspect its saved missions before continuing.")
        .arg(endpointText(_pendingMutation.value(QStringLiteral("endpoint")).toMap()));
}

void DCLinkMissionEditor::_loadRecord(const QVariantMap& record)
{
    if (!validMetadata(record) || !QJsonObject::fromVariantMap(record).value(QStringLiteral("mission")).isObject()) {
        _error = tr("DCLink returned an invalid mission record. The current draft is preserved.");
        return;
    }
    const QVariantMap content = record.value(QStringLiteral("mission")).toMap();
    const QString validation = DCLinkMissionJson::validateMission(QJsonObject::fromVariantMap(content));
    const QString id = record.value(QStringLiteral("mission_id")).toString();
    QString target;
    for (auto it = _drafts.cbegin(); it != _drafts.cend(); ++it) {
        if (it.value().value(QStringLiteral("mission_id")).toString() == id &&
            it.value().value(QStringLiteral("endpoint")).toMap() == _endpoint()) {
            target = it.key();
            break;
        }
    }
    if (target.isEmpty() && _drafts.size() >= 50) {
        _error = tr("Remove an unused local draft before opening another mission.");
        return;
    }
    _draftId = target.isEmpty() ? newId() : target;
    _conflict = false;
    _selectedWaypoint = -1;
    _setDraft({{QStringLiteral("mission"), content},
               {QStringLiteral("base"), content},
               {QStringLiteral("mission_id"), id},
               {QStringLiteral("revision"), record.value(QStringLiteral("revision"))},
               {QStringLiteral("endpoint"), _endpoint()},
               {QStringLiteral("read_only"), !validation.isEmpty()}});
    _error = validation;
    emit routeLoaded();
}

void DCLinkMissionEditor::_applyMutation(const QVariantMap& data)
{
    const QString action = _pendingMutation.value(QStringLiteral("action")).toString();
    const QVariantMap payload = _pendingMutation.value(QStringLiteral("payload")).toMap();
    const bool creating = action == QStringLiteral("missions.create");
    const quint64 expected = creating ? 1
                                      : payload.value(QStringLiteral("expected_revision")).toULongLong() +
                                            (action == QStringLiteral("missions.update") ? 1 : 0);
    if (!validMetadata(data) || data.value(QStringLiteral("revision")).toULongLong() != expected ||
        (!creating && data.value(QStringLiteral("mission_id")) != payload.value(QStringLiteral("mission_id")))) {
        _error = tr("The save confirmation is invalid. Local work is retained; refresh and review the library.");
        return;
    }
    const QString draftId = _pendingMutation.value(QStringLiteral("draft_id")).toString();
    if (action == QStringLiteral("missions.delete")) {
        const QString deletedId = data.value(QStringLiteral("mission_id")).toString();
        const QVariantMap endpoint = _pendingMutation.value(QStringLiteral("endpoint")).toMap();
        for (auto it = _drafts.begin(); it != _drafts.end(); ++it) {
            if (it.value().value(QStringLiteral("mission_id")).toString() == deletedId &&
                it.value().value(QStringLiteral("endpoint")).toMap() == endpoint) {
                it.value().insert(QStringLiteral("mission_id"), QString());
                it.value().insert(QStringLiteral("revision"), 0);
                it.value().insert(QStringLiteral("base"), QVariantMap{});
            }
        }
    } else if (_drafts.contains(draftId)) {
        QVariantMap draft = _drafts.value(draftId);
        const QVariantMap content =
            _pendingMutation.value(QStringLiteral("payload")).toMap().value(QStringLiteral("mission")).toMap();
        draft.insert(QStringLiteral("mission"), content);
        draft.insert(QStringLiteral("base"), content);
        draft.insert(QStringLiteral("mission_id"), data.value(QStringLiteral("mission_id")));
        draft.insert(QStringLiteral("revision"), data.value(QStringLiteral("revision")));
        draft.insert(QStringLiteral("endpoint"), _pendingMutation.value(QStringLiteral("endpoint")));
        _drafts.insert(draftId, draft);
    }
    _pendingMutation.clear();
    _conflict = false;
    if (!_drafts.contains(_draftId)) {
        _createDraft(emptyMission(tr("New mission")));
    }
    _persist();
}

void DCLinkMissionEditor::_finished(const QString& id, const QString& action, const QVariantMap& result)
{
    if (id == _runStatusRequest) {
        _runStatusRequest.clear();
        if (result.value(QStringLiteral("result")).toString() == QStringLiteral("ok")) {
            _receiveRunStatus(result.value(QStringLiteral("data")).toMap());
        }
        return;
    }
    if (id != _requestId) {
        return;
    }
    const QString purpose = _purpose;
    const QString requestedMission = _requestedMissionId;
    _purpose.clear();
    _requestId.clear();
    const QString code = result.value(QStringLiteral("result")).toString();
    const QVariantMap data = result.value(QStringLiteral("data")).toMap();
    if (purpose == QStringLiteral("execution")) {
        _executionFinished(action, code, data);
        return;
    }
    if (code != QStringLiteral("ok")) {
        if (purpose == QStringLiteral("list") && code == QStringLiteral("conflict") && _listRetries++ == 0) {
            _catalogPages.clear();
            _listCursors.clear();
            _listPage();
            return;
        }
        if (purpose == QStringLiteral("mutation") && !_client->outcomeUnknown()) {
            _pendingMutation.clear();
            _persist();
        }
        _conflict = (purpose == QStringLiteral("check_save") || purpose == QStringLiteral("mutation")) &&
                    (code == QStringLiteral("conflict") || code == QStringLiteral("not_found"));
        _error = _conflict ? tr("The saved mission changed or was deleted. Your local draft is preserved.")
                           : tr("DCLink: %1 — %2").arg(code, data.value(QStringLiteral("reason")).toString());
        for (const QVariant& detail : data.value(QStringLiteral("errors")).toList()) {
            const QVariantMap issue = detail.toMap();
            _error += QStringLiteral("\n%1: %2")
                          .arg(issue.value(QStringLiteral("path")).toString(),
                               issue.value(QStringLiteral("message")).toString());
        }
        emit changed();
        return;
    }
    if ((purpose == QStringLiteral("load") || purpose == QStringLiteral("reload") ||
         purpose == QStringLiteral("check_save")) &&
        (!validMetadata(data) || data.value(QStringLiteral("mission_id")).toString() != requestedMission ||
         !QJsonObject::fromVariantMap(data).value(QStringLiteral("mission")).isObject())) {
        _error = tr("The returned mission does not match this request. The current draft is preserved.");
        emit changed();
        return;
    }
    _error.clear();
    if (purpose == QStringLiteral("discover")) {
        _verifiedSession = _client->serviceSession();
        _verifiedEndpoint = _endpoint();
        _catalogPages.clear();
        _listCursors.clear();
        _listPage();
    } else if (purpose == QStringLiteral("list")) {
        const QJsonObject page = QJsonObject::fromVariantMap(data);
        const QJsonValue next = page.value(QStringLiteral("next_cursor"));
        bool valid =
            page.value(QStringLiteral("items")).isArray() &&
            page.value(QStringLiteral("items")).toArray().size() <= 50 &&
            (next.isNull() || (next.isString() && !next.toString().isEmpty() && next.toString().size() <= 128));
        QSet<QString> ids;
        for (const QVariant& entry : _catalogPages + data.value(QStringLiteral("items")).toList()) {
            const QVariantMap metadata = entry.toMap();
            const QString missionId = metadata.value(QStringLiteral("mission_id")).toString();
            valid = valid && validMetadata(metadata) && !ids.contains(missionId) &&
                    QJsonObject::fromVariantMap(metadata).value(QStringLiteral("name")).isString();
            ids.insert(missionId);
        }
        if (!valid) {
            _error = tr("DCLink returned an invalid library page. The previous catalog and drafts are preserved.");
            emit changed();
            return;
        }
        _catalogPages.append(data.value(QStringLiteral("items")).toList());
        const QString cursor = data.value(QStringLiteral("next_cursor")).toString();
        if (_catalogPages.size() > 10000 || (!cursor.isEmpty() && _listCursors.contains(cursor))) {
            _error = tr("The library listing is too large or returned a repeated page. Refresh again.");
        } else if (!cursor.isEmpty()) {
            _listCursors.append(cursor);
            _listPage(cursor);
        } else {
            _catalog = _catalogPages;
            _catalogEndpoint = _endpoint();
            _reviewedCatalog = true;
            emit catalogChanged();
        }
    } else if (purpose == QStringLiteral("load") || purpose == QStringLiteral("reload")) {
        _loadRecord(data);
    } else if (purpose == QStringLiteral("check_save")) {
        if (data.value(QStringLiteral("revision")).toUInt() != revision() ||
            data.value(QStringLiteral("mission")).toMap() != _draft().value(QStringLiteral("base")).toMap()) {
            _conflict = true;
            _error = tr("A newer saved revision is available. Reload it or make a copy of your changes.");
        } else {
            _mutate(QStringLiteral("missions.update"), {{QStringLiteral("mission_id"), missionId()},
                                                        {QStringLiteral("expected_revision"), revision()},
                                                        {QStringLiteral("mission"), mission()}});
        }
    } else if (purpose == QStringLiteral("mutation")) {
        _applyMutation(data);
        if (reviewRequired()) {
            emit changed();
            return;
        }
        _catalogPages.clear();
        _listCursors.clear();
        _listRetries = 0;
        _listPage();
    }
    emit changed();
}

bool DCLinkMissionEditor::_persist()
{
    if (!_storageError.isEmpty()) {
        return false;
    }
    QJsonObject drafts;
    for (auto it = _drafts.cbegin(); it != _drafts.cend(); ++it) {
        drafts.insert(it.key(), QJsonObject::fromVariantMap(it.value()));
    }
    const QByteArray bytes =
        QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                  {QStringLiteral("active"), _draftId},
                                  {QStringLiteral("drafts"), drafts},
                                  {QStringLiteral("pending_change"), QJsonObject::fromVariantMap(_pendingMutation)}})
            .toJson(QJsonDocument::Compact);
    QDir().mkpath(QFileInfo(draftsPath()).absolutePath());
    QSaveFile file(draftsPath());
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        _storageError =
            tr("Could not retain local mission drafts. Remote changes are disabled: %1").arg(file.errorString());
        _error = _storageError;
        return false;
    }
    return true;
}

void DCLinkMissionEditor::_restore()
{
    QFile file(draftsPath());
    if (!file.exists()) {
        return;
    }
    QJsonObject root;
    QString error;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32 * 1024 * 1024 ||
        !DCLinkMissionJson::parseObject(file.readAll(), root, error) ||
        root.value(QStringLiteral("version")).toDouble() != 1 || !root.value(QStringLiteral("drafts")).isObject() ||
        !root.value(QStringLiteral("pending_change")).isObject() ||
        root.value(QStringLiteral("drafts")).toObject().size() > 50) {
        _storageError = tr("Local mission drafts could not be read. The original draft file has been preserved.");
        _error = _storageError;
        return;
    }
    const QJsonObject drafts = root.value(QStringLiteral("drafts")).toObject();
    for (auto it = drafts.begin(); it != drafts.end(); ++it) {
        const QJsonObject stored = it.value().toObject();
        const QVariantMap metadata = stored.toVariantMap();
        const QString missionId = stored.value(QStringLiteral("mission_id")).toString();
        if (!validId(it.key()) || !it.value().isObject() || !stored.value(QStringLiteral("mission")).isObject() ||
            !stored.value(QStringLiteral("base")).isObject() || !stored.value(QStringLiteral("endpoint")).isObject() ||
            !stored.value(QStringLiteral("read_only")).isBool() || (!missionId.isEmpty() && !validMetadata(metadata))) {
            _storageError = tr("Local draft data is invalid. The original draft file has been preserved.");
            _error = _storageError;
            _drafts.clear();
            return;
        }
        QVariantMap draft = it.value().toObject().toVariantMap();
        if (!DCLinkMissionJson::unsupportedMission(
                 QJsonObject::fromVariantMap(draft.value(QStringLiteral("mission")).toMap()))
                 .isEmpty()) {
            draft.insert(QStringLiteral("read_only"), true);
        }
        _drafts.insert(it.key(), draft);
    }
    _draftId = root.value(QStringLiteral("active")).toString();
    if (!_drafts.contains(_draftId) && !_drafts.isEmpty()) {
        _draftId = _drafts.firstKey();
    }
    _pendingMutation = root.value(QStringLiteral("pending_change")).toObject().toVariantMap();
    if (!_pendingMutation.isEmpty()) {
        _error = executionReviewRequired()
            ? tr("QGC closed with an unconfirmed mission command. Check the aircraft and refresh the mission status.")
            : tr("QGC closed with an unconfirmed change. Refresh and review the saved library before continuing.");
    }
}
