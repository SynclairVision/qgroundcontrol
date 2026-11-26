#include "DCLinkMissionJson.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>
#include <QtCore/QUuid>

#include <cmath>

namespace DCLinkMissionJson {
bool uniqueObjectKeys(const QByteArray& json)
{
    // QJsonDocument validates grammar but retains only the last duplicate key.
    struct Scope
    {
        bool object;
        bool keyExpected;
        QSet<QString> keys;
    };

    QList<Scope> scopes;
    for (qsizetype index = 0; index < json.size(); ++index) {
        const char token = json[index];
        if (token == '{' || token == '[') {
            scopes.append({token == '{', token == '{', {}});
        } else if (token == '}' || token == ']') {
            if (scopes.isEmpty()) {
                return false;
            }
            scopes.removeLast();
        } else if (token == ',' && !scopes.isEmpty()) {
            scopes.last().keyExpected = scopes.last().object;
        } else if (token == '"') {
            const qsizetype start = index;
            while (++index < json.size()) {
                if (json[index] == '\\') {
                    ++index;
                } else if (json[index] == '"') {
                    break;
                }
            }
            if (!scopes.isEmpty() && scopes.last().keyExpected) {
                const QByteArray literal = '[' + json.mid(start, index - start + 1) + ']';
                const QJsonDocument keyDocument = QJsonDocument::fromJson(literal);
                if (!keyDocument.isArray() || keyDocument.array().size() != 1) {
                    return false;
                }
                const QString key = keyDocument.array().first().toString();
                if (scopes.last().keys.contains(key)) {
                    return false;
                }
                scopes.last().keys.insert(key);
                scopes.last().keyExpected = false;
            }
        }
    }
    return scopes.isEmpty();
}

bool parseObject(const QByteArray& json, QJsonObject& object, QString& error)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded = decoder(json);
    Q_UNUSED(decoded);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (decoder.hasError() || !document.isObject() || !uniqueObjectKeys(json)) {
        error = QObject::tr("The file must contain a UTF-8 JSON object without duplicate keys.");
        return false;
    }
    object = document.object();
    return true;
}

QString unsupportedMission(const QJsonObject& mission)
{
    const QSet<QString> required{
        QStringLiteral("schema_version"), QStringLiteral("name"),  QStringLiteral("altitude_reference"),
        QStringLiteral("waypoints"),      QStringLiteral("rules"), QStringLiteral("areas")};
    const QStringList keys = mission.keys();
    if (QSet<QString>(keys.begin(), keys.end()) != required ||
        !mission.value(QStringLiteral("schema_version")).isDouble() ||
        mission.value(QStringLiteral("schema_version")).toDouble() != 1 ||
        mission.value(QStringLiteral("altitude_reference")).toString() != QStringLiteral("relative_home") ||
        !mission.value(QStringLiteral("rules")).isArray() || !mission.value(QStringLiteral("areas")).isArray() ||
        !mission.value(QStringLiteral("rules")).toArray().isEmpty() ||
        !mission.value(QStringLiteral("areas")).toArray().isEmpty()) {
        return QObject::tr("This mission contains an unsupported version, altitude reference, or feature. Read only.");
    }
    return {};
}

QString validateMission(const QJsonObject& mission)
{
    const QString unsupported = unsupportedMission(mission);
    if (!unsupported.isEmpty()) {
        return unsupported;
    }
    const QString name = mission.value(QStringLiteral("name")).toString();
    if (name.isEmpty() || name != name.trimmed() || name.toUcs4().size() > 80 || name.front() == QChar(0xfeff) ||
        name.back() == QChar(0xfeff)) {
        return QObject::tr("Use a mission name with 1–80 characters and no surrounding whitespace.");
    }
    if (!name.isValidUtf16()) {
        return QObject::tr("Mission names must contain valid Unicode characters.");
    }
    for (const QChar character : name) {
        if (character.category() == QChar::Other_Control) {
            return QObject::tr("Mission names cannot contain control characters.");
        }
    }
    const QJsonValue route = mission.value(QStringLiteral("waypoints"));
    if (!route.isArray() || route.toArray().size() > 1000) {
        return QObject::tr("A route can contain at most 1000 waypoints.");
    }
    const QSet<QString> waypointKeys{QStringLiteral("id"), QStringLiteral("latitude_deg"),
                                     QStringLiteral("longitude_deg"), QStringLiteral("altitude_m"),
                                     QStringLiteral("acceptance_radius_m")};
    QSet<QString> ids;
    int index = 0;
    for (const QJsonValue point : route.toArray()) {
        ++index;
        const QJsonObject waypoint = point.toObject();
        const QStringList fields = waypoint.keys();
        const QString id = waypoint.value(QStringLiteral("id")).toString();
        const QUuid uuid(id);
        if (!point.isObject() || QSet<QString>(fields.begin(), fields.end()) != waypointKeys ||
            uuid.version() != QUuid::Random || uuid.variant() != QUuid::DCE ||
            uuid.toString(QUuid::WithoutBraces) != id || ids.contains(id)) {
            return QObject::tr("Waypoint %1 has unsupported fields or an invalid or repeated identity.").arg(index);
        }
        ids.insert(id);
        for (const QString& field : {QStringLiteral("latitude_deg"), QStringLiteral("longitude_deg"),
                                     QStringLiteral("altitude_m"), QStringLiteral("acceptance_radius_m")}) {
            const QJsonValue value = waypoint.value(field);
            if (!value.isDouble() || !std::isfinite(value.toDouble())) {
                return QObject::tr("Waypoint %1 requires finite numeric values.").arg(index);
            }
        }
        if (std::abs(waypoint.value(QStringLiteral("latitude_deg")).toDouble()) > 90 ||
            std::abs(waypoint.value(QStringLiteral("longitude_deg")).toDouble()) > 180 ||
            waypoint.value(QStringLiteral("acceptance_radius_m")).toDouble() <= 0) {
            return QObject::tr("Waypoint %1 requires valid coordinates and a positive acceptance radius.").arg(index);
        }
    }
    if (QJsonDocument(mission).toJson(QJsonDocument::Compact).size() > 262144) {
        return QObject::tr("The mission exceeds the DCLink document size limit.");
    }
    return {};
}
}  // namespace DCLinkMissionJson
