/**
 * This file is part of FT8Call.
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

#include <QDebug>
#include <QMap>

#include "varicode.h"

const int nalphabet = 41;
QString alphabet = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"};
QString grid_pattern = {R"((?<grid>[A-R]{2}[0-9]{2})+)"};
QString callsign_pattern1 = {R"((?<callsign>[A-Z0-9/]{2,}))"};
QString callsign_pattern2 = {R"((?<callsign>(\d|[A-Z])+\/?((\d|[A-Z]){3,})(\/(\d|[A-Z])+)?(\/(\d|[A-Z])+)?))"};
QString callsign_pattern3 = {R"(([0-9A-Z ])([0-9A-Z])([0-9])([A-Z ])([A-Z ])([A-Z ]))"};
QString callsign_alphabet = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ "};

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

quint32 nbasecall = 37 * 36 * 10 * 27 * 27 * 27;

QMap<QString, quint32> basecalls = {
    { "CQ DX",   nbasecall + 1 },
    { "CQCQCQ",  nbasecall + 2 },
    { "ALLCALL", nbasecall + 3 },
};

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

quint32 Varicode::packCallsign(QString const& value){
    quint32 packed = 0;

    QString callsign = value.toUpper().trimmed();

    if(basecalls.contains(callsign)){
        return basecalls[callsign];
    }

    // workaround for swaziland
    if(callsign.startsWith("3DA0")){
        callsign = "3D0" + callsign.mid(4);
    }

    // workaround for guinea
    if(callsign.startsWith("3X") && 'A' <= callsign.at(2) && callsign.at(2) <= 'Z'){
        callsign = "Q" + callsign.mid(2);
    }

    int slen = callsign.length();
    if(slen < 2){
        return packed;
    }

    if(slen > 6){
        return packed;
    }

    QStringList permutations = { callsign };
    if(slen == 2){
        permutations.append(" " + callsign + "   ");
    }
    if(slen == 3){
        permutations.append(" " + callsign + "  ");
        permutations.append(callsign + "   ");
    }
    if(slen == 4){
        permutations.append(" " + callsign + " ");
        permutations.append(callsign + "  ");
    }
    if(slen == 5){
        permutations.append(" " + callsign);
        permutations.append(callsign + " ");
    }

    QString matched;
    QRegularExpression m(callsign_pattern3);
    foreach(auto permutation, permutations){
        auto match = m.match(permutation);
        if(match.hasMatch()){
            matched = match.captured(0);
        }
    }
    if(matched.isEmpty()){
        return packed;
    }
    if(matched.length() < 6){
        return packed;
    }

    packed = callsign_alphabet.indexOf(matched.at(0));
    packed = 36*packed + callsign_alphabet.indexOf(matched.at(1));
    packed = 10*packed + callsign_alphabet.indexOf(matched.at(2));
    packed = 27*packed + callsign_alphabet.indexOf(matched.at(3)) - 10;
    packed = 27*packed + callsign_alphabet.indexOf(matched.at(4)) - 10;
    packed = 27*packed + callsign_alphabet.indexOf(matched.at(5)) - 10;

    return packed;
}

QString Varicode::unpackCallsign(quint32 value){
    foreach(auto key, basecalls.keys()){
        if(basecalls[key] == value){
            return key;
        }
    }

    QChar word[6];
    quint32 tmp = value % 27 + 10;
    word[5] = callsign_alphabet.at(tmp);
    value = value/27;

    tmp = value % 27 + 10;
    word[4] = callsign_alphabet.at(tmp);
    value = value/27;

    tmp = value % 27 + 10;
    word[3] = callsign_alphabet.at(tmp);
    value = value/27;

    tmp = value % 10;
    word[2] = callsign_alphabet.at(tmp);
    value = value/10;

    tmp = value % 36;
    word[1] = callsign_alphabet.at(tmp);
    value = value/36;

    tmp = value;
    word[0] = callsign_alphabet.at(tmp);

    QString callsign(word, 6);
    if(callsign.startsWith("3D0")){
        callsign = "3DA0" + callsign.mid(3);
    }

    if(callsign.startsWith("Q") and 'A' <= callsign.at(1) && callsign.at(1) <= 'Z'){
        callsign = "3X" + callsign.mid(1);
    }

    return callsign;
}

QString deg2grid(float dlong, float dlat){
    QChar grid[6];

    if(dlong < -180){
        dlong += 360;
    }
    if(dlong > 180){
        dlong -= 360;
    }

    int nlong = int(60.0*(180.0-dlong)/5);

    int n1 = nlong/240;
    int n2 = (nlong-240*n1)/24;
    int n3 = (nlong-240*n1-24*n2);

    grid[0] = QChar('A' + n1);
    grid[2] = QChar('0' + n2);
    grid[4] = QChar('a' + n3);

    int nlat=int(60.0*(dlat+90)/2.5);

    n1 = nlat/240;
    n2 = (nlat-240*n1)/24;
    n3 = (nlat-240*n1-24*n2);

    grid[1] = QChar('A' + n1);
    grid[3] = QChar('0' + n2);
    grid[5] = QChar('a' + n3);

    return QString(grid, 6);
}

QPair<float, float> grid2deg(QString const &grid){
    QPair<float, float> longLat;

    QString g = grid;
    if(g.length() < 6){
        g = grid.left(4) + "mm";
    }

    g = g.left(4).toUpper() + g.right(2).toLower();

    int nlong = 180 - 20 * (g.at(0).toLatin1() - 'A');
    int n20d  = 2 * (g.at(2).toLatin1() - '0');
    float xminlong  = 5 * (g.at(4).toLatin1() - 'a' + 0.5);
    float dlong = nlong - n20d - xminlong/60.0;

    int nlat = -90 + 10*(g.at(1).toLatin1() - 'A') + g.at(3).toLatin1() - '0';
    float xminlat = 2.5 * (g.at(5).toLatin1() - 'a' + 0.5);
    float dlat = nlat + xminlat/60.0;

    longLat.first = dlong;
    longLat.second = dlat;

    return longLat;
}

quint16 Varicode::packGrid(QString const& grid){
    // TODO: validate grid...

    // TODO: encode non-grid data...

    auto pair = grid2deg(grid.left(4));
    int ilong = pair.first;
    int ilat = pair.second + 90;

    return ((ilong + 180)/2) * 180 + ilat;
}

QString Varicode::unpackGrid(quint16 value){
    if(value > 180*180){
        // TODO: decode non-grid data...
        return "";
    }

    float dlat = value % 180 - 90;
    float dlong = value / 180 * 2 - 180 + 2;

    return deg2grid(dlong, dlat).left(4);
}

QString Varicode::packDirectedMessage(const QString &text, int *n){
    QString frame;
    QString cmds("?$@&| ");
    QRegularExpression r(R"((?<from>[A-Z0-9/]+):(?<to>[A-Z0-9/]+)(?<cmd>[?$@&| ]))");
    auto match = r.match(text);
    if(match.hasMatch()){
        QString from = match.captured("from");
        QString to = match.captured("to");
        QString cmd = match.captured("cmd");
        if(cmd.at(0) != '?'){
            // this is the only allowed one at this point...
            *n = 0;
            return frame;
        }

        quint8 packed_flag = 0;
        quint32 packed_from = Varicode::packCallsign(from);
        quint32 packed_to = Varicode::packCallsign(to);
        quint8 packed_cmd = cmds.indexOf(cmd.at(0));

        // 3 + 28 + 28 + 5 = 64
        auto bits = (
            Varicode::intToBits(packed_flag, 3)    +
            Varicode::intToBits(packed_from, 28)   +
            Varicode::intToBits(packed_to, 28)     +
            Varicode::intToBits(packed_cmd & 7, 5)
        );
        frame = Varicode::pack64bits(Varicode::bitsToInt(bits));
        *n = match.captured(0).length();
    }
    return frame;
}

QStringList Varicode::unpackDirectedMessage(const QString &text){
    QStringList unpacked;
    QString cmds("?$@&| ");
    auto bits = Varicode::bitsToStr(Varicode::intToBits(Varicode::unpack64bits(text), 64));

    quint8 flag = Varicode::bitsToInt(Varicode::strToBits(bits.left(3)));
    quint32 packed_from = Varicode::bitsToInt(Varicode::strToBits(bits.mid(3, 28)));
    quint32 packed_to = Varicode::bitsToInt(Varicode::strToBits(bits.mid(31, 28)));
    quint8 packed_cmd = Varicode::bitsToInt(Varicode::strToBits(bits.mid(59, 5)));

    unpacked.append(Varicode::unpackCallsign(packed_from).trimmed());
    unpacked.append(Varicode::unpackCallsign(packed_to).trimmed());
    unpacked.append(QString(cmds.at(packed_cmd & 5)));

    return unpacked;
}
