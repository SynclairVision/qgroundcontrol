#include "DCLinkManager.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

#include <QtCore/QByteArrayView>
#include <QtCore/QRandomGenerator>
#include <QtCore/QStringConverter>
#include <QtCore/QVariantMap>

#include "MAVLinkLib.h"
#include "DigiviewConnection.h"
#include "DigiviewManager.h"
#include "QGCMAVLink.h"

namespace {

constexpr quint8 COPILOT_SYSTEM_ID = 252;
constexpr quint8 COPILOT_COMPONENT_ID = 67;
constexpr int STATUS_TIMEOUT_MS = 3000;
constexpr int VEHICLE_TIMEOUT_MS = 3500;
constexpr int MAX_CATALOG_ITEMS = 256;
constexpr quint32 REQUEST_ID_COUNT = 65536;

template<std::size_t N>
std::optional<QString> wireText(const char (&value)[N])
{
    const auto end = std::find(std::begin(value), std::end(value), '\0');
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(QByteArrayView(value, end - std::begin(value)));
    return decoder.hasError() ? std::nullopt : std::optional<QString>(text);
}

} // namespace

DCLinkManager::DCLinkManager(DigiviewManager* digiview, QObject* parent)
    : QObject(parent)
    , _digiview(digiview)
    , _nextId(QRandomGenerator::global()->bounded(REQUEST_ID_COUNT))
{
    if (!_digiview || !_digiview->connection()) {
        return;
    }

    auto* connection = _digiview->connection();
    connect(connection, &DigiviewConnection::messageReceived, this, &DCLinkManager::_receiveMessage);
    connect(connection, &DigiviewConnection::connectedChanged, this, &DCLinkManager::_resetSession);
    connect(connection, &DigiviewConnection::hostChanged, this, &DCLinkManager::_resetSession);
    connect(connection, &DigiviewConnection::portChanged, this, &DCLinkManager::_resetSession);
    connect(connection, &DigiviewConnection::listenPortChanged, this, &DCLinkManager::_resetSession);
    connect(_digiview, &DigiviewManager::sessionRequestedChanged, this, &DCLinkManager::_resetSession);
    connect(_digiview, &DigiviewManager::senderIdentityChanged, this, &DCLinkManager::_resetSession);

    connect(&_timer, &QTimer::timeout, this, &DCLinkManager::_tick);
    _timer.start(250);
    _updateVehicle();
}

DCLinkManager::~DCLinkManager() = default;

void DCLinkManager::setSelectedVehicleId(int systemId)
{
    if (_selectedVehicleId != systemId) {
        _selectedVehicleId = systemId;
        _resetSession();
    }
}

bool DCLinkManager::_transportReady() const
{
    return _digiview && _digiview->sessionRequested() && _digiview->transportConnected()
        && _digiview->connection() && !_digiview->connection()->usingLegacyTcpControl();
}

std::optional<quint16> DCLinkManager::_nextRequestId()
{
    // Never reuse an identity, including after reconnects: Copilot retains mutation results until reboot.
    if (_issuedIds >= REQUEST_ID_COUNT) {
        _feedback = tr("The DCLink session limit was reached. Restart QGC to reconnect.");
        return std::nullopt;
    }
    ++_issuedIds;
    return static_cast<quint16>(_nextId++);
}

bool DCLinkManager::_sendStatus(quint16 requestId)
{
    if (!_transportReady()) {
        return false;
    }
    mavlink_copilot_status_t payload{};
    payload.request_id = requestId;
    mavlink_message_t message{};
    mavlink_msg_copilot_status_encode(
        _digiview->senderSystemId(), _digiview->senderComponentId(), &message, &payload);
    return _digiview->connection()->sendMessage(message);
}

bool DCLinkManager::_sendCommand(quint16 requestId, quint8 action, int item, const QString& command)
{
    if (!_transportReady()) {
        return false;
    }
    mavlink_copilot_command_t payload{};
    const auto name = command.toUtf8();
    if (name.size() >= static_cast<qsizetype>(sizeof(payload.name))) {
        return false;
    }
    payload.request_id = requestId;
    payload.action = action;
    payload.id = static_cast<quint16>(item);
    std::copy(name.cbegin(), name.cend(), std::begin(payload.name));
    mavlink_message_t message{};
    mavlink_msg_copilot_command_encode(
        _digiview->senderSystemId(), _digiview->senderComponentId(), &message, &payload);
    return _digiview->connection()->sendMessage(message);
}

void DCLinkManager::_resetCatalog()
{
    _catalogNames.clear();
    _catalogLabels.clear();
    _catalogRequest.reset();
    _catalogReady = false;
    _catalogNextItem = 0;
    _catalogCount = 0;
    _catalogError.clear();
    _catalogRetry = QDeadlineTimer(0);
}

