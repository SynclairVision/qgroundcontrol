#include "DCLinkMissionClient.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStringConverter>
#include <QtCore/QStringList>
#include <QtCore/QUuid>

#include <algorithm>
#include <cmath>
#include <cstddef>

// clang-format off: load the full bus before DigiView's narrower dialect header.
#include "MAVLinkLib.h"
// clang-format on

#include "DCLinkMissionJson.h"
#include "DigiviewConnection.h"
#include "DigiviewManager.h"

namespace {
constexpr qsizetype CHUNK_BYTES = 160;
constexpr qsizetype MAX_TRANSFER_BYTES = 264192;
const QByteArray ZERO_SESSION(16, '\0');

bool validUuid(const QByteArray& bytes)
{
    const QUuid id = QUuid::fromRfc4122(bytes);
    return bytes.size() == 16 && id.version() == QUuid::Random && id.variant() == QUuid::DCE;
}

QString uuidText(const QByteArray& bytes)
{
    return QUuid::fromRfc4122(bytes).toString(QUuid::WithoutBraces);
}

template <std::size_t N>
QByteArray wireBytes(const uint8_t (&value)[N])
{
    return QByteArray(reinterpret_cast<const char*>(value), N);
}

template <std::size_t N>
void setBytes(uint8_t (&destination)[N], const QByteArray& value)
{
    std::copy_n(reinterpret_cast<const uint8_t*>(value.constData()), std::min<qsizetype>(N, value.size()), destination);
}

bool validJson(const QVariant& value, int depth = 0)
{
    if (depth > 64) {
        return false;
    }
    if (value.isNull()) {
        return true;
    }
    switch (value.typeId()) {
        case QMetaType::Bool:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
            return true;
        case QMetaType::Double:
        case QMetaType::Float:
            return std::isfinite(value.toDouble());
        case QMetaType::QString:
            return value.toString().isValidUtf16();
        case QMetaType::QVariantList:
        case QMetaType::QStringList:
            for (const QVariant& item : value.toList()) {
                if (!validJson(item, depth + 1)) {
                    return false;
                }
            }
            return true;
        case QMetaType::QVariantMap: {
            const QVariantMap map = value.toMap();
            for (auto it = map.cbegin(); it != map.cend(); ++it) {
                if (!it.key().isValidUtf16() || !validJson(it.value(), depth + 1)) {
                    return false;
                }
            }
            return true;
        }
        default:
            return false;
    }
}

bool knownAction(const QString& action)
{
    static const QStringList actions = {
        QStringLiteral("missions.capabilities"), QStringLiteral("missions.list"),   QStringLiteral("missions.get"),
        QStringLiteral("missions.create"),       QStringLiteral("missions.update"), QStringLiteral("missions.delete"),
        QStringLiteral("missions.import_route"), QStringLiteral("missions.start"), QStringLiteral("missions.cancel"),
        QStringLiteral("missions.run_status"),
    };
    return actions.contains(action);
}
}  // namespace

DCLinkMissionClient::DCLinkMissionClient(DigiviewManager* digiview, QObject* parent)
    : QObject(parent), _digiview(digiview), _clientId(QUuid::createUuid().toRfc4122())
{
    if (!_digiview || !_digiview->connection()) {
        return;
    }
    auto* connection = _digiview->connection();
    connect(connection, &DigiviewConnection::messageReceived, this, &DCLinkMissionClient::_receiveMessage);
    connect(connection, &DigiviewConnection::connectedChanged, this, &DCLinkMissionClient::_resetSession);
    connect(connection, &DigiviewConnection::hostChanged, this, &DCLinkMissionClient::_resetSession);
    connect(connection, &DigiviewConnection::portChanged, this, &DCLinkMissionClient::_resetSession);
    connect(connection, &DigiviewConnection::listenPortChanged, this, &DCLinkMissionClient::_resetSession);
    connect(_digiview, &DigiviewManager::sessionRequestedChanged, this, &DCLinkMissionClient::_resetSession);
    connect(_digiview, &DigiviewManager::senderIdentityChanged, this, &DCLinkMissionClient::_resetSession);
    connect(&_timer, &QTimer::timeout, this, &DCLinkMissionClient::_tick);
    _timer.start(100);
}

