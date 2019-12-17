#include "widegraph.h"

#include <algorithm>
#include <QApplication>
#include <QSettings>
#include "ui_widegraph.h"
#include "commons.h"
#include "Configuration.hpp"
#include "MessageBox.hpp"
#include "SettingsGroup.hpp"

#include "DriftingDateTime.h"
#include "keyeater.h"

#include "moc_widegraph.cpp"

namespace
{
  auto user_defined = QObject::tr ("User Defined");
  float swide[MAX_SCREENSIZE];
}

WideGraph::WideGraph(QSettings * settings, QWidget *parent) :
  QDialog(parent),
  ui(new Ui::WideGraph),
  m_settings (settings),
  m_palettes_path {":/Palettes"},
  m_ntr0 {0},
  m_n {0},
  m_filterMinWidth {0},
  m_filterMinimum {0},
  m_filterMaximum {5000},
  m_filterEnabled {false},
  m_bHaveTransmitted {false}
{
  ui->setupUi(this);

  setWindowTitle (QApplication::applicationName () + " - " + tr ("Wide Graph"));
  setWindowFlags (Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
  setMaximumWidth (MAX_SCREENSIZE);
  setMaximumHeight (880);

  auto filterEscapeEater = new KeyPressEater();
  connect(filterEscapeEater, &KeyPressEater::keyPressed, this, [this](QObject */*obj*/, QKeyEvent *e, bool *pProcessed){
      if(e->key() != Qt::Key_Escape){
          return;
      }
      setFilter(0, 5000);
      if(pProcessed) *pProcessed=true;
  });
  ui->filterMinSpinBox->installEventFilter(filterEscapeEater);
  ui->filterMaxSpinBox->installEventFilter(filterEscapeEater);

  ui->widePlot->setCursor(Qt::CrossCursor);
  ui->widePlot->setMaximumHeight(800);
  ui->widePlot->setCurrent(false);

  ui->widePlot->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->widePlot, &CPlotter::customContextMenuRequested, this, [this](const QPoint &pos){
      auto menu = new QMenu(this);

      int f = ui->widePlot->frequencyAt(pos.x());

      auto offsetAction = menu->addAction(QString("Set &Offset to %1 Hz").arg(f));
      connect(offsetAction, &QAction::triggered, this, [this, f](){
        ui->offsetSpinBox->setValue(f);
      });

      menu->addSeparator();

      if(m_filterEnabled){
          auto disableAction = menu->addAction(QString("&Disable Filter"));
          connect(disableAction, &QAction::triggered, this, [this](){
            ui->filterCheckBox->setChecked(false);
          });
      }

      auto minAction = menu->addAction(QString("Set Filter &Minimum to %1 Hz").arg(f));
      connect(minAction, &QAction::triggered, this, [this, f](){
        ui->filterMinSpinBox->setValue(f);
        ui->filterCheckBox->setChecked(true);
      });

      auto maxAction = menu->addAction(QString("Set Filter Ma&ximum to %1 Hz").arg(f));
      connect(maxAction, &QAction::triggered, this, [this, f](){
          ui->filterMaxSpinBox->setValue(f);
          ui->filterCheckBox->setChecked(true);
      });

      menu->popup(ui->widePlot->mapToGlobal(pos));
  });

  connect(ui->widePlot, SIGNAL(freezeDecode1(int)),this,
          SLOT(wideFreezeDecode(int)));

  connect(ui->widePlot, SIGNAL(setFreq1(int,int)),this,
          SLOT(setFreq2(int,int)));

  connect(ui->widePlot, &CPlotter::qsy, this, [this](int hzDelta){
    emit qsy(hzDelta);
  });

  {

    //Restore user's settings
    SettingsGroup g {m_settings, "WideGraph"};
    restoreGeometry (m_settings->value ("geometry", saveGeometry ()).toByteArray ());
    ui->widePlot->setPlotZero(m_settings->value("PlotZero", 0).toInt());
    ui->widePlot->setPlotGain(m_settings->value("PlotGain", 0).toInt());
    ui->widePlot->setPlot2dGain(m_settings->value("Plot2dGain", 0).toInt());
    ui->widePlot->setPlot2dZero(m_settings->value("Plot2dZero", 0).toInt());
    ui->zeroSlider->setValue(ui->widePlot->plotZero());
    ui->gainSlider->setValue(ui->widePlot->plotGain());
    ui->gain2dSlider->setValue(ui->widePlot->plot2dGain());
    ui->zero2dSlider->setValue(ui->widePlot->plot2dZero());
    int n = m_settings->value("BinsPerPixel",2).toInt();
    m_bFlatten=m_settings->value("Flatten",true).toBool();
    m_bRef=m_settings->value("UseRef",false).toBool();
    ui->cbFlatten->setChecked(m_bFlatten);
    ui->widePlot->setFlatten(m_bFlatten,m_bRef);
    ui->cbRef->setChecked(m_bRef);
    ui->widePlot->setBreadth(m_settings->value("PlotWidth",1000).toInt());
    ui->bppSpinBox->setValue(n);
    m_nsmo=m_settings->value("SmoothYellow",1).toInt();
    ui->smoSpinBox->setValue(m_nsmo);
    m_Percent2DScreen=m_settings->value("Percent2D", 0).toInt();
    m_waterfallAvg = m_settings->value("WaterfallAvg", 1).toInt();
    ui->waterfallAvgSpinBox->setValue(m_waterfallAvg);
    ui->widePlot->setWaterfallAvg(m_waterfallAvg);
    ui->widePlot->setCurrent(m_settings->value("Current",false).toBool());
    ui->widePlot->setCumulative(m_settings->value("Cumulative",true).toBool());
    ui->widePlot->setLinearAvg(m_settings->value("LinearAvg",false).toBool());
    if(ui->widePlot->current()) ui->spec2dComboBox->setCurrentIndex(0);
    if(ui->widePlot->cumulative()) ui->spec2dComboBox->setCurrentIndex(1);
    if(ui->widePlot->linearAvg()) ui->spec2dComboBox->setCurrentIndex(2);
#if JS8_USE_REFSPEC
    ui->widePlot->setReference(m_settings->value("Reference",false).toBool());
    if(ui->widePlot->Reference()) ui->spec2dComboBox->setCurrentIndex(3);
#endif
    int nbpp=m_settings->value("BinsPerPixel", 2).toInt();
    ui->widePlot->setBinsPerPixel(nbpp);
    ui->sbPercent2dPlot->setValue(m_Percent2DScreen);
    ui->widePlot->SetPercent2DScreen(m_Percent2DScreen);
    ui->widePlot->setStartFreq(m_settings->value("StartFreq", 500).toInt());
    ui->centerSpinBox->setValue(m_settings->value("CenterOffset", 1500).toInt());
    ui->fStartSpinBox->setValue(ui->widePlot->startFreq());
    m_waterfallPalette=m_settings->value("WaterfallPalette","Default").toString();
    m_userPalette = WFPalette {m_settings->value("UserPalette").value<WFPalette::Colours> ()};
    m_fMinPerBand = m_settings->value ("FminPerBand").toHash ();
    setRxRange ();
    ui->controls_widget->setVisible(!m_settings->value("HideControls", false).toBool());
    ui->cbControls->setChecked(!m_settings->value("HideControls", false).toBool());

    auto splitState = m_settings->value("SplitState").toByteArray();
    if(!splitState.isEmpty()){
        ui->splitter->restoreState(splitState);
    }

    setFilter(m_settings->value("FilterMinimum", 500).toInt(), m_settings->value("FilterMaximum", 2500).toInt());
    setFilterEnabled(m_settings->value("FilterEnabled", false).toBool());
  }

  int index=0;
  for (QString const& file:
         m_palettes_path.entryList(QDir::NoDotAndDotDot |
                                   QDir::System | QDir::Hidden |
                                   QDir::AllDirs | QDir::Files,
                                   QDir::DirsFirst)) {
    QString t=file.mid(0,file.length()-4);
    ui->paletteComboBox->addItem(t);
    if(t==m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
    index++;
  }
  ui->paletteComboBox->addItem (user_defined);
  if (user_defined == m_waterfallPalette) ui->paletteComboBox->setCurrentIndex(index);
  readPalette ();
}

WideGraph::~WideGraph ()
{
}

void WideGraph::closeEvent (QCloseEvent * e)
{
  saveSettings ();
  QDialog::closeEvent (e);
}

void WideGraph::saveSettings()                                           //saveSettings
{
  SettingsGroup g {m_settings, "WideGraph"};
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("PlotZero", ui->widePlot->plotZero());
  m_settings->setValue ("PlotGain", ui->widePlot->plotGain());
  m_settings->setValue ("Plot2dGain", ui->widePlot->plot2dGain());
  m_settings->setValue ("Plot2dZero", ui->widePlot->plot2dZero());
  m_settings->setValue ("PlotWidth", ui->widePlot->plotWidth ());
  m_settings->setValue ("BinsPerPixel", ui->bppSpinBox->value ());
  m_settings->setValue ("SmoothYellow", ui->smoSpinBox->value ());
  m_settings->setValue ("Percent2D",m_Percent2DScreen);
  m_settings->setValue ("WaterfallAvg", ui->waterfallAvgSpinBox->value ());
  m_settings->setValue ("Current", ui->widePlot->current());
  m_settings->setValue ("Cumulative", ui->widePlot->cumulative());
  m_settings->setValue ("LinearAvg", ui->widePlot->linearAvg());
#if JS8_USE_REFSPEC
  m_settings->setValue ("Reference", ui->widePlot->Reference());
#endif
  m_settings->setValue ("BinsPerPixel", ui->widePlot->binsPerPixel ());
  m_settings->setValue ("StartFreq", ui->widePlot->startFreq ());
  m_settings->setValue ("WaterfallPalette", m_waterfallPalette);
  m_settings->setValue ("UserPalette", QVariant::fromValue (m_userPalette.colours ()));
  m_settings->setValue("Flatten",m_bFlatten);
  m_settings->setValue("UseRef",m_bRef);
  m_settings->setValue ("HideControls", ui->controls_widget->isHidden ());
  m_settings->setValue ("FminPerBand", m_fMinPerBand);
  m_settings->setValue ("CenterOffset", ui->centerSpinBox->value());
  m_settings->setValue ("FilterMinimum", m_filterMinimum);
  m_settings->setValue ("FilterMaximum", m_filterMaximum);
  m_settings->setValue ("FilterEnabled", m_filterEnabled);
  m_settings->setValue ("SplitState", ui->splitter->saveState());
}

void WideGraph::drawRed(int ia, int ib)
{
  ui->widePlot->drawRed(ia,ib,swide);
}

void WideGraph::dataSink2(float s[], float df3, int ihsym, int ndiskdata)  //dataSink2
{
  static float splot[NSMAX];
  int nbpp = ui->widePlot->binsPerPixel();

//Average spectra over specified number, m_waterfallAvg
  if (m_n==0) {
    for (int i=0; i<NSMAX; i++)
      splot[i]=s[i];
  } else {
    for (int i=0; i<NSMAX; i++)
      splot[i] += s[i];
  }
  m_n++;

  if (m_n>=m_waterfallAvg) {
    for (int i=0; i<NSMAX; i++)
        splot[i] /= m_n;        //Normalize the average
    m_n=0;
    int i=int(ui->widePlot->startFreq()/df3 + 0.5);
    int jz=5000.0/(nbpp*df3);
		if(jz>MAX_SCREENSIZE) jz=MAX_SCREENSIZE;
    m_jz=jz;
    for (int j=0; j<jz; j++) {
      float ss=0.0;
      float smax=0;
      for (int k=0; k<nbpp; k++) {
        float sp=splot[i++];
        ss += sp;
        smax=qMax(smax,sp);
      }
//      swide[j]=nbpp*smax;
      swide[j]=nbpp*ss;
    }

// Time according to this computer
    qint64 ms = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
    int ntr = (ms/1000) % m_TRperiod;
    if((ndiskdata && ihsym <= m_waterfallAvg) || (!ndiskdata && ntr<m_ntr0)) {
      float flagValue=1.0e30;
      if(m_bHaveTransmitted) flagValue=2.0e30;
      for(int i=0; i<MAX_SCREENSIZE; i++) {
        swide[i] = flagValue;
      }
      for(int i=0; i<NSMAX; i++) {
        splot[i] = flagValue;
      }
      m_bHaveTransmitted=false;
    }
    m_ntr0=ntr;
    ui->widePlot->draw(swide,true,false);
  }
}

void WideGraph::on_bppSpinBox_valueChanged(int n)                            //bpp
{
  ui->widePlot->setBinsPerPixel(n);
}

void WideGraph::on_qsyPushButton_clicked(){
    int hzDelta = rxFreq() - centerFreq();
    emit qsy(hzDelta);
}

void WideGraph::on_offsetSpinBox_valueChanged(int n){
  if(n == rxFreq()){
      return;
  }

  // TODO: jsherer - here's where we'd set minimum frequency again (later?)
  n = qMax(0, n);

  setRxFreq(n);
  setTxFreq(n);
  setFreq2(n, n);
}

void WideGraph::on_waterfallAvgSpinBox_valueChanged(int n)                  //Navg
{
  m_waterfallAvg = n;
  ui->widePlot->setWaterfallAvg(n);
}

void WideGraph::keyPressEvent(QKeyEvent *e)                                 //F11, F12
{  
  switch(e->key())
  {
  int n;
  case Qt::Key_F11:
    n=11;
    if(e->modifiers() & Qt::ControlModifier) n+=100;
    emit f11f12(n);
    break;
  case Qt::Key_F12:
    n=12;
    if(e->modifiers() & Qt::ControlModifier) n+=100;
    emit f11f12(n);
    break;
  default:
    QDialog::keyPressEvent (e);
  }
}

void WideGraph::setRxFreq(int n)                                           //setRxFreq
{
  ui->widePlot->setRxFreq(n);
  ui->widePlot->draw(swide,false,false);
  ui->offsetSpinBox->setValue(n);
}

int WideGraph::rxFreq()                                                   //rxFreq
{
  return ui->widePlot->rxFreq();
}

int WideGraph::centerFreq()
{
  return ui->centerSpinBox->value();
}

int WideGraph::nStartFreq()                                             //nStartFreq
{
  return ui->widePlot->startFreq();
}

void WideGraph::wideFreezeDecode(int n)                              //wideFreezeDecode
{
  emit freezeDecode2(n);
}

void WideGraph::setRxRange ()
{
  ui->widePlot->setRxRange (Fmin ());
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

int WideGraph::Fmin()                                              //Fmin
{
  return "60m" == m_rxBand ? 0 : m_fMinPerBand.value (m_rxBand, 2500).toUInt ();
}

int WideGraph::Fmax()                                              //Fmax
{
  return std::min(5000,ui->widePlot->Fmax());
}

int WideGraph::filterMinimum()
{
    return std::max(0, m_filterMinimum);
}

int WideGraph::filterMaximum()
{
    return std::min(m_filterMaximum, 5000);
}

bool WideGraph::filterEnabled()
{
    return m_filterEnabled;
}

void WideGraph::setFilter(int a, int b){
    int low = std::min(a, b);
    int high = std::max(a, b);

    // ensure minimum filter width
    if(high-low < m_filterMinWidth){
        high = low + m_filterMinWidth;
    }

    // update the filter history
    m_filterMinimum = low;
    m_filterMaximum = high;

    // update the spinner UI
    bool blocked = false;
    blocked = ui->filterMinSpinBox->blockSignals(true);
    {
        ui->filterMinSpinBox->setValue(low);
    }
    ui->filterMinSpinBox->blockSignals(blocked);

    blocked = ui->filterMaxSpinBox->blockSignals(true);
    {
        ui->filterMaxSpinBox->setValue(high);
    }
    ui->filterMaxSpinBox->blockSignals(blocked);

    // update the wide plot UI
    int width = high - low;
    ui->widePlot->setFilterCenter(low + width/2);
    ui->widePlot->setFilterWidth(high - low);
}

void WideGraph::setFilterMinimumBandwidth(int width){
    m_filterMinWidth = width;
    setFilter(m_filterMinimum, std::max(m_filterMinimum+width, m_filterMaximum));
}

void WideGraph::setFilterEnabled(bool enabled){
    m_filterEnabled = enabled;

    // update the filter spinner
    ui->filterMinSpinBox->setEnabled(enabled);
    ui->filterMaxSpinBox->setEnabled(enabled);

    // update the checkbox ui
    bool blocked = ui->filterCheckBox->blockSignals(true);
    {
        ui->filterCheckBox->setChecked(enabled);
    }
    ui->filterCheckBox->blockSignals(blocked);

    // update the wideplot
    ui->widePlot->setFilterEnabled(enabled);
}

int WideGraph::fSpan()
{
  return ui->widePlot->fSpan ();
}

void WideGraph::setPeriod(int ntrperiod, int nsps)                  //SetPeriod
{
  m_TRperiod=ntrperiod;
  m_nsps=nsps;
  ui->widePlot->setNsps(ntrperiod, nsps);
}

void WideGraph::setTxFreq(int n)                                   //setTxFreq
{
  emit setXIT2(n);
  ui->widePlot->setTxFreq(n);
  ui->offsetSpinBox->setValue(n);
}

void WideGraph::setMode(QString mode)                              //setMode
{
  m_mode=mode;
  ui->widePlot->setMode(mode);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

void WideGraph::setSubMode(int n)                                  //setSubMode
{
  m_nSubMode=n;
  ui->widePlot->setSubMode(n);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}
void WideGraph::setModeTx(QString modeTx)                          //setModeTx
{
  m_modeTx=modeTx;
  ui->widePlot->setModeTx(modeTx);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

                                                        //Current-Cumulative-Yellow
void WideGraph::on_spec2dComboBox_currentIndexChanged(const QString &arg1)
{
  ui->widePlot->setCurrent(false);
  ui->widePlot->setCumulative(false);
  ui->widePlot->setLinearAvg(false);
  ui->smoSpinBox->setEnabled(false);
  if(arg1=="Current") ui->widePlot->setCurrent(true);
  if(arg1=="Cumulative") ui->widePlot->setCumulative(true);
  if(arg1=="Linear Avg") {
    ui->widePlot->setLinearAvg(true);
    ui->smoSpinBox->setEnabled(true);
  }
#if JS8_USE_REFSPEC
  ui->widePlot->setReference(false);
  if(arg1=="Reference") {
    ui->widePlot->setReference(true);
  }
#endif
  replot();
}

void WideGraph::setFreq2(int rxFreq, int txFreq)                  //setFreq2
{
  emit setFreq3(rxFreq,txFreq);
}

void WideGraph::setDialFreq(double d)                             //setDialFreq
{
  ui->widePlot->setDialFreq(d);
}

void WideGraph::setControlsVisible(bool visible)
{
  ui->cbControls->setChecked(visible);
}

bool WideGraph::controlsVisible(){
  return ui->cbControls->isChecked();
}

void WideGraph::setRxBand (QString const& band)
{
  m_rxBand = band;
  ui->widePlot->setRxBand(band);
  setRxRange ();
}


void WideGraph::on_fStartSpinBox_valueChanged(int n)             //fStart
{
  ui->widePlot->setStartFreq(n);
}

void WideGraph::readPalette ()                                   //readPalette
{
  try
    {
      if (user_defined == m_waterfallPalette)
        {
          ui->widePlot->setColours (WFPalette {m_userPalette}.interpolate ());
        }
      else
        {
          ui->widePlot->setColours (WFPalette {m_palettes_path.absoluteFilePath (m_waterfallPalette + ".pal")}.interpolate());
        }
    }
  catch (std::exception const& e)
    {
      MessageBox::warning_message (this, tr ("Read Palette"), e.what ());
    }
}

void WideGraph::on_paletteComboBox_activated (QString const& palette)    //palette selector
{
  m_waterfallPalette = palette;
  readPalette();
  replot();
}

void WideGraph::on_cbFlatten_toggled(bool b)                          //Flatten On/Off
{
  m_bFlatten=b;
  if(m_bRef and m_bFlatten) {
    m_bRef=false;
    ui->cbRef->setChecked(false);
  }
  ui->widePlot->setFlatten(m_bFlatten,m_bRef);
}

void WideGraph::on_cbRef_toggled(bool b)
{
  m_bRef=b;
  if(m_bRef and m_bFlatten) {
    m_bFlatten=false;
    ui->cbFlatten->setChecked(false);
  }
  ui->widePlot->setFlatten(m_bFlatten,m_bRef);
}

void WideGraph::on_cbControls_toggled(bool b)
{
  ui->controls_widget->setVisible(b);
}

void WideGraph::on_adjust_palette_push_button_clicked (bool)   //Adjust Palette
{
  try
    {
      if (m_userPalette.design ())
        {
          m_waterfallPalette = user_defined;
          ui->paletteComboBox->setCurrentText (m_waterfallPalette);
          readPalette ();
        }
    }
  catch (std::exception const& e)
    {
      MessageBox::warning_message (this, tr ("Read Palette"), e.what ());
    }
}

bool WideGraph::flatten()                                              //Flatten
{
  return m_bFlatten;
}

bool WideGraph::useRef()                                              //Flatten
{
  return m_bRef;
}

void WideGraph::replot()
{
  if(ui->widePlot->scaleOK()) ui->widePlot->replot();
}

void WideGraph::on_gainSlider_valueChanged(int value)                 //Gain
{
  ui->widePlot->setPlotGain(value);
  replot();
}

void WideGraph::on_zeroSlider_valueChanged(int value)                 //Zero
{
  ui->widePlot->setPlotZero(value);
  replot();
}

void WideGraph::on_gain2dSlider_valueChanged(int value)               //Gain2
{
  ui->widePlot->setPlot2dGain(value);
  if(ui->widePlot->scaleOK ()) {
    ui->widePlot->draw(swide,false,false);
    if(m_mode=="QRA64") ui->widePlot->draw(swide,false,true);
  }
}

void WideGraph::on_zero2dSlider_valueChanged(int value)               //Zero2
{
  ui->widePlot->setPlot2dZero(value);
  if(ui->widePlot->scaleOK ()) {
    ui->widePlot->draw(swide,false,false);
    if(m_mode=="QRA64") ui->widePlot->draw(swide,false,true);
  }
}

void WideGraph::setTol(int n)                                         //setTol
{
  ui->widePlot->setTol(n);
  ui->widePlot->DrawOverlay();
  ui->widePlot->update();
}

void WideGraph::on_smoSpinBox_valueChanged(int n)
{
  m_nsmo=n;
}

int WideGraph::smoothYellow()
{
  return m_nsmo;
}

void WideGraph::setWSPRtransmitted()
{
  m_bHaveTransmitted=true;
}

void WideGraph::setVHF(bool bVHF)
{
  ui->widePlot->setVHF(bVHF);
}

void WideGraph::on_sbPercent2dPlot_valueChanged(int n)
{
  m_Percent2DScreen=n;
  ui->widePlot->SetPercent2DScreen(n);
}

void WideGraph::on_filterMinSpinBox_valueChanged(int n){
    setFilter(n, m_filterMaximum);
}

void WideGraph::on_filterMaxSpinBox_valueChanged(int n){
    setFilter(m_filterMinimum, n);
}

void WideGraph::on_filterCheckBox_toggled(bool b){
    setFilterEnabled(b);
}

void WideGraph::setRedFile(QString fRed)
{
  ui->widePlot->setRedFile(fRed);
}

void WideGraph::setTurbo(bool turbo){
  ui->widePlot->setTurbo(turbo);
}

void WideGraph::on_driftSpinBox_valueChanged(int n){
    if(n == DriftingDateTime::drift()){
        return;
    }

    setDrift(n);
}

void WideGraph::on_driftSyncButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();

    int n = 0;
    int nPos = m_TRperiod - (now.time().second() % m_TRperiod);
    int nNeg = (now.time().second() % m_TRperiod) - m_TRperiod;

    if(abs(nNeg) < nPos){
        n = nNeg;
    } else {
        n = nPos;
    }

    setDrift(n * 1000);
}

void WideGraph::on_driftSyncEndButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();

    int n = 0;
    int nPos = m_TRperiod - (now.time().second() % m_TRperiod);
    int nNeg = (now.time().second() % m_TRperiod) - m_TRperiod;

    if(abs(nNeg) < nPos){
        n = nNeg + 2;
    } else {
        n = nPos - 2;
    }

    setDrift(n * 1000);
}

void WideGraph::on_driftSyncMinuteButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();
    int n = 0;
    int s = now.time().second();

    if(s < 30){
        n = -s;
    } else {
        n = 60 - s;
    }

    setDrift(n * 1000);
}

void WideGraph::on_driftSyncResetButton_clicked(){
    setDrift(0);
}

void WideGraph::setDrift(int n){
    DriftingDateTime::setDrift(n);

    qDebug() << qSetRealNumberPrecision(12) << "Drift milliseconds:" << n;
    qDebug() << qSetRealNumberPrecision(12) << "Clock time:" << QDateTime::currentDateTimeUtc();
    qDebug() << qSetRealNumberPrecision(12) << "Drifted time:" << DriftingDateTime::currentDateTimeUtc();

    if(ui->driftSpinBox->value() != n){
        ui->driftSpinBox->setValue(n);
    }
}
