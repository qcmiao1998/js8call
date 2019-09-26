#include "TransmitTextEdit.h"

#include <QDebug>

void setTextEditFont(QTextEdit *edit, QFont font){
  // all uppercase
  font.setCapitalization(QFont::AllUppercase);

  edit->setFont(font);
  edit->setFontFamily(font.family());
  edit->setFontItalic(font.italic());
  edit->setFontPointSize(font.pointSize());
  edit->setFontUnderline(font.underline());
  edit->setFontWeight(font.weight());

  auto d = edit->document();
  d->setDefaultFont(font);
  edit->setDocument(d);

  auto c = edit->textCursor();
  c.select(QTextCursor::Document);
  auto cf = c.blockCharFormat();
  cf.setFont(font);
  cf.setFontCapitalization(QFont::AllUppercase);
  c.mergeBlockCharFormat(cf);

  edit->updateGeometry();
}

void setTextEditStyle(QTextEdit *edit, QColor fg, QColor bg, QFont font){
  edit->setStyleSheet(QString("QTextEdit { color:%1; background: %2; %3; }").arg(fg.name()).arg(bg.name()).arg(font_as_stylesheet(font)));

  //QTimer::singleShot(10, nullptr, [edit, fg, bg](){
        QPalette p = edit->palette();
        p.setColor(QPalette::Base, bg);
        p.setColor(QPalette::Active, QPalette::Base, bg);
        p.setColor(QPalette::Disabled, QPalette::Base, bg);
        p.setColor(QPalette::Inactive, QPalette::Base, bg);

        p.setColor(QPalette::Text, fg);
        p.setColor(QPalette::Active, QPalette::Text, fg);
        p.setColor(QPalette::Disabled, QPalette::Text, fg);
        p.setColor(QPalette::Inactive, QPalette::Text, fg);

        edit->setBackgroundRole(QPalette::Base);
        edit->setForegroundRole(QPalette::Text);
        edit->setPalette(p);

        edit->updateGeometry();
        edit->update();
    //});
}

void highlightBlock(QTextBlock block, QFont font, QColor foreground, QColor background){
  QTextCursor cursor(block);

  // Set background color
  QTextBlockFormat blockFormat = cursor.blockFormat();
  blockFormat.setBackground(background);
  cursor.setBlockFormat(blockFormat);

  // Set font
  /*
  for (QTextBlock::iterator it = cursor.block().begin(); !(it.atEnd()); ++it) {
    QTextCharFormat charFormat = it.fragment().charFormat();
    charFormat.setFont(font);
    charFormat.setFontCapitalization(QFont::AllUppercase);
    charFormat.setForeground(QBrush(foreground));

    QTextCursor tempCursor = cursor;
    tempCursor.setPosition(it.fragment().position());
    tempCursor.setPosition(it.fragment().position() + it.fragment().length(), QTextCursor::KeepAnchor);
    tempCursor.setCharFormat(charFormat);
  }
  */
  cursor.select(QTextCursor::BlockUnderCursor);

  auto charFormat = cursor.charFormat();
  charFormat.setFont(font);
  charFormat.setFontCapitalization(QFont::AllUppercase);
  charFormat.setForeground(QBrush(foreground));
  cursor.setCharFormat(charFormat);
}


TransmitTextEdit::TransmitTextEdit(QWidget *parent):
    QTextEdit(parent),
    m_sent { 0 },
    m_textSent { "" },
    m_protected { false }
{
    connect(this, &QTextEdit::selectionChanged, this, &TransmitTextEdit::on_selectionChanged);
    connect(this, &QTextEdit::cursorPositionChanged, this, &TransmitTextEdit::on_selectionChanged);
    connect(this->document(), &QTextDocument::contentsChange, this, &TransmitTextEdit::on_textContentsChanged);
    installEventFilter(this);
}

void TransmitTextEdit::setCharsSent(int n){
    // update sent display
    auto c = textCursor();
    c.movePosition(QTextCursor::Start);
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, n);

    // keep track of sent text
    m_textSent = c.selectedText().toUpper();
    m_sent = n;

    // highlight the sent text
    //highlightCharsSent();
    highlight();
}

