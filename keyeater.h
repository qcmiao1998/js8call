#ifndef KEYEATER_H
#define KEYEATER_H

#include <QObject>
#include <QKeyEvent>

class EscapeKeyPressEater : public QObject
{
    Q_OBJECT
public:
    EscapeKeyPressEater(){}
    virtual ~EscapeKeyPressEater(){}

protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

class EnterKeyPressEater : public QObject
{
    Q_OBJECT
public:
    EnterKeyPressEater(){}
    virtual ~EnterKeyPressEater(){}

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    Q_SIGNAL void enterKeyPressed(QObject *obj, QKeyEvent *evt, bool *pProcessed);
};


#endif // KEYEATER_H
