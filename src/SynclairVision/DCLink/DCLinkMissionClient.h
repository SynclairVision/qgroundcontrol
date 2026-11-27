#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

#include "MAVLinkMessageType.h"

class DigiviewManager;

class DCLinkMissionClient final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by SVBackend")

    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool outcomeUnknown READ outcomeUnknown NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap uncertainOperation READ uncertainOperation NOTIFY stateChanged)
    Q_PROPERTY(QString serviceSession READ serviceSession NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap lastResult READ lastResult NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(int peerSystemId READ peerSystemId NOTIFY stateChanged)
    Q_PROPERTY(int peerComponentId READ peerComponentId NOTIFY stateChanged)

public:
    explicit DCLinkMissionClient(DigiviewManager* digiview, QObject* parent = nullptr);

    bool available() const;
    bool busy() const;
    bool readingRunStatus() const;

    bool outcomeUnknown() const { return _outcomeUnknown; }

    QVariantMap uncertainOperation() const { return _uncertainOperation; }

    QString serviceSession() const;

    QVariantMap capabilities() const { return _capabilities; }

    QVariantMap lastResult() const { return _lastResult; }

    QString error() const { return _error; }

    int peerSystemId() const { return _peerSystem; }

    int peerComponentId() const { return _peerComponent; }

    Q_INVOKABLE QString discover();
    Q_INVOKABLE QString request(const QString& action, const QVariantMap& payload, const QString& requestId = {});
    Q_INVOKABLE bool setPeer(int systemId, int componentId);
    Q_INVOKABLE void acknowledgeOutcome();
    Q_INVOKABLE void cancelRequest();

signals:
    void stateChanged();
    void finished(const QString& requestId, const QString& action, const QVariantMap& result);

private:
    enum class Phase
    {
        Idle,
        Begin,
        Upload,
        Commit,
        Read
    };

    bool _transportReady() const;
    void _resetSession();
    void _tick();
    void _receiveMessage(const mavlink_message_t& message);
    void _sendStage(bool resend = false);
    void _sendAbort();
    void _rememberUncertain();
    bool _sendRequest(int stage, quint32 offset);
    void _complete(const QVariantMap& result);
    void _fail(const QString& reason, bool resetSession = false, bool abortTransfer = true);

    DigiviewManager* const _digiview;
    const QByteArray _clientId;
    int _peerSystem = 252;
    int _peerComponent = 67;
    QByteArray _session;
    QByteArray _wireSession;
    QByteArray _candidateSession;
    QByteArray _requestId;
    QByteArray _transferId;
    QByteArray _upload;
    QByteArray _uploadDigest;
    QByteArray _reply;
    QByteArray _replyDigest;
    qsizetype _replySize = 0;
    qsizetype _offset = 0;
    qsizetype _sentLength = 0;
    QString _action;
    QString _error;
    QString _uncertainRequestId;
    QVariantMap _capabilities;
    QVariantMap _lastResult;
    QVariantMap _operationEndpoint;
    QVariantMap _uncertainOperation;
    QHash<QString, QByteArray> _mutationBindings;
    Phase _phase = Phase::Idle;
    bool _mutation = false;
    bool _commitSent = false;
    bool _outcomeUnknown = false;
    bool _finishing = false;
    int _resends = 0;
    QTimer _timer;
    QElapsedTimer _sentAt;
};
