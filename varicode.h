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
    // frame type transmitted via itype and decoded by the ft8 decoded
    enum TransmissionType {
        FT8              = 0, // [000]
        FT8Fox           = 1, // [001]
        FT8Call          = 2, // [010]
        FT8CallLast      = 3, // [011] <- used to indicate last frame in transmission
        FT8CallReservedA = 4, // [100]
        FT8CallReservedB = 5, // [101]
        FT8CallReservedC = 6, // [110]
        FT8CallReservedD = 7, // [111]
    };

    enum FrameType {
        FrameBeaconPrefix     = 0, // [000]
        FrameBeaconSuffix     = 1, // [001]
        FrameCompoundPrefix   = 2, // [010]
        FrameCompoundSuffix   = 3, // [011]
        FrameDirectedPositive = 4, // [100]
        FrameDirectedNegative = 5, // [101]
        FrameDataUnpadded     = 6, // [110]
        FrameDataPadded       = 7, // [111]
    };

    //Varicode();

    static QString formatSNR(int snr);
    static QString formatPWR(int dbm);

    static QString checksum16(QString const &input);
    static bool checksum16Valid(QString const &checksum, QString const &input);

    static QString checksum32(QString const &input);
    static bool checksum32Valid(QString const &checksum, QString const &input);

    static QStringList parseCallsigns(QString const &input);
    static QStringList parseGrids(QString const &input);

    static QList<QPair<int, QVector<bool>>> huffEncode(const QMap<QString, QString> &huff, QString const& text);
    static QString huffDecode(const QMap<QString, QString> &huff, QVector<bool> const& bitvec);

    static QString huffUnescape(QString const &input);
    static QString huffEscape(QString const &input);
    static QSet<QString> huffValidChars();
    static bool huffShouldEscape(QString const &input);

    static QVector<bool> bytesToBits(char * bitvec, int n);
    static QVector<bool> strToBits(QString const& bitvec);
    static QString bitsToStr(QVector<bool> const& bitvec);

    static QVector<bool> intToBits(quint64 value, int expected=0);
    static quint64 bitsToInt(QVector<bool> const value);
    static quint64 bitsToInt(QVector<bool>::ConstIterator start, int n);
    static QVector<bool> bitsListToBits(QList<QVector<bool>> &list);

    static quint8 unpack5bits(QString const& value);
    static QString pack5bits(quint8 packed);

    static quint8 unpack6bits(QString const& value);
    static QString pack6bits(quint8 packed);

    static quint16 unpack16bits(QString const& value);
    static QString pack16bits(quint16 packed);

    static quint32 unpack32bits(QString const& value);
    static QString pack32bits(quint32 packed);

    static quint64 unpack64bits(QString const& value);
    static QString pack64bits(quint64 packed);

    static quint32 packCallsignPrefixSuffix(QString const& value);
    static QString unpackCallsignPrefixSuffix(quint32 packed);

    static quint32 packCallsign(QString const& value);
    static QString unpackCallsign(quint32 value);

    static quint16 packGrid(QString const& value);
    static QString unpackGrid(quint16 value);

    static bool isCommandAllowed(const QString &cmd);
    static bool isCommandBuffered(const QString &cmd);

    static QString packBeaconMessage(QString const &text, QString const&callsign, int *n);
    static QStringList unpackBeaconMessage(const QString &text, bool *isBeacon, bool *isAlt);

    static QString packCompoundFrame(const QString &baseCallsign, const QString &fix, bool isPrefix, bool isBeacon, quint16 num);
    static QStringList unpackCompoundFrame(const QString &text, bool *isBeacon, quint16 *pNum);

    static QString packDirectedMessage(QString const& text, QString const& callsign, QString *pTo, QString * pCmd, int *n);
    static QStringList unpackDirectedMessage(QString const& text);

    static QString packDataMessage(QString const& text, QString *out, int *n);
    static QString unpackDataMessage(QString const& text);
};

#endif // VARICODE_H
