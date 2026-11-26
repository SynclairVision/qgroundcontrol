#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QUuid>

#include <cmath>

#include "DCLinkMissionEditor.h"
#include "DCLinkMissionJson.h"
#include "MissionItem.h"

QVariantMap DCLinkMissionEditor::previewPlan(const QString& path, double defaultRadius)
{
    _importPreview.clear();
    QStringList errors;
    QJsonObject root;
    QFile file(path);
    QString error;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024 ||
        !DCLinkMissionJson::parseObject(file.readAll(), root, error)) {
        return {{QStringLiteral("errors"),
                 QStringList{tr("Cannot import this plan: %1").arg(error.isEmpty() ? file.errorString() : error)}}};
    }
    const auto checkKeys = [&errors](const QJsonObject& object, const QSet<QString>& allowed, const QString& context) {
        for (const QString& key : object.keys()) {
            if (!allowed.contains(key)) {
                errors.append(tr("%1: unsupported field %2").arg(context, key));
            }
        }
    };
    checkKeys(root,
              {QStringLiteral("fileType"), QStringLiteral("version"), QStringLiteral("groundStation"),
               QStringLiteral("mission"), QStringLiteral("geoFence"), QStringLiteral("rallyPoints")},
              tr("Plan"));
    if (root.value(QStringLiteral("fileType")).toString() != QStringLiteral("Plan") ||
        root.value(QStringLiteral("version")).toDouble() != 1 || !root.value(QStringLiteral("mission")).isObject()) {
        errors.append(tr("Only QGC version-1 .plan files are supported."));
    }
    for (const QString& section : {QStringLiteral("geoFence"), QStringLiteral("rallyPoints")}) {
        if (!root.contains(section)) {
            continue;
        }
        const QJsonObject object = root.value(section).toObject();
        const QStringList collections = section == QStringLiteral("geoFence")
                                            ? QStringList{QStringLiteral("polygons"), QStringLiteral("circles")}
                                            : QStringList{QStringLiteral("points")};
        QSet<QString> allowed(collections.begin(), collections.end());
        allowed.insert(QStringLiteral("version"));
        checkKeys(object, allowed, section);
        if (!root.value(section).isObject()) {
            errors.append(tr("%1 is not a supported empty object.").arg(section));
        }
        for (const QString& collection : collections) {
            if (object.contains(collection) &&
                (!object.value(collection).isArray() || !object.value(collection).toArray().isEmpty())) {
                errors.append(tr("%1/%2: fence and rally content cannot be converted.").arg(section, collection));
            }
        }
    }
    const QJsonObject plan = root.value(QStringLiteral("mission")).toObject();
    checkKeys(plan,
              {QStringLiteral("version"), QStringLiteral("items"), QStringLiteral("plannedHomePosition"),
               QStringLiteral("firmwareType"), QStringLiteral("vehicleType"), QStringLiteral("cruiseSpeed"),
               QStringLiteral("hoverSpeed"), QStringLiteral("globalPlanAltitudeMode")},
              tr("Mission"));
    if (plan.value(QStringLiteral("version")).toDouble() != 2 || !plan.value(QStringLiteral("items")).isArray() ||
        plan.value(QStringLiteral("items")).toArray().size() > 1000) {
        errors.append(tr("The plan must contain a version-2 mission with at most 1000 simple waypoints."));
    }
    if (plan.contains(QStringLiteral("globalPlanAltitudeMode")) &&
        plan.value(QStringLiteral("globalPlanAltitudeMode")).toDouble(-1) != 0 &&
        plan.value(QStringLiteral("globalPlanAltitudeMode")).toDouble(-1) != 1) {
        errors.append(tr("The plan uses an unsupported global altitude mode."));
    }
    QJsonArray waypoints;
    bool needsRadius = false;
    int index = 0;
    for (const QJsonValue value : plan.value(QStringLiteral("items")).toArray()) {
        ++index;
        const QString context = tr("Item %1").arg(index);
        const QJsonObject item = value.toObject();
        checkKeys(item,
                  {QStringLiteral("type"), QStringLiteral("command"), QStringLiteral("frame"), QStringLiteral("params"),
                   QStringLiteral("autoContinue"), QStringLiteral("doJumpId"), QStringLiteral("Altitude"),
                   QStringLiteral("AltitudeMode"), QStringLiteral("AMSLAltAboveTerrain"), QStringLiteral("coordinate")},
                  context);
        if (!value.isObject() || item.value(QStringLiteral("type")).toString() != QStringLiteral("SimpleItem") ||
            item.value(QStringLiteral("command")).toDouble() != 16 ||
            (item.value(QStringLiteral("frame")).toDouble() != 3 &&
             item.value(QStringLiteral("frame")).toDouble() != 6) ||
            !item.value(QStringLiteral("autoContinue")).isBool() ||
            !item.value(QStringLiteral("autoContinue")).toBool()) {
            errors.append(
                tr("%1: only automatic waypoints in a HOME-relative frame (3 or 6) are supported.").arg(context));
            continue;
        }
        const QJsonArray params = item.value(QStringLiteral("params")).toArray();
        const QJsonArray coordinate = item.value(QStringLiteral("coordinate")).toArray();
        const bool legacyCoordinate = item.contains(QStringLiteral("coordinate"));
        if ((!legacyCoordinate && params.size() != 7) ||
            (legacyCoordinate && (params.size() != 4 || coordinate.size() != 3))) {
            errors.append(tr("%1: invalid waypoint parameters.").arg(context));
            continue;
        }
        if (!params[0].isDouble() || params[0].toDouble() != 0 || !params[2].isDouble() || params[2].toDouble() != 0 ||
            !params[3].isNull()) {
            errors.append(tr("%1: holds, pass radii and explicit yaw cannot be converted.").arg(context));
            continue;
        }
        const QJsonArray location = legacyCoordinate ? coordinate : QJsonArray{params[4], params[5], params[6]};
        bool numeric = params[1].isDouble();
        for (const QJsonValue number : location) {
            numeric = numeric && number.isDouble() && std::isfinite(number.toDouble());
        }
        if (!numeric || params[1].toDouble() < 0) {
            errors.append(tr("%1: coordinates, altitude and radius must be finite numbers.").arg(context));
            continue;
        }
        MissionItem loaded;
        QString loadError;
        if (!loaded.load(item, index, loadError)) {
            errors.append(tr("%1: %2").arg(context, loadError));
            continue;
        }
        if ((item.contains(QStringLiteral("AltitudeMode")) || item.contains(QStringLiteral("Altitude"))) &&
            (item.value(QStringLiteral("AltitudeMode")).toDouble(-1) != 1 ||
             !item.value(QStringLiteral("Altitude")).isDouble() ||
             std::abs(item.value(QStringLiteral("Altitude")).toDouble() - loaded.param7()) > 0.000001)) {
            errors.append(tr("%1: conflicting or non-relative altitude metadata.").arg(context));
        }
        if (item.contains(QStringLiteral("AMSLAltAboveTerrain")) &&
            !item.value(QStringLiteral("AMSLAltAboveTerrain")).isNull()) {
            errors.append(tr("%1: terrain altitude cannot be converted.").arg(context));
        }
        double radius = loaded.param2();
        if (radius == 0) {
            needsRadius = true;
            radius = defaultRadius;
        }
        waypoints.append(QJsonObject{{QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                     {QStringLiteral("latitude_deg"), loaded.param5()},
                                     {QStringLiteral("longitude_deg"), loaded.param6()},
                                     {QStringLiteral("altitude_m"), loaded.param7()},
                                     {QStringLiteral("acceptance_radius_m"), radius}});
    }
    QJsonObject mission{{QStringLiteral("schema_version"), 1},
                        {QStringLiteral("name"), QFileInfo(path).completeBaseName()},
                        {QStringLiteral("altitude_reference"), QStringLiteral("relative_home")},
                        {QStringLiteral("waypoints"), waypoints},
                        {QStringLiteral("rules"), QJsonArray{}},
                        {QStringLiteral("areas"), QJsonArray{}}};
    if (!std::isfinite(defaultRadius) || defaultRadius <= 0) {
        errors.append(tr("Choose a positive acceptance radius for aircraft-default waypoints."));
    }
    const QString validation = DCLinkMissionJson::validateMission(mission);
    if (!validation.isEmpty()) {
        errors.append(validation);
    }
    if (errors.isEmpty()) {
        _importPreview = mission.toVariantMap();
    }
    return {{QStringLiteral("errors"), errors.mid(0, 50)},
            {QStringLiteral("waypoint_count"), waypoints.size()},
            {QStringLiteral("needs_radius"), needsRadius},
            {QStringLiteral("name"), mission.value(QStringLiteral("name")).toString()}};
}

void DCLinkMissionEditor::acceptPlanImport()
{
    if (!busy() && !reviewRequired() && !_importPreview.isEmpty()) {
        _createDraft(_importPreview);
        _importPreview.clear();
        emit routeLoaded();
    }
}
