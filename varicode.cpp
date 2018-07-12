#include "varicode.h"

QString alphabet = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"};

qint16 Varicode::unpack16bits(QString const& triple){
    int a = alphabet.indexOf(triple.at(0));
    int b = alphabet.indexOf(triple.at(1));
    int c = alphabet.indexOf(triple.at(2));
    return (41*41) * a + 41*b + c;
}

QString Varicode::pack16bits(qint16 packed){
    QString out;
    qint16 tmp = packed / (41*41);
    out.append(alphabet.at(tmp));

    tmp = (packed - (tmp * (41*41))) / 41;
    out.append(alphabet.at(tmp));

    tmp = packed % 41;
    out.append(alphabet.at(tmp));

    return out;
}