void DCLinkManager::_markOutcomeUnknown()
{
    if (_mutation) {
        _mutation.reset();
        _awaitingReconciliation = true;
        _feedback = tr("Command outcome unknown. Waiting for a fresh DCLink status; nothing will be resent.");
        // Only a query made after the uncertainty can reconcile the displayed current state.
        _statusRequest.reset();
    }
}

void DCLinkManager::_resetSession()
{
    _markOutcomeUnknown();
    _connected = false;
    _lastStatus.invalidate();
    _lastPoll.invalidate();
    _statusRequest.reset();
    _resetCatalog();
    _observations.clear();
    _activeCommand.clear();
    _updateVehicle();
    emit stateChanged();
}

void DCLinkManager::_catalogFailed(const QString& reason)
{
    _resetCatalog();
    _catalogRetry = QDeadlineTimer(5000);
    _catalogError = reason;
}

void DCLinkManager::_updateVehicle()
{
    for (auto it = _observations.begin(); it != _observations.end();) {
        if (it->age.elapsed() > VEHICLE_TIMEOUT_MS) {
            it = _observations.erase(it);
        } else {
            ++it;
        }
    }

    _vehicleKind = VehicleKind::Unknown;
    if (_observations.isEmpty()) {
        _vehicleReason = tr("Waiting for aircraft telemetry on this connection.");
        return;
    }
    if (_observations.size() != 1) {
        _vehicleReason = tr("Multiple aircraft share this DCLink bus. Commands are unavailable.");
        return;
    }
    const auto it = _observations.constBegin();
    if (_selectedVehicleId != 0 && _selectedVehicleId != it.key()) {
        _vehicleReason = tr("The selected aircraft is not on this DCLink connection.");
        return;
    }
    const auto type = static_cast<MAV_TYPE>(it->type);
    if (type == MAV_TYPE_FIXED_WING) {
        _vehicleKind = VehicleKind::Plane;
    } else if (QGCMAVLink::isMultiRotor(type)) {
        _vehicleKind = VehicleKind::Copter;
    }
    _vehicleReason = _vehicleKind == VehicleKind::Unknown
        ? tr("DCLink commands are unavailable for this aircraft type.") : QString();
}

void DCLinkManager::_tick()
{
    _updateVehicle();
    if (!_transportReady()) {
        emit stateChanged();
        return;
    }
    if (_connected && _lastStatus.elapsed() > STATUS_TIMEOUT_MS) {
        _connected = false;
        _markOutcomeUnknown();
        _resetCatalog();
    }
    if (_mutation && _mutation->deadline.hasExpired()) {
        _markOutcomeUnknown();
    }
    if (_statusRequest && _statusRequest->deadline.hasExpired()) {
        _statusRequest.reset();
    }
    if (!_statusRequest && (!_lastPoll.isValid() || _lastPoll.elapsed() >= 1000)) {
        if (const auto id = _nextRequestId()) {
            _lastPoll.start();
            if (_sendStatus(*id)) {
                _statusRequest = Request{.id = *id};
            }
        }
    }

    if (_catalogRequest && _catalogRequest->deadline.hasExpired()) {
        if (_catalogRequest->attempts >= 3
            || !_sendCommand(_catalogRequest->id, COPILOT_ACTION_GET, _catalogRequest->item)) {
            _catalogFailed(tr("DCLink command list could not be read."));
        } else {
            ++_catalogRequest->attempts;
            _catalogRequest->deadline = QDeadlineTimer(1500);
        }
    }
    if (_connected && !_catalogReady && !_catalogRequest && _catalogRetry.hasExpired()) {
        if (const auto id = _nextRequestId()) {
            if (_sendCommand(*id, COPILOT_ACTION_GET, _catalogNextItem)) {
                _catalogRequest = Request{.id = *id, .item = _catalogNextItem};
            } else {
                _catalogFailed(tr("DCLink command list could not be requested."));
            }
        }
    }
    emit stateChanged();
}

