#include "DCLinkMissionEditor.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QJsonObject>
#include <QtCore/QStringList>
#include <QtCore/QUuid>

#include "DCLinkMissionClient.h"

namespace {
constexpr int RUN_STATUS_MAX_AGE_MS = 3500;
const QString START = QStringLiteral("missions.start");
const QString CANCEL = QStringLiteral("missions.cancel");
const QString RUN_STATUS = QStringLiteral("missions.run_status");
const QStringList ACTIVE_STATES = {QStringLiteral("queued"), QStringLiteral("starting"),
                                   QStringLiteral("running"), QStringLiteral("stopping")};
const QStringList TERMINAL_STATES = {QStringLiteral("completed"), QStringLiteral("canceled"),
                                     QStringLiteral("rejected"), QStringLiteral("interrupted"),
                                     QStringLiteral("failed")};

QString explainReason(const QString& reason, const QVariantMap& data = {})
{
    using Editor = DCLinkMissionEditor;
    if (reason == QStringLiteral("vehicle_unavailable") || reason == QStringLiteral("vehicle_heartbeat_unavailable")) {
        return Editor::tr("Waiting for the aircraft heartbeat.");
    }
    if (reason == QStringLiteral("unsupported_vehicle")) {
        return Editor::tr("Mission execution requires a PX4 multicopter.");
    }
    if (reason == QStringLiteral("vehicle_disarmed") || reason == QStringLiteral("vehicle_not_airborne")) {
        return Editor::tr("The aircraft must already be armed and airborne.");
    }
    if (reason == QStringLiteral("home_unavailable") || reason == QStringLiteral("home_changed")) {
        return Editor::tr("Aircraft HOME is unavailable or has changed.");
    }
    if (reason == QStringLiteral("global_position_unavailable")) {
        return Editor::tr("Aircraft global position is unavailable.");
    }
    if (reason == QStringLiteral("requires_hold_or_position") || reason == QStringLiteral("flight_mode_changed")) {
        return Editor::tr("Put the aircraft in Hold or Position mode.");
    }
    if (reason == QStringLiteral("command_busy") || reason == QStringLiteral("command_changed")) {
        return Editor::tr("Another DCLink command is active or changing.");
    }
    if (reason == QStringLiteral("runtime_safety_active") || reason == QStringLiteral("graceful_exit_pending")) {
        return Editor::tr("DCLink is handling a safety action or shutdown.");
    }
    if (reason == QStringLiteral("mission_in_progress")) {
        return Editor::tr("A mission is already in progress.");
    }
    if (reason == QStringLiteral("vehicle_mismatch")) {
        return Editor::tr("Select the aircraft connected to this DCLink.");
    }
    if (reason == QStringLiteral("gcs_heartbeat_unavailable")) {
        return Editor::tr("DCLink is waiting for the ground station heartbeat.");
    }
    if (reason == QStringLiteral("external_control_lost") || reason == QStringLiteral("external_control_unavailable")) {
        return Editor::tr("Aircraft external control is unavailable.");
    }
    if (reason == QStringLiteral("waypoint_altitude_out_of_bounds")) {
        const int number = data.value(QStringLiteral("waypoint_number")).toInt();
        return number > 0 ? Editor::tr("Waypoint %1 is outside the flight altitude limits.").arg(number)
                          : Editor::tr("A waypoint is outside the flight altitude limits.");
    }
    if (reason == QStringLiteral("operator_cancel")) {
        return Editor::tr("Cancel requested");
    }
    if (reason == QStringLiteral("route_complete") || reason == QStringLiteral("safe_handoff")) {
        return Editor::tr("Waiting for Hold");
    }
    if (reason == QStringLiteral("runtime_unavailable") || reason == QStringLiteral("start_expired")) {
        return Editor::tr("DCLink did not admit the mission in time.");
    }
    if (reason == QStringLiteral("run_not_found") || reason == QStringLiteral("run_finished")) {
        return Editor::tr("This mission run is no longer active.");
    }
    if (reason == QStringLiteral("command_start_failed")) {
        return Editor::tr("DCLink could not start the mission command.");
    }
    if (reason == QStringLiteral("command_ended")) {
        return Editor::tr("DCLink released the mission");
    }
    return reason.isEmpty() ? Editor::tr("Mission execution is unavailable.") : reason;
}

bool validUuid(const QVariant& value)
{
    const QString text = value.toString();
    const QUuid id(text);
    return id.version() == QUuid::Random && id.variant() == QUuid::DCE && text == id.toString(QUuid::WithoutBraces);
}

bool integral(const QJsonValue& field, double minimum, double maximum)
{
    const double value = field.toDouble();
    return field.isDouble() && value >= minimum && value <= maximum && std::floor(value) == value;
}

bool launchable(const QVariantMap& record)
{
    const QJsonObject json = QJsonObject::fromVariantMap(record);
    return json.value(QStringLiteral("schema_version")) == QJsonValue(1) &&
           integral(json.value(QStringLiteral("waypoint_count")), 1, 1000);
}

bool validRun(const QVariantMap& run)
{
    const QJsonObject json = QJsonObject::fromVariantMap(run);
    const QString state = json.value(QStringLiteral("state")).toString();
    const QJsonValue count = json.value(QStringLiteral("waypoint_count"));
    const QJsonValue reached = json.value(QStringLiteral("waypoints_reached"));
    const QJsonValue waypoint = json.value(QStringLiteral("waypoint_id"));
    return validUuid(run.value(QStringLiteral("run_id"))) && validUuid(run.value(QStringLiteral("start_request_id"))) &&
           validUuid(run.value(QStringLiteral("mission_id"))) &&
           integral(json.value(QStringLiteral("revision")), 1, 4294967295.0) &&
           json.value(QStringLiteral("name")).isString() &&
           integral(json.value(QStringLiteral("vehicle_system_id")), 1, 255) &&
           (ACTIVE_STATES.contains(state) || TERMINAL_STATES.contains(state)) &&
           integral(count, 1, 1000) && integral(reached, 0, count.toDouble()) &&
           (count == reached ? waypoint.isNull() : validUuid(waypoint.toVariant()));
}

bool matchesStart(const QVariantMap& run, const QVariantMap& pending)
{
    const QVariantMap payload = pending.value(QStringLiteral("payload")).toMap();
    return run.value(QStringLiteral("start_request_id")) == pending.value(QStringLiteral("request_id")) &&
           run.value(QStringLiteral("mission_id")) == payload.value(QStringLiteral("mission_id")) &&
           run.value(QStringLiteral("revision")).toUInt() == payload.value(QStringLiteral("revision")).toUInt() &&
           run.value(QStringLiteral("vehicle_system_id")).toInt() ==
               payload.value(QStringLiteral("vehicle_system_id")).toInt();
}
}  // namespace

