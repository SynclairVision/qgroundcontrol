#include "DCLinkManager.h"

#include <QtCore/QByteArrayView>
#include <QtCore/QRandomGenerator>
#include <QtCore/QStringConverter>
#include <QtCore/QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>

// clang-format off: load the full bus before DigiView's narrower dialect header.
#include "MAVLinkLib.h"
// clang-format on

#include "DigiviewConnection.h"
#include "DigiviewManager.h"
#include "QGCMAVLink.h"

namespace {

constexpr quint8 DCLINK_SYSTEM_ID = 252;
constexpr quint8 DCLINK_COMPONENT_ID = 67;
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

}  // namespace

DCLinkManager::DCLinkManager(DigiviewManager* digiview, QObject* parent)
    : QObject(parent),
      _digiview(digiview),
      _targetNextId(QRandomGenerator::global()->bounded(1u, 0x80000000u)),
      _nextId(QRandomGenerator::global()->bounded(REQUEST_ID_COUNT))
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
    // Never reuse an identity, including after reconnects: DCLink retains mutation results until reboot.
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
    mavlink_dclink_status_t payload{};
    payload.request_id = requestId;
    mavlink_message_t message{};
    mavlink_msg_dclink_status_encode(_digiview->senderSystemId(), _digiview->senderComponentId(), &message, &payload);
    return _digiview->connection()->sendMessage(message);
}

