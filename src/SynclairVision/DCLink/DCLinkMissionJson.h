#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

namespace DCLinkMissionJson {
bool uniqueObjectKeys(const QByteArray& json);
bool parseObject(const QByteArray& json, QJsonObject& object, QString& error);
QString validateMission(const QJsonObject& mission);
QString unsupportedMission(const QJsonObject& mission);
}  // namespace DCLinkMissionJson
