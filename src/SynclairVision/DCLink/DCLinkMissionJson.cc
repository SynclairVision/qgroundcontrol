#include "DCLinkMissionJson.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QPointF>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>
#include <QtCore/QUuid>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace DCLinkMissionJson {
namespace {
bool hasKeys(const QJsonObject& object, std::initializer_list<QString> keys)
{
    if (object.size() != static_cast<qsizetype>(keys.size())) {
        return false;
    }
    for (const auto& key : keys) {
        if (!object.contains(key)) {
            return false;
        }
    }
    return true;
}

bool validId(const QString& id)
{
    const QUuid uuid(id);
    return uuid.version() == QUuid::Random && uuid.variant() == QUuid::DCE &&
           uuid.toString(QUuid::WithoutBraces) == id;
}

bool validInteger(const QJsonValue& field, int minimum, int maximum)
{
    const double value = field.toDouble();
    return field.isDouble() && value >= minimum && value <= maximum && std::floor(value) == value;
}

bool validText(const QString& message, int limit)
{
    if (message.isEmpty() || !message.isValidUtf16() || message.toUcs4().size() > limit ||
        message != message.trimmed() || message.front() == QChar(0xfeff) || message.back() == QChar(0xfeff)) {
        return false;
    }
    for (const QChar character : message) {
        if (character.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

bool supportsRule(const QJsonObject& rule)
{
    const QString conditionKey = QStringLiteral("condition");
    const auto required = {QStringLiteral("id"), QStringLiteral("trigger"), QStringLiteral("actions")};
    QJsonObject base = rule;
    const QJsonValue conditionValue = base.take(conditionKey);
    base.remove(QStringLiteral("area_id"));
    if (!hasKeys(base, required) || !rule.value(QStringLiteral("actions")).isArray()) {
        return false;
    }
    if (!conditionValue.isUndefined()) {
        const QJsonObject condition = conditionValue.toObject();
        if (condition.value(QStringLiteral("type")) != QStringLiteral("detection_present") ||
            (!hasKeys(condition, {QStringLiteral("type")}) &&
             !hasKeys(condition, {QStringLiteral("type"), QStringLiteral("class_id")}))) {
            return false;
        }
    }
    const QJsonObject trigger = rule.value(QStringLiteral("trigger")).toObject();
    const QString event = trigger.value(QStringLiteral("type")).toString();
    if (event == QStringLiteral("waypoint_reached")) {
        if (!hasKeys(trigger, {QStringLiteral("type"), QStringLiteral("waypoint_id")})) {
            return false;
        }
    } else if (event == QStringLiteral("leg_started")) {
        if (!hasKeys(trigger, {QStringLiteral("type"), QStringLiteral("from_waypoint_id"),
                               QStringLiteral("to_waypoint_id")})) {
            return false;
        }
    } else if (event == QStringLiteral("leg_progress")) {
        if (!hasKeys(trigger, {QStringLiteral("type"), QStringLiteral("from_waypoint_id"),
                               QStringLiteral("to_waypoint_id"), QStringLiteral("position")})) {
            return false;
        }
        const QJsonObject position = trigger.value(QStringLiteral("position")).toObject();
        const QJsonValue type = position.value(QStringLiteral("type"));
        if (type == QStringLiteral("anywhere")) {
            if (!hasKeys(position, {QStringLiteral("type")})) {
                return false;
            }
        } else if (type != QStringLiteral("percent") ||
                   !hasKeys(position, {QStringLiteral("type"), QStringLiteral("percent")})) {
            return false;
        }
    } else {
        return false;
    }
    for (const auto& value : rule.value(QStringLiteral("actions")).toArray()) {
        const QJsonObject action = value.toObject();
        const QString type = action.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("notify")) {
            if (!hasKeys(action, {QStringLiteral("type"), QStringLiteral("text")})) {
                return false;
            }
        } else if (type == QStringLiteral("wait")) {
            if (!hasKeys(action, {QStringLiteral("type"), QStringLiteral("duration_s")})) {
                return false;
            }
        } else if (type == QStringLiteral("command")) {
            if (!hasKeys(action, {QStringLiteral("type"), QStringLiteral("command_id")})) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

bool supportsArea(const QJsonObject& area)
{
    QJsonObject base = area;
    base.remove(QStringLiteral("advisory"));
    return hasKeys(base, {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("vertices")}) &&
           area.value(QStringLiteral("vertices")).isArray();
}

double cross(const QPointF& a, const QPointF& b)
{
    return a.x() * b.y() - a.y() * b.x();
}

bool withinSegmentBounds(const QPointF& p, const QPointF& a, const QPointF& b)
{
    return std::min(a.x(), b.x()) <= p.x() && p.x() <= std::max(a.x(), b.x()) &&
           std::min(a.y(), b.y()) <= p.y() && p.y() <= std::max(a.y(), b.y());
}

bool edgesIntersect(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d)
{
    const double abc = cross(b - a, c - a);
    const double abd = cross(b - a, d - a);
    const double cda = cross(d - c, a - c);
    const double cdb = cross(d - c, b - c);
    const bool crossesFirst = (abc < 0 && abd > 0) || (abc > 0 && abd < 0);
    const bool crossesSecond = (cda < 0 && cdb > 0) || (cda > 0 && cdb < 0);
    return (crossesFirst && crossesSecond) ||
           (abc == 0 && withinSegmentBounds(c, a, b)) || (abd == 0 && withinSegmentBounds(d, a, b)) ||
           (cda == 0 && withinSegmentBounds(a, c, d)) || (cdb == 0 && withinSegmentBounds(b, c, d));
}
}  // namespace

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
        mission.value(QStringLiteral("schema_version")).toDouble() != SCHEMA_VERSION ||
        mission.value(QStringLiteral("altitude_reference")).toString() != QStringLiteral("relative_home") ||
        !mission.value(QStringLiteral("rules")).isArray() || !mission.value(QStringLiteral("areas")).isArray()) {
        return QObject::tr("This mission contains an unsupported version, altitude reference, or feature. Read only.");
    }
    for (const auto& rule : mission.value(QStringLiteral("rules")).toArray()) {
        if (!supportsRule(rule.toObject())) {
            return QObject::tr("This mission contains an unsupported rule. Read only.");
        }
    }
    for (const auto& area : mission.value(QStringLiteral("areas")).toArray()) {
        if (!supportsArea(area.toObject())) {
            return QObject::tr("This mission contains an unsupported area. Read only.");
        }
    }
    return {};
}

QString validateRule(const QJsonObject& rule, const QJsonArray& waypoints, const QSet<QString>& areaIds)
{
    if (!supportsRule(rule) || !validId(rule.value(QStringLiteral("id")).toString())) {
        return QObject::tr("Choose a supported trigger and steps for this rule.");
    }
    const QJsonObject trigger = rule.value(QStringLiteral("trigger")).toObject();
    const bool arrival = trigger.value(QStringLiteral("type")) == QStringLiteral("waypoint_reached");
    const QString first = trigger.value(arrival ? QStringLiteral("waypoint_id")
                                                : QStringLiteral("from_waypoint_id")).toString();
    const QString last = trigger.value(QStringLiteral("to_waypoint_id")).toString();
    bool found = false;
    const bool validReference = validId(first) && (arrival || validId(last));
    for (qsizetype i = 0; validReference && i < waypoints.size(); ++i) {
        if (waypoints.at(i).toObject().value(QStringLiteral("id")).toString() == first) {
            found = arrival || (i + 1 < waypoints.size() &&
                                waypoints.at(i + 1).toObject().value(QStringLiteral("id")).toString() == last);
            break;
        }
    }
    if (!found) {
        return arrival ? QObject::tr("Choose a waypoint on this route.")
                       : QObject::tr("Choose two consecutive waypoints in route order.");
    }
    const QJsonValue areaId = rule.value(QStringLiteral("area_id"));
    if (!areaId.isUndefined() && (!validId(areaId.toString()) || !areaIds.contains(areaId.toString()))) {
        return QObject::tr("Choose an area in this mission or allow the rule anywhere.");
    }
    const QJsonObject position = trigger.value(QStringLiteral("position")).toObject();
    if (position.value(QStringLiteral("type")) == QStringLiteral("percent") &&
        !validInteger(position.value(QStringLiteral("percent")), 0, 100)) {
        return QObject::tr("Enter a whole percentage from 0 to 100.");
    }
    const QJsonValue classId = rule.value(QStringLiteral("condition")).toObject().value(QStringLiteral("class_id"));
    if (!classId.isUndefined() && !validInteger(classId, 0, 32767)) {
        return QObject::tr("Enter a camera class ID from 0 to 32767 or leave it blank for any class.");
    }
    const QJsonArray actions = rule.value(QStringLiteral("actions")).toArray();
    if (actions.isEmpty() || actions.size() > MAX_RULE_ACTIONS) {
        return QObject::tr("Add 1–%1 steps to this rule.").arg(MAX_RULE_ACTIONS);
    }
    for (const auto& value : actions) {
        const QJsonObject action = value.toObject();
        const QJsonValue type = action.value(QStringLiteral("type"));
        if (type == QStringLiteral("notify")) {
            if (!validText(action.value(QStringLiteral("text")).toString(), 100)) {
                return QObject::tr(
                    "Use 1–100 characters in each message, without surrounding spaces or line breaks.");
            }
        } else if (type == QStringLiteral("command")) {
            if (!validCommandId(action.value(QStringLiteral("command_id")).toString())) {
                return QObject::tr("Choose a valid mission command.");
            }
        } else if (!arrival || !validInteger(action.value(QStringLiteral("duration_s")), 1, 60)) {
            return QObject::tr("Wait steps need 1–60 whole seconds and can only be used at waypoint arrivals.");
        }
    }
    return {};
}

QString validateArea(const QJsonObject& area)
{
    if (!supportsArea(area) || !validId(area.value(QStringLiteral("id")).toString())) {
        return QObject::tr("This area has unsupported fields or an invalid identity.");
    }
    if (!validText(area.value(QStringLiteral("name")).toString(), 80)) {
        return QObject::tr("Use an area name with 1–80 characters and no surrounding whitespace.");
    }
    const QJsonValue advisory = area.value(QStringLiteral("advisory"));
    if (!advisory.isUndefined() && !validText(advisory.toString(), 100)) {
        return QObject::tr("Use an advisory with 1–100 characters and no surrounding whitespace or line breaks.");
    }
    const QJsonArray vertices = area.value(QStringLiteral("vertices")).toArray();
    if (vertices.size() < 3 || vertices.size() > MAX_AREA_VERTICES) {
        return QObject::tr("Draw between 3 and %1 corners.").arg(MAX_AREA_VERTICES);
    }
    QList<QPointF> points;
    double minLon = 180.0;
    double maxLon = -180.0;
    for (const QJsonValue& vertex : vertices) {
        const QJsonObject point = vertex.toObject();
        const double latitude = point.value(QStringLiteral("latitude_deg")).toDouble(NAN);
        const double longitude = point.value(QStringLiteral("longitude_deg")).toDouble(NAN);
        if (!hasKeys(point, {QStringLiteral("latitude_deg"), QStringLiteral("longitude_deg")}) ||
            !std::isfinite(latitude) || !std::isfinite(longitude) || std::abs(latitude) >= 90 ||
            std::abs(longitude) > 180) {
            return QObject::tr("Choose valid area corners away from the poles.");
        }
        if (std::ranges::any_of(points, [latitude, longitude](const QPointF& p) {
                return p.x() == longitude && p.y() == latitude;
            })) {
            return QObject::tr("Each area corner needs a different position.");
        }
        points.append({longitude, latitude});
        minLon = std::min(minLon, longitude);
        maxLon = std::max(maxLon, longitude);
    }
    if (maxLon - minLon >= 180.0) {
        return QObject::tr("Draw an area that spans less than 180° longitude on one side of the date line.");
    }
    double areaTwice = 0.0;
    const qsizetype size = points.size();
    for (qsizetype i = 0; i < size; ++i) {
        const qsizetype next = (i + 1) % size;
        const QPointF a = points[i];
        const QPointF b = points[next];
        const QPointF edgeIn = a - b;
        const QPointF edgeOut = points[(next + 1) % size] - b;
        if (cross(edgeIn, edgeOut) == 0 && QPointF::dotProduct(edgeIn, edgeOut) > 0) {
            return QObject::tr("Draw the corners in order around the area so its edges do not overlap.");
        }
        for (qsizetype j = i + 2; j < size; ++j) {
            const qsizetype after = (j + 1) % size;
            if (after != i && edgesIntersect(a, b, points[j], points[after])) {
                return QObject::tr("Draw the corners in order around the area so its edges do not cross.");
            }
        }
        areaTwice += cross(a - points.front(), b - points.front());
    }
    return areaTwice == 0.0 ? QObject::tr("Draw an area with room inside its outline.") : QString();
}

QSet<QString> areaIds(const QJsonArray& areas)
{
    QSet<QString> ids;
    for (const auto& area : areas) {
        ids.insert(area.toObject().value(QStringLiteral("id")).toString());
    }
    return ids;
}

bool validCommandId(const QString& id)
{
    static const QRegularExpression pattern(QStringLiteral(R"(\A[a-z][a-z0-9_]*(/[a-z][a-z0-9_]*)*\z)"));
    return id.size() <= 64 && pattern.match(id).hasMatch();
}

QVariantList compatibleCommands(const QJsonObject& capabilities)
{
    const QJsonArray entries = capabilities.value(QStringLiteral("mission_commands")).toArray();
    const QJsonArray features = capabilities.value(QStringLiteral("features")).toArray();
    if (!features.contains(QJsonValue(QStringLiteral("mission_command_steps")))) {
        return {};
    }
    QVariantList choices;
    QSet<QString> ids;
    for (const QJsonValue& entry : entries) {
        const QJsonObject command = entry.toObject();
        const QString id = command.value(QStringLiteral("id")).toString();
        const QString label = command.value(QStringLiteral("label")).toString();
        if (!validCommandId(id) || ids.contains(id) || !validText(label, 80)) {
            return {};
        }
        ids.insert(id);
        choices.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("label"), label}});
    }
    return choices;
}