bool DCLinkMissionClient::_transportReady() const
{
    return _digiview && _digiview->sessionRequested() && _digiview->transportConnected() && _digiview->connection() &&
           !_digiview->connection()->usingLegacyTcpControl() && _digiview->senderSystemId() > 0 &&
           _digiview->senderSystemId() <= 255 && _digiview->senderComponentId() > 0 &&
           _digiview->senderComponentId() <= 255 &&
           (_peerSystem != _digiview->senderSystemId() || _peerComponent != _digiview->senderComponentId());
}

bool DCLinkMissionClient::available() const
{
    return !_session.isEmpty() && _transportReady();
}

bool DCLinkMissionClient::busy() const
{
    return _phase != Phase::Idle;
}

bool DCLinkMissionClient::readingRunStatus() const
{
    return busy() && _action == QStringLiteral("missions.run_status");
}

QString DCLinkMissionClient::serviceSession() const
{
    return _session.isEmpty() ? QString() : uuidText(_session);
}

bool DCLinkMissionClient::setPeer(int systemId, int componentId)
{
    if (systemId < 1 || systemId > 255 || componentId < 1 || componentId > 255) {
        return false;
    }
    if (_peerSystem != systemId || _peerComponent != componentId) {
        _peerSystem = systemId;
        _peerComponent = componentId;
        _resetSession();
    }
    return true;
}

QString DCLinkMissionClient::discover()
{
    return request(QStringLiteral("missions.capabilities"), {});
}

