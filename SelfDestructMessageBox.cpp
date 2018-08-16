#include "SelfDestructMessageBox.h"

SelfDestructMessageBox::SelfDestructMessageBox(
        int timeout,
        const QString& title,
        const QString& text,
        QMessageBox::Icon icon,
        QMessageBox::StandardButtons buttons,
        QMessageBox::StandardButton defaultButton,
        QWidget* parent,
        Qt::WindowFlags flags)
  : QMessageBox(icon, title, text, buttons, parent, flags),
    m_timeout(timeout),
    m_text(text)
{
    connect(&m_timer, &QTimer::timeout, this, &SelfDestructMessageBox::tick);
    m_timer.setInterval(1000);

    setDefaultButton(defaultButton);
    connect(this->defaultButton(), &QPushButton::clicked, this, &SelfDestructMessageBox::accept);
}

void SelfDestructMessageBox::showEvent(QShowEvent* event)
{
    tick();
    m_timer.start();
    QMessageBox::showEvent(event);
}

void SelfDestructMessageBox::tick(){
    m_timeout--;

    if(m_timeout){
        setText(m_text.arg(m_timeout));
        return;
    }

    m_timer.stop();
    accept();
}