// override
QString TransmitTextEdit::toPlainText() const {
    return QTextEdit::toPlainText().toUpper();
}

// override
void TransmitTextEdit::setPlainText(const QString &text){
    QTextEdit::setPlainText(text);
    m_textSent.clear();
    m_sent = 0;
}

//
void TransmitTextEdit::setFont(QFont f){
    m_font = f;

    // then rehighlight
    highlight();
}

//
void TransmitTextEdit::setFont(QFont f, QColor fg, QColor bg){
    m_font = f;
    m_fg = fg;
    m_bg = bg;

    // then rehighlight
    highlight();
}

// override
void TransmitTextEdit::clear(){
    QTextEdit::clear();
    m_textSent.clear();
    m_sent = 0;
}

void TransmitTextEdit::setProtected(bool protect){
    m_protected = protect;
}

bool TransmitTextEdit::cursorShouldBeProtected(QTextCursor c){
    int start = c.selectionStart();
    int end = c.selectionEnd();
    if(end < start){
        int x = end;
        end = start;
        start = x;
    }

    qDebug() << "selection" << start << end << m_sent;

    if(m_sent && start <= m_sent){
        qDebug() << "selection in protected zone" << start << "<=" << m_sent;
        return true;
    } else {
        return false;
    }
}

// slot
void TransmitTextEdit::on_selectionChanged(){
    auto c = textCursor();

    auto protect = cursorShouldBeProtected(c);

    setProtected(protect);

    // TODO: when protected and text is selected, remove protected region from selection
}

// slot
void TransmitTextEdit::on_textContentsChanged(int pos, int rem, int add){
    if(rem == 0 && add == 0){
        return;
    }

    auto text = toPlainText();
    if(text != m_lastText){
        //qDebug() << "text changed" << pos << rem << add << "from" << m_lastText << "to" << text;

        highlight();

        qDebug() << "sent:" << sentText();
        qDebug() << "unsent:" << unsentText();

        m_lastText = text;
    }
}

void TransmitTextEdit::highlightBase(){
    auto d = document();
    if(!d){
        return;
    }

    auto block = d->firstBlock();
    while(block.isValid()){
        highlightBlock(block, m_font, m_fg, m_bg);
        block = block.next();
    }
}

void TransmitTextEdit::highlightCharsSent(){
    if(!m_sent){
        return;
    }

    auto d = document();
    if(!d){
        return;
    }

    // highlight sent text
    auto c = textCursor();
    if(c.isNull()){
        return;
    }
    c.movePosition(QTextCursor::Start);
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, m_sent);

    auto ch = c.charFormat();
    ch.setFontStrikeOut(true);
    c.mergeCharFormat(ch);
}

void TransmitTextEdit::highlight(){
    highlightBase();
    highlightCharsSent();
}

bool isMovementKeyEvent(QKeyEvent * k){
    return (
       k == QKeySequence::MoveToNextChar        ||
       k == QKeySequence::MoveToPreviousChar    ||
       k == QKeySequence::SelectNextChar        ||
       k == QKeySequence::SelectPreviousChar    ||
       k == QKeySequence::SelectNextWord        ||
       k == QKeySequence::SelectPreviousWord    ||
       k == QKeySequence::SelectStartOfLine     ||
       k == QKeySequence::SelectEndOfLine       ||
       k == QKeySequence::SelectStartOfBlock    ||
       k == QKeySequence::SelectEndOfBlock      ||
       k == QKeySequence::SelectStartOfDocument ||
       k == QKeySequence::SelectEndOfDocument   ||
       k == QKeySequence::SelectPreviousLine    ||
       k == QKeySequence::SelectNextLine        ||
       k == QKeySequence::MoveToNextWord        ||
       k == QKeySequence::MoveToPreviousWord    ||
       k == QKeySequence::MoveToEndOfBlock      ||
       k == QKeySequence::MoveToStartOfBlock    ||
       k == QKeySequence::MoveToNextLine        ||
       k == QKeySequence::MoveToPreviousLine    ||
       k == QKeySequence::MoveToPreviousLine    ||
       k == QKeySequence::MoveToStartOfLine     ||
       k == QKeySequence::MoveToEndOfLine       ||
       k == QKeySequence::MoveToStartOfDocument ||
       k == QKeySequence::MoveToEndOfDocument
    );
}