QString DCLinkMissionClient::request(const QString& action, const QVariantMap& payload, const QString& requestId)
{
    if (_finishing) {
        return {};
    }
    if (readingRunStatus() && action != QStringLiteral("missions.run_status")) {
        _fail({}, false);
    }
    const bool discovery = action == QStringLiteral("missions.capabilities");
    if (busy() || !_transportReady() || !knownAction(action) || (!discovery && !available())) {
        _error = tr("Connect to DCLink and discover its mission service before sending a request.");
        emit stateChanged();
        return {};
    }
    if (!discovery && !_capabilities.value(QStringLiteral("operations")).toStringList().contains(action)) {
        _error = tr("The DCLink mission service does not support this operation.");
        emit stateChanged();
        return {};
    }
    if (!validJson(payload)) {
        _error = tr("Mission data must contain valid, finite JSON values.");
        emit stateChanged();
        return {};
    }
    const QUuid id = requestId.isEmpty() ? QUuid::createUuid() : QUuid(requestId);
    if (!validUuid(id.toRfc4122()) || (!requestId.isEmpty() && id.toString(QUuid::WithoutBraces) != requestId)) {
        _error = tr("The mission request ID must be a lowercase UUIDv4.");
        emit stateChanged();
        return {};
    }
    const QByteArray upload = QJsonDocument(QJsonObject{
                                                {QStringLiteral("action"), action},
                                                {QStringLiteral("payload"), QJsonObject::fromVariantMap(payload)},
                                            })
                                  .toJson(QJsonDocument::Compact);
    if (upload.size() > MAX_TRANSFER_BYTES) {
        _error = tr("The mission request is too large.");
        emit stateChanged();
        return {};
    }
    const bool mutation = action != QStringLiteral("missions.capabilities") &&
                          action != QStringLiteral("missions.list") && action != QStringLiteral("missions.get") &&
                          action != QStringLiteral("missions.run_status");
    const QJsonObject bindingData{
        {QStringLiteral("host"), _digiview->host()},
        {QStringLiteral("port"), _digiview->port()},
        {QStringLiteral("listenPort"), _digiview->listenPort()},
        {QStringLiteral("senderSystem"), _digiview->senderSystemId()},
        {QStringLiteral("senderComponent"), _digiview->senderComponentId()},
        {QStringLiteral("peerSystem"), _peerSystem},
        {QStringLiteral("peerComponent"), _peerComponent},
        {QStringLiteral("session"), serviceSession()},
        {QStringLiteral("bodySha256"),
         QString::fromLatin1(QCryptographicHash::hash(upload, QCryptographicHash::Sha256).toHex())},
    };
    const QByteArray binding =
        QCryptographicHash::hash(QJsonDocument(bindingData).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
    const QString textId = id.toString(QUuid::WithoutBraces);
    if (mutation && _outcomeUnknown && textId != _uncertainRequestId) {
        _error = tr("Review the uncertain mission change before sending another change, or retry its original ID.");
        emit stateChanged();
        return {};
    }
    if (_mutationBindings.contains(textId) && _mutationBindings.value(textId) != binding) {
        _error = tr("This mission request ID belongs to different content, service, or connection.");
        emit stateChanged();
        return {};
    }
    if (mutation && !_mutationBindings.contains(textId)) {
        if (_mutationBindings.size() >= 1024) {
            _error = tr("The mission request history is full for this QGC session.");
            emit stateChanged();
            return {};
        }
        _mutationBindings.insert(textId, binding);
    }
    _action = action;
    _operationEndpoint = {
        {QStringLiteral("host"), _digiview->host()},
        {QStringLiteral("port"), _digiview->port()},
        {QStringLiteral("listen_port"), _digiview->listenPort()},
        {QStringLiteral("peer_system_id"), _peerSystem},
        {QStringLiteral("peer_component_id"), _peerComponent},
    };
    _requestId = id.toRfc4122();
    _transferId = QUuid::createUuid().toRfc4122();
    _wireSession = discovery ? ZERO_SESSION : _session;
    _candidateSession.clear();
    _upload = upload;
    _uploadDigest = QCryptographicHash::hash(upload, QCryptographicHash::Sha256);
    _reply.clear();
    _replyDigest.clear();
    _replySize = 0;
    _offset = 0;
    _commitSent = false;
    _mutation = mutation;
    _error.clear();
    _phase = Phase::Begin;
    _sendStage();
    emit stateChanged();
    return textId;
}

bool DCLinkMissionClient::_sendRequest(int stage, quint32 offset)
{
    mavlink_dclink_mission_request_t payload{};
    payload.protocol_version = 1;
    payload.target_system = _peerSystem;
    payload.target_component = _peerComponent;
    payload.stage = stage;
    payload.offset = offset;
    setBytes(payload.service_session, _wireSession);
    setBytes(payload.client_id, _clientId);
    setBytes(payload.request_id, _requestId);
    setBytes(payload.transfer_id, _transferId);
    if (stage == DCLINK_MISSION_STAGE_BEGIN || stage == DCLINK_MISSION_STAGE_COMMIT) {
        payload.total_size = _upload.size();
        setBytes(payload.sha256, _uploadDigest);
    }
    mavlink_message_t frame{};
    mavlink_msg_dclink_mission_request_encode(_digiview->senderSystemId(), _digiview->senderComponentId(), &frame,
                                              &payload);
    if (stage == DCLINK_MISSION_STAGE_COMMIT) {
        _commitSent = true;
    }
    return _digiview->connection()->sendMessage(frame);
}

void DCLinkMissionClient::_sendStage(bool resend)
{
    if (!_transportReady()) {
        _fail(tr("The mission connection is unavailable."), true, false);
        return;
    }
    if (!resend) {
        _resends = 0;
    }
    _sentAt.start();
    bool sent = false;
    if (_phase == Phase::Upload) {
        mavlink_dclink_mission_data_t payload{};
        payload.protocol_version = 1;
        payload.target_system = _peerSystem;
        payload.target_component = _peerComponent;
        payload.offset = _offset;
        _sentLength = std::min(CHUNK_BYTES, _upload.size() - _offset);
        payload.data_length = _sentLength;
        setBytes(payload.service_session, _wireSession);
        setBytes(payload.client_id, _clientId);
        setBytes(payload.request_id, _requestId);
        setBytes(payload.transfer_id, _transferId);
        setBytes(payload.data, _upload.mid(_offset, _sentLength));
        mavlink_message_t frame{};
        mavlink_msg_dclink_mission_data_encode(_digiview->senderSystemId(), _digiview->senderComponentId(), &frame,
                                               &payload);
        sent = _digiview->connection()->sendMessage(frame);
    } else {
        const int stage = _phase == Phase::Begin
                              ? DCLINK_MISSION_STAGE_BEGIN
                              : (_phase == Phase::Commit ? DCLINK_MISSION_STAGE_COMMIT : DCLINK_MISSION_STAGE_READ);
        sent = _sendRequest(stage, _phase == Phase::Begin ? 0 : _offset);
        if (_phase == Phase::Commit && sent) {
            _commitSent = true;
        }
    }
    if (!sent) {
        _fail(tr("The mission request could not be sent."), true, false);
    }
}

void DCLinkMissionClient::_tick()
{
    if (!busy()) {
        return;
    }
    if (!_transportReady()) {
        _resetSession();
        return;
    }
    if (_sentAt.elapsed() >= 2000) {
        if (_resends >= 3) {
            _fail(tr("The mission exchange timed out."));
            return;
        }
        ++_resends;
        _sendStage(true);
    }
}

void DCLinkMissionClient::_receiveMessage(const mavlink_message_t& message)
{
    if (!busy() || message.sysid != _peerSystem || message.compid != _peerComponent) {
        return;
    }
    QByteArray session;
    if (message.msgid == MAVLINK_MSG_ID_DCLINK_MISSION_RESULT) {
        mavlink_dclink_mission_result_t payload{};
        mavlink_msg_dclink_mission_result_decode(&message, &payload);
        if (payload.protocol_version != 1 || payload.target_system != _digiview->senderSystemId() ||
            payload.target_component != _digiview->senderComponentId() || wireBytes(payload.client_id) != _clientId ||
            wireBytes(payload.request_id) != _requestId || wireBytes(payload.transfer_id) != _transferId) {
            return;
        }
        const int expected = _phase == Phase::Begin
                                 ? DCLINK_MISSION_STAGE_BEGIN
                                 : (_phase == Phase::Upload ? DCLINK_MISSION_STAGE_CHUNK
                                                            : (_phase == Phase::Commit ? DCLINK_MISSION_STAGE_COMMIT
                                                                                       : DCLINK_MISSION_STAGE_READ));
        if (payload.stage != expected) {
            return;
        }
        session = wireBytes(payload.service_session);
        if (!validUuid(session) || (!_candidateSession.isEmpty() && session != _candidateSession) ||
            payload.result == DCLINK_MISSION_TRANSFER_RESULT_STALE_SESSION) {
            _fail(tr("The DCLink mission service changed. Discover it again before continuing."), true, false);
            return;
        }
        if (_wireSession != ZERO_SESSION && session != _wireSession) {
            _fail(tr("The DCLink mission service changed."), true, false);
            return;
        }
        if (payload.result != 0) {
            _fail(tr("The mission transfer was rejected (%1).").arg(payload.result));
            return;
        }
        if (_phase == Phase::Begin || _phase == Phase::Upload) {
            if (payload.total_size != _upload.size() || wireBytes(payload.sha256) != _uploadDigest) {
                _fail(tr("The mission upload acknowledgement is invalid."));
                return;
            }
            if (_phase == Phase::Upload && payload.offset != _offset + _sentLength) {
                return;
            }
            if (payload.offset > _upload.size()) {
                _fail(tr("The mission upload offset is invalid."));
                return;
            }
            _candidateSession = session;
            _offset = payload.offset;
            _phase = _offset == _upload.size() ? Phase::Commit : Phase::Upload;
        } else if (_phase == Phase::Commit) {
            if (payload.offset != 0 || payload.total_size == 0 || payload.total_size > MAX_TRANSFER_BYTES) {
                _fail(tr("The mission reply descriptor is invalid."));
                return;
            }
            _replySize = payload.total_size;
            _replyDigest = wireBytes(payload.sha256);
            _offset = 0;
            _phase = Phase::Read;
        } else {
            _fail(tr("Unexpected mission read acknowledgement."));
            return;
        }
        _sendStage();
        return;
    }
    if (message.msgid != MAVLINK_MSG_ID_DCLINK_MISSION_DATA || _phase != Phase::Read) {
        return;
    }
    mavlink_dclink_mission_data_t payload{};
    mavlink_msg_dclink_mission_data_decode(&message, &payload);
    if (payload.protocol_version != 1 || payload.target_system != _digiview->senderSystemId() ||
        payload.target_component != _digiview->senderComponentId() || wireBytes(payload.client_id) != _clientId ||
        wireBytes(payload.request_id) != _requestId || wireBytes(payload.transfer_id) != _transferId) {
        return;
    }
    if (wireBytes(payload.service_session) != _candidateSession) {
        _fail(tr("The DCLink mission service changed."), true, false);
        return;
    }
    if (payload.offset != _offset) {
        return;
    }
    if (payload.data_length < 1 || payload.data_length > CHUNK_BYTES || _offset + payload.data_length > _replySize) {
        _fail(tr("The mission reply chunk is invalid."));
        return;
    }
    _reply.append(reinterpret_cast<const char*>(payload.data), payload.data_length);
    _offset += payload.data_length;
    if (_offset < _replySize) {
        _sendStage();
        return;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(_reply);
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(_reply, &parseError);
    if (decoder.hasError() || QCryptographicHash::hash(_reply, QCryptographicHash::Sha256) != _replyDigest ||
        parseError.error != QJsonParseError::NoError || !document.isObject() || !text.isValidUtf16()) {
        _fail(tr("The complete mission reply is invalid."));
        return;
    }
    const QVariantMap result = document.object().toVariantMap();
    const QString code = result.value(QStringLiteral("result")).toString();
    const QStringList codes = {QStringLiteral("ok"),
                               QStringLiteral("not_found"),
                               QStringLiteral("invalid_value"),
                               QStringLiteral("unsupported"),
                               QStringLiteral("conflict"),
                               QStringLiteral("busy"),
                               QStringLiteral("stale_session"),
                               QStringLiteral("error")};
    if (!validJson(result) || !DCLinkMissionJson::uniqueObjectKeys(_reply) ||
        result.value(QStringLiteral("client_id")).toString() != uuidText(_clientId) ||
        result.value(QStringLiteral("request_id")).toString() != uuidText(_requestId) ||
        result.value(QStringLiteral("service_session")).toString() != uuidText(_candidateSession) ||
        result.value(QStringLiteral("action")).toString() != _action || !codes.contains(code) ||
        !document.object().value(QStringLiteral("data")).isObject()) {
        _fail(tr("The mission reply does not match this request."));
        return;
    }
    if (_action == QStringLiteral("missions.capabilities")) {
        const QVariantMap capabilities = result.value(QStringLiteral("data")).toMap();
        const QJsonObject data = document.object().value(QStringLiteral("data")).toObject();
        const QJsonObject limits = data.value(QStringLiteral("limits")).toObject();
        if (code != QStringLiteral("ok") || capabilities.value(QStringLiteral("protocol_version")).toInt() != 1 ||
            !data.value(QStringLiteral("schema_versions")).toArray().contains(
                QJsonValue(DCLinkMissionJson::SCHEMA_VERSION)) ||
            limits.value(QStringLiteral("max_chunk_bytes")).toInt() != CHUNK_BYTES ||
            limits.value(QStringLiteral("max_transfer_bytes")).toInt() != MAX_TRANSFER_BYTES ||
            !data.value(QStringLiteral("transports")).toArray().contains(QJsonValue(QStringLiteral("mavlink"))) ||
            !capabilities.value(QStringLiteral("operations")).toStringList().contains(QStringLiteral("missions.get"))) {
            _fail(tr("The DCLink mission protocol is unsupported."), true);
            return;
        }
        _session = _candidateSession;
        _capabilities = capabilities;
    }
    _complete(result);
}

void DCLinkMissionClient::_sendAbort()
{
    if (_transportReady()) {
        _sendRequest(DCLINK_MISSION_STAGE_ABORT, 0);
    }
}

void DCLinkMissionClient::_complete(const QVariantMap& result)
{
    _finishing = true;
    const QString id = uuidText(_requestId);
    const QString action = _action;
    _sendAbort();
    _phase = Phase::Idle;
    _lastResult = result;
    if (_mutation && result.value(QStringLiteral("result")).toString() == QStringLiteral("error")) {
        _rememberUncertain();
    } else if (id == _uncertainRequestId) {
        _outcomeUnknown = false;
        _uncertainRequestId.clear();
        _uncertainOperation.clear();
    }
    _error = result.value(QStringLiteral("result")).toString() == QStringLiteral("ok")
                 ? QString()
                 : result.value(QStringLiteral("result")).toString();
    _upload.clear();
    _reply.clear();
    _finishing = false;
    emit stateChanged();
    emit finished(id, action, result);
}

void DCLinkMissionClient::_fail(const QString& reason, bool resetSession, bool abortTransfer)
{
    if (!busy() || _finishing) {
        return;
    }
    _finishing = true;
    const QString id = uuidText(_requestId);
    const QString action = _action;
    if (abortTransfer) {
        _sendAbort();
    }
    if (_mutation && _commitSent) {
        _rememberUncertain();
    }
    _phase = Phase::Idle;
    _error = reason;
    if (resetSession) {
        _session.clear();
        _capabilities.clear();
    }
    _upload.clear();
    _reply.clear();
    _lastResult = {
        {QStringLiteral("request_id"), id},
        {QStringLiteral("action"), action},
        {QStringLiteral("client_id"), uuidText(_clientId)},
        {QStringLiteral("service_session"), _wireSession == ZERO_SESSION ? QString() : uuidText(_wireSession)},
        {QStringLiteral("result"), QStringLiteral("error")},
        {QStringLiteral("data"),
         QVariantMap{{QStringLiteral("reason"), reason}, {QStringLiteral("outcome_unknown"), _outcomeUnknown}}}};
    const QVariantMap result = _lastResult;
    _finishing = false;
    emit stateChanged();
    emit finished(id, action, result);
}

void DCLinkMissionClient::_resetSession()
{
    if (_finishing) {
        _session.clear();
        _capabilities.clear();
        return;
    }
    if (busy()) {
        _fail(tr("The mission connection changed. Discover the service again."), true, false);
    } else {
        _session.clear();
        _capabilities.clear();
        emit stateChanged();
    }
}

void DCLinkMissionClient::_rememberUncertain()
{
    _outcomeUnknown = true;
    _uncertainRequestId = uuidText(_requestId);
    _uncertainOperation = {
        {QStringLiteral("request_id"), _uncertainRequestId},
        {QStringLiteral("action"), _action},
        {QStringLiteral("service_session"), uuidText(_wireSession)},
        {QStringLiteral("endpoint"), _operationEndpoint},
    };
}

void DCLinkMissionClient::acknowledgeOutcome()
{
    if (!busy()) {
        _outcomeUnknown = false;
        _uncertainRequestId.clear();
        _uncertainOperation.clear();
        emit stateChanged();
    }
}

void DCLinkMissionClient::cancelRequest()
{
    if (busy()) {
        _fail(tr("The mission transfer was canceled."));
    }
}
