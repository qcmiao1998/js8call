#include "TransmitTextEdit.h"

#include <QDebug>

TransmitTextEdit::TransmitTextEdit(QWidget *parent):
    QTextEdit(parent)
{
    connect(this, &QTextEdit::selectionChanged, this, &TransmitTextEdit::on_selectionChanged);
    connect(this, &QTextEdit::cursorPositionChanged, this, &TransmitTextEdit::on_selectionChanged);
}

void TransmitTextEdit::markCharsSent(int n){
    // update sent display
    auto c = textCursor();
    c.movePosition(QTextCursor::Start);
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, n);

    auto ch = c.charFormat();
    ch.setFontStrikeOut(true);
    c.mergeCharFormat(ch);

    // keep track
    m_sent = n;
}

// override
void TransmitTextEdit::setPlainText(const QString &text){
    QTextEdit::setPlainText(text);
    m_sent = 0;
}

// override
void TransmitTextEdit::clear(){
    QTextEdit::clear();
    m_sent = 0;
}

// slot
void TransmitTextEdit::on_selectionChanged(){
    auto c = textCursor();
    int start = c.selectionStart();
    int end = c.selectionEnd();
    if(end < start){
        int x = end;
        end = start;
        start = x;
    }
    qDebug() << "selection" << start << end;

    if(start <= m_sent){
        qDebug() << "selection in protected zone" << start << "<=" << m_sent;
    }
}
