#pragma once

#include <QtCore/QObject>
#include <QtQmlIntegration/QtQmlIntegration>

class DigiviewManager;
class DCLinkManager;
class DCLinkMissionClient;
class QJSEngine;
class QQmlEngine;

Q_MOC_INCLUDE("DigiviewManager.h")
Q_MOC_INCLUDE("DCLinkManager.h")
Q_MOC_INCLUDE("DCLinkMissionClient.h")

class SVBackend final : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SVBackend)
    QML_SINGLETON

    Q_PROPERTY(DigiviewManager* digiview READ digiview CONSTANT)
    Q_PROPERTY(DCLinkManager* dclink READ dclink CONSTANT)
    Q_PROPERTY(DCLinkMissionClient* missions READ missions CONSTANT)

public:
    explicit SVBackend(DigiviewManager* digiview, QObject* parent = nullptr);

    static SVBackend* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    DigiviewManager* digiview() const { return _digiview; }
    DCLinkManager* dclink() const { return _dclink; }
    DCLinkMissionClient* missions() const { return _missions; }

    Q_INVOKABLE void configureNetworkVideo(bool enabled, const QString& uri, bool forceRtspTcp);

private:
    DigiviewManager* const _digiview;
    DCLinkManager* const _dclink;
    DCLinkMissionClient* const _missions;
};
