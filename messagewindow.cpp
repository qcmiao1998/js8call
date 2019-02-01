#include "messagewindow.h"
#include "ui_messagewindow.h"

MessageWindow::MessageWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessageWindow)
{
    ui->setupUi(this);

    this->setWindowTitle("Message History");

    ui->messageTableWidget->resizeColumnsToContents();
}

MessageWindow::~MessageWindow()
{
    delete ui;
}
