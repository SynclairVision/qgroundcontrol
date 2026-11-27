#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace DCLinkMissionJson {
inline constexpr int SCHEMA_VERSION = 2;
inline constexpr int MAX_RULES = 16;
inline constexpr int MAX_RULE_ACTIONS = 4;
bool uniqueObjectKeys(const QByteArray& json);
bool parseObject(const QByteArray& json, QJsonObject& object, QString& error);
QString validateMission(const QJsonObject& mission);
QString unsupportedMission(const QJsonObject& mission);
QString validateRule(const QJsonObject& rule, const QJsonArray& waypoints);
bool validRuleNotification(const QJsonValue& value);
}  // namespace DCLinkMissionJson
