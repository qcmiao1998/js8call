#ifndef VARICODE_H
#define VARICODE_H

#include <QString>

class Varicode
{
public:
    //Varicode();

    qint16 unpack16bits(QString const& triple);
    QString pack16bits(qint16 packed);
};

#endif // VARICODE_H
