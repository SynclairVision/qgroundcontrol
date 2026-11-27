#pragma once

#include <QtCore/QDeadlineTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

#include <optional>

#include "MAVLinkMessageType.h"

class DigiviewManager;

class DCLinkManager final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by SVBackend")

    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString vehicleText READ vehicleText NOTIFY vehicleTextChanged)
    Q_PROPERTY(QString commandText READ commandText NOTIFY commandTextChanged)
    Q_PROPERTY(QString commandNameText READ commandNameText NOTIFY commandNameTextChanged)
    Q_PROPERTY(QString commandStateText READ commandStateText NOTIFY commandStateTextChanged)
    Q_PROPERTY(bool commandOutcomeUnknown READ commandOutcomeUnknown NOTIFY commandOutcomeUnknownChanged)
    Q_PROPERTY(bool commandPending READ commandPending NOTIFY commandPendingChanged)
    Q_PROPERTY(QString targetText READ targetText NOTIFY targetTextChanged)
    Q_PROPERTY(QString targetStateText READ targetStateText NOTIFY targetStateTextChanged)
    Q_PROPERTY(QString navigationText READ navigationText NOTIFY navigationTextChanged)
    Q_PROPERTY(bool navigationHealthy READ navigationHealthy NOTIFY navigationHealthyChanged)
    Q_PROPERTY(double aircraftHeadingDeg READ aircraftHeadingDeg NOTIFY aircraftHeadingDegChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
    Q_PROPERTY(QString availabilityReason READ availabilityReason NOTIFY availabilityReasonChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY actionsChanged)
    Q_PROPERTY(QVariantMap targetView READ targetView NOTIFY targetViewChanged)

public:
    explicit DCLinkManager(DigiviewManager* digiview, QObject* parent = nullptr);
    ~DCLinkManager() override;

    bool connected() const;
    QString statusText() const;
    QString vehicleText() const;
    QString commandText() const;
    QString commandNameText() const;
    QString commandStateText() const;

    bool commandOutcomeUnknown() const { return _awaitingReconciliation; }

    bool commandPending() const { return _mutation.has_value(); }
    QString targetText() const;
    QString targetStateText() const;
    QString navigationText() const;
    bool navigationHealthy() const;
    double aircraftHeadingDeg() const;
    QString feedback() const { return _feedback; }
    QString availabilityReason() const;
    QVariantList actions() const;
    QVariantMap targetView() const;

    Q_INVOKABLE bool startAction(const QString& command);
    void setSelectedVehicleId(int systemId);

signals:
    void connectedChanged();
    void statusTextChanged();
    void vehicleTextChanged();
    void commandTextChanged();
    void commandNameTextChanged();
    void commandStateTextChanged();
    void commandOutcomeUnknownChanged();
    void commandPendingChanged();
    void targetTextChanged();
    void targetStateTextChanged();
    void navigationTextChanged();
    void navigationHealthyChanged();
    void aircraftHeadingDegChanged();
    void feedbackChanged();
    void availabilityReasonChanged();
    void actionsChanged();
    void targetViewChanged();

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

    struct Properties {
        bool connected = false;
        QString statusText;
        QString vehicleText;
        QString commandText;
        QString commandNameText;
        QString commandStateText;
        bool commandOutcomeUnknown = false;
        bool commandPending = false;
        QString targetText;
        QString targetStateText;
        QString navigationText;
        bool navigationHealthy = false;
        double aircraftHeadingDeg = 0;
        QString feedback;
        QString availabilityReason;
        QVariantList actions;
        QVariantMap targetView;
    };

    bool _transportReady() const;
    quint16 _nextReadRequestId();
    std::optional<quint16> _nextMutationRequestId();
    bool _sendStatus(quint16 requestId);
    bool _sendCommand(quint16 requestId, quint8 action, int item, const QString& command = {});
    void _pollTarget();
    std::optional<int> _aircraftContextId() const;
    void _clearTarget();
    void _receiveTarget(const mavlink_message_t& message);
    void _tick();
    void _receiveMessage(const mavlink_message_t& message);
    void _receiveHeading(const mavlink_message_t& message);
    void _resetSession();
    void _resetCatalog();
    void _markOutcomeUnknown();
    void _updateVehicle();
    void _notifyChanges();
    void _catalogFailed(const QString& reason);
    QString _actionReason(const QString& command) const;
    QString _commandLabel(const QString& command) const;

    DigiviewManager* const _digiview;
    int _selectedVehicleId = 0;
    QTimer _timer;
    QElapsedTimer _lastStatus;
    QElapsedTimer _lastPoll;
    QElapsedTimer _lastTarget;
    QElapsedTimer _lastHeading;
    double _aircraftHeadingDeg = 0;
    int _headingVehicleId = 0;
    QElapsedTimer _targetQueryAge;
    QVariantMap _targetData;
    quint64 _targetNextId = 1;
    quint64 _targetBootId = 0;
    quint32 _targetSequence = 0;
    quint32 _targetRequestId = 0;
    quint32 _statusFlags = 0;
    quint32 _targetFlags = 0;
    int _targetRemainingMs = 0;
    int _targetVehicleId = 0;
    int _targetContextId = -1;
    QDeadlineTimer _catalogRetry{0};
    QHash<int, Observation> _observations;
    QSet<QString> _catalogNames;
    QHash<QString, QString> _catalogLabels;
    std::optional<Request> _statusRequest;
    std::optional<Request> _catalogRequest;
    std::optional<Request> _mutation;
    quint16 _nextReadId = 0;
    quint16 _nextMutationId = 1;
    quint32 _issuedMutationIds = 0;
    int _catalogCount = 0;
    int _catalogNextItem = 0;
    int _autopilotConnection = 0;
    int _commandState = 0;
    int _targetStatus = 0;
    int _navigationType = 0;
    int _navigationStatus = 0;
    VehicleKind _vehicleKind = VehicleKind::Unknown;
    QString _vehicleReason;
    QString _activeCommand;
    QString _catalogError;
    QString _feedback;
    Properties _notifiedProperties;
    bool _connected = false;
    bool _catalogReady = false;
    bool _awaitingReconciliation = false;
};
