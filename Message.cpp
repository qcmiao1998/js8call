/**
 * This file is part of JS8Call.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 *
 **/

#include "Message.h"
#include "DriftingDateTime.h"

const qint64 EPOCH = 1499299200000; // July 6, 2017

#if USE_SNOWFLAKE
quint64 snowflake(quint64 epoch, quint16 machine, quint16 sequence){
    quint64 value = (DriftingDateTime::currentMSecsSinceEpoch() - epoch) << 22;
    value |= machine & 0x3FF << 12;
    value |= sequence & 0xFFF;
    return value;
}
#endif

Message::Message()
{
}

Message::Message(QString const &type, QString const &value):
    type_{ type },
    value_{ value }
{
    params_["_ID"] = QString::number(DriftingDateTime::currentMSecsSinceEpoch()-EPOCH);
}

Message::Message(QString const &type, QString const &value,  QMap<QString, QVariant> const &params):
    type_{ type },
    value_{ value },
    params_{ params }
{
    if(params_.value("_ID", 0).toLongLong() == 0){
        params_["_ID"] = QString::number(DriftingDateTime::currentMSecsSinceEpoch()-EPOCH);
    }
}

qint64 Message::ensureId(){
    // if a non-zero id exists, we're good
    if(params_.contains("_ID")){
        auto id = params_.value("_ID", 0).toLongLong();
        if(id != 0){
            return id;
        }
    }

    // otherwise, generate one
    auto id = DriftingDateTime::currentMSecsSinceEpoch()-EPOCH;
    params_["_ID"] = QString::number(id);
    return id;
}

void Message::read(const QJsonObject &json){
    if(json.contains("type") && json["type"].isString()){
        type_ = json["type"].toString();
    }

    if(json.contains("value") && json["value"].isString()){
        value_ = json["value"].toString();
    }

    if(json.contains("params") && json["params"].isObject()){
        params_.clear();

        QJsonObject params = json["params"].toObject();
        foreach(auto key, params.keys()){
            params_[key] = params[key].toVariant();
        }
    }
}

void Message::write(QJsonObject &json) const{
    json["type"] = type_;
    json["value"] = value_;

    QJsonObject params;
    foreach(auto key, params_.keys()){
        params.insert(key, QJsonValue::fromVariant(params_[key]));
    }
    json["params"] = params;
}

QByteArray Message::toJson() const {
    QJsonObject o;
    write(o);

    QJsonDocument d(o);
    return d.toJson(QJsonDocument::Compact);
}

QVariantMap Message::toVariantMap() const {
    QVariantMap m;
    m["type"] = QVariant(type_);
    m["value"] = QVariant(value_);
    m["params"] = QVariant(params_);
    return m;
}
