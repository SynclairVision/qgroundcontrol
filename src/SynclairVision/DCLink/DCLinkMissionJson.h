#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>
#include <QtCore/QString>

namespace DCLinkMissionJson {
inline constexpr int SCHEMA_VERSION = 2;
inline constexpr int MAX_RULES = 16;
inline constexpr int MAX_RULE_ACTIONS = 4;
inline constexpr int MAX_AREAS = 8;
inline constexpr int MAX_AREA_VERTICES = 32;
bool uniqueObjectKeys(const QByteArray& json);
bool parseObject(const QByteArray& json, QJsonObject& object, QString& error);
QString validateMission(const QJsonObject& mission);
QString unsupportedMission(const QJsonObject& mission);
QString validateRule(const QJsonObject& rule, const QJsonArray& waypoints, const QSet<QString>& areaIds);
QString validateArea(const QJsonObject& area);
QSet<QString> areaIds(const QJsonArray& areas);
bool validRuleNotification(const QJsonValue& value);
bool validActiveArea(const QJsonObject& area);
}  // namespace DCLinkMissionJson
