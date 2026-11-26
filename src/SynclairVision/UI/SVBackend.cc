#include "SVBackend.h"

#include "DCLinkManager.h"
#include "DCLinkMissionClient.h"
#include "DCLinkMissionEditor.h"
#include "DigiviewManager.h"
#include "VideoManager.h"
#include "VideoSourceConfiguration.h"

SVBackend::SVBackend(DigiviewManager* digiview, QObject* parent)
    : QObject(parent)
    , _digiview(digiview)
    , _dclink(new DCLinkManager(digiview, this))
    , _missions(new DCLinkMissionClient(digiview, this))
    , _missionEditor(new DCLinkMissionEditor(_missions, digiview, this))
{
}

void SVBackend::configureNetworkVideo(bool enabled, const QString& uri, bool forceRtspTcp)
{
    _digiview->setLegacyTcpControlEnabled(forceRtspTcp);

    VideoSourceConfiguration primaryConfiguration;
    primaryConfiguration.uriOverrideEnabled = enabled;
    primaryConfiguration.uri = enabled ? uri : QString();
    primaryConfiguration.forceRtspTcp = enabled && forceRtspTcp;

    VideoSourceConfiguration thermalConfiguration;
    thermalConfiguration.uriOverrideEnabled = enabled;
    thermalConfiguration.forceRtspTcp = enabled && forceRtspTcp;

    VideoManager::instance()->setSourceConfiguration(QStringLiteral("thermalVideo"), thermalConfiguration);
    VideoManager::instance()->setSourceConfiguration(QStringLiteral("videoContent"), primaryConfiguration);
}