bool DCLinkMissionEditor::executionSupported() const
{
    const QStringList operations = _client->capabilities().value(QStringLiteral("operations")).toStringList();
    return operations.contains(START) && operations.contains(RUN_STATUS);
}

bool DCLinkMissionEditor::catalogReady() const
{
    return fresh() && _reviewedCatalog && _catalogEndpoint == _endpoint();
}

QVariantList DCLinkMissionEditor::launchEntries() const
{
    QVariantList entries;
    if (!catalogReady()) {
        return entries;
    }
    QMap<QString, int> names;
    for (const QVariant& item : _catalog) {
        ++names[item.toMap().value(QStringLiteral("name")).toString()];
    }
    for (const QVariant& item : _catalog) {
        QVariantMap record = item.toMap();
        if (!launchable(record)) {
            continue;
        }
        QString label = record.value(QStringLiteral("name")).toString();
        if (names.value(label) > 1) {
            label = tr("%1 · %2").arg(label, record.value(QStringLiteral("mission_id")).toString().left(8));
        }
        const QString revision = record.value(QStringLiteral("revision")).toString();
        record.insert(QStringLiteral("label"), tr("%1 · r%2").arg(label, revision));
        entries.append(record);
    }
    return entries;
}

bool DCLinkMissionEditor::executionReviewRequired() const
{
    const QString action = _pendingMutation.value(QStringLiteral("action")).toString();
    return action == START || action == CANCEL;
}

bool DCLinkMissionEditor::_executionStatusFresh() const
{
    return _client->available() && _executionEndpoint == _endpoint() &&
           _executionServiceSession == _client->serviceSession() && _runReportAge.isValid() &&
           _runReportAge.elapsed() < RUN_STATUS_MAX_AGE_MS;
}

