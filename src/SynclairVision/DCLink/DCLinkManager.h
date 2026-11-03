#pragma once

#include <optional>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "MAVLinkMessageType.h"

class DigiviewManager;

class DCLinkManager final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by SVBackend")

    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString vehicleText READ vehicleText NOTIFY stateChanged)
    Q_PROPERTY(QString commandText READ commandText NOTIFY stateChanged)
    Q_PROPERTY(QString targetText READ targetText NOTIFY stateChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY stateChanged)
    Q_PROPERTY(QString availabilityReason READ availabilityReason NOTIFY stateChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY stateChanged)

public:
    explicit DCLinkManager(DigiviewManager* digiview, QObject* parent = nullptr);
    ~DCLinkManager() override;

    bool connected() const { return _connected; }
    QString statusText() const;
    QString vehicleText() const;
    QString commandText() const;
    QString targetText() const;
    QString feedback() const { return _feedback; }
    QString availabilityReason() const;
    QVariantList actions() const;

    Q_INVOKABLE bool startAction(const QString& command);
    void setSelectedVehicleId(int systemId);

signals:
    void stateChanged();

private:
    enum class VehicleKind { Unknown, Copter, Plane };

    struct Request {
        quint16 id = 0;
        QString command{};
        int item = 0;
        int attempts = 1;
        QDeadlineTimer deadline{1500};
    };

    struct Observation {
        quint8 type = 0;
        QElapsedTimer age;
    };

    bool _transportReady() const;
    std::optional<quint16> _nextRequestId();
    bool _sendStatus(quint16 requestId);
    bool _sendCommand(quint16 requestId, quint8 action, int item, const QString& command = {});
    void _tick();
    void _receiveMessage(const mavlink_message_t& message);
    void _resetSession();
    void _resetCatalog();
    void _markOutcomeUnknown();
    void _updateVehicle();
    void _catalogFailed(const QString& reason);
    QString _actionReason(const QString& command) const;
    QString _commandLabel(const QString& command) const;
    QStringList _pilotCommands() const;

    DigiviewManager* const _digiview;
    int _selectedVehicleId = 0;
    QTimer _timer;
    QElapsedTimer _lastStatus;
    QElapsedTimer _lastPoll;
    QDeadlineTimer _catalogRetry{0};
    QHash<int, Observation> _observations;
    QSet<QString> _catalogNames;
    QHash<QString, QString> _catalogLabels;
    std::optional<Request> _statusRequest;
    std::optional<Request> _catalogRequest;
    std::optional<Request> _mutation;
    quint32 _nextId = 0;
    quint32 _issuedIds = 0;
    int _catalogCount = 0;
    int _catalogNextItem = 0;
    int _autopilotConnection = 0;
    int _commandState = 0;
    int _targetStatus = 0;
    VehicleKind _vehicleKind = VehicleKind::Unknown;
    QString _vehicleReason;
    QString _activeCommand;
    QString _catalogError;
    QString _feedback;
    bool _connected = false;
    bool _catalogReady = false;
    bool _awaitingReconciliation = false;
};
