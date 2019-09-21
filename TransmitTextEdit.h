#ifndef TRANSMITTEXTEDIT_H
#define TRANSMITTEXTEDIT_H

#include <QTextEdit>

class TransmitTextEdit : public QTextEdit
{
public:
    TransmitTextEdit(QWidget *parent);

    void markCharsSent(int n);
    void setPlainText(const QString &text);
    void clear();

public slots:
    void on_selectionChanged();

private:
    int m_sent;
};

#endif // TRANSMITTEXTEDIT_H
