#include "logqso.h"

#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QUdpSocket>

#include "logbook/adif.h"
#include "MessageBox.hpp"
#include "Configuration.hpp"
#include "Bands.hpp"
#include "MaidenheadLocatorValidator.hpp"
#include "DriftingDateTime.h"

#include "ui_logqso.h"
#include "moc_logqso.cpp"

LogQSO::LogQSO(QString const& programTitle, QSettings * settings
               , Configuration const * config, QWidget *parent)
  : QDialog {parent, Qt::WindowStaysOnTopHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint}
  , ui(new Ui::LogQSO)
  , m_settings (settings)
  , m_config {config}
{
  ui->setupUi(this);
  setWindowTitle(programTitle + " - Log QSO");
  ui->grid->setValidator (new MaidenheadLocatorValidator {this});
}

LogQSO::~LogQSO ()
{
}

bool LogQSO::acceptText(QString text){
    auto w = focusWidget();
    if(!w){
        return false;
    }

    auto name = QString(w->metaObject()->className());
    if(name != "QLineEdit"){
        return false;
    }

    auto l = static_cast<QLineEdit*>(w);
    if(l->text().isEmpty()){
        // set
        l->setText(text);
    } else {
        // append
        l->setText(QString("%1 %2").arg(l->text()).arg(text));
    }

    return true;
}

void LogQSO::on_start_now_button_pressed(){
  ui->start_date_time->setDateTime(DriftingDateTime::currentDateTimeUtc());
}

void LogQSO::on_end_now_button_pressed(){
  ui->end_date_time->setDateTime(DriftingDateTime::currentDateTimeUtc());
}

void LogQSO::on_add_new_field_button_pressed(){
    createAdditionalField();
}

void LogQSO::createAdditionalField(QString key, QString value){
    QLineEdit * l = new QLineEdit(this);
    if(!value.isEmpty()){
        l->setText(value);
    }

    QComboBox * c = new QComboBox(this);
    c->insertItem(0, "");
    c->insertItems(0, ADIF_FIELDS);
    c->setEditable(false);
    connect(c, &QComboBox::currentTextChanged, this, [this, l](const QString &text){
       l->setProperty("fieldKey", QVariant(text));
    });
    if(!key.isEmpty()){
        c->setCurrentText(key);
    }

    auto layout = static_cast<QFormLayout*>(ui->additionalFields->layout());
    layout->removeItem(ui->field_button_layout);
    layout->addRow(c, l);
    layout->addItem(ui->field_button_layout);
    m_additionalFieldsControls.append(l);
}

void LogQSO::resetAdditionalFields(){
    if(m_additionalFieldsControls.isEmpty()){
        return;
    }

    auto layout = static_cast<QFormLayout*>(ui->additionalFields->layout());
    layout->removeItem(ui->field_button_layout);
    for(int i = 0, count = layout->rowCount(); i < count; i++){
        layout->removeRow(0);
    }
    layout->addItem(ui->field_button_layout);
    m_additionalFieldsControls.clear();
}

void LogQSO::loadSettings ()
{
  m_settings->beginGroup ("LogQSO");
  restoreGeometry (m_settings->value ("geometry", saveGeometry ()).toByteArray ());
  ui->cbComments->setChecked (m_settings->value ("SaveComments", false).toBool ());
  m_comments = m_settings->value ("LogComments", "").toString();

  resetAdditionalFields();
  auto additionalFields = QSet<QString>::fromList(m_settings->value("AdditionalFields", {}).toStringList());
  foreach(auto key, additionalFields){
      createAdditionalField(key);
  }

  m_settings->endGroup ();
}

void LogQSO::storeSettings () const
{
  m_settings->beginGroup ("LogQSO");
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("SaveComments", ui->cbComments->isChecked ());
  m_settings->setValue ("LogComments", m_comments);

  auto additionalFields = QStringList{};
  foreach(auto field, m_additionalFieldsControls){
      additionalFields.append(field->property("fieldKey").toString());
  }
  m_settings->setValue ("AdditionalFields", additionalFields);

  m_settings->endGroup ();
}

