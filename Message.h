#ifndef MESSAGE_H
#define MESSAGE_H

/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include <QMap>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariant>


class Message {
public:
    Message();
    Message(QString const &type, QString const &value="");
    Message(QString const &type, QString const &value, QMap<QString, QVariant> const &params);

    void read(const QJsonObject &json);
    void write(QJsonObject &json) const;

    QByteArray toJson() const;
    QVariantMap toVariantMap() const;

    QString type() const { return type_; }
    void setType(QString type){ type_ = type; }

    QString value() const { return value_; }
    void setValue(QString value){ value_ = value; }
    qint64 id() const { return params_.value("_ID").toLongLong(); }
    qint64 ensureId();
    QMap<QString, QVariant> params() const { return params_; }

private:
    QString type_;
    QString value_;
    QMap<QString, QVariant> params_;
};


#endif // MESSAGE_H
