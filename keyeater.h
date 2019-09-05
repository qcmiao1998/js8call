#ifndef KEYEATER_H
#define KEYEATER_H

#include <QObject>
#include <QKeyEvent>
#include <QMouseEvent>

class KeyPressEater : public QObject
{
    Q_OBJECT
public:
    KeyPressEater(){}
    virtual ~KeyPressEater(){}

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    Q_SIGNAL void keyPressed(QObject *obj, QKeyEvent *evt, bool *pProcessed);
};

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

class MousePressEater : public QObject
{
    Q_OBJECT
public:
    MousePressEater(){}
    virtual ~MousePressEater(){}

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    Q_SIGNAL void mousePressed(QObject *obj, QMouseEvent *evt, bool *pProcessed);
};

class MouseDoubleClickEater : public QObject
{
    Q_OBJECT
public:
    MouseDoubleClickEater(){}
    virtual ~MouseDoubleClickEater(){}

protected:
    bool eventFilter(QObject *obj, QEvent *event);

public:
    Q_SIGNAL void mouseDoubleClicked(QObject *obj, QMouseEvent *evt, bool *pProcessed);
};



#endif // KEYEATER_H