bool DCLinkManager::_sendCommand(quint16 requestId, quint8 action, int item, const QString& command)
{
    if (!_transportReady()) {
        return false;
    }
    mavlink_dclink_command_t payload{};
    const auto name = command.toUtf8();
    if (name.size() >= static_cast<qsizetype>(sizeof(payload.name))) {
        return false;
    }
    payload.request_id = requestId;
    payload.action = action;
    payload.id = static_cast<quint16>(item);
    std::copy(name.cbegin(), name.cend(), std::begin(payload.name));
    mavlink_message_t message{};
    mavlink_msg_dclink_command_encode(_digiview->senderSystemId(), _digiview->senderComponentId(), &message, &payload);
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
    _statusFlags = 0;
    _clearTarget();
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
    _pollTarget();
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
        if (_catalogRequest->attempts >= 3 ||
            !_sendCommand(_catalogRequest->id, DCLINK_ACTION_GET, _catalogRequest->item)) {
            _catalogFailed(tr("DCLink command list could not be read."));
        } else {
            ++_catalogRequest->attempts;
            _catalogRequest->deadline = QDeadlineTimer(1500);
        }
    }
    if (_connected && !_catalogReady && !_catalogRequest && _catalogRetry.hasExpired()) {
        if (const auto id = _nextRequestId()) {
            if (_sendCommand(*id, DCLINK_ACTION_GET, _catalogNextItem)) {
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
    if (message.sysid != DCLINK_SYSTEM_ID || message.compid != DCLINK_COMPONENT_ID) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_DCLINK_STATUS) {
        mavlink_dclink_status_t status{};
        mavlink_msg_dclink_status_decode(&message, &status);
        const auto active = wireText(status.active_command);
        if (!_statusRequest || status.request_id != _statusRequest->id || !active ||
            (status.flags & DCLINK_STATUS_FLAGS_ERROR) ||
            status.autopilot_connection > DCLINK_AUTOPILOT_CONNECTION_ERROR ||
            status.command_state > DCLINK_COMMAND_STATE_ERROR || status.target_status > DCLINK_TARGET_STATUS_LOST) {
            return;
        }
        _statusRequest.reset();
        _lastStatus.start();
        _connected = true;
        _statusFlags = status.flags;
        if (!(_statusFlags & DCLINK_STATUS_FLAGS_TARGET_VIEW)) {
            _clearTarget();
        }
        _autopilotConnection = status.autopilot_connection;
        _commandState = status.command_state;
        _targetStatus = status.target_status;
        _navigationType = status.navigation_type;
        _navigationStatus = status.navigation_status;
        _activeCommand = *active;
        if (_awaitingReconciliation) {
            _awaitingReconciliation = false;
            _feedback = tr("Current state refreshed. The previous command acknowledgement was not received.");
        }
        emit stateChanged();
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_DCLINK_TARGET_VIEW) {
        _receiveTarget(message);
        return;
    }
    if (message.msgid != MAVLINK_MSG_ID_DCLINK_COMMAND) {
        return;
    }
    mavlink_dclink_command_t response{};
    mavlink_msg_dclink_command_decode(&message, &response);
    const auto name = wireText(response.name);
    if (!name) {
        return;
    }

    if (_mutation && response.request_id == _mutation->id &&
        (*name == _mutation->command || (name->isEmpty() && response.action == DCLINK_ACTION_ERROR)) &&
        (response.action == DCLINK_ACTION_ACK || response.action == DCLINK_ACTION_ERROR)) {
        if (response.action == DCLINK_ACTION_ACK && response.result == DCLINK_RESULT_OK) {
            _feedback = tr("%1 request accepted. Check the reported command state.")
                .arg(_commandLabel(_mutation->command));
        } else if (response.result == DCLINK_RESULT_BUSY) {
            _feedback = tr("DCLink is busy. Check the current command before trying again.");
        } else if (response.result == DCLINK_RESULT_NOT_FOUND) {
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
    if (response.result != DCLINK_RESULT_OK || response.total > MAX_CATALOG_ITEMS ||
        (requestedItem == 0 && response.action != DCLINK_ACTION_ACK) ||
        (requestedItem != 0 && (response.action != DCLINK_ACTION_ITEM || response.id != requestedItem ||
                                response.total != _catalogCount || name->isEmpty() || _catalogNames.contains(*name)))) {
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

bool DCLinkManager::connected() const
{
    return _connected && _lastStatus.isValid() && _lastStatus.elapsed() <= STATUS_TIMEOUT_MS;
}

QString DCLinkManager::statusText() const
{
    if (_digiview && _digiview->connection() && _digiview->connection()->usingLegacyTcpControl()) {
        return tr("DCLink requires MAVLink UDP.");
    }
    if (!_transportReady()) return tr("Connect the DigiView network profile.");
    return connected() ? tr("DCLink online") : tr("Waiting for DCLink");
}

QString DCLinkManager::vehicleText() const
{
    if (_vehicleKind == VehicleKind::Plane) return tr("Plane");
    if (_vehicleKind == VehicleKind::Copter) return tr("Copter");
    return _vehicleReason;
}

QString DCLinkManager::commandText() const
{
    if (!connected())
        return tr("Current command unavailable");
    if (_activeCommand.isEmpty() || _activeCommand == QStringLiteral("idle")
        || _activeCommand == QStringLiteral("none")) {
        return tr("Idle");
    }
    const auto label = _commandLabel(_activeCommand);
    if (_commandState == DCLINK_COMMAND_STATE_STOPPING)
        return tr("%1 · Stopping").arg(label);
    if (_commandState == DCLINK_COMMAND_STATE_ERROR)
        return tr("%1 · Error").arg(label);
    return _commandState == DCLINK_COMMAND_STATE_RUNNING ? tr("%1 · Running").arg(label) : label;
}

QString DCLinkManager::targetText() const
{
    if (!connected())
        return tr("Target unavailable");
    switch (_targetStatus) {
        case DCLINK_TARGET_STATUS_SEEN:
            return tr("Target seen");
        case DCLINK_TARGET_STATUS_STABLE:
            return tr("Target stable");
        case DCLINK_TARGET_STATUS_LOCKED:
            return tr("Target locked");
        case DCLINK_TARGET_STATUS_LOST:
            return tr("Target lost");
        default:
            return tr("No target");
    }
}

QString DCLinkManager::navigationText() const
{
    if (!connected()) {
        return tr("Navigation unavailable");
    }

    QString source;
    switch (_navigationType) {
        case DCLINK_NAVIGATION_TYPE_NONE:
            source = tr("None");
            break;
        case DCLINK_NAVIGATION_TYPE_GPS:
            source = tr("GPS");
            break;
        case DCLINK_NAVIGATION_TYPE_MYGGNAV:
            source = tr("Myggnav");
            break;
        case DCLINK_NAVIGATION_TYPE_STELLAR:
            source = tr("Stellar");
            break;
        case DCLINK_NAVIGATION_TYPE_CUSTOM:
            source = tr("Custom");
            break;
        default:
            source = tr("Unknown");
            break;
    }

    QString health;
    switch (_navigationStatus) {
        case DCLINK_NAVIGATION_STATUS_READY:
            health = tr("Ready");
            break;
        case DCLINK_NAVIGATION_STATUS_ACTIVE:
            health = tr("Active");
            break;
        case DCLINK_NAVIGATION_STATUS_DEGRADED:
            health = tr("Degraded");
            break;
        case DCLINK_NAVIGATION_STATUS_LOST:
            health = tr("Lost");
            break;
        case DCLINK_NAVIGATION_STATUS_ERROR:
            health = tr("Error");
            break;
        default:
            health = tr("Unknown");
            break;
    }
    return tr("Navigation: %1 · %2").arg(source, health);
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
        if (_autopilotConnection != DCLINK_AUTOPILOT_CONNECTION_CONNECTED) {
            return tr("DCLink is not connected to the autopilot.");
        }
        if (_commandState == DCLINK_COMMAND_STATE_STOPPING)
            return tr("DCLink is stopping the current command.");
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
    if (!_sendCommand(*id, DCLINK_ACTION_START, 0, command)) {
        _feedback = tr("The command could not be sent.");
        emit stateChanged();
        return false;
    }
    _mutation = Request{.id = *id, .command = command};
    _feedback = tr("Sending %1…").arg(_commandLabel(command));
    emit stateChanged();
    return true;
}

void DCLinkManager::_clearTarget()
{
    _lastTarget.invalidate();
    _targetQueryAge.invalidate();
    _targetRequestId = 0;
    _targetFlags = 0;
    _targetRemainingMs = 0;
    _targetData.clear();
}

void DCLinkManager::_pollTarget()
{
    if (!_transportReady() || !connected() || !_vehicleReason.isEmpty() ||
        !(_statusFlags & DCLINK_STATUS_FLAGS_TARGET_VIEW)) {
        _clearTarget();
        return;
    }
    if (_targetVehicleId != 0 && _targetVehicleId != _observations.constBegin().key()) {
        _clearTarget();
    }
    if (_targetRequestId != 0) {
        if (_targetQueryAge.elapsed() < 1000) {
            return;
        }
        _clearTarget();
    }
    // A separate 32-bit read identity avoids exhausting the command identity pool.
    if (_targetNextId > std::numeric_limits<quint32>::max()) {
        _clearTarget();
        return;
    }
    mavlink_dclink_target_view_t payload{};
    payload.request_id = static_cast<quint32>(_targetNextId++);
    mavlink_message_t message{};
    mavlink_msg_dclink_target_view_encode(_digiview->senderSystemId(), _digiview->senderComponentId(), &message,
                                          &payload);
    if (_digiview->connection()->sendMessage(message)) {
        _targetRequestId = payload.request_id;
        _targetQueryAge.start();
    }
}

void DCLinkManager::_receiveTarget(const mavlink_message_t& message)
{
    mavlink_dclink_target_view_t target{};
    mavlink_msg_dclink_target_view_decode(&message, &target);
    if (_targetRequestId == 0 || target.request_id != _targetRequestId) {
        return;
    }
    _updateVehicle();
    const auto id = wireText(target.target_id);
    const auto source = wireText(target.target_source);
    const auto valid = [&target](quint32 flag, double value, double minimum, double maximum) {
        return !(target.flags & flag) || (std::isfinite(value) && value >= minimum && value <= maximum);
    };
    if (!connected() || !(_statusFlags & DCLINK_STATUS_FLAGS_TARGET_VIEW) || !_vehicleReason.isEmpty() ||
        target.schema_version != 1 || target.boot_id == 0 || !id || !source ||
        target.vehicle_system_id != _observations.constBegin().key() ||
        target.vehicle_component_id != MAV_COMP_ID_AUTOPILOT1 || target.control_owner > DCLINK_CONTROL_OWNER_DCLINK ||
        target.valid_for_ms > 3000 || (target.flags & ~quint32(511)) ||
        ((target.flags & ~quint32(DCLINK_TARGET_VIEW_FLAGS_PRESENT)) &&
         !(target.flags & DCLINK_TARGET_VIEW_FLAGS_PRESENT)) ||
        ((target.flags & DCLINK_TARGET_VIEW_FLAGS_BEHIND) && !(target.flags & DCLINK_TARGET_VIEW_FLAGS_AZIMUTH)) ||
        ((target.flags & DCLINK_TARGET_VIEW_FLAGS_DRIVING) && target.control_owner != DCLINK_CONTROL_OWNER_DCLINK) ||
        ((target.flags & DCLINK_TARGET_VIEW_FLAGS_RANGE) &&
         target.range_source != DCLINK_RANGE_SOURCE_RUNTIME_ESTIMATE) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_AZIMUTH, target.azimuth_deg, -180, 180) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_ELEVATION, target.elevation_deg, -90, 90) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_RANGE, target.horizontal_range_m, 0, 1e9) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_GLOBAL_POSITION, target.latitude_deg, -90, 90) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_GLOBAL_POSITION, target.longitude_deg, -180, 180) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_RELATIVE_POSITION, target.relative_north_m, -1e9, 1e9) ||
        !valid(DCLINK_TARGET_VIEW_FLAGS_RELATIVE_POSITION, target.relative_east_m, -1e9, 1e9) ||
        (_targetBootId == target.boot_id && target.sequence <= _targetSequence)) {
        _clearTarget();
        emit stateChanged();
        return;
    }
    const auto roundTripMs = _targetQueryAge.elapsed();
    if (roundTripMs >= 1000) {
        _clearTarget();
        emit stateChanged();
        return;
    }
    _targetRequestId = 0;
    _targetQueryAge.invalidate();
    _targetBootId = target.boot_id;
    _targetSequence = target.sequence;
    _targetVehicleId = target.vehicle_system_id;
    _targetFlags = target.flags;
    _targetRemainingMs = std::max(0, target.valid_for_ms - static_cast<int>(roundTripMs));
    _targetData = {{QStringLiteral("targetId"), *id},
                   {QStringLiteral("source"), *source},
                   {QStringLiteral("vehicleSystemId"), target.vehicle_system_id},
                   {QStringLiteral("vehicleComponentId"), target.vehicle_component_id},
                   {QStringLiteral("controlOwner"), target.control_owner},
                   {QStringLiteral("rangeSource"), target.range_source},
                   {QStringLiteral("ageMs"), static_cast<qint64>(target.age_ms) + roundTripMs},
                   {QStringLiteral("sourceAgeMs"), static_cast<qint64>(target.source_age_ms) + roundTripMs},
                   {QStringLiteral("azimuthDeg"), target.azimuth_deg},
                   {QStringLiteral("elevationDeg"), target.elevation_deg},
                   {QStringLiteral("horizontalRangeM"), target.horizontal_range_m},
                   {QStringLiteral("latitudeDeg"), target.latitude_deg},
                   {QStringLiteral("longitudeDeg"), target.longitude_deg},
                   {QStringLiteral("relativeNorthM"), target.relative_north_m},
                   {QStringLiteral("relativeEastM"), target.relative_east_m}};
    _lastTarget.start();
    emit stateChanged();
}