void DCLinkManager::_receiveMessage(const mavlink_message_t& message)
{
    if (!_transportReady()) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_HEARTBEAT && message.compid == MAV_COMP_ID_AUTOPILOT1) {
        mavlink_heartbeat_t heartbeat{};
        mavlink_msg_heartbeat_decode(&message, &heartbeat);
        if (heartbeat.autopilot != MAV_AUTOPILOT_INVALID && message.sysid != 0) {
            auto& observation = _observations[message.sysid];
            observation.type = heartbeat.type;
            observation.age.start();
            _updateVehicle();
            emit stateChanged();
        }
        return;
    }
    if (message.sysid != COPILOT_SYSTEM_ID || message.compid != COPILOT_COMPONENT_ID) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_COPILOT_STATUS) {
        mavlink_copilot_status_t status{};
        mavlink_msg_copilot_status_decode(&message, &status);
        const auto active = wireText(status.active_command);
        if (!_statusRequest || status.request_id != _statusRequest->id || !active
            || status.autopilot_connection > COPILOT_AUTOPILOT_CONNECTION_ERROR
            || status.command_state > COPILOT_COMMAND_STATE_ERROR
            || status.target_status > COPILOT_TARGET_STATUS_LOST) {
            return;
        }
        _statusRequest.reset();
        _lastStatus.start();
        _connected = true;
        _autopilotConnection = status.autopilot_connection;
        _commandState = status.command_state;
        _targetStatus = status.target_status;
        _activeCommand = *active;
        if (_awaitingReconciliation) {
            _awaitingReconciliation = false;
            _feedback = tr("Current state refreshed. The previous command acknowledgement was not received.");
        }
        emit stateChanged();
        return;
    }
    if (message.msgid != MAVLINK_MSG_ID_COPILOT_COMMAND) {
        return;
    }
    mavlink_copilot_command_t response{};
    mavlink_msg_copilot_command_decode(&message, &response);
    const auto name = wireText(response.name);
    if (!name) {
        return;
    }

    if (_mutation && response.request_id == _mutation->id
        && (*name == _mutation->command || (name->isEmpty() && response.action == COPILOT_ACTION_ERROR))
        && (response.action == COPILOT_ACTION_ACK || response.action == COPILOT_ACTION_ERROR)) {
        if (response.action == COPILOT_ACTION_ACK && response.result == COPILOT_RESULT_OK) {
            _feedback = tr("%1 request accepted. Check the reported command state.")
                .arg(_commandLabel(_mutation->command));
        } else if (response.result == COPILOT_RESULT_BUSY) {
            _feedback = tr("DCLink is busy. Check the current command before trying again.");
        } else if (response.result == COPILOT_RESULT_NOT_FOUND) {
            _feedback = tr("This command is unavailable on DCLink.");
            _resetCatalog();
        } else {
            _feedback = tr("DCLink rejected the command.");
        }
        _mutation.reset();
        emit stateChanged();
        return;
    }
    if (!_catalogRequest || response.request_id != _catalogRequest->id) {
        return;
    }
    const int requestedItem = _catalogRequest->item;
    if (response.result != COPILOT_RESULT_OK || response.total > MAX_CATALOG_ITEMS
        || (requestedItem == 0 && response.action != COPILOT_ACTION_ACK)
        || (requestedItem != 0 && (response.action != COPILOT_ACTION_ITEM || response.id != requestedItem
            || response.total != _catalogCount || name->isEmpty() || _catalogNames.contains(*name)))) {
        _catalogFailed(tr("DCLink command list changed or returned an invalid response."));
        emit stateChanged();
        return;
    }

    _catalogRequest.reset();
    if (requestedItem == 0) {
        _catalogCount = response.total;
    } else {
        const auto label = wireText(response.label);
        if (!label) {
            _catalogFailed(tr("DCLink returned an invalid command label."));
            return;
        }
        _catalogNames.insert(*name);
        _catalogLabels.insert(*name, *label);
    }
    _catalogNextItem = requestedItem + 1;
    _catalogReady = requestedItem == _catalogCount;
    if (_catalogReady) {
        _catalogError.clear();
    }
    emit stateChanged();
}

QStringList DCLinkManager::_pilotCommands() const
{
    if (_vehicleKind == VehicleKind::Plane) {
        return {QStringLiteral("cancel"), QStringLiteral("plane/loiter"),
                QStringLiteral("plane/target_intercept"), QStringLiteral("plane/target_loiter")};
    }
    if (_vehicleKind == VehicleKind::Copter) {
        return {QStringLiteral("cancel"), QStringLiteral("copter/hold"), QStringLiteral("copter/mission"),
                QStringLiteral("copter/target_follow"), QStringLiteral("copter/target_loiter")};
    }
    return {};
}

QString DCLinkManager::_commandLabel(const QString& command) const
{
    if (command == QStringLiteral("cancel")) return tr("Cancel");
    if (command == QStringLiteral("copter/hold")) return tr("Hold");
    if (command == QStringLiteral("plane/loiter")) return tr("Hold (Loiter)");
    if (command == QStringLiteral("copter/mission")) return tr("Mission");
    if (command == QStringLiteral("copter/target_follow")) return tr("Follow Target");
    if (command == QStringLiteral("plane/target_intercept")) return tr("Intercept");
    if (command == QStringLiteral("copter/target_loiter") || command == QStringLiteral("plane/target_loiter")) {
        return tr("Target Loiter");
    }
    return _catalogLabels.value(command, tr("Other command"));
}

