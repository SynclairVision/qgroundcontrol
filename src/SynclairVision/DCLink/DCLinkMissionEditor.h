#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQmlIntegration/QtQmlIntegration>

class DCLinkMissionClient;
class DigiviewManager;

/// Local route drafts and revision-aware library editing, independent of aircraft plans.
class DCLinkMissionEditor final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by SVBackend")

    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QString currentEntry READ currentEntry NOTIFY changed)
    Q_PROPERTY(QVariantMap mission READ mission NOTIFY changed)
    Q_PROPERTY(QVariantList waypoints READ waypoints NOTIFY changed)
    Q_PROPERTY(QString missionId READ missionId NOTIFY changed)
    Q_PROPERTY(quint32 revision READ revision NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool hasDraft READ hasDraft NOTIFY changed)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canEdit READ canEdit NOTIFY changed)
    Q_PROPERTY(bool canSave READ canSave NOTIFY changed)
    Q_PROPERTY(bool fresh READ fresh NOTIFY changed)
    Q_PROPERTY(bool destinationMatches READ destinationMatches NOTIFY changed)
    Q_PROPERTY(bool conflict READ conflict NOTIFY changed)
    Q_PROPERTY(bool reviewRequired READ reviewRequired NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
    Q_PROPERTY(bool canAcknowledgeReview READ canAcknowledgeReview NOTIFY changed)
    Q_PROPERTY(QString reviewDescription READ reviewDescription NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString draftSource READ draftSource NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString saveReason READ saveReason NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool executionSupported READ executionSupported NOTIFY changed)
    Q_PROPERTY(bool catalogReady READ catalogReady NOTIFY catalogChanged)
    Q_PROPERTY(QVariantList launchEntries READ launchEntries NOTIFY catalogChanged)
    Q_PROPERTY(bool executionReviewRequired READ executionReviewRequired NOTIFY changed)
    Q_PROPERTY(int executionVehicleId READ executionVehicleId NOTIFY changed)
    Q_PROPERTY(QString startReason READ startReason NOTIFY changed)
    Q_PROPERTY(QString runId READ runId NOTIFY changed)
    Q_PROPERTY(QString runText READ runText NOTIFY changed)
    Q_PROPERTY(QString flightRunText READ flightRunText NOTIFY changed)
    Q_PROPERTY(bool canCancelRun READ canCancelRun NOTIFY changed)
    Q_PROPERTY(int selectedWaypoint READ selectedWaypoint WRITE setSelectedWaypoint NOTIFY changed)

public:
    DCLinkMissionEditor(DCLinkMissionClient* client, DigiviewManager* digiview, QObject* parent = nullptr);

    QVariantList entries() const;
    QString currentEntry() const;
    QVariantMap mission() const;
    QVariantList waypoints() const;
    QString missionId() const;
    quint32 revision() const;
    bool dirty() const;

    bool hasDraft() const { return _drafts.contains(_draftId); }

    bool readOnly() const;
    bool busy() const;
    bool canEdit() const;
    bool canSave() const;
    bool fresh() const;
    bool destinationMatches() const;

    bool conflict() const;

    bool reviewRequired() const { return !_pendingMutation.isEmpty(); }

    bool canRetry() const;
    bool canAcknowledgeReview() const;
    QString reviewDescription() const;
    QString source() const;
    QString draftSource() const;
    QString status() const;
    QString saveReason() const;

    QString error() const { return _error; }

    bool executionSupported() const;
    bool catalogReady() const;
    QVariantList launchEntries() const;
    bool executionReviewRequired() const;
    int executionVehicleId() const;
    QString startReason() const;
    QString runId() const;
    QString runText() const;
    QString flightRunText() const;
    bool canCancelRun() const;

    int selectedWaypoint() const { return _selectedWaypoint; }

    void setSelectedWaypoint(int index);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectEntry(const QString& key);
    Q_INVOKABLE void newDraft();
    Q_INVOKABLE void duplicateDraft();
    Q_INVOKABLE void discardDraft();
    Q_INVOKABLE void setName(const QString& name);
    Q_INVOKABLE void addWaypoint(double latitude, double longitude);
    Q_INVOKABLE void updateWaypoint(int index, const QString& field, double value);
    Q_INVOKABLE void setWaypointCoordinate(int index, double latitude, double longitude);
    Q_INVOKABLE void moveWaypoint(int from, int to);
    Q_INVOKABLE void removeWaypoint(int index);
    Q_INVOKABLE void save();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void deleteMission();
    Q_INVOKABLE void startMission(const QString& savedId, quint32 savedRevision, int vehicleId,
                                  const QString& session);
    Q_INVOKABLE void cancelRun(const QString& expectedRunId);
    Q_INVOKABLE void retryChange();
    Q_INVOKABLE void acknowledgeReview();
    Q_INVOKABLE QVariantMap previewPlan(const QString& path, double defaultRadius);
    Q_INVOKABLE void acceptPlanImport();

signals:
    void changed();
    void catalogChanged();
    void routeLoaded();

private:
    QVariantMap _endpoint() const;
    QVariantMap _draft() const;
    void _setDraft(const QVariantMap& draft);
    void _editMission(const QVariantMap& mission);
    void _createDraft(const QVariantMap& mission);
    bool _persist();
    void _restore();
    bool _send(const QString& action, const QVariantMap& payload, const QString& purpose,
               const QString& requestId = {});
    void _mutate(const QString& action, const QVariantMap& payload);
    void _finished(const QString& id, const QString& action, const QVariantMap& result);
    void _listPage(const QString& cursor = {});
    void _invalidateCatalog();
    void _loadRecord(const QVariantMap& record);
    void _applyMutation(const QVariantMap& data);
    bool _executionStatusFresh() const;
    bool _runFresh() const;
    void _pollRunStatus();
    void _receiveRunStatus(const QVariantMap& data);
    void _executionFinished(const QString& action, const QString& code, const QVariantMap& data);

    DCLinkMissionClient* const _client;
    DigiviewManager* const _digiview;
    const QString _processId;
    QString _draftId;
    QMap<QString, QVariantMap> _drafts;
    QVariantList _catalog;
    QVariantList _catalogPages;
    QStringList _listCursors;
    QVariantMap _catalogEndpoint;
    QVariantMap _pendingMutation;
    QVariantMap _importPreview;
    QString _verifiedSession;
    QVariantMap _verifiedEndpoint;
    QString _requestId;
    QString _requestedMissionId;
    QString _purpose;
    QString _error;
    QString _storageError;
    bool _conflict = false;
    bool _reviewedCatalog = false;
    int _listRetries = 0;
    int _selectedWaypoint = -1;
    QTimer _runPoll;
    QElapsedTimer _runReportAge;
    QElapsedTimer _runRequestAge;
    QString _runStatusRequest;
    QVariantMap _executionStatus;
    QVariantMap _runEndpoint;
    QString _runServiceSession;
    QVariantMap _run;
    QVariantMap _executionEndpoint;
    QString _executionServiceSession;
};