QVariantMap DCLinkManager::targetView() const
{
    const bool available = _transportReady() && connected() && _vehicleReason.isEmpty() && _observations.size() == 1 &&
                           _targetVehicleId == _observations.constBegin().key() && _lastTarget.isValid() &&
                           _lastTarget.elapsed() < 1000;
    const auto elapsed = available ? _lastTarget.elapsed() : 0;
    const quint32 flags = available && elapsed < _targetRemainingMs ? _targetFlags : 0;
    QVariantMap view{
        {QStringLiteral("available"), available},
        {QStringLiteral("present"), bool(flags & DCLINK_TARGET_VIEW_FLAGS_PRESENT)},
        {QStringLiteral("behind"), bool(flags & DCLINK_TARGET_VIEW_FLAGS_BEHIND)},
        {QStringLiteral("driving"), bool(flags & DCLINK_TARGET_VIEW_FLAGS_DRIVING)},
        {QStringLiteral("controlOwner"), available ? _targetData.value(QStringLiteral("controlOwner")) : QVariant(0)},
        {QStringLiteral("vehicleSystemId"), available ? QVariant(_targetVehicleId) : QVariant()},
        {QStringLiteral("vehicleComponentId"),
         available ? _targetData.value(QStringLiteral("vehicleComponentId")) : QVariant()},
        {QStringLiteral("targetId"), flags ? _targetData.value(QStringLiteral("targetId")) : QVariant()},
        {QStringLiteral("source"), flags ? _targetData.value(QStringLiteral("source")) : QVariant()},
        {QStringLiteral("rangeSource"),
         flags & DCLINK_TARGET_VIEW_FLAGS_RANGE ? _targetData.value(QStringLiteral("rangeSource")) : QVariant(0)}};
    for (const auto& field : {std::pair{"azimuthDeg", DCLINK_TARGET_VIEW_FLAGS_AZIMUTH},
                              {"elevationDeg", DCLINK_TARGET_VIEW_FLAGS_ELEVATION},
                              {"horizontalRangeM", DCLINK_TARGET_VIEW_FLAGS_RANGE},
                              {"latitudeDeg", DCLINK_TARGET_VIEW_FLAGS_GLOBAL_POSITION},
                              {"longitudeDeg", DCLINK_TARGET_VIEW_FLAGS_GLOBAL_POSITION},
                              {"relativeNorthM", DCLINK_TARGET_VIEW_FLAGS_RELATIVE_POSITION},
                              {"relativeEastM", DCLINK_TARGET_VIEW_FLAGS_RELATIVE_POSITION}}) {
        const auto key = QString::fromLatin1(field.first);
        view.insert(key, flags & field.second ? _targetData.value(key) : QVariant());
    }
    view.insert(QStringLiteral("ageMs"),
                flags ? QVariant(_targetData.value(QStringLiteral("ageMs")).toLongLong() + elapsed) : QVariant());
    view.insert(QStringLiteral("sourceAgeMs"),
                flags & DCLINK_TARGET_VIEW_FLAGS_SOURCE_AGE
                    ? QVariant(_targetData.value(QStringLiteral("sourceAgeMs")).toLongLong() + elapsed)
                    : QVariant());
    return view;
}
