#ifndef DECODER_H
#define DECODER_H

/**
 * (C) 2019 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include "ProcessThread.h"

#include <QDebug>
#include <QByteArray>
#include <QPointer>
#include <QProcess>


class Worker : public QObject{
    Q_OBJECT
public:
    ~Worker();
public slots:
    void start(QString path, QStringList args);
    void quit();

private:
    void setProcess(QProcess *proc, int msecs=1000);

signals:
    void ready(QByteArray t);
    void error();
    void finished();

private:
    QScopedPointer<QProcess> m_proc;
};


class Decoder: public QObject
{
    Q_OBJECT
public:
    Decoder(QObject *parent=nullptr);
    ~Decoder();

    void lock();
    void unlock();

private:
    Worker* createWorker();

public slots:
    void start(QThread::Priority priority);
    void quit();
    bool wait();

    void processStart(QString path, QStringList args);
    void processReady(QByteArray t);
    void processQuit();

signals:
    void startWorker(QString path, QStringList args);
    void quitWorker();

    void ready(QByteArray t);

private:
    QPointer<Worker> m_worker;
    QThread m_thread;
};


#endif // DECODER_H
