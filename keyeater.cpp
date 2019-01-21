#include "keyeater.h"

bool EscapeKeyPressEater::eventFilter(QObject *obj, QEvent *event){
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if(keyEvent->key() == Qt::Key_Escape){
            return true;
        }
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}

bool EnterKeyPressEater::eventFilter(QObject *obj, QEvent *event){
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if(keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return){
            bool processed = false;
            emit this->enterKeyPressed(obj, keyEvent, &processed);
            if(processed){
                return true;
            }
        }
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}

bool MousePressEater::eventFilter(QObject *obj, QEvent *event){
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        bool processed = false;
        emit this->mousePressed(obj, mouseEvent, &processed);
        if(processed){
            return true;
        }
    }

    // standard event processing
    return QObject::eventFilter(obj, event);
}

