#ifndef SELFDESTRUCTMESSAGEBOX_H
#define SELFDESTRUCTMESSAGEBOX_H

#include <QWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>

class SelfDestructMessageBox : public QMessageBox
{
    Q_OBJECT

public:
    SelfDestructMessageBox(int timeout,
                           const QString& title,
                           const QString& text,
                           QMessageBox::Icon icon,
                           QMessageBox::StandardButtons buttons = QMessageBox::Ok | QMessageBox::Cancel,
                           QMessageBox::StandardButton defaultButton = QMessageBox::Ok,
                           QWidget* parent = nullptr,
                           Qt::WindowFlags flags = 0);

    void showEvent(QShowEvent* event) override;

private slots:
    void tick();

private:
    int m_timeout;
    QString m_text;
    QTimer m_timer;
};

#endif // SELFDESTRUCTMESSAGEBOX_H
