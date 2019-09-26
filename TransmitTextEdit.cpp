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

// slot
void TransmitTextEdit::on_selectionChanged(){
    auto c = textCursor();
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
        setProtected(true);
    } else {
        setProtected(false);
    }
}

// slot
void TransmitTextEdit::on_textContentsChanged(int pos, int rem, int add){
    if(rem == 0 && add == 0){
        return;
    }

    auto text = toPlainText();
    if(text != m_lastText){
        qDebug() << "text changed" << pos << rem << add << "from" << m_lastText << "to" << text;
        highlight();
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

