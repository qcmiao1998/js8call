#include "extendedqso.h"
#include "ui_extendedqso.h"

ExtendedQSO::ExtendedQSO(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ExtendedQSO)
{
    ui->setupUi(this);
}

ExtendedQSO::~ExtendedQSO()
{
    delete ui;
}