bool DCLinkMissionEditor::_runFresh() const
{
    return _executionStatusFresh() && _runEndpoint == _endpoint() &&
           _runServiceSession == _client->serviceSession();
}

int DCLinkMissionEditor::executionVehicleId() const
{
    return _executionStatusFresh() ? _executionStatus.value(QStringLiteral("vehicle_system_id")).toInt() : 0;
}

QString DCLinkMissionEditor::startReason() const
{
    if (reviewRequired() || !_storageError.isEmpty()) {
        return reviewRequired() ? tr("Review the previous request first.") : _storageError;
    }
    if (busy()) {
        return tr("Waiting for DCLink…");
    }
    if (!catalogReady()) {
        return tr("Refresh the DCLink mission list.");
    }
    if (!executionSupported()) {
        return tr("This DCLink does not support mission execution.");
    }
    if (!_executionStatusFresh()) {
        return tr("Waiting for mission status…");
    }
    if (ACTIVE_STATES.contains(_run.value(QStringLiteral("state")).toString()) && _runFresh()) {
        return tr("A mission is already in progress.");
    }
    if (!_executionStatus.value(QStringLiteral("ready")).toBool()) {
        return explainReason(_executionStatus.value(QStringLiteral("reason")).toString());
    }
    return {};
}

QString DCLinkMissionEditor::runId() const
{
    return _runFresh() ? _run.value(QStringLiteral("run_id")).toString() : QString();
}

QString DCLinkMissionEditor::runText() const
{
    if (_run.isEmpty() || _runEndpoint != _endpoint()) {
        return {};
    }
    if (!_runFresh()) {
        return tr("Mission status unknown · refresh DCLink");
    }
    const QString state = _run.value(QStringLiteral("state")).toString();
    const int reached = _run.value(QStringLiteral("waypoints_reached")).toInt();
    const int count = _run.value(QStringLiteral("waypoint_count")).toInt();
    QString details;
    if (state == QStringLiteral("queued") || state == QStringLiteral("starting")) {
        details = tr("Starting");
    } else if (state == QStringLiteral("running")) {
        details = tr("Waypoint %1/%2").arg(reached + 1).arg(count);
    } else if (state == QStringLiteral("completed")) {
        details = tr("Completed · Hold");
    } else if (state == QStringLiteral("canceled")) {
        details = tr("Canceled");
    } else {
        details = explainReason(_run.value(QStringLiteral("reason")).toString());
        if (state != QStringLiteral("stopping")) {
            details = tr("Stopped · %1").arg(details);
        }
    }
    return tr("%1 · %2 · r%3").arg(details, _run.value(QStringLiteral("name")).toString(),
                                   _run.value(QStringLiteral("revision")).toString());
}

QString DCLinkMissionEditor::flightRunText() const
{
    if (executionReviewRequired()) {
        return busy() ? tr("Sending mission request…") : tr("Mission command outcome unknown");
    }
    return ACTIVE_STATES.contains(_run.value(QStringLiteral("state")).toString()) ? runText() : QString();
}

bool DCLinkMissionEditor::canCancelRun() const
{
    const QString state = _run.value(QStringLiteral("state")).toString();
    return !busy() && !reviewRequired() && _storageError.isEmpty() && _runFresh() &&
           _client->capabilities().value(QStringLiteral("operations")).toStringList().contains(CANCEL) &&
           ACTIVE_STATES.contains(state) && state != QStringLiteral("stopping");
}

void DCLinkMissionEditor::startMission(const QString& savedId, quint32 savedRevision, int vehicleId,
                                       const QString& session)
{
    const bool confirmedRevision = std::ranges::any_of(_catalog, [&](const QVariant& item) {
        const QVariantMap record = item.toMap();
        return record.value(QStringLiteral("mission_id")).toString() == savedId &&
               record.value(QStringLiteral("revision")).toUInt() == savedRevision && launchable(record);
    });
    if (!startReason().isEmpty() || !confirmedRevision || session != _client->serviceSession() ||
        vehicleId < 1 || vehicleId != executionVehicleId()) {
        _error = tr("The mission, connection or aircraft changed. Check readiness and confirm Start again.");
        emit changed();
        return;
    }
    _mutate(START, {{QStringLiteral("mission_id"), savedId}, {QStringLiteral("revision"), savedRevision},
                    {QStringLiteral("vehicle_system_id"), vehicleId}});
}

