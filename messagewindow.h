#ifndef MESSAGEWINDOW_H
#define MESSAGEWINDOW_H

#include <QWidget>

namespace Ui {
class MessageWindow;
}

class MessageWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MessageWindow(QWidget *parent = 0);
    ~MessageWindow();

private:
    Ui::MessageWindow *ui;
};

#endif // MESSAGEWINDOW_H
