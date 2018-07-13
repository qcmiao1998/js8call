#ifndef VARICODE_H
#define VARICODE_H

/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include <QBitArray>
#include <QRegularExpression>
#include <QRegExp>
#include <QString>
#include <QVector>


class Varicode
{
public:
    //Varicode();

    static QStringList parseCallsigns(QString const &input);
    static QStringList parseGrids(QString const &input);

    static QList<QVector<bool>> huffEncode(QString const& text);
    static QVector<bool> huffFlatten(QList<QVector<bool>> &list);
    static QString huffDecode(QVector<bool> const& bitvec);

    static QVector<bool> strToBits(QString const& bitvec);
    static QString bitsToStr(QVector<bool> const& bitvec);

    static QVector<bool> intToBits(quint64 value, int expected=0);
    static quint64 bitsToInt(QVector<bool> const value);

    static quint8 unpack5bits(QString const& value);
    static QString pack5bits(quint8 packed);

    static quint16 unpack16bits(QString const& value);
    static QString pack16bits(quint16 packed);

    static quint32 unpack32bits(QString const& value);
    static QString pack32bits(quint32 packed);

    static quint64 unpack64bits(QString const& value);
    static QString pack64bits(quint64 packed);

    static quint32 packCallsign(QString const& value);
    static QString unpackCallsign(quint32 value);

};

#endif // VARICODE_H