void DCLinkMissionEditor::cancelRun(const QString& expectedRunId)
{
    if (canCancelRun() && runId() == expectedRunId) {
        _mutate(CANCEL, {{QStringLiteral("run_id"), expectedRunId}});
    }
}

void DCLinkMissionEditor::_pollRunStatus()
{
    if (_runReportAge.isValid() && _runReportAge.elapsed() >= RUN_STATUS_MAX_AGE_MS) {
        _runReportAge.invalidate();
        emit changed();
    }
    if (executionSupported() && _client->available() && !_client->busy() && _requestId.isEmpty()) {
        _runRequestAge.start();
        _runStatusRequest = _client->request(RUN_STATUS, {});
    }
}

void DCLinkMissionEditor::_receiveRunStatus(const QVariantMap& data)
{
    const QVariantMap run = data.value(QStringLiteral("run")).toMap();
    const QVariantMap execution = data.value(QStringLiteral("execution")).toMap();
    const QJsonObject readiness = QJsonObject::fromVariantMap(execution);
    const QJsonObject report = QJsonObject::fromVariantMap(data);
    if (!_runRequestAge.isValid() || _runRequestAge.elapsed() >= RUN_STATUS_MAX_AGE_MS ||
        report.value(QStringLiteral("fresh")) != QJsonValue(true) ||
        !readiness.value(QStringLiteral("ready")).isBool() ||
        !integral(readiness.value(QStringLiteral("vehicle_system_id")), 0, 255) ||
        (!report.value(QStringLiteral("run")).isNull() && !validRun(run))) {
        _runReportAge.invalidate();
        emit changed();
        return;
    }
    _executionEndpoint = _endpoint();
    _executionServiceSession = _client->serviceSession();
    _executionStatus = execution;
    _runReportAge.restart();
    if (!run.isEmpty() || _runEndpoint != _executionEndpoint || _runServiceSession == _executionServiceSession ||
        !ACTIVE_STATES.contains(_run.value(QStringLiteral("state")).toString())) {
        _run = run;
        _runEndpoint = _executionEndpoint;
        _runServiceSession = _executionServiceSession;
    }

    const QVariantMap payload = _pendingMutation.value(QStringLiteral("payload")).toMap();
    const QString action = _pendingMutation.value(QStringLiteral("action")).toString();
    const bool matchingStart = action == START && matchesStart(run, _pendingMutation);
    const bool matchingCancel = action == CANCEL &&
        run.value(QStringLiteral("run_id")) == payload.value(QStringLiteral("run_id")) &&
        run.value(QStringLiteral("reason")).toString() == QStringLiteral("operator_cancel");
    if ((matchingStart || matchingCancel) && !run.isEmpty() &&
        _pendingMutation.value(QStringLiteral("session")).toString() == _executionServiceSession &&
        _pendingMutation.value(QStringLiteral("endpoint")).toMap() == _executionEndpoint) {
        _client->acknowledgeOutcome();
        _pendingMutation.clear();
        _error.clear();
        _persist();
    }
    emit changed();
}

void DCLinkMissionEditor::_executionFinished(const QString& action, const QString& code, const QVariantMap& data)
{
    const QVariantMap run = data.value(QStringLiteral("run")).toMap();
    const QVariantMap payload = _pendingMutation.value(QStringLiteral("payload")).toMap();
    const bool matches = action == START
        ? matchesStart(run, _pendingMutation)
        : run.value(QStringLiteral("run_id")) == payload.value(QStringLiteral("run_id"));
    if (code == QStringLiteral("ok") && matches && validRun(run)) {
        _run = run;
        _runEndpoint = _endpoint();
        _runServiceSession = _client->serviceSession();
        _error.clear();
    } else if (_client->outcomeUnknown() || code == QStringLiteral("ok")) {
        _error = tr("Mission command outcome unknown. Check the aircraft and current run.");
        emit changed();
        return;
    } else if (action == START && (code == QStringLiteral("conflict") || code == QStringLiteral("not_found"))) {
        _invalidateCatalog();
        _error = tr("The saved mission changed or was deleted. Refresh and choose a saved revision before starting.");
    } else {
        _error = explainReason(data.value(QStringLiteral("reason")).toString(), data);
    }
    _pendingMutation.clear();
    _persist();
    _pollRunStatus();
    emit changed();
}
