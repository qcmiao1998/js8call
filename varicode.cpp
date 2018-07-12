/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include "varicode.h"

const int nalphabet = 41;
QString alphabet = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"};

QVector<bool> Varicode::intToBits(qint64 value, int expected){
    QVector<bool> bits;

    while(value){
        bits.prepend((bool) value & 1);
        value = value >> 1;
    }

    if(expected){
        while(bits.count() < expected){
            bits.prepend((bool) 0);
        }
    }

    return bits;
}

qint64 Varicode::bitsToInt(QVector<bool> const value){
    qint64 v = 0;
    foreach(bool bit, value){
        v = (v << 1) + (int)(bit);
    }
    return v;
}

qint8 Varicode::unpack5bits(QString const& value){
    return alphabet.indexOf(value.at(0));
}

QString Varicode::pack5bits(qint8 packed){
    return alphabet.at(packed % nalphabet);
}

qint16 Varicode::unpack16bits(QString const& value){
    int a = alphabet.indexOf(value.at(0));
    int b = alphabet.indexOf(value.at(1));
    int c = alphabet.indexOf(value.at(2));
    return (nalphabet*nalphabet) * a + nalphabet*b + c;
}

QString Varicode::pack16bits(qint16 packed){
    QString out;
    qint16 tmp = packed / (nalphabet*nalphabet);
    out.append(alphabet.at(tmp));

    tmp = (packed - (tmp * (nalphabet*nalphabet))) / nalphabet;
    out.append(alphabet.at(tmp));

    tmp = packed % nalphabet;
    out.append(alphabet.at(tmp));

    return out;
}

qint32 Varicode::unpack32bits(QString const& value){
    return unpack16bits(value.left(3)) << 16 | unpack16bits(value.right(3));
}

QString Varicode::pack32bits(qint32 packed){
    qint16 a = (packed & 0xFFFF0000) >> 16;
    qint16 b = packed & 0xFFFF;
    return pack16bits(a) + pack16bits(b);
}

qint64 Varicode::unpack64bits(QString const& value){
    return unpack16bits(value.left(6)) << 32 | unpack16bits(value.right(6));
}

QString Varicode::pack64bits(qint64 packed){
    qint32 a = (packed & 0xFFFFFFFF00000000) >> 32;
    qint32 b = packed & 0xFFFFFFFF;
}