QString DCLinkManager::statusText() const
{
    if (_digiview && _digiview->connection() && _digiview->connection()->usingLegacyTcpControl()) {
        return tr("DCLink requires MAVLink UDP.");
    }
    if (!_transportReady()) return tr("Connect the DigiView network profile.");
    return _connected ? tr("DCLink online") : tr("Waiting for DCLink");
}

QString DCLinkManager::vehicleText() const
{
    if (_vehicleKind == VehicleKind::Plane) return tr("Plane");
    if (_vehicleKind == VehicleKind::Copter) return tr("Copter");
    return _vehicleReason;
}

QString DCLinkManager::commandText() const
{
    if (!_connected) return tr("Current command unavailable");
    if (_activeCommand.isEmpty() || _activeCommand == QStringLiteral("idle")
        || _activeCommand == QStringLiteral("none")) {
        return tr("Idle");
    }
    const auto label = _commandLabel(_activeCommand);
    if (_commandState == COPILOT_COMMAND_STATE_STOPPING) return tr("%1 · Stopping").arg(label);
    if (_commandState == COPILOT_COMMAND_STATE_ERROR) return tr("%1 · Error").arg(label);
    return _commandState == COPILOT_COMMAND_STATE_RUNNING ? tr("%1 · Running").arg(label) : label;
}

QString DCLinkManager::targetText() const
{
    if (!_connected) return tr("Target unavailable");
    switch (_targetStatus) {
    case COPILOT_TARGET_STATUS_SEEN: return tr("Target seen");
    case COPILOT_TARGET_STATUS_STABLE: return tr("Target stable");
    case COPILOT_TARGET_STATUS_LOCKED: return tr("Target locked");
    case COPILOT_TARGET_STATUS_LOST: return tr("Target lost");
    default: return tr("No target");
    }
}

QString DCLinkManager::availabilityReason() const
{
    if (!_transportReady() || !_connected) return statusText();
    if (!_lastStatus.isValid() || _lastStatus.elapsed() > STATUS_TIMEOUT_MS) {
        return tr("Waiting for fresh DCLink status.");
    }
    if (!_vehicleReason.isEmpty()) return _vehicleReason;
    if (_issuedIds >= REQUEST_ID_COUNT) return tr("Restart QGC to renew the DCLink session.");
    if (_awaitingReconciliation) return tr("Waiting for current command state.");
    if (_mutation) return tr("Waiting for command acknowledgement.");
    if (!_catalogReady) {
        return _catalogError.isEmpty() ? tr("Reading available DCLink commands.") : _catalogError;
    }
    return {};
}

QString DCLinkManager::_actionReason(const QString& command) const
{
    const auto reason = availabilityReason();
    if (!reason.isEmpty()) return reason;
    if (!_pilotCommands().contains(command) || !_catalogNames.contains(command)) {
        return tr("Unavailable on the connected DCLink.");
    }
    if (command != QStringLiteral("cancel")) {
        if (_autopilotConnection != COPILOT_AUTOPILOT_CONNECTION_CONNECTED) {
            return tr("DCLink is not connected to the autopilot.");
        }
        if (_commandState == COPILOT_COMMAND_STATE_STOPPING) return tr("DCLink is stopping the current command.");
    }
    return {};
}

QVariantList DCLinkManager::actions() const
{
    QVariantList result;
    for (const auto& command : _pilotCommands()) {
        const auto reason = _actionReason(command);
        result.append(QVariantMap{{QStringLiteral("command"), command},
                                  {QStringLiteral("label"), _commandLabel(command)},
                                  {QStringLiteral("enabled"), reason.isEmpty()},
                                  {QStringLiteral("reason"), reason}});
    }
    return result;
}

bool DCLinkManager::startAction(const QString& command)
{
    _updateVehicle();
    const auto reason = _actionReason(command);
    if (!reason.isEmpty()) {
        _feedback = reason;
        emit stateChanged();
        return false;
    }
    const auto id = _nextRequestId();
    if (!id) {
        emit stateChanged();
        return false;
    }
    // Mutations are sent exactly once. Only read requests are eligible for retries.
    if (!_sendCommand(*id, COPILOT_ACTION_START, 0, command)) {
        _feedback = tr("The command could not be sent.");
        emit stateChanged();
        return false;
    }
    _mutation = Request{.id = *id, .command = command};
    _feedback = tr("Sending %1…").arg(_commandLabel(command));
    emit stateChanged();
    return true;
}
