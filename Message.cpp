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

Message::Message()
{
}

Message::Message(QString const &type, QString const &value):
    type_{ type },
    value_{ value }
{
}

Message::Message(QString const &type, QString const &value,  QMap<QString, QVariant> const &params):
    type_{ type },
    value_{ value },
    params_{ params }
{
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
