/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/
#include <QDebug>
#include <QMap>

#include "varicode.h"

const int nalphabet = 41;
QString alphabet = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"};
QString grid_pattern = {R"((?<grid>[A-R]{2}[0-9]{2})+)"};
QString callsign_pattern1 = {R"((?<callsign>[A-Z0-9/]{2,}))"};
QString callsign_pattern2 = {R"((?<callsign>(\d|[A-Z])+\/?((\d|[A-Z]){3,})(\/(\d|[A-Z])+)?(\/(\d|[A-Z])+)?))"};

QMap<QChar, QString> huff = {
    // char   code                 weight
    {' '    , "001"          }, // 1300
    {'E'    , "000"          }, // 1270.2
    {'T'    , "1100"         }, // 905.6
    {'A'    , "1010"         }, // 816.7
    {'O'    , "0111"         }, // 750.7
    {'I'    , "0101"         }, // 696.6
    {'N'    , "0100"         }, // 674.9
    {'S'    , "11111"        }, // 632.7
    {'H'    , "11110"        }, // 609.4
    {'R'    , "11101"        }, // 598.7
    {'D'    , "10111"        }, // 425.3
    {'L'    , "10110"        }, // 402.5
    {'C'    , "111001"       }, // 278.2
    {'U'    , "111000"       }, // 275.8
    {'M'    , "110111"       }, // 240.6
    {'W'    , "110110"       }, // 236.0
    {'F'    , "110100"       }, // 222.8
    {'G'    , "100111"       }, // 201.5
    {'Q'    , "100110"       }, // 200
    {'Y'    , "011010"       }, // 197.4
    {'P'    , "011001"       }, // 192.9
    {'B'    , "011000"       }, // 149.2
    {'!'    , "0110111"      }, // 100
    {'.'    , "1000000"      }, // 100
    {'0'    , "1000001"      }, // 100
    {'1'    , "1000010"      }, // 100
    {'2'    , "1000011"      }, // 100
    {'3'    , "1000100"      }, // 100
    {'4'    , "1000101"      }, // 100
    {'5'    , "1000110"      }, // 100
    {'6'    , "1000111"      }, // 100
    {'7'    , "1001000"      }, // 100
    {'8'    , "1001001"      }, // 100
    {'9'    , "1001010"      }, // 100
    {'?'    , "1001011"      }, // 100
    {'^'    , "1101010"      }, // 100     <- shift
    {'V'    , "0110110"      }, // 97.8
    {'K'    , "11010111"     }, // 77.2
    {'J'    , "1101011010"   }, // 15.3
    {'X'    , "1101011001"   }, // 15.0
    {'Z'    , "11010110110"  }, // 7.4
    {':'    , "11010110000"  }, // 5
    {'+'    , "110101100011" }, // 5
    {'-'    , "110101101110" }, // 5
    {'/'    , "110101101111" }, // 5
    {'\x04' , "110101100010" }, // 1       <- eot
};

QChar huffeot = '\x04';

QStringList Varicode::parseCallsigns(QString const &input){
    QStringList callsigns;
    QRegularExpression re(callsign_pattern2);
    QRegularExpressionMatchIterator iter = re.globalMatch(input);
    while(iter.hasNext()){
        QRegularExpressionMatch match = iter.next();
        if(!match.hasMatch()){
            continue;
        }
        QString callsign = match.captured("callsign");
        QRegularExpression m(grid_pattern);
        if(m.match(callsign).hasMatch()){
            continue;
        }
        callsigns.append(callsign);
    }
    return callsigns;
}

QStringList Varicode::parseGrids(const QString &input){
    QStringList grids;
    QRegularExpression re(grid_pattern);
    QRegularExpressionMatchIterator iter = re.globalMatch(input);
    while(iter.hasNext()){
        QRegularExpressionMatch match = iter.next();
        if(!match.hasMatch()){
            continue;
        }
        auto grid = match.captured("grid");
        if(grid == "RR73"){
            continue;
        }
        grids.append(grid);
    }
    return grids;
}

QList<QVector<bool>> Varicode::huffEncode(QString const& text){
    QList<QVector<bool>> out;

    foreach(auto ch, text){
        if(!huff.contains(ch)){
            continue;
        }
        out.append(strToBits(huff[ch]));
    }

    return out;
}

QVector<bool> Varicode::huffFlatten(QList<QVector<bool>> &list){
    QVector<bool> out;
    foreach(auto vec, list){
        out += vec;
    }
    return out;
}

QString Varicode::huffDecode(QVector<bool> const& bitvec){
    QString out;

    QString bits = bitsToStr(bitvec);

    // TODO: jsherer - this is naive...
    while(bits.length() > 0){
        bool found = false;
        foreach(auto key, huff.keys()){
            if(bits.startsWith(huff[key])){
                if(key == huffeot){
                    break;
                }
                out.append(key);
                bits = bits.mid(huff[key].length());
                found = true;
            }
        }
        if(!found){
            break;
        }
    }

    return out;
}

QVector<bool> Varicode::strToBits(QString const& bitvec){
    QVector<bool> bits;
    foreach(auto ch, bitvec){
        bits.append(ch == '1');
    }
    return bits;
}

QString Varicode::bitsToStr(QVector<bool> const& bitvec){
    QString bits;
    foreach(auto bit, bitvec){
        bits.append(bit ? "1" : "0");
    }
    return bits;
}

QVector<bool> Varicode::intToBits(quint64 value, int expected){
    QVector<bool> bits;

    while(value){
        bits.prepend((bool)(value & 1));
        value = value >> 1;
    }

    if(expected){
        while(bits.count() < expected){
            bits.prepend((bool) 0);
        }
    }

    return bits;
}

quint64 Varicode::bitsToInt(QVector<bool> const value){
    quint64 v = 0;
    foreach(bool bit, value){
        v = (v << 1) + (int)(bit);
    }
    return v;
}

quint8 Varicode::unpack5bits(QString const& value){
    return alphabet.indexOf(value.at(0));
}

QString Varicode::pack5bits(quint8 packed){
    return alphabet.at(packed % nalphabet);
}

quint16 Varicode::unpack16bits(QString const& value){
    int a = alphabet.indexOf(value.at(0));
    int b = alphabet.indexOf(value.at(1));
    int c = alphabet.indexOf(value.at(2));
    return (nalphabet*nalphabet) * a + nalphabet*b + c;
}

QString Varicode::pack16bits(quint16 packed){
    QString out;
    quint16 tmp = packed / (nalphabet*nalphabet);

    out.append(alphabet.at(tmp));

    tmp = (packed - (tmp * (nalphabet*nalphabet))) / nalphabet;
    out.append(alphabet.at(tmp));

    tmp = packed % nalphabet;
    out.append(alphabet.at(tmp));



    return out;
}

quint32 Varicode::unpack32bits(QString const& value){
    return (quint32)(unpack16bits(value.left(3))) << 16 | unpack16bits(value.right(3));
}

QString Varicode::pack32bits(quint32 packed){
    quint16 a = (packed & 0xFFFF0000) >> 16;
    quint16 b = packed & 0xFFFF;
    return pack16bits(a) + pack16bits(b);
}

quint64 Varicode::unpack64bits(QString const& value){
    return (quint64)(unpack32bits(value.left(6))) << 32 | unpack32bits(value.right(6));
}

QString Varicode::pack64bits(quint64 packed){
    quint32 a = (packed & 0xFFFFFFFF00000000) >> 32;
    quint32 b = packed & 0xFFFFFFFF;
    return pack32bits(a) + pack32bits(b);
}
