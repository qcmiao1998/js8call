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
#include <QSet>

#define CRCPP_INCLUDE_ESOTERIC_CRC_DEFINITIONS
#include "crc.h"

#include "varicode.h"

const int nalphabet = 41;
QString alphabet = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?"}; // alphabet to encode _into_ for FT8 freetext transmission
QString alphabet72 = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+/?."};
QString grid_pattern = {R"((?<grid>[A-R]{2}[0-9]{2})+)"};
QString orig_compound_callsign_pattern = {R"((?<callsign>(\d|[A-Z])+\/?((\d|[A-Z]){2,})(\/(\d|[A-Z])+)?(\/(\d|[A-Z])+)?))"};
QString compound_callsign_pattern = {R"((?<callsign>\b(?<prefix>[A-Z0-9]{1,4}\/)?(?<base>([0-9A-Z])?([0-9A-Z])([0-9])([A-Z])?([A-Z])?([A-Z])?)(?<suffix>\/[A-Z0-9]{1,4})?)\b)"};
QString pack_callsign_pattern = {R"(([0-9A-Z ])([0-9A-Z])([0-9])([A-Z ])([A-Z ])([A-Z ]))"};
QString alphanumeric = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ "}; // callsign and grid alphabet

QMap<QString, int> directed_cmds = {
    // any changes here need to be made also in the directed regular xpression for parsing

    // directed queries
    {"?",     0  }, // query snr
    {"@",     1  }, // query qth
    {"&",     2  }, // query station message
    {"$",     3  }, // query station(s) heard
    {"%",     5  }, // query pwr
    // 4.
    {"|",     6  }, // retransmit message
    {"!",     7  }, // alert message
    {"#",     8  }, // all or nothing message

    // {"=",     9  }, // unused? (can we even use equals?)
    // {"/",     10  }, // unused? (can we even use stroke?)

    // directed responses
    {" HEARING", 20  }, // i am hearing the following stations
    {" RR",      21  }, // roger roger (not visible in UI but still exists)
    {" QSL?",    22  }, // do you copy?
    {" QSL",     23  }, // i copy
    {" PWR",     24  }, // power level
    {" SNR",     25  }, // seen a station at the provided snr
    {" NO",      26  }, // negative confirm
    {" YES",     27  }, // confirm
    {" 73",      28  }, // best regards, end of contact
    {" ACK",     29  }, // acknowledge
    {" AGN?",    30  }, // repeat message
    {" ",        31 },  // send freetext
};

