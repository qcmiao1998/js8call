/**
 * This file is part of JS8Call.
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

#include <QDebug>
#include <QCache>

QMap<QString, quint32> LOOKUP_CACHE;

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

    QString space(" ");

    foreach(QString w, text.split(" ", QString::KeepEmptyParts)){
        bool ok = false;

        bool isSpaceCharacter = false;

        // if this is an empty part, it should be a space.
        if(w.isEmpty()){
            w = space;
            isSpaceCharacter = true;
        }

        while(!w.isEmpty()){
            // this does both prefix and full match lookup
            auto index = lookup(w, &ok);
            if(!ok){
                break;
            }

            auto t = JSC::map[index];
            w = QString(w).mid(t.size);

            bool isLast = w.isEmpty();
            bool shouldAppendSpace = isLast && !isSpaceCharacter;
            out.append({ codeword(index, shouldAppendSpace, b, s, c), (quint32)t.size + (shouldAppendSpace ? 1 : 0) /* for the space that follows */});
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

        auto word = QString(JSC::map[j].str);

        out.append(word);
        if(!separators.isEmpty() && separators.first() == start + k){
            out.append(" ");
            separators.removeFirst();
        }

        start = start + (k + 1);
    }

    return out.join("");
}

quint32 JSC::lookup(QString w, bool * ok){
    if(LOOKUP_CACHE.contains(w)){
        if(ok) *ok = true;
        return LOOKUP_CACHE[w];
    }

    bool found = false;
    quint32 result = lookup(w.toLatin1().data(), &found);
    if(found){
        LOOKUP_CACHE[w] = result;
    }

    if(ok) *ok = found;
    return result;
}

quint32 JSC::lookup(char const* b, bool *ok){
    quint32 index = 0;
    quint32 count = 0;
    bool found = false;

    // first find prefix match to jump into the list faster
    for(quint32 i = 0; i < JSC::prefixSize; i++){
        // skip obvious non-prefixes...
        if(b[0] != JSC::prefix[i].str[0]){
            continue;
        }

        // ok, we found one... let's end early for single char strings.
        if(JSC::prefix[i].size == 1){
            if(ok) *ok = true;
            return JSC::list[JSC::prefix[i].index].index;
        }

        // otherwise, keep track of the first index in the list and the number of elements
        index = JSC::prefix[i].index;
        count = JSC::prefix[i].size;
        found = true;
        break;
    }

    // no prefix found... no lookup
    if(!found){
        if(ok) *ok = false;
        return 0;
    }

    // now that we have the first index in the list, let's just iterate through the list, comparing words along the way
    for(quint32 i = index; i < index + count; i++){
        quint32 len = JSC::list[i].size;
        if(strncmp(b, JSC::list[i].str, len) == 0){
            if(ok) *ok = true;
            return JSC::list[i].index;
        }
    }

    if(ok) *ok = false;
    return 0;
}