QTextCursor::MoveOperation movementKeyEventToMoveOperation(QKeyEvent *e){
    QTextCursor::MoveOperation op = QTextCursor::NoMove;

    if (e == QKeySequence::MoveToNextChar) {
            op = QTextCursor::Right;
    }
    else if (e == QKeySequence::MoveToPreviousChar) {
            op = QTextCursor::Left;
    }
    else if (e == QKeySequence::SelectNextChar) {
            op = QTextCursor::Right;
    }
    else if (e == QKeySequence::SelectPreviousChar) {
            op = QTextCursor::Left;
    }
    else if (e == QKeySequence::SelectNextWord) {
            op = QTextCursor::WordRight;
    }
    else if (e == QKeySequence::SelectPreviousWord) {
            op = QTextCursor::WordLeft;
    }
    else if (e == QKeySequence::SelectStartOfLine) {
            op = QTextCursor::StartOfLine;
    }
    else if (e == QKeySequence::SelectEndOfLine) {
            op = QTextCursor::EndOfLine;
    }
    else if (e == QKeySequence::SelectStartOfBlock) {
            op = QTextCursor::StartOfBlock;
    }
    else if (e == QKeySequence::SelectEndOfBlock) {
            op = QTextCursor::EndOfBlock;
    }
    else if (e == QKeySequence::SelectStartOfDocument) {
            op = QTextCursor::Start;
    }
    else if (e == QKeySequence::SelectEndOfDocument) {
            op = QTextCursor::End;
    }
    else if (e == QKeySequence::SelectPreviousLine) {
            op = QTextCursor::Up;
    }
    else if (e == QKeySequence::SelectNextLine) {
            op = QTextCursor::Down;
    }
    else if (e == QKeySequence::MoveToNextWord) {
            op = QTextCursor::WordRight;
    }
    else if (e == QKeySequence::MoveToPreviousWord) {
            op = QTextCursor::WordLeft;
    }
    else if (e == QKeySequence::MoveToEndOfBlock) {
            op = QTextCursor::EndOfBlock;
    }
    else if (e == QKeySequence::MoveToStartOfBlock) {
            op = QTextCursor::StartOfBlock;
    }
    else if (e == QKeySequence::MoveToNextLine) {
            op = QTextCursor::Down;
    }
    else if (e == QKeySequence::MoveToPreviousLine) {
            op = QTextCursor::Up;
    }
    else if (e == QKeySequence::MoveToPreviousLine) {
            op = QTextCursor::Up;
    }
    else if (e == QKeySequence::MoveToStartOfLine) {
            op = QTextCursor::StartOfLine;
    }
    else if (e == QKeySequence::MoveToEndOfLine) {
            op = QTextCursor::EndOfLine;
    }
    else if (e == QKeySequence::MoveToStartOfDocument) {
            op = QTextCursor::Start;
    }
    else if (e == QKeySequence::MoveToEndOfDocument) {
            op = QTextCursor::End;
    }

    return op;
}


bool TransmitTextEdit::eventFilter(QObject */*o*/, QEvent *e){
    if(e->type() != QEvent::KeyPress){
        return false;
    }

    QKeyEvent *k = static_cast<QKeyEvent *>(e);

    auto c = textCursor();

    auto c2 = QTextCursor(c);
    c2.movePosition(movementKeyEventToMoveOperation(k));

    bool shouldBeProtected = cursorShouldBeProtected(c2);

    // 0. only filter when in a protected range
    // 0a. but only if we're not moving/deleting _into_ the protected range :/
    if(!isProtected() && !shouldBeProtected){
        return false;
    }

    // 1. do not filter movement sequences
    if(isMovementKeyEvent(k)){
        return false;
    }

    // 2. if on the edge, do not filter if not a backspace
    int start = qMin(c.selectionStart(), c.selectionEnd());
    if(start == m_sent && k->key() != Qt::Key_Backspace){
        return false;
    }

    return true;
}

