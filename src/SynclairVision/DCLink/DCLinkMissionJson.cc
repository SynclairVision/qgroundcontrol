#include "DCLinkMissionJson.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QStringDecoder>
#include <QtCore/QUuid>

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

bool validNoticeText(const QString& message)
{
    if (message.isEmpty() || !message.isValidUtf16() || message.toUcs4().size() > 100 ||
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
        } else {
            return false;
        }
    }
    return true;
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
        !mission.value(QStringLiteral("rules")).isArray() || !mission.value(QStringLiteral("areas")).isArray() ||
        !mission.value(QStringLiteral("areas")).toArray().isEmpty()) {
        return QObject::tr("This mission contains an unsupported version, altitude reference, or feature. Read only.");
    }
    for (const auto& rule : mission.value(QStringLiteral("rules")).toArray()) {
        if (!supportsRule(rule.toObject())) {
            return QObject::tr("This mission contains an unsupported rule. Read only.");
        }
    }
    return {};
}

QString validateRule(const QJsonObject& rule, const QJsonArray& waypoints)
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
        if (action.value(QStringLiteral("type")) == QStringLiteral("notify")) {
            if (!validNoticeText(action.value(QStringLiteral("text")).toString())) {
                return QObject::tr(
                    "Use 1–100 characters in each message, without surrounding spaces or line breaks.");
            }
        } else if (!arrival || !validInteger(action.value(QStringLiteral("duration_s")), 1, 60)) {
            return QObject::tr("Wait steps need 1–60 whole seconds and can only be used at waypoint arrivals.");
        }
    }
    return {};
}

bool validRuleNotification(const QJsonValue& value)
{
    const QJsonObject event = value.toObject();
    return event.value(QStringLiteral("type")) == QStringLiteral("notify") &&
           validId(event.value(QStringLiteral("rule_id")).toString()) &&
           validInteger(event.value(QStringLiteral("action_index")), 1, MAX_RULE_ACTIONS) &&
           validNoticeText(event.value(QStringLiteral("text")).toString());
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
    const QJsonArray rules = mission.value(QStringLiteral("rules")).toArray();
    if (rules.size() > MAX_RULES) {
        return QObject::tr("A mission can contain at most %1 rules.").arg(MAX_RULES);
    }
    QSet<QString> ruleIds;
    int totalWait = 0;
    for (qsizetype i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        const QString ruleError = validateRule(rule, route.toArray());
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
