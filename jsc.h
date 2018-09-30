#ifndef JSC_H
#define JSC_H

/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include <QTextStream>
#include <QList>
#include <QMap>
#include <QPair>
#include <QVector>

typedef QMap<QString, int> CompressionMap;             // Map(Word, I) where I is the word index sorted by frequency
typedef QList<QString> CompressionList;                // List(Word) where list is sorted
typedef QPair<CompressionMap, CompressionList> CompressionTable; // Tuple(Map, List)
typedef QPair<QVector<bool>, int> CodewordPair;        // Tuple(Codeword, N) where N = number of characters
typedef QVector<bool> Codeword;

class JSC
{
public:
    static CompressionTable loadCompressionTable(QTextStream &stream);
    static QList<CodewordPair> compress(CompressionTable table, QString text);
    static QString decompress(CompressionTable table, Codeword const& bits);
};

#endif // JSC_H