bool validRuleNotification(const QJsonValue& value)
{
    const QJsonObject event = value.toObject();
    return event.value(QStringLiteral("type")) == QStringLiteral("notify") &&
           validId(event.value(QStringLiteral("rule_id")).toString()) &&
           validInteger(event.value(QStringLiteral("action_index")), 1, MAX_RULE_ACTIONS) &&
           validText(event.value(QStringLiteral("text")).toString(), 100);
}

bool validActiveArea(const QJsonObject& area)
{
    const QJsonValue advisory = area.value(QStringLiteral("advisory"));
    return validId(area.value(QStringLiteral("area_id")).toString()) &&
           validText(area.value(QStringLiteral("name")).toString(), 80) &&
           (advisory.isUndefined() || validText(advisory.toString(), 100));
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
    const QJsonArray areas = mission.value(QStringLiteral("areas")).toArray();
    if (areas.size() > MAX_AREAS) {
        return QObject::tr("A mission can contain at most %1 areas.").arg(MAX_AREAS);
    }
    QSet<QString> checkedAreaIds;
    for (qsizetype i = 0; i < areas.size(); ++i) {
        const QJsonObject area = areas.at(i).toObject();
        const QString error = validateArea(area);
        if (!error.isEmpty()) {
            return QObject::tr("Area %1. %2").arg(i + 1).arg(error);
        }
        const QString id = area.value(QStringLiteral("id")).toString();
        if (checkedAreaIds.contains(id)) {
            return QObject::tr("Area %1 has a repeated identity.").arg(i + 1);
        }
        checkedAreaIds.insert(id);
    }
    const QJsonArray rules = mission.value(QStringLiteral("rules")).toArray();
    if (rules.size() > MAX_RULES) {
        return QObject::tr("A mission can contain at most %1 rules.").arg(MAX_RULES);
    }
    QSet<QString> ruleIds;
    int totalWait = 0;
    for (qsizetype i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        const QString ruleError = validateRule(rule, route.toArray(), checkedAreaIds);
        if (!ruleError.isEmpty()) {
            return QObject::tr("Rule %1. %2").arg(i + 1).arg(ruleError);
        }
        const QString id = rule.value(QStringLiteral("id")).toString();
        if (ruleIds.contains(id)) {
            return QObject::tr("Rule %1 has a repeated identity.").arg(i + 1);
        }
        ruleIds.insert(id);
        for (const auto& action : rule.value(QStringLiteral("actions")).toArray()) {
            totalWait += action.toObject().value(QStringLiteral("duration_s")).toInt();
        }
    }
    if (totalWait > 300) {
        return QObject::tr("All Wait steps together may total at most 300 seconds.");
    }
    if (QJsonDocument(mission).toJson(QJsonDocument::Compact).size() > 262144) {
        return QObject::tr("The mission exceeds the DCLink document size limit.");
    }
    return {};
}
}  // namespace DCLinkMissionJson
