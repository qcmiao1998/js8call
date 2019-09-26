#ifndef TRANSMITTEXTEDIT_H
#define TRANSMITTEXTEDIT_H

#include "qt_helpers.hpp"

#include <QTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QBrush>
#include <QColor>
#include <QFont>

void setTextEditFont(QTextEdit *edit, QFont font);
void setTextEditStyle(QTextEdit *edit, QColor fg, QColor bg, QFont font);
void highlightBlock(QTextBlock block, QFont font, QColor foreground, QColor background);

class TransmitTextEdit : public QTextEdit
{
public:
    TransmitTextEdit(QWidget *parent);

    int charsSent() const {
        return m_sent;
    }
    void setCharsSent(int n);

    QString toPlainText() const;
    void setPlainText(const QString &text);
    void setFont(QFont f);
    void setFont(QFont f, QColor fg, QColor bg);
    void clear();

    bool isProtected() const {
        return m_protected;
    }
    void setProtected(bool protect);
    void highlightBase();
    void highlightCharsSent();
    void highlight();

public slots:
    void on_selectionChanged();
    void on_textContentsChanged(int pos, int rem, int add);

private:
    QString m_lastText;
    QString m_textSent;
    int m_sent;
    bool m_protected;
    QFont m_font;
    QColor m_fg;
    QColor m_bg;
};

#endif // TRANSMITTEXTEDIT_H