void LogQSO::initLogQSO(QString const& hisCall, QString const& hisGrid, QString mode,
                        QString const& rptSent, QString const& rptRcvd,
                        QDateTime const& dateTimeOn, QDateTime const& dateTimeOff,
                        Radio::Frequency dialFreq, QString const& myCall, QString const& myGrid,
                        bool toDATA, bool dBtoComments, bool bFox, QString const& opCall, QString const& comments)
{
  if(!isHidden()) return;

  loadSettings();

  ui->call->setFocus();
  ui->call->setText(hisCall);
  ui->grid->setText(hisGrid);
  ui->name->setText("");
  ui->comments->setText("");

  if (ui->cbComments->isChecked ()) ui->comments->setText(m_comments);

  if(dBtoComments) {
    QString t=mode;
    if(rptSent!="") t+="  Sent: " + rptSent;
    if(rptRcvd!="") t+="  Rcvd: " + rptRcvd;
    ui->comments->setText(t);
  }

  if(!comments.isEmpty()){
    ui->comments->setText(comments);
  }

  if(toDATA) mode="DATA";

  ui->mode->setText(mode);
  ui->sent->setText(rptSent);
  ui->rcvd->setText(rptRcvd);
  ui->start_date_time->setDateTime (dateTimeOn);
  ui->end_date_time->setDateTime (dateTimeOff);

  m_dialFreq=dialFreq;
  m_myCall=myCall;
  m_myGrid=myGrid;

  ui->band->setText (m_config->bands ()->find (dialFreq));
  ui->loggedOperator->setText(opCall);

  if(bFox) {
    accept();
  } else {
    show ();
  }
}

void LogQSO::accept()
{
  QString hisCall,hisGrid,mode,submode,rptSent,rptRcvd,dateOn,dateOff,timeOn,timeOff,band,operator_call;
  QString comments,name;

  hisCall=ui->call->text().toUpper();
  hisGrid=ui->grid->text().toUpper();
  mode = ui->mode->text().toUpper();
  if(mode == "JS8"){
    mode="MFSK";
    submode="JS8";
  }
  rptSent=ui->sent->text();
  rptRcvd=ui->rcvd->text();
  m_dateTimeOn = ui->start_date_time->dateTime ();
  m_dateTimeOff = ui->end_date_time->dateTime ();
  band=ui->band->text();
  name=ui->name->text();
  comments=ui->comments->text();
  m_comments=comments;
  QString strDialFreq(QString::number(m_dialFreq / 1.e6,'f',6));
  operator_call = ui->loggedOperator->text();
  //Log this QSO to ADIF file "js8call_log.adi"
  QString filename = "js8call_log.adi";  // TODO allow user to set
  ADIF adifile;
  auto adifilePath = QDir {QStandardPaths::writableLocation (QStandardPaths::DataLocation)}.absoluteFilePath (filename);
  adifile.init(adifilePath);

  QByteArray ADIF {adifile.QSOToADIF (hisCall, hisGrid, mode, submode, rptSent, rptRcvd, m_dateTimeOn, m_dateTimeOff, band
                                      , comments, name, strDialFreq, m_myCall, m_myGrid, operator_call)};
  if (!adifile.addQSOToFile (ADIF))
  {
    MessageBox::warning_message (this, tr ("Log file error"),
                                 tr ("Cannot open \"%1\"").arg (adifilePath));
  }

  // Log to N1MM Logger
  if (m_config->broadcast_to_n1mm() && m_config->valid_n1mm_info())  {
    const QHostAddress n1mmhost = QHostAddress(m_config->n1mm_server_name());
    QUdpSocket _sock;
    auto rzult = _sock.writeDatagram (ADIF + " <eor>", n1mmhost, quint16(m_config->n1mm_server_port()));
    if (rzult == -1) {
      MessageBox::warning_message (this, tr ("Error sending log to N1MM"),
                                   tr ("Write returned \"%1\"").arg (rzult));
    }
  }

//Log this QSO to file "js8call.log"
  static QFile f {QDir {QStandardPaths::writableLocation (QStandardPaths::DataLocation)}.absoluteFilePath ("js8call.log")};
  if(!f.open(QIODevice::Text | QIODevice::Append)) {
    MessageBox::warning_message (this, tr ("Log file error"),
                                 tr ("Cannot open \"%1\" for append").arg (f.fileName ()),
                                 tr ("Error: %1").arg (f.errorString ()));
  } else {
    QString logEntry=m_dateTimeOn.date().toString("yyyy-MM-dd,") +
      m_dateTimeOn.time().toString("hh:mm:ss,") +
      m_dateTimeOff.date().toString("yyyy-MM-dd,") +
      m_dateTimeOff.time().toString("hh:mm:ss,") + hisCall + "," +
      hisGrid + "," + strDialFreq + "," + (mode == "MFSK" ? "JS8" : mode) +
      "," + rptSent + "," + rptRcvd +
      "," + comments + "," + name;
    QTextStream out(&f);
    out << logEntry << endl;
    f.close();
  }

//Clean up and finish logging
  Q_EMIT acceptQSO (m_dateTimeOff, hisCall, hisGrid, m_dialFreq, mode, submode, rptSent, rptRcvd, comments, name,m_dateTimeOn, operator_call, m_myCall, m_myGrid, ADIF);
  QDialog::accept();
}

// closeEvent is only called from the system menu close widget for a
// modeless dialog so we use the hideEvent override to store the
// window settings
void LogQSO::hideEvent (QHideEvent * e)
{
  storeSettings ();
  QDialog::hideEvent (e);
}
