#ifndef EXTENDEDQSO_H
#define EXTENDEDQSO_H

#include <QWidget>

namespace Ui {
class ExtendedQSO;
}

class ExtendedQSO : public QWidget
{
    Q_OBJECT

public:
    explicit ExtendedQSO(QWidget *parent = 0);
    ~ExtendedQSO();

private:
    Ui::ExtendedQSO *ui;
};

#endif // EXTENDEDQSO_H
