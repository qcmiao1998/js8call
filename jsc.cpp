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


Codeword codeword(quint32 index, bool separate, quint32 bytesize, quint32 s, quint32 c){
    QList<Codeword> out;

    quint32 v = ((index % s) << 1) + (int)separate;
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

QPair<CompressionMap, CompressionList> JSC::loadCompressionTable(){
    CompressionMap out;
    CompressionList words;

    for(int i = 0; i < JSC::size; i++){
        out[JSC::map[i]] = i;
    }

    words.reserve(JSC::size);
    for(int i = 0; i < JSC::size; i++){
        words.append(JSC::list[i]);
    }

    return {out, words};
}

QPair<CompressionMap, CompressionList> JSC::loadCompressionTable(QTextStream &stream){
    CompressionMap out;
    CompressionList words;

    int index = 0;

    while(!stream.atEnd()){
        // assume that this is in sorted order, that each word is already upper case.
        auto word = stream.readLine().trimmed();
        if(out.contains(word)){
            continue;
        }
        out[word] = index;
        words.append(word);
        index++;
    }

#if 1
    qStableSort(words.begin(), words.end(), [out](QString const &left, QString const &right){
        return out[left] < out[right];
    });
    qStableSort(words.begin(), words.end(), [](QString const &left, QString const &right){
        if(left.length() == right.length()){
            return left < right;
        }
        return left.length() < right.length();
    });
#endif

    return {out, words};
}

QList<CodewordPair> JSC::compress(CompressionTable table, QString text){
    QList<CodewordPair> out;

    const quint32 b = 4;
    const quint32 s = 7;
    const quint32 c = pow(2, 4) - s;

    auto map = table.first;
    auto list = table.second;

    foreach(QString w, text.split(" ", QString::SkipEmptyParts)){
        if(map.contains(w)){
            auto index = map[w];
            out.append({ codeword(index, true, b, s, c), w.length() });
        } else {
            // prefix match?
            while(!w.isEmpty()){
                bool hasPrefix = false;

                foreach(QString word, list){
                    if(w.startsWith(word)){
                        w = QString(w.mid(word.length()));
                        out.append({ codeword(map[word], w.isEmpty(), b, s, c), word.length()});
                        hasPrefix = true;
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

QString JSC::decompress(CompressionTable table, Codeword const& bitvec){
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

        out.append(table.first.key(j));
        if(separators.first() == start + k){
            out.append(" ");
            separators.removeFirst();
        }

        start = start + (k + 1);
    }

    return out.join("");
}
