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
        FT8Call          = 0, // [000] <- any other frame of the message
        FT8CallFirst     = 1, // [001] <- the first frame of a message
        FT8CallLast      = 2, // [010] <- the last frame of a message
        FT8CallReserved  = 4, // [100] <- a reserved flag for future use...
    };

    enum FrameType {
        FrameUnknown          = 255, // [11111111] <- only used as a sentinel
        FrameBeacon           = 0,   // [000]
        FrameCompound         = 1,   // [001]
        FrameCompoundDirected = 2,   // [010]
        FrameDirected         = 3,   // [011]
        FrameReservedA        = 4,   // [100] <- Reserved for future use, likely an extension of one of these formats.
        FrameDataUnpadded     = 5,   // [101]
        FrameDataPadded       = 6,   // [110]
        FrameReservedB        = 7,   // [111] <- Reserved for future use, likely binary data / other formats.
    };

    static const quint8 FrameTypeMax = 7;

    static QString frameTypeString(quint8 type) {
        const char* FrameTypeStrings[] = {
            "FrameBeacon",
            "FrameCompound",
            "FrameCompoundDirected",
            "FrameDirected",
            "FrameReservedA",
            "FrameDataUnpadded",
            "FrameDataPadded",
            "FrameReservedB"
        };

        if(type > FrameTypeMax){
            return "FrameUnknown";
        }
        return FrameTypeStrings[type];
    }

    //Varicode();

    static QMap<QString, QString> defaultHuffTable();
    static QMap<QString, QString> defaultHuffTableEscaped();
    static QString cqString(int number);
    static bool startsWithCQ(QString text);
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

    static quint64 unpack72bits(QString const& value, quint8 *pRem);
    static QString pack72bits(quint64 value, quint8 rem);

    static quint32 packAlphaNumeric22(QString const& value, bool isFlag);
    static QString unpackAlphaNumeric22(quint32 packed, bool *isFlag);

    static quint32 packCallsign(QString const& value);
    static QString unpackCallsign(quint32 value);

    static QString deg2grid(float dlong, float dlat);
    static QPair<float, float> grid2deg(QString const &grid);
    static quint16 packGrid(QString const& value);
    static QString unpackGrid(quint16 value);

    static quint8 packNum(QString const &num, bool *ok);
    static quint8 packPwr(QString const &pwr, bool *ok);
    static quint8 packCmd(quint8 cmd, quint8 num, bool *pPackedNum);
    static quint8 unpackCmd(quint8 value, quint8 *pNum);

    static bool isCommandAllowed(const QString &cmd);
    static bool isCommandBuffered(const QString &cmd);
    static int isCommandChecksumed(const QString &cmd);

    static QString packBeaconMessage(QString const &text, QString const&callsign, int *n);
    static QStringList unpackBeaconMessage(const QString &text, quint8 *pType, bool *isAlt, quint8 *pBits3);

    static QString packCompoundMessage(QString const &text, int *n);
    static QStringList unpackCompoundMessage(const QString &text, quint8 *pType, quint8 *pBits3);

    static QString packCompoundFrame(const QString &baseCallsign, const QString &fix, bool isPrefix, quint8 type, quint16 num, quint8 bits3);
    static QStringList unpackCompoundFrame(const QString &text, quint8 *pType, quint16 *pNum, quint8 *pBits3);

    static QString packDirectedMessage(QString const& text, QString const& callsign, QString *pTo, QString * pCmd, QString *pNum, int *n);
    static QStringList unpackDirectedMessage(QString const& text, quint8 *pType);

    static QString packDataMessage(QString const& text, int *n);
    static QString unpackDataMessage(QString const& text, quint8 *pType);
};

#endif // VARICODE_H