QSet<int> allowed_cmds = {0, 1, 2, 3, /*4,*/ 5, 6, 7, 8, /*...*/ 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

QSet<int> buffered_cmds = {6, 7, 8};

QString callsign_pattern = QString("(?<callsign>[A-Z0-9/]+)");
QString optional_cmd_pattern = QString("(?<cmd>\\s?(?:AGN[?]|ACK|73|YES|NO|SNR|PWR|QSL[?]?|RR|HEARING|[?@&$%|!# ]))?");
QString optional_grid_pattern = QString("(?<grid>\\s?[A-R]{2}[0-9]{2})?");
QString optional_extended_grid_pattern = QString("^(?<grid>\\s?(?:[A-R]{2}[0-9]{2}(?:[A-X]{2}(?:[0-9]{2})?)*))?");
QString optional_pwr_pattern = QString("(?<pwr>(?<=PWR)\\s?\\d+\\s?[KM]?W)?");
QString optional_num_pattern = QString("(?<num>(?<=SNR|HEARING)\\s?[-+]?(?:3[01]|[0-2]?[0-9]))?");

QRegularExpression directed_re("^"                    +
                               callsign_pattern       +
                               optional_cmd_pattern   +
                               optional_pwr_pattern   +
                               optional_num_pattern);

QRegularExpression beacon_re(R"(^(?<type>CQCQCQ|BEACON)(?:\s(?<grid>[A-R]{2}[0-9]{2}))?\b)");

QRegularExpression compound_re("^[<]"                  +
                               callsign_pattern        +
                               "(?<extra>"             +
                                 optional_grid_pattern +
                                 optional_cmd_pattern  +
                                 optional_pwr_pattern  +
                                 optional_num_pattern  +
                               ")[>]");

QMap<QString, QString> hufftable = {
    // char   code                 weight
    // 3 bits
    { " "    , "000"          }, // 1300
    { "E"    , "001"          }, // 1270.2

    // 4 bits
    { "T"    , "1100"         }, // 905.6
    { "A"    , "1010"         }, // 816.7
    { "O"    , "0111"         }, // 750.7
    { "I"    , "0101"         }, // 696.6
    { "N"    , "0100"         }, // 674.9

    // 5 bits
    { "S"    , "11111"        }, // 632.7
    { "H"    , "11110"        }, // 609.4
    { "R"    , "11101"        }, // 598.7
    { "D"    , "10111"        }, // 425.3
    { "L"    , "10110"        }, // 402.5

    // 6 bits
    { "C"    , "111001"       }, // 278.2
    { "U"    , "111000"       }, // 275.8
    { "M"    , "110111"       }, // 240.6
    { "W"    , "110110"       }, // 236.0
    { "F"    , "110100"       }, // 222.8
    { "G"    , "100111"       }, // 201.5
    { "Q"    , "100110"       }, // 200
    { "Y"    , "011010"       }, // 197.4
    { "P"    , "011001"       }, // 192.9
    { "B"    , "011000"       }, // 149.2

    // 7 bits
    { "\\"   , "0110111"      }, // 100     <- escape
    { "."    , "1000000"      }, // 100
    { "0"    , "1000001"      }, // 100
    { "1"    , "1000010"      }, // 100
    { "2"    , "1000011"      }, // 100
    { "3"    , "1000100"      }, // 100
    { "4"    , "1000101"      }, // 100
    { "5"    , "1000110"      }, // 100
    { "6"    , "1000111"      }, // 100
    { "7"    , "1001000"      }, // 100
    { "8"    , "1001001"      }, // 100
    { "9"    , "1001010"      }, // 100
    { "?"    , "1001011"      }, // 100
    { "/"    , "1101010"      }, // 100
    { "V"    , "0110110"      }, // 97.8

    // 8 bits
    { "K"    , "11010111"     }, // 77.2

    // 10 bits
    { "J"    , "1101011010"   }, // 15.3
    { "X"    , "1101011001"   }, // 15.0

    // 11 bits
    { "Z"    , "11010110110"  }, // 7.4
    { ":"    , "11010110000"  }, // 5

    // 12 bits
    { "+"    , "110101100011" }, // 5
    { "-"    , "110101101110" }, // 5
    { "!"    , "110101101111" }, // 5
    { "\x04" , "110101100010" }, // 1       <- eot

    /*
    A-Z 0-9 Space \\ ? / : - + !
    special chars that are escaped will be added here too...
    */
};

/*
via https://www3.nd.edu/~busiforc/handouts/cryptography/Letter%20Frequencies.html#Most_common_trigrams_.28in_order.29

most common trigrams:
the = 12 bits
and = 13 bits
tha = 13 bits
ent = 11 bits
ing = 14 bits **
ion = 12 bits
tio = 12 bits
for = 15 bits **
nde = 12 bits
has = 14 bits
nce = 13 bits
edt = 12 bits
tis = 13 bits
oft = 14 bits
sth = 14 bits
men = 13 bits
her = 13 bits
hat = 13 bits
his = 14 bits
ere = 11 bits
ter = 12 bits
was = 15 bits
you = 16 bits **
ith = 13 bits
ver = 15 bits
all = 14 bits
wit = 14 bits
thi = 13 bits

most common quadgrams:
that = 17 bits
ther = 17 bits
with = 18 bits
tion = 16 bits
here = 16 bits
ould = 20 bits **
ight = 19 bits
have = 19 bits
hich = 20 bits **
whic = 21 bits **
this = 18 bits
thin = 18 bits
they = 18 bits
atio = 16 bits
ever = 18 bits
from = 21 bits **
ough = 21 bits **
were = 17 bits
hing = 18 bits
ment = 17 bits

potential contenders:
_DE_ = 14 bits
BTU = 16 bits
... = 21 bits
599 = 21 bits
FT8 = 17 bits
BAND = 19 bits
FT8CALL = 37 bits
DIPOLE = 27 bits
VERT = 19 bits
BEAM = 19 bits
*/


/*
original: Space \\ ? / : - + !
needed: ^,&@#$%'"()<>|*[]{}=;_~`
*/
QMap<QString, QString> huffescapes = {

    // 10 bits
#if 0
    {  "\\ ",   ""  },
#endif
    {  "\\E",   "," },

    // 11 bits
    {  "\\T",   "&"  },
    {  "\\A",   "@"  },
    {  "\\O",   "#"  },
    {  "\\I",   "$"  },
    {  "\\N",   "%"  },

    // 12 bits
    {  "\\S",   "\'" },
    {  "\\H",   "\"" },
    {  "\\R",   "("  },
    {  "\\D",   ")"  },
    {  "\\L",   "|"  },

    // 13 bits
    // trigram / quadgram efficiency
    {  "\\C",   "YOU"   }, // 16 bits - 3 bit savings
    {  "\\U",   "THAT"  }, // 17 bits - 4 bit savings
    {  "\\M",   "THER"  }, // 17 bits - 4 bit savings
    {  "\\W",   "WITH"  }, // 18 bits - 5 bit savings
    {  "\\F",   "TION"  }, // 16 bits - 3 bit savings
    {  "\\G",   "HERE"  }, // 16 bits - 3 bit savings
    {  "\\Q",   "OULD"  }, // 20 bits - 7 bit savings
    {  "\\Y",   "IGHT"  }, // 19 bits - 6 bit savings
    {  "\\P",   "HAVE"  }, // 19 bits - 6 bit savings
    {  "\\B",   "HICH"  }, // 20 bits - 7 bit savings

#if 0
    // 14 bits
    {  "\\.",   ""  },
    {  "\\0",   ""  },
#endif

    {  "\\1",   "<"  },
    {  "\\2",   ">"  },
    {  "\\3",   "["  },
    {  "\\4",   "]"  },
    {  "\\5",   "{"  },
    {  "\\6",   "}"  },
    {  "\\7",   "*"  },
    {  "\\8",   "="  },
    {  "\\9",   ";"  },
    {  "\\?",   "WHIC"  }, // 21 bits - 7 bit savings
    {  "\\/",   "THIS"  }, // 18 bits - 4 bit savings
    {  "\\V",   "FROM"  }, // 21 bits - 7 bit savings

    // 15 bits
    // quadgram efficiency
    {  "\\K"  , "OUGH"  }, // 21 bits - 6 bit savings

    // 17 bits
#if 0
    {  "\\J"  , ""   },
    {  "\\X"  , ""   },
#endif

    // 18 bits
    {  "\\Z"  , "^"  },
    {  "\\:"  , "~"  },

    // 19 bits
    {  "\\+"  , "`"  },
    {  "\\-"  , "_"  },

    // special case :)
    {  "\\!"  , "FT8CALL"  }, // 37 bits - 18 bit savings
};

QChar ESC = '\\';   // Escape char
QChar EOT = '\x04'; // EOT char

quint32 nbasecall = 37 * 36 * 10 * 27 * 27 * 27;
quint16 nbasegrid = 180 * 180;
quint16 nusergrid = nbasegrid + 10;
quint16 nmaxgrid  = (1<<15)-1;

QMap<QString, quint32> basecalls = {
    { "<....>",  nbasecall + 1 }, // incomplete callsign
    { "ALLCALL", nbasecall + 2 },
};

QMap<int, int> dbm2mw = {
    {0  , 1},       //   1mW
    {3  , 2},       //   2mW
    {7  , 5},       //   5mW
    {10 , 10},      //  10mW
    {13 , 20},      //  20mW
    {17 , 50},      //  50mW
    {20 , 100},     // 100mW
    {23 , 200},     // 200mW
    {27 , 500},     // 500mW
    {30 , 1000},    //    1W
    {33 , 2000},    //    2W
    {37 , 5000},    //    5W
    {40 , 10000},   //   10W
    {43 , 20000},   //   20W
    {47 , 50000},   //   50W
    {50 , 100000},  //  100W
    {53 , 200000},  //  200W
    {57 , 500000},  //  500W
    {60 , 1000000}, // 1000W
};


QMap<QString, QString> initializeEscapes(QMap<QString, QString> huff, QMap<QString, QString> escapes){
    QMap<QString, QString> newhuff(huff);
    foreach(auto escapeString, escapes.keys()){
        auto ch = escapes[escapeString];
        auto encoded = Varicode::huffEncode(huff, escapeString);
        QList<QVector<bool>> e;
        foreach(auto pair, encoded){
            e.append(pair.second);
        }
        auto bits = Varicode::bitsListToBits(e);
        newhuff[ch] = Varicode::bitsToStr(bits);
    }

#if PRINT_VARICODE_ALPHABET
    auto keys = newhuff.keys();
    qSort(keys.begin(), keys.end(), [newhuff](QChar a, QChar b){
        return newhuff[a].length() < newhuff[b].length();
    });
    foreach(auto ch, keys){
        qDebug() << ch << newhuff[ch] << newhuff[ch].length();
    }
#endif

    return newhuff;
}

QMap<QString, QString> hufftableescaped = initializeEscapes(hufftable, huffescapes);

/*
 * UTILITIES
 */

int mwattsToDbm(int mwatts){
    int dbm = 0;
    auto values = dbm2mw.values();
    qSort(values);
    foreach(auto mw, values){
        if(mw < mwatts){ continue; }
        dbm = dbm2mw.key(mw);
        break;
    }

    return dbm;
}

int dbmTomwatts(int dbm){
    if(dbm2mw.contains(dbm)){
        return dbm2mw[dbm];
    }
    auto iter = dbm2mw.lowerBound(dbm);
    if(iter == dbm2mw.end()){
        return dbm2mw.last();
    }
    return iter.value();
}

/*
 * VARICODE
 */

QString Varicode::formatSNR(int snr){
    if(snr < -60 || snr > 60){
        return QString();
    }

    return QString("%1%2").arg(snr >= 0 ? "+" : "").arg(snr, snr < 0 ? 3 : 2, 10, QChar('0'));
}

QString Varicode::formatPWR(int dbm){
    if(dbm < 0 || dbm > 60){
        return QString();
    }

    int mwatts = dbmTomwatts(dbm);
    if(mwatts < 1000){
        return QString("%1mW").arg(mwatts);
    }

    return QString("%1W").arg(mwatts/1000);
}

QString Varicode::checksum16(QString const &input){
    auto fromBytes = input.toLocal8Bit();
    auto crc = CRC::Calculate(fromBytes.data(), fromBytes.length(), CRC::CRC_16_KERMIT());
    auto checksum = Varicode::pack16bits(crc);
    if(checksum.length() < 3){
        checksum += QString(" ").repeated(3-checksum.length());
    }
    return checksum;
}

bool Varicode::checksum16Valid(QString const &checksum, QString const &input){
    auto fromBytes = input.toLocal8Bit();
    auto crc = CRC::Calculate(fromBytes.data(), fromBytes.length(), CRC::CRC_16_KERMIT());
    return Varicode::pack16bits(crc) == checksum;
}

QString Varicode::checksum32(QString const &input){
    auto fromBytes = input.toLocal8Bit();
    auto crc = CRC::Calculate(fromBytes.data(), fromBytes.length(), CRC::CRC_32_BZIP2());
    auto checksum = Varicode::pack32bits(crc);
    if(checksum.length() < 6){
        checksum += QString(" ").repeated(6-checksum.length());
    }
    return checksum;
}

bool Varicode::checksum32Valid(QString const &checksum, QString const &input){
    auto fromBytes = input.toLocal8Bit();
    auto crc = CRC::Calculate(fromBytes.data(), fromBytes.length(), CRC::CRC_32_BZIP2());
    return Varicode::pack32bits(crc) == checksum;
}

QStringList Varicode::parseCallsigns(QString const &input){
    QStringList callsigns;
    QRegularExpression re(compound_callsign_pattern);
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

QList<QPair<int, QVector<bool>>> Varicode::huffEncode(const QMap<QString, QString> &huff, QString const& text){
    QList<QPair<int, QVector<bool>>> out;

    int i = 0;

    auto keys = huff.keys();
    qSort(keys.begin(), keys.end(), [](QString const &a, QString const &b){
        auto alen = a.length();
        auto blen = b.length();
        if(blen < alen){
            return true;
        }
        if(alen < blen){
            return false;
        }

        return b < a;
    });

    while(i < text.length()){
        bool found = false;
        foreach(auto ch, keys){
            if(text.midRef(i).startsWith(ch)){
                out.append({ ch.length(), Varicode::strToBits(huff[ch])});
                i += ch.length();
                found = true;
                break;
            }
        }

        if(!found){
            i++;
        }
    }

    return out;
}

QString Varicode::huffDecode(QMap<QString, QString> const &huff, QVector<bool> const& bitvec){
    QString text;

    QString bits = Varicode::bitsToStr(bitvec); //.mid(0, bitvec.length()-pad);

    // TODO: jsherer - this is naive...
    while(bits.length() > 0){
        bool found = false;
        foreach(auto key, huff.keys()){
            if(bits.startsWith(huff[key])){
                if(key == EOT){
                    text.append(" ");
                    found = false;
                    break;
                }
                text.append(key);
                bits = bits.mid(huff[key].length());
                found = true;
            }
        }
        if(!found){
            break;
        }
    }

    return text;
}

QString Varicode::huffUnescape(QString const &input){
    QString text = input;
    // unescape alternate alphabet
    foreach(auto escaped, huffescapes.keys()){
        text = text.replace(escaped, huffescapes[escaped]);
    }
    return text;
}

QString Varicode::huffEscape(QString const &input){
    QString text = input;
    // escape alternate alphabet
    foreach(auto unescaped, huffescapes.values()){
        text = text.replace(unescaped, huffescapes.key(unescaped));
    }
    return text;
}

QSet<QString> Varicode::huffValidChars(){
    return QSet<QString>::fromList(hufftableescaped.keys());
}

bool Varicode::huffShouldEscape(QString const &input){
    foreach(auto ch, huffescapes.values()){
        if(input.contains(ch)){
            return true;
        }
    }

    return false;
}


// convert char* array of 0 bytes and 1 bytes to bool vector
QVector<bool> Varicode::bytesToBits(char *bitvec, int n){
    QVector<bool> bits;
    for(int i = 0; i < n; i++){
        bits.append(bitvec[i] == 0x01);
    }
    return bits;
}

// convert string of 0s and 1s to bool vector
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

quint64 Varicode::bitsToInt(QVector<bool>::ConstIterator start, int n){
    quint64 v = 0;
    for(int i = 0; i < n; i++){
        int bit = (int)(*start);
        v = (v << 1) + (int)(bit);
        start++;
    }
    return v;
}

QVector<bool> Varicode::bitsListToBits(QList<QVector<bool>> &list){
    QVector<bool> out;
    foreach(auto vec, list){
        out += vec;
    }
    return out;
}

quint8 Varicode::unpack5bits(QString const& value){
    return alphabet.indexOf(value.at(0));
}

// pack a 5-bit value from 0 to 31 into a single character
QString Varicode::pack5bits(quint8 packed){
    return alphabet.at(packed % 32);
}

quint8 Varicode::unpack6bits(QString const& value){
    return alphabet.indexOf(value.at(0));
}

// pack a 6-bit value from 0 to 40 into a single character
QString Varicode::pack6bits(quint8 packed){
    return alphabet.at(packed % 41);
}

quint16 Varicode::unpack16bits(QString const& value){
    int a = alphabet.indexOf(value.at(0));
    int b = alphabet.indexOf(value.at(1));
    int c = alphabet.indexOf(value.at(2));

    int unpacked = (nalphabet * nalphabet) * a + nalphabet * b + c;
    if(unpacked > (1<<16)-1){
        // BASE-41 can produce a value larger than 16 bits... ala "???" == 70643
        return 0;
    }

    return unpacked & ((1<<16)-1);
}

// pack a 16-bit value into a three character sequence
QString Varicode::pack16bits(quint16 packed){
    QString out;
    quint16 tmp = packed / (nalphabet * nalphabet);

    out.append(alphabet.at(tmp));

    tmp = (packed - (tmp * (nalphabet * nalphabet))) / nalphabet;
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

// returns the first 64 bits and sets the last 8 bits in pRem
quint64 Varicode::unpack72bits(QString const& text, quint8 *pRem){
    quint64 value = 0;
    quint8 rem = 0;
    quint8 mask2 = ((1<<2)-1);

    for(int i = 0; i < 10; i++){
        value |= (quint64)(alphabet72.indexOf(text.at(i))) << (58-6*i);
    }

    quint8 remHigh = alphabet72.indexOf(text.at(10));
    value |= remHigh >> 2;

    quint8 remLow = alphabet72.indexOf(text.at(11));
    rem = ((remHigh & mask2) << 6) | remLow;

    if(pRem) *pRem = rem;
    return value;
}

QString Varicode::pack72bits(quint64 value, quint8 rem){
    QChar packed[12]; // 12 x 6bit characters

    quint8 mask4 = ((1<<4)-1);
    quint8 mask6 = ((1<<6)-1);

    quint8 remHigh = ((value & mask4) << 2) | (rem >> 6);
    quint8 remLow = rem & mask6;
    value = value >> 4;

    packed[11] = alphabet72.at(remLow);
    packed[10] = alphabet72.at(remHigh);

    for(int i = 0; i < 10; i++){
        packed[9-i] = alphabet72.at(value & mask6);
        value = value >> 6;
    }

    return QString(packed, 12);
}




//     //
// --- //
//     //


// pack a 4-digit alpha-numeric + space into a 22 bit value
// 21 bits for the data + 1 bit for a flag indicator
// giving us a total of 5.5 bits per character
quint32 Varicode::packAlphaNumeric22(QString const& value, bool isFlag){
    QString word = QString(value).replace(QRegExp("[^A-Z0-9]"), "");
    if(word.length() < 4){
        word = word + QString(" ").repeated(4-word.length());
    }

    quint32 a = 37 * 37 * 37 * alphanumeric.indexOf(word.at(0));
    quint32 b = 37 * 37 * alphanumeric.indexOf(word.at(1));
    quint32 c = 37 * alphanumeric.indexOf(word.at(2));
    quint32 d = alphanumeric.indexOf(word.at(3));

    quint32 packed = a + b + c + d;
    packed = (packed << 1) + (int)isFlag;

    return packed;
}

QString Varicode::unpackAlphaNumeric22(quint32 packed, bool *isFlag){
    QChar word[4];

    if(isFlag) *isFlag = packed & 1;
    packed = packed >> 1;

    quint32 tmp = packed % 37;
    word[3] = alphanumeric.at(tmp);
    packed = packed / 37;

    tmp = packed % 37;
    word[2] = alphanumeric.at(tmp);
    packed = packed / 37;

    tmp = packed % 37;
    word[1] = alphanumeric.at(tmp);
    packed = packed / 37;

    tmp = packed % 37;
    word[0] = alphanumeric.at(tmp);
    packed = packed / 37;

    return QString(word, 4);
}

// pack a callsign into a 28-bit value
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
    QRegularExpression m(pack_callsign_pattern);
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

    packed = alphanumeric.indexOf(matched.at(0));
    packed = 36*packed + alphanumeric.indexOf(matched.at(1));
    packed = 10*packed + alphanumeric.indexOf(matched.at(2));
    packed = 27*packed + alphanumeric.indexOf(matched.at(3)) - 10;
    packed = 27*packed + alphanumeric.indexOf(matched.at(4)) - 10;
    packed = 27*packed + alphanumeric.indexOf(matched.at(5)) - 10;

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
    word[5] = alphanumeric.at(tmp);
    value = value/27;

    tmp = value % 27 + 10;
    word[4] = alphanumeric.at(tmp);
    value = value/27;

    tmp = value % 27 + 10;
    word[3] = alphanumeric.at(tmp);
    value = value/27;

    tmp = value % 10;
    word[2] = alphanumeric.at(tmp);
    value = value/10;

    tmp = value % 36;
    word[1] = alphanumeric.at(tmp);
    value = value/36;

    tmp = value;
    word[0] = alphanumeric.at(tmp);

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

// pack a 4-digit maidenhead grid locator into a 15-bit value
quint16 Varicode::packGrid(QString const& value){
    QString grid = QString(value).trimmed();
    if(grid.length() < 4){      
        return (1<<15)-1;
    }

    auto pair = grid2deg(grid.left(4));
    int ilong = pair.first;
    int ilat = pair.second + 90;

    return ((ilong + 180)/2) * 180 + ilat;
}

QString Varicode::unpackGrid(quint16 value){
    if(value > nbasegrid){
        return "";
    }

    float dlat = value % 180 - 90;
    float dlong = value / 180 * 2 - 180 + 2;

    return deg2grid(dlong, dlat).left(4);
}

// pack a number or snr into an integer between 0 & 62
quint8 Varicode::packNum(QString const &num, bool *ok){
    int inum = 0;
    if(num.isEmpty()){
        if(ok) *ok = false;
        return inum;
    }

    inum = qMax(-30, qMin(num.toInt(ok, 10), 31));
    return inum + 30 + 1;
}

// pack pwr string into a dbm between 0 and 62
quint8 Varicode::packPwr(QString const &pwr, bool *ok){
    int ipwr = 0;
    if(pwr.isEmpty()){
        if(ok) *ok = false;
        return ipwr;
    }

    int factor = 1000;
    if(pwr.endsWith("KW")){
        factor = 1000000;
    }
    else if(pwr.endsWith("MW")){
        factor = 1;
    }

    ipwr = QString(pwr).replace(QRegExp("[KM]?W", Qt::CaseInsensitive), "").toInt() * factor;
    ipwr = mwattsToDbm(ipwr);

    if(ok) *ok = true;
    return ipwr + 1;
}

// pack a reduced fidelity command and a number into the extra bits provided between nbasegrid and nmaxgrid
quint8 Varicode::packCmd(quint8 cmd, quint8 num, bool *pPackedNum){
    //quint8 allowed = nmaxgrid - nbasegrid - 1;

    // if cmd == pwr || cmd == snr
    quint8 value = 0;
    if(cmd == directed_cmds[" PWR"] || cmd == directed_cmds[" SNR"]){
        // 8 bits - 1 bit flag + 1 bit type + 6 bit number
        // [1][X][6]
        // X = 0 == SNR
        // X = 1 == PWR

        value = ((1 << 1) + ((int)cmd == directed_cmds[" PWR"])) << 6;
        value = value + num;
        if(pPackedNum) *pPackedNum = true;
    } else {
        value = cmd & ((1<<7)-1);
        if(pPackedNum) *pPackedNum = false;
    }

    return value;
}

quint8 Varicode::unpackCmd(quint8 value, quint8 *pNum){
    // if the first bit is 1, the second bit will indicate if its pwr or snr...
    if(value & (1<<7)){
        // either pwr or snr...
        bool pwr = value & (1<<6);

        if(pNum) *pNum = value & ((1<<6)-1);

        if(pwr){
            return directed_cmds[" PWR"];
        } else {
            return directed_cmds[" SNR"];
        }
    } else {
        if(pNum) *pNum = 0;
        return value & ((1<<7)-1);
    }
}

bool Varicode::isCommandAllowed(const QString &cmd){
    return directed_cmds.contains(cmd) && allowed_cmds.contains(directed_cmds[cmd]);
}

bool Varicode::isCommandBuffered(const QString &cmd){
    return directed_cmds.contains(cmd) && buffered_cmds.contains(directed_cmds[cmd]);
}

// CQCQCQ EM73
// BEACON EM73
QString Varicode::packBeaconMessage(QString const &text, const QString &callsign, int *n){
    QString frame;

    auto parsedText = beacon_re.match(text);
    if(!parsedText.hasMatch()){
        if(n) *n = 0;
        return frame;
    }

    auto extra = parsedText.captured("grid");

    // Beacon Alt Type
    // ---------------
    // 1      0   BEACON
    // 1      1   CQCQCQ

    auto type = parsedText.captured("type");
    auto isAlt = type.startsWith("CQ");

    auto parsedCall = QRegularExpression(compound_callsign_pattern).match(callsign);
    if(!parsedCall.hasMatch()){
        if(n) *n = 0;
        return frame;
    }

    QString base = parsedCall.captured("base");

    bool isPrefix = false;
    QString fix = parsedCall.captured("prefix");
    if(!fix.isEmpty()){
        isPrefix = true;
    }

    if(!isPrefix){
        fix = parsedCall.captured("suffix");
    }

    quint16 packed_extra = nmaxgrid; // which will display an empty string
    if(extra.length() == 4 && QRegularExpression(grid_pattern).match(extra).hasMatch()){
        packed_extra = Varicode::packGrid(extra);
    }

    if(isAlt){
        packed_extra |= (1<<15);
    }


    frame = packCompoundFrame(base, fix, isPrefix, FrameBeacon, packed_extra);
    if(frame.isEmpty()){
        if(n) *n = 0;
        return frame;
    }

    if(n) *n = parsedText.captured(0).length();
    return frame;
}

QStringList Varicode::unpackBeaconMessage(const QString &text, quint8 *pType, bool * isAlt){
    quint8 type = FrameBeacon;
    quint16 num = nmaxgrid;

    QStringList unpacked = unpackCompoundFrame(text, &type, &num);
    if(unpacked.isEmpty() || type != FrameBeacon){
        return QStringList{};
    }

    unpacked.append(Varicode::unpackGrid(num & ((1<<15)-1)));

    if(isAlt) *isAlt = (num & (1<<15));
    if(pType) *pType = type;

    return unpacked;
}


// KN4CRD/XXXX EM73
// XXXX/KN4CRD EM73
// KN4CRD/P
QString Varicode::packCompoundMessage(QString const &text, int *n){
    QString frame;

    qDebug() << "trying to pack compound message" << text;
    auto parsedText = compound_re.match(text);
    if(!parsedText.hasMatch()){
        if(n) *n = 0;
        return frame;
    }

    qDebug() << parsedText.capturedTexts();

    QString callsign = parsedText.captured("callsign");
    QString grid = parsedText.captured("grid");
    QString cmd = parsedText.captured("cmd");
    QString num = parsedText.captured("num").trimmed();
    QString pwr = parsedText.captured("pwr").trimmed().toUpper();

    QString base;
    QString fix;
    bool isPrefix = false;

    if(basecalls.contains(callsign)){
        // if it's a basecall, use it verbatim with no prefix/suffix
        base = callsign;
        fix = "";
    } else {
        // otherwise, parse the callsign for prefix/suffix
        auto parsedCall = QRegularExpression(compound_callsign_pattern).match(callsign);
        if(!parsedCall.hasMatch()){
            if(n) *n = 0;
            return frame;
        }

        base = parsedCall.captured("base");
        fix = parsedCall.captured("prefix");
        if(!fix.isEmpty()){
            isPrefix = true;
        }
        if(!isPrefix){
            fix = parsedCall.captured("suffix");
        }
    }

    quint8 type = FrameCompound;
    quint16 extra = nmaxgrid;

    if (!cmd.isEmpty() && directed_cmds.contains(cmd) && Varicode::isCommandAllowed(cmd)){
        bool packedNum = false;
        qint8 inum = Varicode::packNum(num, nullptr);
        if(cmd == " PWR"){
            inum = Varicode::packPwr(pwr, nullptr);
        }
        extra = nusergrid + Varicode::packCmd(directed_cmds[cmd], inum, &packedNum);

        type = FrameCompoundDirected;
    } else if(!grid.isEmpty()){
        extra = Varicode::packGrid(grid);
    }

    frame = Varicode::packCompoundFrame(base, fix, isPrefix, type, extra);

    if(n) *n = parsedText.captured(0).length();
    return frame;
}

QStringList Varicode::unpackCompoundMessage(const QString &text, quint8 *pType){
    quint8 type = FrameCompound;
    quint16 extra = nmaxgrid;

    QStringList unpacked = unpackCompoundFrame(text, &type, &extra);
    if(unpacked.isEmpty() || (type != FrameCompound && type != FrameCompoundDirected)){
        return QStringList {};
    }

    if(extra <= nbasegrid){
        unpacked.append(" " + Varicode::unpackGrid(extra));
    } else if (nusergrid <= extra && extra < nmaxgrid) {
        quint8 num = 0;
        auto cmd = Varicode::unpackCmd(extra - nusergrid, &num);

        unpacked.append(directed_cmds.key(cmd));

        if(cmd == directed_cmds[" PWR"]){
            unpacked.append(Varicode::formatPWR(num - 1));
        } else if(cmd == directed_cmds[" SNR"]){
            unpacked.append(Varicode::formatSNR(num - 31));
        }
    }

    if(pType) *pType = type;

    return unpacked;
}

QString Varicode::packCompoundFrame(const QString &baseCallsign, const QString &fix, bool isPrefix, quint8 type, quint16 num){
    QString frame;

    // needs to be a compound type...
    if(type == FrameDataPadded || type == FrameDataUnpadded || type == FrameDirected){
        return frame;
    }

    quint8 packed_flag = type;
    quint32 packed_base = Varicode::packCallsign(baseCallsign);
    quint32 packed_fix = Varicode::packAlphaNumeric22(fix, isPrefix);

    if(packed_base == 0 || packed_fix == 0){
        return frame;
    }

    quint16 mask11 = ((1<<11)-1)<<5;
    quint8 mask5 = (1<<5)-1;

    quint16 packed_11 = (num & mask11) >> 5;
    quint8 packed_5 = num & mask5;

    // [3][28][22][11],[3][5] = 72
    auto bits = (
        Varicode::intToBits(packed_flag,  3) +
        Varicode::intToBits(packed_base, 28) +
        Varicode::intToBits(packed_fix,  22) +
        Varicode::intToBits(packed_11,   11)
    );

    return Varicode::pack72bits(Varicode::bitsToInt(bits), packed_5 % 32);
}

QStringList Varicode::unpackCompoundFrame(const QString &text, quint8 *pType, quint16 *pNum){
    QStringList unpacked;

    if(text.length() < 12 || text.contains(" ")){
        return unpacked;
    }

    // [3][28][22][11],[3][5] = 72
    quint8 packed_5 = 0;
    auto bits = Varicode::intToBits(Varicode::unpack72bits(text, &packed_5), 64);

    quint8 packed_flag = Varicode::bitsToInt(bits.mid(0, 3));

    // needs to be a beacon type...
    if(packed_flag == FrameDataPadded || packed_flag == FrameDataUnpadded || packed_flag == FrameDirected){
        return unpacked;
    }

    quint32 packed_base = Varicode::bitsToInt(bits.mid(3, 28));
    quint32 packed_fix = Varicode::bitsToInt(bits.mid(31, 22));
    quint16 packed_11 = Varicode::bitsToInt(bits.mid(53, 11));

    QString base = Varicode::unpackCallsign(packed_base).trimmed();

    bool isPrefix = false;
    QString fix = Varicode::unpackAlphaNumeric22(packed_fix, &isPrefix).trimmed();

    quint16 num = (packed_11 << 5) | packed_5;

    if(pType) *pType = packed_flag;
    if(pNum) *pNum = num;

    if(isPrefix){
        unpacked.append(fix);
    }

    unpacked.append(base);

    if(!isPrefix){
        unpacked.append(fix);
    }

    return unpacked;
}

// J1Y ACK
// J1Y?
// KN4CRD: J1Y$
// KN4CRD: J1Y! HELLO WORLD
QString Varicode::packDirectedMessage(const QString &text, const QString &callsign, QString *pTo, QString * pCmd, QString *pNum, int *n){
    QString frame;

    auto match = directed_re.match(text);
    if(!match.hasMatch()){
        if(n) *n = 0;
        return frame;
    }

    QString from = callsign;
    QString to = match.captured("callsign");
    QString cmd = match.captured("cmd");
    QString num = match.captured("num").trimmed();
    QString pwr = match.captured("pwr").trimmed().toUpper();

    // ensure we have a directed command
    if(cmd.isEmpty()){
        if(n) *n = 0;
        return frame;
    }

    // validate "to" callsign
    auto parsedTo = QRegularExpression(compound_callsign_pattern).match(to);
    bool validToCallsign = (to != callsign) && (basecalls.contains(to) || parsedTo.hasMatch());
    if(!validToCallsign){
        if(n) *n = 0;
        return frame;
    }

    if(basecalls.contains(to)){
        if(pTo) *pTo = to;
    } else if(parsedTo.hasMatch()){
        if(pTo) *pTo = parsedTo.captured(0);

        auto parsedBase = parsedTo.captured("base");
        if(parsedBase.length() != to.length()){
            to = "<....>"; // parsedBase;
        }
    }


    // validate command
    if(!Varicode::isCommandAllowed(cmd)){
        if(n) *n = 0;
        return frame;
    }

    // packing general number...
    quint8 inum = 0;

    if(cmd.trimmed() == "PWR"){
        inum = Varicode::packPwr(pwr, nullptr);
        if(pNum) *pNum = pwr;
    } else {
        inum = Varicode::packNum(num, nullptr);
        if(pNum) *pNum = num;
    }

    quint32 packed_from = Varicode::packCallsign(from);
    quint32 packed_to = Varicode::packCallsign(to);

    if(packed_from == 0 || packed_to == 0){
        if(n) *n = 0;
        return frame;
    }

    quint8 packed_cmd = directed_cmds[cmd];
    quint8 packed_flag = FrameDirected;
    quint8 packed_extra = inum;

    // [3][28][28][5],[2][6] = 72
    auto bits = (
        Varicode::intToBits(packed_flag,      3) +
        Varicode::intToBits(packed_from,     28) +
        Varicode::intToBits(packed_to,       28) +
        Varicode::intToBits(packed_cmd % 32,  5)
    );

    if(pCmd) *pCmd = cmd;
    if(n) *n = match.captured(0).length();
    return Varicode::pack72bits(Varicode::bitsToInt(bits), packed_extra % 64);
}

QStringList Varicode::unpackDirectedMessage(const QString &text, quint8 *pType){
    QStringList unpacked;

    if(text.length() < 12 || text.contains(" ")){
        return unpacked;
    }

    // [3][28][22][11],[2][6] = 72
    quint8 extra = 0;
    auto bits = Varicode::intToBits(Varicode::unpack72bits(text, &extra), 64);

    quint8 packed_flag = Varicode::bitsToInt(bits.mid(0, 3));
    if(packed_flag != FrameDirected){
        return unpacked;
    }

    quint32 packed_from = Varicode::bitsToInt(bits.mid(3, 28));
    quint32 packed_to = Varicode::bitsToInt(bits.mid(31, 28));
    quint8 packed_cmd = Varicode::bitsToInt(bits.mid(59, 5));

    QString from = Varicode::unpackCallsign(packed_from).trimmed();
    QString to = Varicode::unpackCallsign(packed_to).trimmed();
    QString cmd = directed_cmds.key(packed_cmd % 32);

    unpacked.append(from);
    unpacked.append(to);
    unpacked.append(cmd);

    if(extra != 0){
        // TODO: jsherer - should we decide which format to use on the command, or something else?
        if(packed_cmd == directed_cmds[" PWR"]){
            unpacked.append(Varicode::formatPWR(extra-1));
        } else if(packed_cmd == directed_cmds[" SNR"]) {
            unpacked.append(Varicode::formatSNR((int)extra-31));
        } else {
            unpacked.append(QString("%1").arg(extra-31));
        }
    }

    if(pType) *pType = packed_flag;
    return unpacked;
}

QString Varicode::packDataMessage(const QString &input, int *n){
    static const int frameSize = 72;

    QString frame;

    // [3][69] = 72
    QVector<bool> frameDataBits;

    QVector<bool> frameHeaderBits = Varicode::intToBits(FrameDataUnpadded, 3);

    int i = 0;

    // we use the escaped table here, so they the escapes and the characters are packed together...
    foreach(auto pair, Varicode::huffEncode(hufftableescaped, input)){
        auto charN = pair.first;
        auto charBits = pair.second;
        if(frameHeaderBits.length() + frameDataBits.length() + charBits.length() <= frameSize){
            frameDataBits += charBits;
            i += charN;
            continue;
        }
        break;
    }

    QVector<bool> framePadBits;

    int pad = frameSize - frameHeaderBits.length() - frameDataBits.length();
    if(pad){
        frameHeaderBits = Varicode::intToBits(FrameDataPadded, 3);

        // the way we will pad is this...
        // set the bit after the frame to 0 and every bit after that a 1
        // to unpad, seek from the end of the bits until you hit a zero... the rest is the actual frame.
        for(int i = 0; i < pad; i++){
            framePadBits.append(i == 0 ? (bool)0 : (bool)1);
        }
    }

    QVector<bool> allBits = frameHeaderBits + frameDataBits + framePadBits;

    quint64 value = Varicode::bitsToInt(allBits.constBegin(), 64);
    quint8 rem = (quint8)Varicode::bitsToInt(allBits.constBegin() + 64, 8);
    frame = Varicode::pack72bits(value, rem);

    *n = i;

    return frame;
}

QString Varicode::unpackDataMessage(const QString &text, quint8 *pType){
    QString unpacked;

    if(text.length() < 12 || text.contains(" ")){
        return unpacked;
    }

    quint8 rem = 0;
    quint64 value = Varicode::unpack72bits(text, &rem);
    auto bits = Varicode::intToBits(value, 64) + Varicode::intToBits(rem, 8);

    quint8 type = Varicode::bitsToInt(bits.mid(0, 3));
    if(type == FrameDataUnpadded){
        bits = bits.mid(3);
    } else if(type == FrameDataPadded) {
        int n = bits.lastIndexOf(0);
        bits = bits.mid(3, n-3);
    } else {
        return unpacked;
    }

    // huff decode the bits (without escapes)
    unpacked = Varicode::huffDecode(hufftable, bits);

    // then... unescape special characters
    unpacked = Varicode::huffUnescape(unpacked);

    if(pType) *pType = type;

    return unpacked;
}
