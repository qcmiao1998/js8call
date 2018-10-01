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

#include "jsc.h"
#include "varicode.h"

#include <cmath>


Codeword JSC::codeword(quint32 index, bool separate, quint32 bytesize, quint32 s, quint32 c){
    QList<Codeword> out;

    quint32 v = ((index % s) << 1) + (quint32)separate;
    out.prepend(Varicode::intToBits(v, bytesize + 1));

    quint32 x = index / s;
    while(x > 0){
        x -= 1;
        out.prepend(Varicode::intToBits((x % c) + s, bytesize));
        x /= c;
    }

    Codeword word;
    foreach(auto w, out){
        word.append(w);
    }

    return word;
}

QList<CodewordPair> JSC::compress(QString text){
    QList<CodewordPair> out;

    const quint32 b = 4;
    const quint32 s = 7;
    const quint32 c = pow(2, 4) - s;

    foreach(QString w, text.split(" ", QString::SkipEmptyParts)){
        bool ok = false;
        auto index = lookup(w, &ok);
        if(ok){
            // cool, we found the word...
            out.append({ codeword(index, true, b, s, c), (quint32)w.length() + 1 /* for the space that follows */ });
        } else {
            // hmm. no dice. let's go for a prefix match
            while(!w.isEmpty()){
                bool hasPrefix = false;
                auto d = w.toLatin1().data();
                for(quint32 i = 0; i < JSC::size; i++){

                    // TODO: we could probably precompute these sizes
                    quint32 len = strlen(JSC::list[i]);

                    if(strncmp(d, JSC::list[i], len) == 0){
                        w = QString(w.mid(len));

                        auto word = JSC::list[i];
                        auto index = lookup(word, &ok);
                        if(ok){
                            bool isLast = w.isEmpty();
                            out.append({ codeword(index, isLast, b, s, c), len + (isLast ? 1 : 0) /* for the space that follows */});
                            hasPrefix = true;
                            break;
                        }

                        break;
                    }
                }

                if(!hasPrefix){
                    // no match...SOL
                    break;
                }
            }
        }
    }

    return out;
}

QString JSC::decompress(Codeword const& bitvec){
    const quint32 b = 4;
    const quint32 s = 7;
    const quint32 c = pow(2, b) - s;

    QStringList out;

    quint32 base[8];
    base[0] = 0;
    base[1] = s;
    base[2] = base[1] + s*c;
    base[3] = base[2] + s*c*c;
    base[4] = base[3] + s*c*c*c;
    base[5] = base[4] + s*c*c*c*c;
    base[6] = base[5] + s*c*c*c*c*c;
    base[7] = base[6] + s*c*c*c*c*c*c;

    QList<quint64> bytes;
    QList<int> separators;
    auto iter = bitvec.begin();
    while(iter != bitvec.end()){
        quint64 byte = Varicode::bitsToInt(iter, 4);
        iter += 4;
        bytes.append(byte);

        if(byte < s){
            if(*iter){
                separators.append(bytes.length()-1);
            }
            iter += 1;
        }
    }

    int start = 0;
    while(start < bytes.length()){
        int k = 0;
        int j = 0;

        while(start + k < bytes.length() && bytes[start + k] >= s){
            j = j*c + (bytes[start + k] - s);
            k++;
        }

        j = j*s + bytes[start + k] + base[k];

        out.append(QString(JSC::map[j])); // table.first.key(j));
        if(separators.first() == start + k){
            out.append(" ");
            separators.removeFirst();
        }

        start = start + (k + 1);
    }

    return out.join("");
}

quint32 JSC::lookup(QString w, bool * ok){
    return lookup(w.toLatin1().data(), ok);
}

quint32 JSC::lookup(char const* b, bool *ok){
    for(quint32 i = 0; i < JSC::size; i++){
        if(strcmp(b, JSC::map[i]) == 0){
            if(ok) *ok = true;
            return i;
        }
    }

    if(ok) *ok = false;
    return 0;
}
