//---------------------------------------------------------- MainWindow
#include "mainwindow.h"
#include <cmath>
#include <cinttypes>
#include <limits>
#include <functional>
#include <fstream>
#include <iterator>
#include <fftw3.h>
#include <QLineEdit>
#include <QRegExpValidator>
#include <QRegExp>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>
#include <QProgressDialog>
#include <QHostInfo>
#include <QVector>
#include <QCursor>
#include <QToolTip>
#include <QAction>
#include <QActionGroup>
#include <QSplashScreen>
#include <QSound>
#include <QUdpSocket>
#include <QVariant>

#include "APRSISClient.h"
#include "revision_utils.hpp"
#include "qt_helpers.hpp"
#include "NetworkAccessManager.hpp"
#include "soundout.h"
#include "soundin.h"
#include "Modulator.hpp"
#include "Detector.hpp"
#include "plotter.h"
#include "echoplot.h"
#include "echograph.h"
#include "fastplot.h"
#include "fastgraph.h"
#include "about.h"
#include "messageaveraging.h"
#include "widegraph.h"
#include "sleep.h"
#include "logqso.h"
#include "decodedtext.h"
#include "Radio.hpp"
#include "Bands.hpp"
#include "TransceiverFactory.hpp"
#include "StationList.hpp"
#include "LiveFrequencyValidator.hpp"
#include "MessageClient.hpp"
#include "wsprnet.h"
#include "signalmeter.h"
#include "HelpTextWindow.hpp"
#include "Audio/BWFFile.hpp"
#include "MultiSettings.hpp"
#include "MaidenheadLocatorValidator.hpp"
#include "CallsignValidator.hpp"
#include "EqualizationToolsDialog.hpp"
#include "SelfDestructMessageBox.h"
#include "messagereplydialog.h"
#include "DriftingDateTime.h"
#include "jsc.h"
#include "jsc_checker.h"
#include "Inbox.h"
#include "messagewindow.h"

#include "ui_mainwindow.h"
#include "moc_mainwindow.cpp"

#define STATE_RX 1
#define STATE_TX 2


extern "C" {
  //----------------------------------------------------- C and Fortran routines
  void symspec_(struct dec_data *, int* k, int* ntrperiod, int* nsps, int* ingain,
                int* minw, float* px, float s[], float* df3, int* nhsym, int* npts8,
                float *m_pxmax);

  void genft8_(char* msg, char* MyGrid, bool* bcontest, int* i3bit, char* msgsent,
               char ft8msgbits[], int itone[], fortran_charlen_t, fortran_charlen_t,
               fortran_charlen_t);

  void genjs8_(char* msg, char* MyGrid, bool* bcontest, int* i3bit, char* msgsent,
               char ft8msgbits[], int itone[], fortran_charlen_t, fortran_charlen_t,
               fortran_charlen_t);

  void azdist_(char* MyGrid, char* HisGrid, double* utch, int* nAz, int* nEl,
               int* nDmiles, int* nDkm, int* nHotAz, int* nHotABetter,
               fortran_charlen_t, fortran_charlen_t);

  void morse_(char* msg, int* icw, int* ncw, fortran_charlen_t);

  int ptt_(int nport, int ntx, int* iptt, int* nopen);

  void wspr_downsample_(short int d2[], int* k);

  int savec2_(char* fname, int* TR_seconds, double* dial_freq, fortran_charlen_t);

  void avecho_( short id2[], int* dop, int* nfrit, int* nqual, float* f1,
                float* level, float* sigdb, float* snr, float* dfreq,
                float* width);

  void degrade_snr_(short d2[], int* n, float* db, float* bandwidth);

  void wav12_(short d2[], short d1[], int* nbytes, short* nbitsam2);

  void refspectrum_(short int d2[], bool* bclearrefspec,
                    bool* brefspec, bool* buseref, const char* c_fname, fortran_charlen_t);

  void freqcal_(short d2[], int* k, int* nkhz,int* noffset, int* ntol,
                char line[], fortran_charlen_t);

  void fix_contest_msg_(char* MyGrid, char* msg, fortran_charlen_t, fortran_charlen_t);

  void calibrate_(char data_dir[], int* iz, double* a, double* b, double* rms,
                  double* sigmaa, double* sigmab, int* irc, fortran_charlen_t);

  void foxgen_();

  void plotsave_(float swide[], int* m_w , int* m_h1, int* irow);
}

const int NEAR_THRESHOLD_RX = 10;

int volatile itone[NUM_ISCAT_SYMBOLS];  //Audio tones for all Tx symbols
int volatile icw[NUM_CW_SYMBOLS];       //Dits for CW ID
struct dec_data dec_data;               // for sharing with Fortran

int outBufSize;
int rc;
qint32  g_iptt {0};
wchar_t buffer[256];
float fast_green[703];
float fast_green2[703];
float fast_s[44992];                                    //44992=64*703
float fast_s2[44992];
int   fast_jh {0};
int   fast_jhpeak {0};
int   fast_jh2 {0};
int narg[15];
QVector<QColor> g_ColorTbl;

namespace
{
  Radio::Frequency constexpr default_frequency {14078000};

  QRegExp message_alphabet {"[^\\x00-\\x1F]*"}; // base alphabet supported by JS8CALL

  // grid exact match excluding RR73
  QRegularExpression grid_regexp {"\\A(?![Rr]{2}73)[A-Ra-r]{2}[0-9]{2}([A-Xa-x]{2}){0,1}\\z"};

  bool message_is_73 (int type, QStringList const& msg_parts)
  {
    return type >= 0
      && (((type < 6 || 7 == type)
           && (msg_parts.contains ("73") || msg_parts.contains ("RR73")))
          || (type == 6 && !msg_parts.filter ("73").isEmpty ()));
  }

  int ms_minute_error ()
  {
    auto const& now = DriftingDateTime::currentDateTime ();
    auto const& time = now.time ();
    auto second = time.second ();
    return now.msecsTo (now.addSecs (second > 30 ? 60 - second : -second)) - time.msec ();
  }

  QString since(QDateTime time){
      int delta = time.toUTC().secsTo(DriftingDateTime::currentDateTimeUtc());
      if(delta < 15){
          return QString("now");
      }

      int seconds = delta % 60;
      delta = delta / 60;
      int minutes = delta % 60;
      delta = delta / 60;
      int hours = delta % 24;
      delta = delta / 24;
      int days = delta;

      if(days){
          return QString("%1d").arg(days);
      }
      if(hours){
          return QString("%1h").arg(hours);
      }
      if(minutes){
          return QString("%1m").arg(minutes);
      }
      if(seconds){
          return QString("%1s").arg(seconds - seconds%15);
      }

      return QString {};
  }

  QString timeSince(int delta){
      int seconds = delta % 60;
      delta = delta / 60;
      int minutes = delta;
      if(minutes && seconds){
          return QString("%1m %2s").arg(minutes).arg(seconds);
      } else if(minutes){
          return QString("%1m").arg(minutes);
      } else {
          return QString("%1s").arg(seconds);
      }
  }

  void clearTableWidget(QTableWidget *widget){
      if(!widget){
          return;
      }
      for(int i = widget->rowCount(); i >= 0; i--){
        widget->removeRow(i);
      }
  }

#if 0
  int round(int numToRound, int multiple)
  {
   if(multiple == 0)
   {
    return numToRound;
   }

   int roundDown = ( (int) (numToRound) / multiple) * multiple;

   if(numToRound - roundDown > multiple/2){
    return roundDown + multiple;
   }

   return roundDown;
  }
#endif

  int roundUp(int numToRound, int multiple)
  {
   if(multiple == 0)
   {
    return numToRound;
   }

   int roundDown = ( (int) (numToRound) / multiple) * multiple;
   return roundDown + multiple;
  }

  template<typename T>
  QList<T> listCopyReverse(QList<T> const &list){
      QList<T> newList = QList<T>();
      auto iter = list.end();
      while(iter != list.begin()){
          newList.append(*(--iter));
      }
      return newList;
  }
}

//--------------------------------------------------- MainWindow constructor
MainWindow::MainWindow(QDir const& temp_directory, bool multiple,
                       MultiSettings * multi_settings, QSharedMemory *shdmem,
                       unsigned downSampleFactor,
                       QSplashScreen * splash, QWidget *parent) :
  QMainWindow(parent),
  m_network_manager {this},
  m_valid {true},
  m_splash {splash},
  m_revision {revision ()},
  m_multiple {multiple},
  m_multi_settings {multi_settings},
  m_configurations_button {0},
  m_settings {multi_settings->settings ()},
  m_settings_read {false},
  ui(new Ui::MainWindow),
  m_config {temp_directory, m_settings, this},
  m_WSPR_band_hopping {m_settings, &m_config, this},
  m_WSPR_tx_next {false},
  m_rigErrorMessageBox {MessageBox::Critical, tr ("Rig Control Error")
      , MessageBox::Cancel | MessageBox::Ok | MessageBox::Retry},
  m_wideGraph (new WideGraph(m_settings)),
  m_echoGraph (new EchoGraph(m_settings)),
  m_fastGraph (new FastGraph(m_settings)),
  // no parent so that it has a taskbar icon
  m_logDlg (new LogQSO (program_title (), m_settings, &m_config, nullptr)),
  m_lastDialFreq {0},
  m_dialFreqRxWSPR {0},
  m_detector {new Detector {RX_SAMPLE_RATE, NTMAX, downSampleFactor}},
  m_FFTSize {6192 / 2},         // conservative value to avoid buffer overruns
  m_soundInput {new SoundInput},
  m_modulator {new Modulator {TX_SAMPLE_RATE, NTMAX}},
  m_soundOutput {new SoundOutput},
  m_msErase {0},
  m_secBandChanged {0},
  m_freqNominal {0},
  m_freqTxNominal {0},
  m_s6 {0.},
  m_tRemaining {0.},
  m_DTtol {3.0},
  m_waterfallAvg {1},
  m_ntx {1},
  m_gen_message_is_cq {false},
  m_send_RR73 {false},
  m_XIT {0},
  m_sec0 {-1},
  m_RxLog {1},      //Write Date and Time to RxLog
  m_nutc0 {999999},
  m_ntr {0},
  m_tx {0},
  m_TRperiod {60},
  m_inGain {0},
  m_secID {0},
  m_idleMinutes {0},
  m_nSubMode {0},
  m_nclearave {1},
  m_pctx {0},
  m_nseq {0},
  m_nWSPRdecodes {0},
  m_k0 {9999999},
  m_nPick {0},
  m_frequency_list_fcal_iter {m_config.frequencies ()->begin ()},
  m_nTx73 {0},
  m_btxok {false},
  m_diskData {false},
  m_loopall {false},
  m_auto {false},
  m_restart {false},
  m_startAnother {false},
  m_saveDecoded {false},
  m_saveAll {false},
  m_widebandDecode {false},
  m_dataAvailable {false},
  m_blankLine {false},
  m_decodedText2 {false},
  m_freeText {false},
  m_sentFirst73 {false},
  m_currentMessageType {-1},
  m_lastMessageType {-1},
  m_bShMsgs {false},
  m_bSWL {false},
  m_uploading {false},
  m_txNext {false},
  m_grid6 {false},
  m_tuneup {false},
  m_bTxTime {false},
  m_rxDone {false},
  m_bSimplex {false},
  m_bEchoTxOK {false},
  m_bTransmittedEcho {false},
  m_bEchoTxed {false},
  m_bFastDecodeCalled {false},
  m_bDoubleClickAfterCQnnn {false},
  m_bRefSpec {false},
  m_bClearRefSpec {false},
  m_bTrain {false},
  m_bAutoReply {false},
  m_QSOProgress {CALLING},
  m_ihsym {0},
  m_nzap {0},
  m_px {0.0},
  m_iptt0 {0},
  m_btxok0 {false},
  m_nsendingsh {0},
  m_onAirFreq0 {0.0},
  m_first_error {true},
  tx_status_label {"Receiving"},
  wsprNet {new WSPRNet {&m_network_manager, this}},
  m_appDir {QApplication::applicationDirPath ()},
  m_palette {"Linrad"},
  m_mode {"FT8"},
  m_rpt {"-15"},
  m_pfx {
    "1A", "1S",
      "3A", "3B6", "3B8", "3B9", "3C", "3C0", "3D2", "3D2C",
      "3D2R", "3DA", "3V", "3W", "3X", "3Y", "3YB", "3YP",
      "4J", "4L", "4S", "4U1I", "4U1U", "4W", "4X",
      "5A", "5B", "5H", "5N", "5R", "5T", "5U", "5V", "5W", "5X", "5Z",
      "6W", "6Y",
      "7O", "7P", "7Q", "7X",
      "8P", "8Q", "8R",
      "9A", "9G", "9H", "9J", "9K", "9L", "9M2", "9M6", "9N",
      "9Q", "9U", "9V", "9X", "9Y",
      "A2", "A3", "A4", "A5", "A6", "A7", "A9", "AP",
      "BS7", "BV", "BV9", "BY",
      "C2", "C3", "C5", "C6", "C9", "CE", "CE0X", "CE0Y",
      "CE0Z", "CE9", "CM", "CN", "CP", "CT", "CT3", "CU",
      "CX", "CY0", "CY9",
      "D2", "D4", "D6", "DL", "DU",
      "E3", "E4", "E5", "EA", "EA6", "EA8", "EA9", "EI", "EK",
      "EL", "EP", "ER", "ES", "ET", "EU", "EX", "EY", "EZ",
      "F", "FG", "FH", "FJ", "FK", "FKC", "FM", "FO", "FOA",
      "FOC", "FOM", "FP", "FR", "FRG", "FRJ", "FRT", "FT5W",
      "FT5X", "FT5Z", "FW", "FY",
      "M", "MD", "MI", "MJ", "MM", "MU", "MW",
      "H4", "H40", "HA", "HB", "HB0", "HC", "HC8", "HH",
      "HI", "HK", "HK0", "HK0M", "HL", "HM", "HP", "HR",
      "HS", "HV", "HZ",
      "I", "IS", "IS0",
      "J2", "J3", "J5", "J6", "J7", "J8", "JA", "JDM",
      "JDO", "JT", "JW", "JX", "JY",
      "K", "KC4", "KG4", "KH0", "KH1", "KH2", "KH3", "KH4", "KH5",
      "KH5K", "KH6", "KH7", "KH8", "KH9", "KL", "KP1", "KP2",
      "KP4", "KP5",
      "LA", "LU", "LX", "LY", "LZ",
      "OA", "OD", "OE", "OH", "OH0", "OJ0", "OK", "OM", "ON",
      "OX", "OY", "OZ",
      "P2", "P4", "PA", "PJ2", "PJ7", "PY", "PY0F", "PT0S", "PY0T", "PZ",
      "R1F", "R1M",
      "S0", "S2", "S5", "S7", "S9", "SM", "SP", "ST", "SU",
      "SV", "SVA", "SV5", "SV9",
      "T2", "T30", "T31", "T32", "T33", "T5", "T7", "T8", "T9", "TA",
      "TF", "TG", "TI", "TI9", "TJ", "TK", "TL", "TN", "TR", "TT",
      "TU", "TY", "TZ",
      "UA", "UA2", "UA9", "UK", "UN", "UR",
      "V2", "V3", "V4", "V5", "V6", "V7", "V8", "VE", "VK", "VK0H",
      "VK0M", "VK9C", "VK9L", "VK9M", "VK9N", "VK9W", "VK9X", "VP2E",
      "VP2M", "VP2V", "VP5", "VP6", "VP6D", "VP8", "VP8G", "VP8H",
      "VP8O", "VP8S", "VP9", "VQ9", "VR", "VU", "VU4", "VU7",
      "XE", "XF4", "XT", "XU", "XW", "XX9", "XZ",
      "YA", "YB", "YI", "YJ", "YK", "YL", "YN", "YO", "YS", "YU", "YV", "YV0",
      "Z2", "Z3", "ZA", "ZB", "ZC4", "ZD7", "ZD8", "ZD9", "ZF", "ZK1N",
      "ZK1S", "ZK2", "ZK3", "ZL", "ZL7", "ZL8", "ZL9", "ZP", "ZS", "ZS8"
      },
  m_sfx {"P",  "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "A"},
  mem_js8 {shdmem},
  m_msAudioOutputBuffered (0u),
  m_framesAudioInputBuffered (RX_SAMPLE_RATE / 10),
  m_downSampleFactor (downSampleFactor),
  m_audioThreadPriority (QThread::HighPriority),
  m_bandEdited {false},
  m_splitMode {false},
  m_monitoring {false},
  m_tx_when_ready {false},
  m_transmitting {false},
  m_tune {false},
  m_tx_watchdog {false},
  m_block_pwr_tooltip {false},
  m_PwrBandSetOK {true},
  m_lastMonitoredFrequency {default_frequency},
  m_toneSpacing {0.},
  m_firstDecode {0},
  m_optimizingProgress {"Optimizing decoder FFTs for your CPU.\n"
      "Please be patient,\n"
      "this may take a few minutes", QString {}, 0, 1, this},
  m_messageClient {new MessageClient {QApplication::applicationName (),
        version (), revision (),
        m_config.udp_server_name (), m_config.udp_server_port (),
        this}},
  m_n3fjpClient { new TCPClient{this}},
  m_spotClient { new SpotClient{m_messageClient, this}},
  m_aprsClient {new APRSISClient{"rotate.aprs2.net", 14580, this}},
  psk_Reporter {new PSK_Reporter {m_messageClient, this}},
  m_i3bit {0},
  m_manual {&m_network_manager},
  m_txFrameCount {0},
  m_txFrameCountSent {0},
  m_txTextDirty {false},
  m_txFrameCountEstimate {0},
  m_previousFreq {0},
  m_hbPaused { false },
  m_hbInterval {0},
  m_cqInterval {0},
  m_cqPaused { false }
{
  ui->setupUi(this);

  createStatusBar();
  add_child_to_event_filter (this);
  ui->dxGridEntry->setValidator (new MaidenheadLocatorValidator {this});
  ui->dxCallEntry->setValidator (new CallsignValidator {this});
  ui->sbTR->values ({5, 10, 15, 30});

  m_baseCall = Radio::base_callsign (m_config.my_callsign ());
  m_opCall = m_config.opCall();

  m_optimizingProgress.setWindowModality (Qt::WindowModal);
  m_optimizingProgress.setAutoReset (false);
  m_optimizingProgress.setMinimumDuration (15000); // only show after 15s delay

  // Closedown.
  connect (ui->actionExit, &QAction::triggered, this, &QMainWindow::close);

  // parts of the rig error message box that are fixed
  m_rigErrorMessageBox.setInformativeText (tr ("Do you want to reconfigure the radio interface?"));
  m_rigErrorMessageBox.setDefaultButton (MessageBox::Ok);

  // start audio thread and hook up slots & signals for shutdown management
  // these objects need to be in the audio thread so that invoking
  // their slots is done in a thread safe way
  m_soundOutput->moveToThread (&m_audioThread);
  m_modulator->moveToThread (&m_audioThread);
  m_soundInput->moveToThread (&m_audioThread);
  m_detector->moveToThread (&m_audioThread);

  // hook up sound output stream slots & signals and disposal
  connect (this, &MainWindow::initializeAudioOutputStream, m_soundOutput, &SoundOutput::setFormat);
  connect (m_soundOutput, &SoundOutput::error, this, &MainWindow::showSoundOutError);
  // connect (m_soundOutput, &SoundOutput::status, this, &MainWindow::showStatusMessage);
  connect (this, &MainWindow::outAttenuationChanged, m_soundOutput, &SoundOutput::setAttenuation);
  connect (&m_audioThread, &QThread::finished, m_soundOutput, &QObject::deleteLater);

  // hook up Modulator slots and disposal
  connect (this, &MainWindow::transmitFrequency, m_modulator, &Modulator::setFrequency);
  connect (this, &MainWindow::endTransmitMessage, m_modulator, &Modulator::stop);
  connect (this, &MainWindow::tune, m_modulator, &Modulator::tune);
  connect (this, &MainWindow::sendMessage, m_modulator, &Modulator::start);
  connect (&m_audioThread, &QThread::finished, m_modulator, &QObject::deleteLater);

  // hook up the audio input stream signals, slots and disposal
  connect (this, &MainWindow::startAudioInputStream, m_soundInput, &SoundInput::start);
  connect (this, &MainWindow::suspendAudioInputStream, m_soundInput, &SoundInput::suspend);
  connect (this, &MainWindow::resumeAudioInputStream, m_soundInput, &SoundInput::resume);
  connect (this, &MainWindow::finished, m_soundInput, &SoundInput::stop);
  connect(m_soundInput, &SoundInput::error, this, &MainWindow::showSoundInError);
  // connect(m_soundInput, &SoundInput::status, this, &MainWindow::showStatusMessage);
  connect (&m_audioThread, &QThread::finished, m_soundInput, &QObject::deleteLater);

  connect (this, &MainWindow::finished, this, &MainWindow::close);

  // hook up the detector signals, slots and disposal
  connect (this, &MainWindow::FFTSize, m_detector, &Detector::setBlockSize);
  connect(m_detector, &Detector::framesWritten, this, &MainWindow::dataSink);
  connect (&m_audioThread, &QThread::finished, m_detector, &QObject::deleteLater);

  // setup the waterfall
  connect(m_wideGraph.data (), SIGNAL(freezeDecode2(int)),this,SLOT(freezeDecode(int)));
  connect(m_wideGraph.data (), SIGNAL(f11f12(int)),this,SLOT(bumpFqso(int)));
  connect(m_wideGraph.data (), SIGNAL(setXIT2(int)),this,SLOT(setXIT(int)));

  connect (m_fastGraph.data (), &FastGraph::fastPick, this, &MainWindow::fastPick);

  connect (this, &MainWindow::finished, m_wideGraph.data (), &WideGraph::close);
  connect (this, &MainWindow::finished, m_echoGraph.data (), &EchoGraph::close);
  connect (this, &MainWindow::finished, m_fastGraph.data (), &FastGraph::close);

  // setup the log QSO dialog
  connect (m_logDlg.data (), &LogQSO::acceptQSO, this, &MainWindow::acceptQSO);
  connect (this, &MainWindow::finished, m_logDlg.data (), &LogQSO::close);


  // Network message handlers
  connect (m_messageClient, &MessageClient::error, this, &MainWindow::networkError);
  connect (m_messageClient, &MessageClient::message, this, &MainWindow::networkMessage);

#if 0
  // Hook up WSPR band hopping
  connect (ui->band_hopping_schedule_push_button, &QPushButton::clicked
           , &m_WSPR_band_hopping, &WSPRBandHopping::show_dialog);
  connect (ui->sbTxPercent, static_cast<void (QSpinBox::*) (int)> (&QSpinBox::valueChanged)
           , &m_WSPR_band_hopping, &WSPRBandHopping::set_tx_percent);
#endif

  on_EraseButton_clicked ();

  QActionGroup* modeGroup = new QActionGroup(this);
  ui->actionFT8->setActionGroup(modeGroup);
  ui->actionJT9->setActionGroup(modeGroup);
  ui->actionJT65->setActionGroup(modeGroup);
  ui->actionJT9_JT65->setActionGroup(modeGroup);
  ui->actionJT4->setActionGroup(modeGroup);
  ui->actionWSPR->setActionGroup(modeGroup);
  ui->actionWSPR_LF->setActionGroup(modeGroup);
  ui->actionEcho->setActionGroup(modeGroup);
  ui->actionISCAT->setActionGroup(modeGroup);
  ui->actionMSK144->setActionGroup(modeGroup);
  ui->actionQRA64->setActionGroup(modeGroup);
  ui->actionFreqCal->setActionGroup(modeGroup);

  QActionGroup* saveGroup = new QActionGroup(this);
  ui->actionNone->setActionGroup(saveGroup);
  ui->actionSave_decoded->setActionGroup(saveGroup);
  ui->actionSave_all->setActionGroup(saveGroup);

  QActionGroup* DepthGroup = new QActionGroup(this);
  ui->actionQuickDecode->setActionGroup(DepthGroup);
  ui->actionMediumDecode->setActionGroup(DepthGroup);
  ui->actionDeepDecode->setActionGroup(DepthGroup);
  ui->actionDeepestDecode->setActionGroup(DepthGroup);

  connect (ui->view_phase_response_action, &QAction::triggered, [this] () {
      if (!m_equalizationToolsDialog)
        {
          m_equalizationToolsDialog.reset (new EqualizationToolsDialog {m_settings, m_config.writeable_data_dir (), m_phaseEqCoefficients, this});
          connect (m_equalizationToolsDialog.data (), &EqualizationToolsDialog::phase_equalization_changed,
                   [this] (QVector<double> const& coeffs) {
                     m_phaseEqCoefficients = coeffs;
                   });
        }
      m_equalizationToolsDialog->show ();
    });

  QButtonGroup* txMsgButtonGroup = new QButtonGroup {this};
  txMsgButtonGroup->addButton(ui->txrb1,1);
  txMsgButtonGroup->addButton(ui->txrb2,2);
  txMsgButtonGroup->addButton(ui->txrb3,3);
  txMsgButtonGroup->addButton(ui->txrb4,4);
  txMsgButtonGroup->addButton(ui->txrb5,5);
  txMsgButtonGroup->addButton(ui->txrb6,6);
  set_dateTimeQSO(-1);
  connect(txMsgButtonGroup,SIGNAL(buttonClicked(int)),SLOT(set_ntx(int)));

  // initialize decoded text font and hook up font change signals
  // defer initialization until after construction otherwise menu
  // fonts do not get set
  QTimer::singleShot (0, this, SLOT (initialize_fonts ()));
  connect (&m_config, &Configuration::gui_text_font_changed, [this] (QFont const& font) {
      set_application_font (font);
  });
  connect (&m_config, &Configuration::table_font_changed, [this] (QFont const&) {
      ui->tableWidgetRXAll->setFont(m_config.table_font());
      ui->tableWidgetCalls->setFont(m_config.table_font());
  });
  connect (&m_config, &Configuration::rx_text_font_changed, [this] (QFont const&) {
      setTextEditFont(ui->textEditRX, m_config.rx_text_font());
  });
  connect (&m_config, &Configuration::compose_text_font_changed, [this] (QFont const&) {
      setTextEditFont(ui->extFreeTextMsgEdit, m_config.compose_text_font());
  });
  connect (&m_config, &Configuration::colors_changed, [this](){
     setTextEditStyle(ui->textEditRX, m_config.color_rx_foreground(), m_config.color_rx_background(), m_config.rx_text_font());
     setTextEditStyle(ui->extFreeTextMsgEdit, m_config.color_compose_foreground(), m_config.color_compose_background(), m_config.compose_text_font());
     ui->extFreeTextMsgEdit->setFont(m_config.compose_text_font(), m_config.color_compose_foreground(), m_config.color_compose_background());

     // rehighlight
     auto d = ui->textEditRX->document();
     if(d){
         for(int i = 0; i < d->lineCount(); i++){
             auto b = d->findBlockByLineNumber(i);

             switch(b.userState()){
             case STATE_RX:
                 highlightBlock(b, m_config.rx_text_font(), m_config.color_rx_foreground(), QColor(Qt::transparent));
                 break;
             case STATE_TX:
                 highlightBlock(b, m_config.tx_text_font(), m_config.color_tx_foreground(), QColor(Qt::transparent));
                 break;
             }
         }
     }


  });

  setWindowTitle (program_title ());

  connect(&proc_js8, &QProcess::readyReadStandardOutput, this, &MainWindow::readFromStdout);
  connect(&proc_js8, static_cast<void (QProcess::*) (QProcess::ProcessError)> (&QProcess::error),
          [this] (QProcess::ProcessError error) {
            subProcessError (&proc_js8, error);
          });
  connect(&proc_js8, static_cast<void (QProcess::*) (int, QProcess::ExitStatus)> (&QProcess::finished),
          [this] (int exitCode, QProcess::ExitStatus status) {
            subProcessFailed (&proc_js8, exitCode, status);
          });

  connect(&p1, &QProcess::readyReadStandardOutput, this, &MainWindow::p1ReadFromStdout);
  connect(&p1, static_cast<void (QProcess::*) (QProcess::ProcessError)> (&QProcess::error),
          [this] (QProcess::ProcessError error) {
            subProcessError (&p1, error);
          });
  connect(&p1, static_cast<void (QProcess::*) (int, QProcess::ExitStatus)> (&QProcess::finished),
          [this] (int exitCode, QProcess::ExitStatus status) {
            subProcessFailed (&p1, exitCode, status);
          });

  connect(&p3, static_cast<void (QProcess::*) (QProcess::ProcessError)> (&QProcess::error),
          [this] (QProcess::ProcessError error) {
            subProcessError (&p3, error);
          });
  connect(&p3, static_cast<void (QProcess::*) (int, QProcess::ExitStatus)> (&QProcess::finished),
          [this] (int exitCode, QProcess::ExitStatus status) {
            subProcessFailed (&p3, exitCode, status);
          });

  // hook up save WAV file exit handling
  connect (&m_saveWAVWatcher, &QFutureWatcher<QString>::finished, [this] {
      // extract the promise from the future
      auto const& result = m_saveWAVWatcher.future ().result ();
      if (!result.isEmpty ())   // error
        {
          MessageBox::critical_message (this, tr("Error Writing WAV File"), result);
        }
    });

  // Hook up working frequencies.
  ui->currentFreq->setCursor(QCursor(Qt::PointingHandCursor));
  ui->currentFreq->display("14.078 000");
  auto cfmp = new MousePressEater();
  connect(cfmp, &MousePressEater::mousePressed, this, [this](QObject *, QMouseEvent * e, bool *pProcessed){
      QMenu * menu = new QMenu(ui->currentFreq);
      buildFrequencyMenu(menu);
      menu->popup(e->globalPos());
      if(pProcessed) *pProcessed = true;
  });
  ui->currentFreq->installEventFilter(cfmp);

  ui->labDialFreqOffset->setCursor(QCursor(Qt::PointingHandCursor));
  auto ldmp = new MousePressEater();
  connect(ldmp, &MousePressEater::mousePressed, this, [this](QObject *, QMouseEvent *, bool *pProcessed){
      on_actionSetOffset_triggered();

      if(pProcessed) *pProcessed = true;
  });
  ui->labDialFreqOffset->installEventFilter(ldmp);

  // Hook up callsign label click to open preferenses
  ui->labCallsign->setCursor(QCursor(Qt::PointingHandCursor));
  auto clmp = new MousePressEater();
  connect(clmp, &MousePressEater::mousePressed, this, [this](QObject *, QMouseEvent * e, bool *pProcessed){
      openSettings(0);
      if(pProcessed) *pProcessed = true;
  });
  ui->labCallsign->installEventFilter(clmp);

  ui->bandComboBox->setVisible(false);
  ui->bandComboBox->setModel (m_config.frequencies ());
  ui->bandComboBox->setModelColumn (FrequencyList_v2::frequency_mhz_column);

  // combo box drop down width defaults to the line edit + decorator width,
  // here we change that to the column width size hint of the model column
  ui->bandComboBox->view ()->setMinimumWidth (ui->bandComboBox->view ()->sizeHintForColumn (FrequencyList_v2::frequency_mhz_column));

  // Enable live band combo box entry validation and action.
  auto band_validator = new LiveFrequencyValidator {ui->bandComboBox
                                                    , m_config.bands ()
                                                    , m_config.frequencies ()
                                                    , &m_freqNominal
                                                    , this};
  ui->bandComboBox->setValidator (band_validator);

  // Hook up signals.
  connect (band_validator, &LiveFrequencyValidator::valid, this, &MainWindow::band_changed);
  connect (ui->bandComboBox->lineEdit (), &QLineEdit::textEdited, [this] (QString const&) {m_bandEdited = true;});

  // hook up configuration signals
  connect (&m_config, &Configuration::transceiver_update, this, &MainWindow::handle_transceiver_update);
  connect (&m_config, &Configuration::transceiver_failure, this, &MainWindow::handle_transceiver_failure);
  connect (&m_config, &Configuration::udp_server_changed, m_messageClient, &MessageClient::set_server);
  connect (&m_config, &Configuration::udp_server_port_changed, m_messageClient, &MessageClient::set_server_port);
  connect (&m_config, &Configuration::band_schedule_changed, this, [this](){
    this->m_bandHopped = true;
  });

  // set up configurations menu
  connect (m_multi_settings, &MultiSettings::configurationNameChanged, [this] (QString const& name) {
      if ("Default" != name) {
        config_label.setText (name);
        config_label.show ();
      }
      else {
        config_label.hide ();
      }
    });
  m_multi_settings->create_menu_actions (this, ui->menuConfig);
  m_configurations_button = m_rigErrorMessageBox.addButton (tr ("Configurations...")
                                                            , QMessageBox::ActionRole);

  // set up message text validators
  ui->tx1->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->tx2->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->tx3->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->tx4->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->tx5->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->tx6->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->freeTextMsg->setValidator (new QRegExpValidator {message_alphabet, this});
  ui->nextFreeTextMsg->setValidator (new QRegExpValidator {message_alphabet, this});
  //ui->extFreeTextMsg->setValidator (new QRegExpValidator {message_alphabet, this});

  // Free text macros model to widget hook up.
  //ui->tx5->setModel (m_config.macros ());
  //connect (ui->tx5->lineEdit(), &QLineEdit::editingFinished,
  //         [this] () {on_tx5_currentTextChanged (ui->tx5->lineEdit()->text());});
  //ui->freeTextMsg->setModel (m_config.macros ());
  connect (ui->freeTextMsg->lineEdit ()
           , &QLineEdit::editingFinished
           , [this] () {on_freeTextMsg_currentTextChanged (ui->freeTextMsg->lineEdit ()->text ());});
  connect (ui->nextFreeTextMsg
           , &QLineEdit::editingFinished
           , [this] () {on_nextFreeTextMsg_currentTextChanged (ui->nextFreeTextMsg->text ());});
  connect (ui->extFreeTextMsgEdit
           , &QTextEdit::textChanged
           , [this] () {on_extFreeTextMsgEdit_currentTextChanged (ui->extFreeTextMsgEdit->toPlainText ());});

  m_guiTimer.setSingleShot(true);
  connect(&m_guiTimer, &QTimer::timeout, this, &MainWindow::guiUpdate);
  m_guiTimer.start(100);   //### Don't change the 100 ms! ###

  ptt0Timer.setSingleShot(true);
  connect(&ptt0Timer, &QTimer::timeout, this, &MainWindow::stopTx2);

  ptt1Timer.setSingleShot(true);
  connect(&ptt1Timer, &QTimer::timeout, this, &MainWindow::startTx2);

  p1Timer.setSingleShot(true);
  connect(&p1Timer, &QTimer::timeout, this, &MainWindow::startP1);

  logQSOTimer.setSingleShot(true);
  connect(&logQSOTimer, &QTimer::timeout, this, &MainWindow::on_logQSOButton_clicked);

  tuneButtonTimer.setSingleShot(true);
  connect(&tuneButtonTimer, &QTimer::timeout, this, &MainWindow::on_stopTxButton_clicked);

  tuneATU_Timer.setSingleShot(true);
  connect(&tuneATU_Timer, &QTimer::timeout, this, &MainWindow::stopTuneATU);

  killFileTimer.setSingleShot(true);
  connect(&killFileTimer, &QTimer::timeout, this, &MainWindow::killFile);

  uploadTimer.setSingleShot(true);
  connect(&uploadTimer, SIGNAL(timeout()), this, SLOT(uploadSpots()));

  TxAgainTimer.setSingleShot(true);
  connect(&TxAgainTimer, SIGNAL(timeout()), this, SLOT(TxAgain()));

  repeatTimer.setSingleShot(false);
  repeatTimer.setInterval(1000);
  connect(&repeatTimer, &QTimer::timeout, this, &MainWindow::checkRepeat);

  connect(m_wideGraph.data (), SIGNAL(setFreq3(int,int)),this,
          SLOT(setFreq4(int,int)));

  connect(m_wideGraph.data(), &WideGraph::qsy, this, &MainWindow::qsy);

  decodeBusy(false);
  QString t1[28]={"1 uW","2 uW","5 uW","10 uW","20 uW","50 uW","100 uW","200 uW","500 uW",
                  "1 mW","2 mW","5 mW","10 mW","20 mW","50 mW","100 mW","200 mW","500 mW",
                  "1 W","2 W","5 W","10 W","20 W","50 W","100 W","200 W","500 W","1 kW"};

  m_msg[0][0]=0;
  m_bQRAsyncWarned=false;
  ui->labDXped->setVisible(false);

  for(int i=0; i<28; i++)  {                      //Initialize dBm values
    float dbm=(10.0*i)/3.0 - 30.0;
    int ndbm=0;
    if(dbm<0) ndbm=int(dbm-0.5);
    if(dbm>=0) ndbm=int(dbm+0.5);
    QString t;
    t.sprintf("%d dBm  ",ndbm);
    t+=t1[i];
    ui->TxPowerComboBox->addItem(t);
  }

  ui->labAz->setStyleSheet("border: 0px;");
//  ui->labDist->setStyleSheet("border: 0px;");

  auto t = "UTC   dB   DT Freq    Message";
  ui->decodedTextLabel->setText(t);
  ui->decodedTextLabel2->setText(t);

  displayDialFrequency();
  readSettings();            //Restore user's setup params
  m_audioThread.start (m_audioThreadPriority);

#ifdef WIN32
  if (!m_multiple)
    {
      while(true)
        {
          int iret=killbyname("js8.exe");
          if(iret == 603) break;
          if(iret != 0)
            MessageBox::warning_message (this, tr ("Error Killing js8.exe Process")
                                         , tr ("KillByName return code: %1")
                                         .arg (iret));
        }
    }
#endif

  {
    //delete any .quit file that might have been left lying around
    //since its presence will cause jt9 to exit a soon as we start it
    //and decodes will hang
    QFile quitFile {m_config.temp_dir ().absoluteFilePath (".quit")};
    while (quitFile.exists ())
      {
        if (!quitFile.remove ())
          {
            MessageBox::query_message (this, tr ("Error removing \"%1\"").arg (quitFile.fileName ())
                                       , tr ("Click OK to retry"));
          }
      }
  }

  //Create .lock so jt9 will wait
  QFile {m_config.temp_dir ().absoluteFilePath (".lock")}.open(QIODevice::ReadWrite);

  QStringList js8_args {
    "-s", QApplication::applicationName () // shared memory key,
                                           // includes rig
#ifdef NDEBUG
      , "-w", "1"               //FFTW patience - release
#else
      , "-w", "1"               //FFTW patience - debug builds for speed
#endif
      // The number  of threads for  FFTW specified here is  chosen as
      // three because  that gives  the best  throughput of  the large
      // FFTs used  in jt9.  The count  is the minimum of  (the number
      // available CPU threads less one) and three.  This ensures that
      // there is always at least one free CPU thread to run the other
      // mode decoder in parallel.
      , "-m", QString::number (qMin (qMax (QThread::idealThreadCount () - 1, 1), 3)) //FFTW threads

      , "-e", QDir::toNativeSeparators (m_appDir)
      , "-a", QDir::toNativeSeparators (m_config.writeable_data_dir ().absolutePath ())
      , "-t", QDir::toNativeSeparators (m_config.temp_dir ().absolutePath ())
      };
  QProcessEnvironment env {QProcessEnvironment::systemEnvironment ()};
  env.insert ("OMP_STACKSIZE", "4M");
  proc_js8.setProcessEnvironment (env);
  proc_js8.start(QDir::toNativeSeparators (m_appDir) + QDir::separator () +
          "js8", js8_args, QIODevice::ReadWrite | QIODevice::Unbuffered);

  QString fname {QDir::toNativeSeparators(m_config.writeable_data_dir ().absoluteFilePath ("wsjtx_wisdom.dat"))};
  QByteArray cfname=fname.toLocal8Bit();
  fftwf_import_wisdom_from_filename(cfname);

  m_ntx = 6;
  ui->txrb6->setChecked(true);

  connect (&m_wav_future_watcher, &QFutureWatcher<void>::finished, this, &MainWindow::diskDat);

//  Q_EMIT startAudioInputStream (m_config.audio_input_device (), m_framesAudioInputBuffered, &m_detector, m_downSampleFactor, m_config.audio_input_channel ());
  Q_EMIT startAudioInputStream (m_config.audio_input_device (), m_framesAudioInputBuffered, m_detector, m_downSampleFactor, m_config.audio_input_channel ());
  Q_EMIT initializeAudioOutputStream (m_config.audio_output_device (), AudioDevice::Mono == m_config.audio_output_channel () ? 1 : 2, m_msAudioOutputBuffered);
  Q_EMIT transmitFrequency (ui->TxFreqSpinBox->value () - m_XIT);

  enable_DXCC_entity (m_config.DXCC ());  // sets text window proportions and (re)inits the logbook

  ui->label_9->setStyleSheet("QLabel{background-color: #aabec8}");
  ui->label_10->setStyleSheet("QLabel{background-color: #aabec8}");

  // this must be done before initializing the mode as some modes need
  // to turn off split on the rig e.g. WSPR
  m_config.transceiver_online ();
  bool vhf {m_config.enable_VHF_features ()};

  morse_(const_cast<char *> (m_config.my_callsign ().toLatin1().constData()),
         const_cast<int *> (icw), &m_ncw, m_config.my_callsign ().length());
  on_actionWide_Waterfall_triggered();
  ui->cbShMsgs->setChecked(m_bShMsgs);
  ui->cbSWL->setChecked(m_bSWL);
  if(m_bFast9) m_bFastMode=true;
  ui->cbFast9->setChecked(m_bFast9 or m_bFastMode);

  if(true || m_mode=="FT8") on_actionJS8_triggered();

  // TODO: jsherer - is this needed?
  //ui->sbSubmode->setValue (vhf ? m_nSubMode : 0);

  if(m_mode=="MSK144") {
    Q_EMIT transmitFrequency (1000.0);
  } else {
    Q_EMIT transmitFrequency (ui->TxFreqSpinBox->value() - m_XIT);
  }
  m_saveDecoded=ui->actionSave_decoded->isChecked();
  m_saveAll=ui->actionSave_all->isChecked();
  ui->sbTxPercent->setValue(m_pctx);
  ui->TxPowerComboBox->setCurrentIndex(int(0.3*(m_dBm + 30.0)+0.2));
  ui->cbUploadWSPR_Spots->setChecked(m_uploadSpots);
  if((m_ndepth&7)==1) ui->actionQuickDecode->setChecked(true);
  if((m_ndepth&7)==2) ui->actionMediumDecode->setChecked(true);
  if((m_ndepth&7)==3) ui->actionDeepDecode->setChecked(true);
  if((m_ndepth&7)==4) ui->actionDeepestDecode->setChecked(true);
  ui->actionInclude_averaging->setChecked(m_ndepth&16);
  ui->actionInclude_correlation->setChecked(m_ndepth&32);
  ui->actionEnable_AP_DXcall->setChecked(m_ndepth&64);

  m_UTCdisk=-1;
  m_fCPUmskrtd=0.0;
  m_bFastDone=false;
  m_bAltV=false;
  m_bNoMoreFiles=false;
  m_bVHFwarned=false;
  m_bDoubleClicked=false;
  m_bCallingCQ=false;
  m_bCheckedContest=false;
  m_bDisplayedOnce=false;
  m_wait=0;
  m_isort=-3;
  m_max_dB=30;
  m_CQtype="CQ";

  if(m_mode.startsWith ("WSPR") and m_pctx>0)  {
    QPalette palette {ui->sbTxPercent->palette ()};
    palette.setColor(QPalette::Base,Qt::yellow);
    ui->sbTxPercent->setPalette(palette);
  }
  fixStop();
  VHF_features_enabled(m_config.enable_VHF_features());
  m_wideGraph->setVHF(m_config.enable_VHF_features());

  connect( wsprNet, SIGNAL(uploadStatus(QString)), this, SLOT(uploadResponse(QString)));

  statusChanged();

  m_fastGraph->setMode(m_mode);
  m_wideGraph->setMode(m_mode);
  m_wideGraph->setModeTx(m_modeTx);

  connect (&minuteTimer, &QTimer::timeout, this, &MainWindow::on_the_minute);
  minuteTimer.setSingleShot (true);
  minuteTimer.start (ms_minute_error () + 60 * 1000);

  //connect (&splashTimer, &QTimer::timeout, this, &MainWindow::splash_done);
  //splashTimer.setSingleShot (true);
  //splashTimer.start (20 * 1000);

  QTimer::singleShot (0, this, SLOT (checkStartupWarnings ()));

  if(!ui->cbMenus->isChecked()) {
    ui->cbMenus->setChecked(true);
    ui->cbMenus->setChecked(false);
  }

  //UI Customizations & Tweaks
  m_wideGraph.data()->installEventFilter(new EscapeKeyPressEater());
  ui->mdiArea->addSubWindow(m_wideGraph.data(), Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::Tool)->showMaximized();
  //ui->menuDecode->setEnabled(true);
  ui->menuMode->setVisible(false);
  ui->menuSave->setEnabled(true);
  ui->menuTools->setEnabled(false);
  ui->menuView->setEnabled(false);
  foreach(auto action, ui->menuBar->actions()){
      if(action->text() == "Old View") ui->menuBar->removeAction(action);
      if(action->text() == "Old Mode") ui->menuBar->removeAction(action);
      if(action->text() == "Old Tools") ui->menuBar->removeAction(action);
  }
  ui->dxCallEntry->clear();
  ui->dxGridEntry->clear();

  //auto f = findFreeFreqOffset(1000, 2000, 50);
  //setFreqOffsetForRestore(f, false);

  ui->cbVHFcontest->setChecked(false); // this needs to always be false

  ui->actionModeAutoreply->setChecked(m_config.autoreply_on_at_startup());
  ui->spotButton->setChecked(m_config.spot_to_reporting_networks());

  QActionGroup * modeActionGroup = new QActionGroup(this);
  ui->actionModeJS8Normal->setActionGroup(modeActionGroup);
  ui->actionModeJS8Fast->setActionGroup(modeActionGroup);
  ui->actionModeJS8Turbo->setActionGroup(modeActionGroup);
  ui->actionModeJS8Ultra->setActionGroup(modeActionGroup);

  auto mbmp = new MousePressEater();
  connect(mbmp, &MousePressEater::mousePressed, this, [this](QObject *, QMouseEvent * e, bool *pProcessed){
      ui->menuModeJS8->popup(e->globalPos());
      if(pProcessed) *pProcessed = true;
  });
  ui->modeButton->installEventFilter(mbmp);
  if(!JS8_ENABLE_JS8B){
      ui->actionModeJS8Fast->setVisible(false);
  }
  if(!JS8_ENABLE_JS8C){
      ui->actionModeJS8Turbo->setVisible(false);
  }
  if(!JS8_ENABLE_JS8D){
      ui->actionModeJS8Ultra->setVisible(false);
  }

  // prep
  prepareHeartbeatMode(ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked());
  prepareSpotting();

  auto enterFilter = new EnterKeyPressEater();
  connect(enterFilter, &EnterKeyPressEater::enterKeyPressed, this, [this](QObject *, QKeyEvent *, bool *pProcessed){
      if(QApplication::keyboardModifiers() & Qt::ShiftModifier){
          if(pProcessed) *pProcessed = false;
          return;
      }
      if(ui->extFreeTextMsgEdit->isReadOnly()){
          if(pProcessed) *pProcessed = false;
          return;
      }

      if(pProcessed) *pProcessed = true;

      if(ui->extFreeTextMsgEdit->toPlainText().trimmed().isEmpty()){
          return;
      }

      if(!ensureCallsignSet(true)){
          return;
      }

      toggleTx(true);
  });
  ui->extFreeTextMsgEdit->installEventFilter(enterFilter);

  auto doubleClickFilter = new MouseDoubleClickEater();
  connect(doubleClickFilter, &MouseDoubleClickEater::mouseDoubleClicked, this, [this](QObject *, QMouseEvent *, bool *){
      QTimer::singleShot(150, this, &MainWindow::on_textEditRX_mouseDoubleClicked);
  });
  ui->textEditRX->viewport()->installEventFilter(doubleClickFilter);

  auto clearActionSep = new QAction(nullptr);
  clearActionSep->setSeparator(true);

  auto clearActionAll = new QAction(QString("Clear All"), nullptr);
  connect(clearActionAll, &QAction::triggered, this, [this](){
      if (QMessageBox::Yes != QMessageBox::question(this, "Clear All Activity", "Are you sure you would like to clear all activity?", QMessageBox::Yes|QMessageBox::No)){
          return;
      }

      clearActivity();
  });

  // setup tablewidget context menus
  auto clearAction1 = new QAction(QString("Clear"), ui->textEditRX);
  connect(clearAction1, &QAction::triggered, this, [this](){ this->on_clearAction_triggered(ui->textEditRX); });

  auto saveAction = new QAction(QString("Save As..."), ui->textEditRX);
  connect(saveAction, &QAction::triggered, this, [this](){
    auto writePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    auto writeDir = QDir(writePath);
    auto defaultFilename = writeDir.absoluteFilePath(QString("js8call-%1.txt").arg(DriftingDateTime::currentDateTimeUtc().toString("yyyyMMdd")));

    QString selectedFilter = "*.txt";

    auto filename = QFileDialog::getSaveFileName(this,
        "Save As...",
        defaultFilename,
        "Text files (*.txt);; All files (*)",
        &selectedFilter
    );
    if(filename.isEmpty()){
        return;
    }

    auto text = ui->textEditRX->toPlainText();
    QFile f(filename);
    if (f.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&f);
        stream << text;
    }
  });

  ui->textEditRX->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->textEditRX, &QTableWidget::customContextMenuRequested, this, [this, clearAction1, clearActionAll, saveAction](QPoint const &point){
      QMenu * menu = new QMenu(ui->textEditRX);

      buildEditMenu(menu, ui->textEditRX);

      menu->addSeparator();

      menu->addAction(clearAction1);
      menu->addAction(clearActionAll);

      menu->addSeparator();
      menu->addAction(saveAction);

      menu->popup(ui->textEditRX->mapToGlobal(point));

  });

  auto clearAction2 = new QAction(QString("Clear"), ui->extFreeTextMsgEdit);
  connect(clearAction2, &QAction::triggered, this, [this](){ this->on_clearAction_triggered(ui->extFreeTextMsgEdit); });

  auto restoreAction = new QAction(QString("Restore Previous Message"), ui->extFreeTextMsgEdit);
  connect(restoreAction, &QAction::triggered, this, [this](){ this->restoreMessage(); });

  ui->extFreeTextMsgEdit->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->extFreeTextMsgEdit, &QTableWidget::customContextMenuRequested, this, [this, clearAction2, clearActionAll, restoreAction](QPoint const &point){
    QMenu * menu = new QMenu(ui->extFreeTextMsgEdit);

    auto selectedCall = callsignSelected();
    bool missingCallsign = selectedCall.isEmpty();

    buildSuggestionsMenu(menu, ui->extFreeTextMsgEdit, point);

    restoreAction->setDisabled(m_lastTxMessage.isEmpty());
    menu->addAction(restoreAction);

    auto savedMenu = menu->addMenu("Saved Messages...");
    buildSavedMessagesMenu(savedMenu);

    auto directedMenu = menu->addMenu(QString("Directed to %1...").arg(selectedCall));
    directedMenu->setDisabled(missingCallsign);
    buildQueryMenu(directedMenu, selectedCall);

    auto relayMenu = menu->addMenu("Relay via...");
    relayMenu->setDisabled(ui->extFreeTextMsgEdit->toPlainText().isEmpty() || m_callActivity.isEmpty());
    buildRelayMenu(relayMenu);

    menu->addSeparator();

    buildEditMenu(menu, ui->extFreeTextMsgEdit);

    menu->addSeparator();

    menu->addAction(clearAction2);
    menu->addAction(clearActionAll);

    menu->popup(ui->extFreeTextMsgEdit->mapToGlobal(point));

    displayActivity(true);
  });



  auto clearAction3 = new QAction(QString("Clear"), ui->tableWidgetRXAll);
  connect(clearAction3, &QAction::triggered, this, [this](){ this->on_clearAction_triggered(ui->tableWidgetRXAll); });

  auto removeActivity = new QAction(QString("Remove Activity"), ui->tableWidgetRXAll);
  connect(removeActivity, &QAction::triggered, this, [this](){
      if(ui->tableWidgetRXAll->selectedItems().isEmpty()){
          return;
      }

      auto selectedItems = ui->tableWidgetRXAll->selectedItems();
      int selectedOffset = selectedItems.first()->data(Qt::UserRole).toInt();

      m_bandActivity.remove(selectedOffset);
      displayActivity(true);
  });

  auto logAction = new QAction(QString("Log..."), ui->tableWidgetCalls);
  connect(logAction, &QAction::triggered, this, &MainWindow::on_logQSOButton_clicked);


  ui->tableWidgetRXAll->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->tableWidgetRXAll->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this](QPoint const &point){
      QMenu * menu = new QMenu(ui->tableWidgetRXAll);

      QMenu * sortByMenu = menu->addMenu("Sort By...");
      buildBandActivitySortByMenu(sortByMenu);

      QMenu * showColumnsMenu = menu->addMenu("Show Columns...");
      buildShowColumnsMenu(showColumnsMenu, "band");

      menu->popup(ui->tableWidgetRXAll->horizontalHeader()->mapToGlobal(point));
  });


  ui->tableWidgetRXAll->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->tableWidgetRXAll, &QTableWidget::customContextMenuRequested, this, [this, clearAction3, clearActionAll, removeActivity, logAction](QPoint const &point){
    QMenu * menu = new QMenu(ui->tableWidgetRXAll);

    // clear the selection of the call widget on right click
    ui->tableWidgetCalls->selectionModel()->clearSelection();

    QString selectedCall = callsignSelected();
    bool missingCallsign = selectedCall.isEmpty();
    bool isAllCall = isAllCallIncluded(selectedCall);

    int selectedOffset = -1;
    if(!ui->tableWidgetRXAll->selectedItems().isEmpty()){
        auto selectedItems = ui->tableWidgetRXAll->selectedItems();
        selectedOffset = selectedItems.first()->data(Qt::UserRole).toInt();
    }

    if(selectedOffset != -1){
        auto qsyAction = menu->addAction(QString("Jump to %1Hz").arg(selectedOffset));
        connect(qsyAction, &QAction::triggered, this, [this, selectedOffset](){
            setFreqOffsetForRestore(selectedOffset, false);
            // TODO: prompt mode switch?
        });
        menu->addSeparator();
    }

    menu->addAction(logAction);
    logAction->setDisabled(missingCallsign || isAllCall);

    menu->addSeparator();

    auto savedMenu = menu->addMenu("Saved Messages...");
    buildSavedMessagesMenu(savedMenu);

    auto directedMenu = menu->addMenu(QString("Directed to %1...").arg(selectedCall));
    directedMenu->setDisabled(missingCallsign);
    buildQueryMenu(directedMenu, selectedCall);

    auto relayAction = buildRelayAction(selectedCall);
    relayAction->setText(QString("Relay via %1...").arg(selectedCall));
    relayAction->setDisabled(missingCallsign);
    menu->addActions({ relayAction });

    auto deselectAction = menu->addAction(QString("Deselect %1").arg(selectedCall));
    deselectAction->setDisabled(missingCallsign);
    connect(deselectAction, &QAction::triggered, this, [this](){
        ui->tableWidgetRXAll->clearSelection();
        ui->tableWidgetCalls->clearSelection();
    });

    // savedMenu->setEnabled(savedMenu->isEnabled() && !ui->actionModeJS8HB->isChecked());
    // directedMenu->setEnabled(directedMenu->isEnabled() && !ui->actionModeJS8HB->isChecked());
    // relayAction->setEnabled(relayAction->isEnabled() && !ui->actionModeJS8HB->isChecked());

    menu->addSeparator();

    removeActivity->setDisabled(selectedOffset == -1);
    menu->addAction(removeActivity);

    menu->addSeparator();
    menu->addAction(clearAction3);
    menu->addAction(clearActionAll);

    menu->popup(ui->tableWidgetRXAll->mapToGlobal(point));

    displayActivity(true);
  });




  auto clearAction4 = new QAction(QString("Clear"), ui->tableWidgetCalls);
  connect(clearAction4, &QAction::triggered, this, [this](){ this->on_clearAction_triggered(ui->tableWidgetCalls); });

  auto addStation = new QAction(QString("Add New Station or Group..."), ui->tableWidgetCalls);
  connect(addStation, &QAction::triggered, this, [this](){
      bool ok = false;
      QString callsign = QInputDialog::getText(this, tr("Add New Station or Group"),
                                               tr("Station or Group Callsign:"), QLineEdit::Normal,
                                               "", &ok).toUpper().trimmed();
      if(!ok || callsign.trimmed().isEmpty()){
         return;
      }

      // if we're adding allcall, turn off allcall avoidance
      if(callsign == "@ALLCALL"){
          m_config.set_avoid_allcall(false);
      }
      else if(callsign.startsWith("@")){
          if(Varicode::isCompoundCallsign(callsign)){
              m_config.addGroup(callsign);
          } else {
              MessageBox::critical_message (this, QString("%1 is not a valid group").arg(callsign));
          }

      } else {
          if(Varicode::isValidCallsign(callsign, nullptr)){
              CallDetail cd = {};
              cd.call = callsign;
              m_callActivity[callsign] = cd;
          } else {
              MessageBox::critical_message (this, QString("%1 is not a valid callsign or group").arg(callsign));
          }
      }

      displayActivity(true);
  });

  auto removeStation = new QAction(QString("Remove Station"), ui->tableWidgetCalls);
  connect(removeStation, &QAction::triggered, this, [this](){
      QString selectedCall = callsignSelected();
      if(selectedCall.isEmpty()){
          return;
      }

      if (selectedCall == "@ALLCALL"){
          m_config.set_avoid_allcall(true);
      }
      else if (selectedCall.startsWith("@")){
          m_config.removeGroup(selectedCall);
      }
      else if(m_callActivity.contains(selectedCall)){
          m_callActivity.remove(selectedCall);
      }

      displayActivity(true);
  });

  auto historyAction = new QAction(QString("Show Message Inbox..."), ui->tableWidgetCalls);
  connect(ui->actionShow_Message_Inbox, &QAction::triggered, historyAction, &QAction::trigger);
  connect(historyAction, &QAction::triggered, this, [this](){
      QString selectedCall = callsignSelected();
      if(selectedCall.isEmpty()){
          selectedCall = "%";
      }

      Inbox inbox(inboxPath());
      if(!inbox.open()){
          return;
      }

      QList<QPair<int, Message> > msgs;

      msgs.append(inbox.values("STORE", "$.params.TO", selectedCall, 0, 1000));

      msgs.append(inbox.values("READ", "$.params.FROM", selectedCall, 0, 1000));

      foreach(auto pair, inbox.values("UNREAD", "$.params.FROM", selectedCall, 0, 1000)){
          msgs.append(pair);

          // mark as read
          auto msg = pair.second;
          msg.setType("READ");
          inbox.set(pair.first, msg);
      }

      qStableSort(msgs.begin(), msgs.end(), [](QPair<int, Message> const &a, QPair<int, Message> const &b){
          return a.second.params().value("UTC") > b.second.params().value("UTC");
      });

      auto mw = new MessageWindow(this);
      connect(mw, &MessageWindow::finished, this, [this](int){
          refreshInboxCounts();
          displayCallActivity();
      });
      connect(mw, &MessageWindow::deleteMessage, this, [this](int id){
          Inbox inbox(inboxPath());
          if(!inbox.open()){
              return;
          }

          inbox.del(id);
      });
      connect(mw, &MessageWindow::replyMessage, this, [this, mw](const QString &text){
          addMessageText(text, true, true);
          refreshInboxCounts();
          displayCallActivity();
          mw->close();
      });
      mw->setCall(selectedCall);
      mw->populateMessages(msgs);
      mw->show();
  });

  auto localMessageAction = new QAction(QString("Store Message..."), ui->tableWidgetCalls);
  connect(localMessageAction, &QAction::triggered, this, [this](){
      QString selectedCall = callsignSelected();
      if(selectedCall.isEmpty()){
          return;
      }

      auto m = new MessageReplyDialog(this);
      m->setWindowTitle("Message");
      m->setLabel(QString("Store this message locally for %1:").arg(selectedCall));
      if(m->exec() != QMessageBox::Accepted){
          return;
      }

      CommandDetail d = {};
      d.cmd = " MSG ";
      d.to = selectedCall;
      d.from = m_config.my_callsign();
      d.relayPath = d.from;
      d.text = m->textValue();
      d.utcTimestamp = DriftingDateTime::currentDateTimeUtc();

      addCommandToStorage("STORE", d);
  });

  ui->tableWidgetCalls->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->tableWidgetCalls->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this](QPoint const &point){
      QMenu * menu = new QMenu(ui->tableWidgetCalls);

      QMenu * sortByMenu = menu->addMenu("Sort By...");
      buildCallActivitySortByMenu(sortByMenu);

      QMenu * showColumnsMenu = menu->addMenu("Show Columns...");
      buildShowColumnsMenu(showColumnsMenu, "call");

      menu->popup(ui->tableWidgetCalls->horizontalHeader()->mapToGlobal(point));
  });

  ui->tableWidgetCalls->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->tableWidgetCalls, &QTableWidget::customContextMenuRequested, this, [this, logAction, historyAction, localMessageAction, clearAction4, clearActionAll, addStation, removeStation](QPoint const &point){
    QMenu * menu = new QMenu(ui->tableWidgetCalls);

    ui->tableWidgetRXAll->selectionModel()->clearSelection();

    QString selectedCall = callsignSelected();
    bool isAllCall = isAllCallIncluded(selectedCall);
    bool isGroupCall = isGroupCallIncluded(selectedCall);
    bool missingCallsign = selectedCall.isEmpty();

    if(!missingCallsign && !isAllCall){
        int selectedOffset = m_callActivity[selectedCall].freq;
        if(selectedOffset != -1){
            auto qsyAction = menu->addAction(QString("Jump to %1Hz").arg(selectedOffset));
            connect(qsyAction, &QAction::triggered, this, [this, selectedOffset](){
                setFreqOffsetForRestore(selectedOffset, false);
                // TODO: prompt mode switch?
            });
            menu->addSeparator();
        }
    }

    menu->addAction(logAction);
    logAction->setDisabled(missingCallsign || isAllCall);

    menu->addAction(historyAction);
    historyAction->setDisabled(missingCallsign || isAllCall || isGroupCall || !hasMessageHistory(selectedCall));

    menu->addAction(localMessageAction);
    localMessageAction->setDisabled(missingCallsign || isAllCall || isGroupCall);

    menu->addSeparator();

    auto savedMenu = menu->addMenu("Saved Messages...");
    buildSavedMessagesMenu(savedMenu);

    auto directedMenu = menu->addMenu(QString("Directed to %1...").arg(selectedCall));
    directedMenu->setDisabled(missingCallsign);
    buildQueryMenu(directedMenu, selectedCall);

    auto relayAction = buildRelayAction(selectedCall);
    relayAction->setText(QString("Relay via %1...").arg(selectedCall));
    relayAction->setDisabled(missingCallsign || isAllCall);
    menu->addActions({ relayAction });

    auto deselect = menu->addAction(QString("Deselect %1").arg(selectedCall));
    deselect->setDisabled(missingCallsign);
    connect(deselect, &QAction::triggered, this, [this](){
        ui->tableWidgetRXAll->clearSelection();
        ui->tableWidgetCalls->clearSelection();
    });

    // savedMenu->setEnabled(savedMenu->isEnabled() && !ui->actionModeJS8HB->isChecked());
    // directedMenu->setEnabled(directedMenu->isEnabled() && !ui->actionModeJS8HB->isChecked());
    // relayAction->setEnabled(relayAction->isEnabled() && !ui->actionModeJS8HB->isChecked());

    menu->addSeparator();

    menu->addAction(addStation);
    removeStation->setDisabled(missingCallsign);
    removeStation->setText(selectedCall.startsWith("@") ? "Remove Group" : "Remove Station");
    menu->addAction(removeStation);

    menu->addSeparator();
    menu->addAction(clearAction4);
    menu->addAction(clearActionAll);

    menu->popup(ui->tableWidgetCalls->mapToGlobal(point));
  });

  connect(ui->tableWidgetRXAll->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::on_tableWidgetRXAll_selectionChanged);
  connect(ui->tableWidgetCalls->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::on_tableWidgetCalls_selectionChanged);

  auto p = ui->tableWidgetRXAll->palette();
  p.setColor(QPalette::Inactive, QPalette::Highlight, p.color(QPalette::Active, QPalette::Highlight));
  ui->tableWidgetRXAll->setPalette(p);

  p = ui->tableWidgetCalls->palette();
  p.setColor(QPalette::Inactive, QPalette::Highlight, p.color(QPalette::Active, QPalette::Highlight));
  ui->tableWidgetCalls->setPalette(p);

  ui->hbMacroButton->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->hbMacroButton, &QPushButton::customContextMenuRequested, this, [this](QPoint const &point){
      QMenu * menu = new QMenu(ui->hbMacroButton);

      buildHeartbeatMenu(menu);

      menu->popup(ui->hbMacroButton->mapToGlobal(point));
  });

  ui->cqMacroButton->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(ui->cqMacroButton, &QPushButton::customContextMenuRequested, this, [this](QPoint const &point){
      QMenu * menu = new QMenu(ui->cqMacroButton);

      buildCQMenu(menu);

      menu->popup(ui->cqMacroButton->mapToGlobal(point));
  });

  // Don't block heartbeat's first run...
  m_lastTxTime = DriftingDateTime::currentDateTimeUtc().addSecs(-300);

  int width = 75;
  /*
  QList<QPushButton*> btns;
  foreach(auto child, ui->buttonGrid->children()){
      if(!child->isWidgetType()){
          continue;
      }

      if(!child->objectName().contains("Button")){
          continue;
      }

      auto b = qobject_cast<QPushButton*>(child);
      width = qMax(width, b->geometry().width());
      btns.append(b);
  }
  */
  foreach(auto child, ui->buttonGrid->children()){
      if(!child->isWidgetType()){
          continue;
      }

      if(!child->objectName().contains("Button")){
          continue;
      }

      auto b = qobject_cast<QPushButton*>(child);
      b->setCursor(QCursor(Qt::PointingHandCursor));
  }
  auto buttonLayout = ui->buttonGrid->layout();
  auto gridButtonLayout = qobject_cast<QGridLayout*>(buttonLayout);
  gridButtonLayout->setColumnMinimumWidth(0, width);
  gridButtonLayout->setColumnMinimumWidth(1, width);
  gridButtonLayout->setColumnMinimumWidth(2, width);
  gridButtonLayout->setColumnStretch(0, 1);
  gridButtonLayout->setColumnStretch(1, 1);
  gridButtonLayout->setColumnStretch(2, 1);


  // Prepare spotting configuration...
  prepareSpotting();

  displayActivity(true);

#if TEST_FOX_WAVE_GEN
  ui->turboButton->setVisible(true);
  ui->turboButton->setEnabled(true);
#endif

  m_txTextDirtyDebounce.setSingleShot(true);
  connect(&m_txTextDirtyDebounce, &QTimer::timeout, this, &MainWindow::refreshTextDisplay);

  QTimer::singleShot(500, this, &MainWindow::initializeDummyData);

  // this must be the last statement of constructor
  if (!m_valid) throw std::runtime_error {"Fatal initialization exception"};
}

QPair<QPair<int, int>, int> splitVersion(QString v){
    int hyphenPos = v.lastIndexOf("-");
    if(hyphenPos >= 0){
        v = v.left(hyphenPos);
    }

    QVector<int> intSegs;
    foreach(QString seg, v.split(".")){
        bool ok = false;
        int i = seg.toInt(&ok);
        if(!ok){
            break;
        }
        intSegs.append(i);
    }

    int len = intSegs.count();
    QPair<QPair<int, int>, int> tuple;
    if(len > 0){
        tuple.first.first = intSegs.at(0);
    }
    if(len > 1){
        tuple.first.second = intSegs.at(1);
    }
    if(len > 2){
        tuple.second = intSegs.at(2);
    }
    return tuple;
}

void MainWindow::checkVersion(bool alertOnUpToDate){
  auto m = new QNetworkAccessManager(this);
  connect(m, &QNetworkAccessManager::finished, this, [this, alertOnUpToDate](QNetworkReply * reply){
    if(reply->error()){
        qDebug() << "Checking for Updates Error:" << reply->errorString();
        return;
    }

    QString content = reply->readAll().trimmed();

    auto currentVersion = splitVersion(version());
    auto networkVersion = splitVersion(content);

    qDebug() << "Checking Version" << currentVersion << "with" << networkVersion;

    if(currentVersion < networkVersion){

        SelfDestructMessageBox * m = new SelfDestructMessageBox(60,
          "New Updates Available",
          QString("A new version (%1) of JS8Call is now available. Please see js8call.com for more details.").arg(content),
          QMessageBox::Information,
          QMessageBox::Ok,
          QMessageBox::Ok,
          false,
          this);

        m->show();

    } else if(alertOnUpToDate){

        SelfDestructMessageBox * m = new SelfDestructMessageBox(60,
          "No Updates Available",
          QString("Your version (%1) of JS8Call is up-to-date.").arg(version()),
          QMessageBox::Information,
          QMessageBox::Ok,
          QMessageBox::Ok,
          false,
          this);

        m->show();

    }
  });

  qDebug() << "Checking for Updates...";
  QUrl url("http://files.js8call.com/version.txt");
  QNetworkRequest r(url);
  m->get(r);
}

void MainWindow::checkStartupWarnings ()
{
  if(m_config.check_for_updates()){
      checkVersion(false);
  }
  ensureCallsignSet(false);
}

void MainWindow::initializeDummyData(){
    if(!QApplication::applicationName().contains("dummy")){
        return;
    }

#if 0
    auto t = new QTimer(this);
    t->setInterval(150);
    connect(t, &QTimer::timeout, this, [this](){
        if(!ui->extFreeTextMsgEdit->hasFocus()){
            return;
        }

        auto c = ui->extFreeTextMsgEdit->textCursor();
        int pos = qMin(c.selectionStart(), c.selectionEnd());

        if(pos <= 5 && c.selectionStart() != c.selectionEnd()){
            c.clearSelection();
            ui->extFreeTextMsgEdit->setTextCursor(c);
        }
    });
    t->start();

    auto kpe = new KeyPressEater();
    connect(kpe, &KeyPressEater::keyPressed, this, [this](QObject *obj, QKeyEvent * e, bool *pProcessed){
        auto t = e->text();
        auto c = ui->extFreeTextMsgEdit->textCursor();
        int pos = qMin(c.selectionStart(), c.selectionEnd());

        if(e->key() == Qt::Key_Escape){
            *pProcessed = false;
            return;
        }

        QTextCursor cc(c);
        cc.setPosition(5);
        cc.movePosition(QTextCursor::NextWord);
        int cpos = qMax(cc.selectionStart(), cc.selectionEnd());
        if(e->key() == Qt::Key_Backspace && e->modifiers().testFlag(Qt::ControlModifier) && pos < cpos){
            *pProcessed = true;
            return;
        }

        if((e->key() == Qt::Key_Backspace && pos <= 5) ||
           (e->key() == Qt::Key_Delete && pos < 5)){
            *pProcessed = true;
            return;
        }

        if(e->key() == Qt::Key_V && e->modifiers().testFlag(Qt::ControlModifier) && pos <= 5){
            *pProcessed = true;
            return;
        }

        if(!t.isEmpty() && pos < 5){
            *pProcessed = true;
            return;
        }
    });
    ui->extFreeTextMsgEdit->installEventFilter(kpe);

    connect(ui->extFreeTextMsgEdit, &QTextEdit::copyAvailable, this, [this](bool available){
        if(!available){
            return;
        }
        auto c = ui->extFreeTextMsgEdit->textCursor();

        qDebug() << "select" << c.selectionStart() << c.selectionEnd();

        int pos = qMin(c.selectionStart(), c.selectionEnd());
        if(pos <= 5){
            auto text = c.selectedText();
            if(!text.isEmpty()){
                c.clearSelection();
                ui->extFreeTextMsgEdit->setTextCursor(c);
            }
        }
    });

    /*
    connect(ui->extFreeTextMsgEdit->document(), &QTextDocument::cursorPositionChanged, this, [this](const QTextCursor &){
        auto c = ui->extFreeTextMsgEdit->textCursor();
        int pos = qMin(c.selectionStart(), c.selectionEnd());
        if(pos <= 5){
            auto text = c.selectedText();
            if(!text.isEmpty()){
                c.clearSelection();
                ui->extFreeTextMsgEdit->setTextCursor(c);
            }
        }
    });

    connect(ui->extFreeTextMsgEdit, &QTextEdit::cursorPositionChanged, this, [this](){
        auto c = ui->extFreeTextMsgEdit->textCursor();
        int pos = qMin(c.selectionStart(), c.selectionEnd());

        if(pos <= 5){
            auto text = c.selectedText();
            if(!text.isEmpty()){
                c.clearSelection();
                ui->extFreeTextMsgEdit->setTextCursor(c);
            }
        }
    });
    */

    /*connect(ui->extFreeTextMsgEdit->document(), &QTextDocument::contentsChange, this, [this](int from, int removed, int added){
        if(from < 5 && removed == 1){
            ui->extFreeTextMsgEdit->document()->blockSignals(true);
            ui->extFreeTextMsgEdit->document()->undo();
            ui->extFreeTextMsgEdit->document()->clearUndoRedoStacks(QTextDocument::RedoStack);
            ui->extFreeTextMsgEdit->document()->blockSignals(false);
        }
    });*/

    ui->extFreeTextMsgEdit->setText("HELLO BRAVE NEW WORLD");
    auto c = ui->extFreeTextMsgEdit->textCursor();
    c.setPosition(0);
    c.setPosition(5, QTextCursor::KeepAnchor);
    auto f = c.charFormat();
    f.setFontStrikeOut(true);
    c.setCharFormat(f);
#endif

    ui->extFreeTextMsgEdit->setPlainText("HELLO BRAVE NEW WORLD");
    ui->extFreeTextMsgEdit->setCharsSent(6);

    logHeardGraph("KN4CRD", "OH8STN");
    logHeardGraph("KN4CRD", "K0OG");
    logHeardGraph("K0OG", "KN4CRD");

    auto path = QDir::toNativeSeparators(m_config.writeable_data_dir ().absoluteFilePath(QString("test.db3")));
    auto inbox = Inbox(path);
    if(inbox.open()){
        qDebug() << "test inbox opened" << inbox.count("test", "$", "%") << "messages";

        int i = inbox.append(Message("test", "booya1"));

        i = inbox.append(Message("test", "booya2"));
        qDebug() << "i" << i;

        qDebug() << inbox.set(i, Message("test", "booya3"));

        auto m = inbox.value(i);
        qDebug() << QString(m.toJson());

        qDebug() << inbox.del(i);

        foreach(auto pair, inbox.values("test", "$", "%", 0, 5)){
            qDebug() << pair.first << QString(pair.second.toJson());
        }
    }

    auto d = DecodedText("SN5-lUuJkby0", Varicode::JS8CallFirst, 1);
    qDebug () << "KN4CRD: K0OG ===>" << d.message();

    // qDebug() << Varicode::isValidCallsign("@GROUP1", nullptr);
    // qDebug() << Varicode::packAlphaNumeric50("VE7/KN4CRD");
    // qDebug() << Varicode::unpackAlphaNumeric50(Varicode::packAlphaNumeric50("VE7/KN4CRD"));
    // qDebug() << Varicode::unpackAlphaNumeric50(Varicode::packAlphaNumeric50("@GROUP/42"));
    // qDebug() << Varicode::unpackAlphaNumeric50(Varicode::packAlphaNumeric50("SP1ATOM"));

    if(!m_config.my_groups().contains("@GROUP42")){
        m_config.addGroup("@GROUP42");
    }

    QList<QString> calls = {
        "KN4CRD",
        "VE7/KN4CRD",
        "KN4CRD/P",
        "KC9QNE",
        "KI6SSI",
        "K0OG",
        "LB9YH",
        "VE7/LB9YH",
        "M0IAX",
        "N0JDS",
        "OH8STN",
        "VA3OSO",
        "VK1MIC",
        "W0FW"
    };

    auto dt = DriftingDateTime::currentDateTimeUtc().addSecs(-300);

    int i = 0;
    foreach(auto call, calls){
        CallDetail cd = {};
        cd.call = call;
        cd.freq = 500 + 100*i;
        cd.snr = i == 3 ? -100 : i;
        cd.ackTimestamp = i == 1 ? dt.addSecs(-900) : QDateTime{};
        cd.utcTimestamp = dt;
        cd.grid = i == 5 ? "J042" : i == 6 ? " FN42FN42FN" : "";
        cd.tdrift = 0.1*i;
        cd.mode = currentMode();
        logCallActivity(cd, false);

        ActivityDetail ad = {};
        ad.bits = Varicode::JS8CallFirst | Varicode::JS8CallLast;
        ad.snr = i == 3 ? -100 : i;
        ad.freq = 500 + 100*i;
        ad.text = QString("%1: %2 TEST MESSAGE").arg(call).arg(m_config.my_callsign());
        ad.utcTimestamp = dt;
        ad.mode = cd.mode;
        m_bandActivity[500+100*i] = { ad };

        markOffsetDirected(500+100*i, false);

        i++;
    }

    ActivityDetail adHB1 = {};
    adHB1.bits = Varicode::JS8CallFirst;
    adHB1.snr = 0;
    adHB1.freq = 750;
    adHB1.text = QString("KN4CRD: HB AUTO EM73");
    adHB1.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
    adHB1.mode = "NORMAL";
    m_bandActivity[750].append(adHB1);

    ActivityDetail adHB2 = {};
    adHB2.bits = Varicode::JS8CallLast;
    adHB2.snr = 0;
    adHB2.freq = 750;
    adHB2.text = QString(" MSG ID 1");
    adHB2.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
    adHB2.mode = "NORMAL";
    m_bandActivity[750].append(adHB2);

    CommandDetail cmd = {};
    cmd.cmd = ">";
    cmd.to = m_config.my_callsign();
    cmd.from = "N0JDS";
    cmd.relayPath = "N0JDS>OH8STN";
    cmd.text = "HELLO BRAVE SOUL";
    cmd.utcTimestamp = dt;
    addCommandToMyInbox(cmd);

    QString eot = m_config.eot();

    displayTextForFreq(QString("KN4CRD: @ALLCALL? %1 ").arg(eot), 42, DriftingDateTime::currentDateTimeUtc().addSecs(-315), true, true, true);
    displayTextForFreq(QString("J1Y: KN4CRD SNR -05 %1 ").arg(eot), 42, DriftingDateTime::currentDateTimeUtc().addSecs(-300), false, true, true);
    displayTextForFreq(QString("HELLO BRAVE  NEW   WORLD    %1 ").arg(eot), 42, DriftingDateTime::currentDateTimeUtc().addSecs(-300), false, true, true);

    auto now = DriftingDateTime::currentDateTimeUtc();
    displayTextForFreq(QString("KN4CRD: JY1 ACK -12 %1 ").arg(eot), 780, now, false, true, true);
    displayTextForFreq(QString("KN4CRD: JY1 ACK -12 %1 ").arg(eot), 780, now, false, true, true); // should be hidden (duplicate)
    displayTextForFreq(QString("OH8STN: JY1 ACK -12 %1 ").arg(eot), 780, now, false, true, true);

    displayTextForFreq(QString("KN4CRD: JY1 ACK -10 %1 ").arg(eot), 800, now, false, true, true);
    displayTextForFreq(QString("KN4CRD: JY1 ACK -12 %1 ").arg(eot), 780, now.addSecs(120), false, true, true);

    displayTextForFreq(QString("HELLO\\nBRAVE\\nNEW\\nWORLD %1 ").arg(eot), 1500, now, false, true, true);

    displayActivity(true);
}

void MainWindow::initialize_fonts ()
{
  set_application_font (m_config.text_font ());

  setTextEditFont(ui->textEditRX, m_config.rx_text_font());
  setTextEditFont(ui->extFreeTextMsgEdit, m_config.tx_text_font());

  displayActivity(true);
}

void MainWindow::splash_done ()
{
  m_splash && m_splash->close ();
}

void MainWindow::on_the_minute ()
{
  if (minuteTimer.isSingleShot ())
    {
      minuteTimer.setSingleShot (false);
      minuteTimer.start (60 * 1000); // run free
    }
  else
    {
        auto const& ms_error = ms_minute_error ();
        if (qAbs (ms_error) > 1000) // keep drift within +-1s
        {
          minuteTimer.setSingleShot (true);
          minuteTimer.start (ms_error + 60 * 1000);
        }
    }

  if (m_config.watchdog ())
    {
      incrementIdleTimer();
      update_watchdog_label ();
    }
  else
    {
      tx_watchdog (false);
    }
}

void MainWindow::tryBandHop(){
  // see if we need to hop bands...
  if(!m_config.auto_switch_bands()){
      return;
  }

  // make sure we're not transmitting
  if(isMessageQueuedForTransmit()){
      return;
  }

  // get the current band
  auto dialFreq = dialFrequency();

  auto currentBand = m_config.bands()->find(dialFreq);

  // get the stations list
  auto stations = m_config.stations()->station_list();

  // order stations by (switch_at, switch_until) time tuple
  qStableSort(stations.begin(), stations.end(), [](StationList::Station const &a, StationList::Station const &b){
    return (a.switch_at_ < b.switch_at_) || (a.switch_at_ == b.switch_at_ && a.switch_until_ < b.switch_until_);
  });

  // we just set the date to a known y/m/d to make the comparisons easier
  QDateTime d = DriftingDateTime::currentDateTimeUtc();
  d.setDate(QDate(2000, 1, 1));

  QDateTime startOfDay = QDateTime(QDate(2000, 1, 1), QTime(0, 0));
  QDateTime endOfDay = QDateTime(QDate(2000, 1, 1), QTime(23, 59));

  // see if we can find a needed band switch...
  foreach(auto station, stations){
      // we can switch to this frequency if we're in the time range, inclusive of switch_at, exclusive of switch_until
      // and if we are switching to a different frequency than the last hop. this allows us to switch bands at that time,
      // but then later we can later switch to a different band if needed without the automatic band switching to take over
      bool inTimeRange = (
        (station.switch_at_ <= d && d <= station.switch_until_) ||          // <- normal range, 12-16 && 6-8, evalued as 12 <= d <= 16 || 6 <= d <= 8

        (station.switch_until_ < station.switch_at_ && (                    // <- say for a range of 12->2 & 2->12;  12->2,
             (station.switch_at_ <= d && d <= endOfDay)         ||          //    should be evaluated as 12 <= d <= 23:59 || 00:00 <= d <= 2
             (startOfDay <= d && d <= station.switch_until_)
        ))
      );

      bool noOverride = (
        (m_bandHopped || (!m_bandHopped && station.frequency_ != m_bandHoppedFreq))
      );

      bool freqIsDifferent = (station.frequency_ != dialFreq);

      bool canSwitch = (
        inTimeRange     &&
        noOverride      &&
        freqIsDifferent
      );

      // switch, if we can and the band is different than our current band
      if(canSwitch){
          Frequency frequency = station.frequency_;

          m_bandHopped = false;
          m_bandHoppedFreq = frequency;

          SelfDestructMessageBox * m = new SelfDestructMessageBox(30,
            "Scheduled Frequency Change",
            QString("A scheduled frequency change has arrived. The rig frequency will be changed to %1 MHz in %2 second(s).").arg(Radio::frequency_MHz_string(station.frequency_)),
            QMessageBox::Information,
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Ok,
            true,
            this);

          connect(m, &SelfDestructMessageBox::accepted, this, [this, frequency](){
              m_bandHopped = true;
              setRig(frequency);
          });

          m->show();

#if 0
          // TODO: jsherer - this is totally a hack because of the signal that gets emitted to clearActivity on band change...
          QTimer *t = new QTimer(this);
          t->setInterval(250);
          t->setSingleShot(true);
          connect(t, &QTimer::timeout, this, [this, station, dialFreq](){
              auto message = QString("Scheduled frequency switch from %1 MHz to %2 MHz");
              message = message.arg(Radio::frequency_MHz_string(dialFreq));
              message = message.arg(Radio::frequency_MHz_string(station.frequency_));
              writeNoticeTextToUI(DriftingDateTime::currentDateTimeUtc(), message);
          });
          t->start();
#endif

          return;
      }
  }
}

//--------------------------------------------------- MainWindow destructor
MainWindow::~MainWindow()
{
  m_astroWidget.reset ();
  QString fname {QDir::toNativeSeparators(m_config.writeable_data_dir ().absoluteFilePath ("wsjtx_wisdom.dat"))};
  QByteArray cfname=fname.toLocal8Bit();
  fftwf_export_wisdom_to_filename(cfname);
  m_audioThread.quit ();
  m_audioThread.wait ();
  remove_child_from_event_filter (this);
}

//-------------------------------------------------------- writeSettings()
void MainWindow::writeSettings()
{
  m_settings->beginGroup("MainWindow");
  m_settings->setValue ("geometry", saveGeometry ());
  m_settings->setValue ("geometryNoControls", m_geometryNoControls);
  m_settings->setValue ("state", saveState ());
  m_settings->setValue("MRUdir", m_path);
  m_settings->setValue("DXcall",ui->dxCallEntry->text());
  m_settings->setValue("DXgrid",ui->dxGridEntry->text());
  m_settings->setValue ("AstroDisplayed", m_astroWidget && m_astroWidget->isVisible());
  m_settings->setValue ("MsgAvgDisplayed", m_msgAvgWidget && m_msgAvgWidget->isVisible());
  m_settings->setValue ("FreeText", ui->freeTextMsg->currentText ());
  m_settings->setValue("ShowMenus",ui->cbMenus->isChecked());
  m_settings->setValue("CallFirst",ui->cbFirst->isChecked());

  m_settings->setValue("MainSplitter", ui->mainSplitter->saveState());
  m_settings->setValue("TextHorizontalSplitter", ui->textHorizontalSplitter->saveState());
  m_settings->setValue("BandActivityVisible", ui->tableWidgetRXAll->isVisible());
  m_settings->setValue("BandHBActivityVisible", ui->actionShow_Band_Heartbeats_and_ACKs->isChecked());
  m_settings->setValue("TextVerticalSplitter", ui->textVerticalSplitter->saveState());
  m_settings->setValue("ShowTimeDrift", ui->driftSyncFrame->isVisible());
  m_settings->setValue("TimeDrift", ui->driftSpinBox->value());
  m_settings->setValue("ShowTooltips", ui->actionShow_Tooltips->isChecked());
  m_settings->setValue("ShowStatusbar", ui->statusBar->isVisible());
  m_settings->setValue("RXActivity", ui->textEditRX->toHtml());

  m_settings->endGroup();

  m_settings->beginGroup("Common");
  m_settings->setValue("Mode",m_mode);
  m_settings->setValue("ModeTx",m_modeTx);
  m_settings->setValue("SaveNone",ui->actionNone->isChecked());
  m_settings->setValue("SaveDecoded",ui->actionSave_decoded->isChecked());
  m_settings->setValue("SaveAll",ui->actionSave_all->isChecked());
  m_settings->setValue("NDepth",m_ndepth);
  m_settings->setValue("RxFreq",ui->RxFreqSpinBox->value());
  m_settings->setValue("TxFreq",ui->TxFreqSpinBox->value());
  m_settings->setValue("WSPRfreq",ui->WSPRfreqSpinBox->value());
  m_settings->setValue("SubMode",m_nSubMode);
  m_settings->setValue("SubModeHB", ui->actionModeJS8HB->isChecked());
  m_settings->setValue("SubModeHBAck", ui->actionHeartbeatAcknowledgements->isChecked());
  m_settings->setValue("DTtol",m_DTtol);
  m_settings->setValue("Ftol", ui->sbFtol->value ());
  m_settings->setValue("MinSync",m_minSync);
  m_settings->setValue ("AutoSeq", ui->cbAutoSeq->isChecked ());
  m_settings->setValue ("RxAll", ui->cbRxAll->isChecked ());
  m_settings->setValue ("VHFcontest", ui->cbVHFcontest->isChecked ());
  m_settings->setValue("ShMsgs",m_bShMsgs);
  m_settings->setValue("SWL",ui->cbSWL->isChecked());
  m_settings->setValue ("DialFreq", QVariant::fromValue(m_lastMonitoredFrequency));
  m_settings->setValue("OutAttenuation", ui->outAttenuation->value ());
  m_settings->setValue("NoSuffix",m_noSuffix);
  m_settings->setValue("GUItab",ui->tabWidget->currentIndex());
  m_settings->setValue("OutBufSize",outBufSize);
  m_settings->setValue ("HoldTxFreq", ui->cbHoldTxFreq->isChecked ());
  m_settings->setValue("PctTx",m_pctx);
  m_settings->setValue("dBm",m_dBm);
  m_settings->setValue ("WSPRPreferType1", ui->WSPR_prefer_type_1_check_box->isChecked ());
  m_settings->setValue("UploadSpots",m_uploadSpots);
  m_settings->setValue ("BandHopping", ui->band_hopping_group_box->isChecked ());
  m_settings->setValue ("TRPeriod", ui->sbTR->value ());
  m_settings->setValue("FastMode",m_bFastMode);
  m_settings->setValue("Fast9",m_bFast9);
  m_settings->setValue ("CQTxfreq", ui->sbCQTxFreq->value ());
  m_settings->setValue("pwrBandTxMemory",m_pwrBandTxMemory);
  m_settings->setValue("pwrBandTuneMemory",m_pwrBandTuneMemory);
  m_settings->setValue ("FT8AP", ui->actionEnable_AP_FT8->isChecked ());
  m_settings->setValue ("JT65AP", ui->actionEnable_AP_JT65->isChecked ());
  m_settings->setValue("SortBy", QVariant(m_sortCache));
  m_settings->setValue("ShowColumns", QVariant(m_showColumnsCache));
  m_settings->setValue("ShowHBBandActivity", m_hbInterval);
  m_settings->setValue("HBInterval", m_hbInterval);
  m_settings->setValue("CQInterval", m_cqInterval);





  // TODO: jsherer - need any other customizations?
  /*m_settings->setValue("PanelLeftGeometry", ui->tableWidgetRXAll->geometry());
  m_settings->setValue("PanelRightGeometry", ui->tableWidgetCalls->geometry());
  m_settings->setValue("PanelTopGeometry", ui->extFreeTextMsg->geometry());
  m_settings->setValue("PanelBottomGeometry", ui->extFreeTextMsgEdit->geometry());
  m_settings->setValue("PanelWaterfallGeometry", ui->bandHorizontalWidget->geometry());*/
  //m_settings->setValue("MainSplitter", QVariant::fromValue(ui->mainSplitter->sizes()));

  {
    QList<QVariant> coeffs;     // suitable for QSettings
    for (auto const& coeff : m_phaseEqCoefficients)
      {
        coeffs << coeff;
      }
    m_settings->setValue ("PhaseEqualizationCoefficients", QVariant {coeffs});
  }
  m_settings->endGroup();


  auto now = DriftingDateTime::currentDateTimeUtc();
  int callsignAging = m_config.callsign_aging();

  m_settings->beginGroup("CallActivity");
  m_settings->remove(""); // remove all keys in current group
  foreach(auto cd, m_callActivity.values()){
      if (cd.call.trimmed().isEmpty()){
          continue;
      }
      if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
          continue;
      }
      m_settings->setValue(cd.call.trimmed(), QVariantMap{
        {"snr", QVariant(cd.snr)},
        {"grid", QVariant(cd.grid)},
        {"freq", QVariant(cd.freq)},
        {"tdrift", QVariant(cd.tdrift)},
#if CACHE_CALL_DATETIME_AS_STRINGS
        {"ackTimestamp", QVariant(cd.ackTimestamp.toString("yyyy-MM-dd hh:mm:ss"))},
        {"utcTimestamp", QVariant(cd.utcTimestamp.toString("yyyy-MM-dd hh:mm:ss"))},
#else
        {"ackTimestamp", QVariant(cd.ackTimestamp)},
        {"utcTimestamp", QVariant(cd.utcTimestamp)},
#endif
        {"mode", QVariant(cd.mode)},
      });
  }
  m_settings->endGroup();
}

//---------------------------------------------------------- readSettings()
void MainWindow::readSettings()
{
  ui->cbVHFcontest->setVisible(false);
  ui->cbAutoSeq->setVisible(false);
  ui->cbFirst->setVisible(false);
  m_settings->beginGroup("MainWindow");
  setMinimumSize(800, 400);
  restoreGeometry (m_settings->value ("geometry", saveGeometry ()).toByteArray ());
  setMinimumSize(800, 400);

  m_geometryNoControls = m_settings->value ("geometryNoControls",saveGeometry()).toByteArray();
  restoreState (m_settings->value ("state", saveState ()).toByteArray ());
  ui->dxCallEntry->setText (m_settings->value ("DXcall", QString {}).toString ());
  ui->dxGridEntry->setText (m_settings->value ("DXgrid", QString {}).toString ());
  m_path = m_settings->value("MRUdir", m_config.save_directory ().absolutePath ()).toString ();
  auto displayAstro = m_settings->value ("AstroDisplayed", false).toBool ();
  auto displayMsgAvg = m_settings->value ("MsgAvgDisplayed", false).toBool ();
  if (m_settings->contains ("FreeText")) ui->freeTextMsg->setCurrentText (
        m_settings->value ("FreeText").toString ());
  ui->cbMenus->setChecked(m_settings->value("ShowMenus",true).toBool());
  ui->cbFirst->setChecked(m_settings->value("CallFirst",true).toBool());

  auto mainSplitterState = m_settings->value("MainSplitter").toByteArray();
  if(!mainSplitterState.isEmpty()){
    ui->mainSplitter->restoreState(mainSplitterState);
  }
  auto horizontalState = m_settings->value("TextHorizontalSplitter").toByteArray();
  if(!horizontalState.isEmpty()){
    ui->textHorizontalSplitter->restoreState(horizontalState);
    auto hsizes = ui->textHorizontalSplitter->sizes();

    ui->tableWidgetRXAll->setVisible(hsizes.at(0) > 0);
    ui->tableWidgetCalls->setVisible(hsizes.at(2) > 0);
  }

  m_bandActivityWasVisible = m_settings->value("BandActivityVisible", true).toBool();
  ui->tableWidgetRXAll->setVisible(m_bandActivityWasVisible);

  auto verticalState = m_settings->value("TextVerticalSplitter").toByteArray();
  if(!verticalState.isEmpty()){
    ui->textVerticalSplitter->restoreState(verticalState);
  }
  ui->driftSyncFrame->setVisible(m_settings->value("ShowTimeDrift", false).toBool());
  ui->driftSpinBox->setValue(m_settings->value("TimeDrift", 0).toInt());
  ui->actionShow_Tooltips->setChecked(m_settings->value("ShowTooltips", true).toBool());
  ui->actionShow_Statusbar->setChecked(m_settings->value("ShowStatusbar",true).toBool());
  ui->statusBar->setVisible(ui->actionShow_Statusbar->isChecked());
  ui->textEditRX->setHtml(m_config.reset_activity() ? "" : m_settings->value("RXActivity", "").toString());
  ui->actionShow_Band_Heartbeats_and_ACKs->setChecked(m_settings->value("BandHBActivityVisible", false).toBool());
  m_settings->endGroup();

  // do this outside of settings group because it uses groups internally
  ui->actionAstronomical_data->setChecked (displayAstro);

  m_settings->beginGroup("Common");
  m_mode=m_settings->value("Mode","JT9").toString();
  m_modeTx=m_settings->value("ModeTx","JT9").toString();
  if(m_modeTx.mid(0,3)=="JT9") ui->pbTxMode->setText("Tx JT9  @");
  if(m_modeTx=="JT65") ui->pbTxMode->setText("Tx JT65  #");

  // these save settings should never be enabled unless specifically called out by the user for every session.
  ui->actionNone->setChecked(true);
  ui->actionSave_decoded->setChecked(false);
  ui->actionSave_all->setChecked(false);

  // set the frequency offset
  setFreqOffsetForRestore(m_settings->value("RxFreq",1500).toInt(), false);

  m_nSubMode=m_settings->value("SubMode",0).toInt();
  ui->actionModeJS8HB->setChecked(m_nSubMode == Varicode::JS8CallNormal && m_settings->value("SubModeHB", false).toBool());
  ui->actionHeartbeatAcknowledgements->setChecked(m_settings->value("SubModeHBAck", false).toBool());
  ui->actionModeJS8Normal->setChecked(m_nSubMode == Varicode::JS8CallNormal);
  ui->actionModeJS8Fast->setChecked(m_nSubMode == Varicode::JS8CallFast);
  ui->actionModeJS8Turbo->setChecked(m_nSubMode == Varicode::JS8CallTurbo);
  ui->actionModeJS8Ultra->setChecked(m_nSubMode == Varicode::JS8CallUltra);
  ui->sbFtol->setValue (m_settings->value("Ftol", 20).toInt());
  m_minSync=m_settings->value("MinSync",0).toInt();
  ui->syncSpinBox->setValue(m_minSync);
  ui->cbAutoSeq->setChecked (m_settings->value ("AutoSeq", false).toBool());
  ui->cbRxAll->setChecked (m_settings->value ("RxAll", false).toBool());
  ui->cbVHFcontest->setChecked (m_settings->value ("VHFcontest", false).toBool());
  m_bShMsgs=m_settings->value("ShMsgs",false).toBool();
  m_bSWL=m_settings->value("SWL",false).toBool();
  m_bFast9=m_settings->value("Fast9",false).toBool();
  m_bFastMode=m_settings->value("FastMode",false).toBool();
  ui->sbTR->setValue (m_settings->value ("TRPeriod", 30).toInt());
  m_lastMonitoredFrequency = m_settings->value ("DialFreq",
    QVariant::fromValue<Frequency> (default_frequency)).value<Frequency> ();
  ui->WSPRfreqSpinBox->setValue(0); // ensure a change is signaled
  ui->WSPRfreqSpinBox->setValue(m_settings->value("WSPRfreq",1500).toInt());
  ui->TxFreqSpinBox->setValue(0); // ensure a change is signaled
  ui->TxFreqSpinBox->setValue(m_settings->value("TxFreq",1500).toInt());
  m_ndepth=m_settings->value("NDepth",3).toInt();
  m_pctx=m_settings->value("PctTx",20).toInt();
  m_dBm=m_settings->value("dBm",37).toInt();
  ui->WSPR_prefer_type_1_check_box->setChecked (m_settings->value ("WSPRPreferType1", true).toBool ());
  m_uploadSpots=m_settings->value("UploadSpots",false).toBool();
  if(!m_uploadSpots) ui->cbUploadWSPR_Spots->setStyleSheet("QCheckBox{background-color: yellow}");
  ui->band_hopping_group_box->setChecked (m_settings->value ("BandHopping", false).toBool());
  // setup initial value of tx attenuator
  m_block_pwr_tooltip = true;
  ui->outAttenuation->setValue (m_settings->value ("OutAttenuation", 0).toInt ());
  m_block_pwr_tooltip = false;
  ui->sbCQTxFreq->setValue (m_settings->value ("CQTxFreq", 260).toInt());
  m_noSuffix=m_settings->value("NoSuffix",false).toBool();
  int n=m_settings->value("GUItab",0).toInt();
  ui->tabWidget->setCurrentIndex(n);
  outBufSize=m_settings->value("OutBufSize",4096).toInt();
  ui->cbHoldTxFreq->setChecked (m_settings->value ("HoldTxFreq", false).toBool ());
  m_pwrBandTxMemory=m_settings->value("pwrBandTxMemory").toHash();
  m_pwrBandTuneMemory=m_settings->value("pwrBandTuneMemory").toHash();
  ui->actionEnable_AP_FT8->setChecked (m_settings->value ("FT8AP", false).toBool());
  ui->actionEnable_AP_JT65->setChecked (m_settings->value ("JT65AP", false).toBool());

  m_sortCache = m_settings->value("SortBy").toMap();
  m_showColumnsCache = m_settings->value("ShowColumns").toMap();
  m_hbInterval = m_settings->value("HBInterval", 0).toInt();
  m_cqInterval = m_settings->value("CQInterval", 0).toInt();

  // TODO: jsherer - any other customizations?
  //ui->mainSplitter->setSizes(m_settings->value("MainSplitter", QVariant::fromValue(ui->mainSplitter->sizes())).value<QList<int> >());
  //ui->tableWidgetRXAll->restoreGeometry(m_settings->value("PanelLeftGeometry", ui->tableWidgetRXAll->saveGeometry()).toByteArray());
  //ui->tableWidgetCalls->restoreGeometry(m_settings->value("PanelRightGeometry", ui->tableWidgetCalls->saveGeometry()).toByteArray());
  //ui->extFreeTextMsg->setGeometry( m_settings->value("PanelTopGeometry", ui->extFreeTextMsg->geometry()).toRect());
  //ui->extFreeTextMsgEdit->setGeometry( m_settings->value("PanelBottomGeometry", ui->extFreeTextMsgEdit->geometry()).toRect());
  //ui->bandHorizontalWidget->setGeometry( m_settings->value("PanelWaterfallGeometry", ui->bandHorizontalWidget->geometry()).toRect());
  //qDebug() << m_settings->value("PanelTopGeometry") << ui->extFreeTextMsg;

  setTextEditStyle(ui->textEditRX, m_config.color_rx_foreground(), m_config.color_rx_background(), m_config.rx_text_font());
  setTextEditStyle(ui->extFreeTextMsgEdit, m_config.color_compose_foreground(), m_config.color_compose_background(), m_config.compose_text_font());
  ui->extFreeTextMsgEdit->setFont(m_config.compose_text_font(), m_config.color_compose_foreground(), m_config.color_compose_background());


  {
    auto const& coeffs = m_settings->value ("PhaseEqualizationCoefficients"
                                            , QList<QVariant> {0., 0., 0., 0., 0.}).toList ();
    m_phaseEqCoefficients.clear ();
    for (auto const& coeff : coeffs)
      {
        m_phaseEqCoefficients.append (coeff.value<double> ());
      }
  }
  m_settings->endGroup();

  // use these initialisation settings to tune the audio o/p buffer
  // size and audio thread priority
  m_settings->beginGroup ("Tune");
  m_msAudioOutputBuffered = m_settings->value ("Audio/OutputBufferMs").toInt ();
  m_framesAudioInputBuffered = m_settings->value ("Audio/InputBufferFrames", RX_SAMPLE_RATE / 10).toInt ();
  m_audioThreadPriority = static_cast<QThread::Priority> (m_settings->value ("Audio/ThreadPriority", QThread::HighPriority).toInt () % 8);
  m_settings->endGroup ();

  if(m_config.reset_activity()){
      // NOOP
  } else {
      m_settings->beginGroup("CallActivity");
      foreach(auto call, m_settings->allKeys()){

          auto values = m_settings->value(call).toMap();

          auto snr = values.value("snr", -64).toInt();
          auto grid = values.value("grid", "").toString();
          auto freq = values.value("freq", 0).toInt();
          auto tdrift = values.value("tdrift", 0).toFloat();


#if CACHE_CALL_DATETIME_AS_STRINGS
          auto ackTimestampStr = values.value("ackTimestamp", "").toString();
          auto ackTimestamp = QDateTime::fromString(ackTimestampStr, "yyyy-MM-dd hh:mm:ss");
          ackTimestamp.setUtcOffset(0);

          auto utcTimestampStr = values.value("utcTimestamp", "").toString();
          auto utcTimestamp = QDateTime::fromString(utcTimestampStr, "yyyy-MM-dd hh:mm:ss");
          utcTimestamp.setUtcOffset(0);
#else
          auto ackTimestamp = values.value("ackTimestamp").toDateTime();
          auto utcTimestamp = values.value("utcTimestamp").toDateTime();
#endif
          auto mode = values.value("mode", "JS8").toString();

          CallDetail cd = {};
          cd.call = call;
          cd.snr = snr;
          cd.grid = grid;
          cd.freq = freq;
          cd.tdrift = tdrift;
          cd.ackTimestamp = ackTimestamp;
          cd.utcTimestamp = utcTimestamp;
          cd.mode = mode;

          logCallActivity(cd, false);
      }
      m_settings->endGroup();
  }

  if (displayMsgAvg) on_actionMessage_averaging_triggered();

  m_settings_read = true;
}

void MainWindow::set_application_font (QFont const& font)
{
  qApp->setFont (font);
  // set font in the application style sheet as well in case it has
  // been modified in the style sheet which has priority
  qApp->setStyleSheet (qApp->styleSheet () + "* {" + font_as_stylesheet (font) + '}');
  for (auto& widget : qApp->topLevelWidgets ())
    {
      widget->updateGeometry ();
    }
}

void MainWindow::setDecodedTextFont (QFont const& font)
{
  ui->decodedTextBrowser->setContentFont (font);
  ui->decodedTextBrowser2->setContentFont (font);
  ui->textBrowser4->setContentFont(font);
  ui->textBrowser4->displayFoxToBeCalled(" ","#ffffff");
  ui->textBrowser4->setText("");
  auto style_sheet = "QLabel {" + font_as_stylesheet (font) + '}';
  ui->decodedTextLabel->setStyleSheet (ui->decodedTextLabel->styleSheet () + style_sheet);
  ui->decodedTextLabel2->setStyleSheet (ui->decodedTextLabel2->styleSheet () + style_sheet);
  if (m_msgAvgWidget) {
    m_msgAvgWidget->changeFont (font);
  }
  updateGeometry ();
}

void MainWindow::fixStop()
{
    m_hsymStop=((int(m_TRperiod/0.288))/8)*8 - 1; // 0.288 because 6912/12000/2 = 0.288

    if(m_nSubMode == Varicode::JS8CallUltra){
        m_hsymStop++;
    }
}

//-------------------------------------------------------------- dataSink()
void MainWindow::dataSink(qint64 frames)
{
  static float s[NSMAX];
  char line[80];

  int k (frames);
  QString fname {QDir::toNativeSeparators(m_config.writeable_data_dir ().absoluteFilePath ("refspec.dat"))};
  QByteArray bafname = fname.toLatin1();
  const char *c_fname = bafname.data();
  int len=fname.length();

  if(m_diskData) {
    dec_data.params.ndiskdat=1;
  } else {
    dec_data.params.ndiskdat=0;
  }

  m_bUseRef=m_wideGraph->useRef();
  refspectrum_(&dec_data.d2[k-m_nsps/2],&m_bClearRefSpec,&m_bRefSpec,
      &m_bUseRef,c_fname,len);
  m_bClearRefSpec=false;

// Get power, spectrum, and ihsym
  int trmin=m_TRperiod/60;
//  int k (frames - 1);
  dec_data.params.nfa=m_wideGraph->nStartFreq();
  dec_data.params.nfb=m_wideGraph->Fmax();
  int nsps=m_nsps;
  if(m_bFastMode) nsps=6912;
  int nsmo=m_wideGraph->smoothYellow()-1;
  symspec_(&dec_data,&k,&trmin,&nsps,&m_inGain,&nsmo,&m_px,s,&m_df3,&m_ihsym,&m_npts8,&m_pxmax);
  if(m_mode=="WSPR") wspr_downsample_(dec_data.d2,&k);
  if(m_ihsym <=0) return;
  if(ui) ui->signal_meter_widget->setValue(m_px,m_pxmax); // Update thermometer
  if(m_monitoring || m_diskData) {
    m_wideGraph->dataSink2(s,m_df3,m_ihsym,m_diskData);
  }
  if(m_mode=="MSK144") return;

  fixStop();

  if(m_ihsym==3*m_hsymStop/4) {
    m_dialFreqRxWSPR=m_freqNominal;
  }

  //qDebug() << m_ihsym << m_hsymStop;
  if(m_ihsym == m_hsymStop) {
    if(m_mode=="Echo") {
      float snr=0;
      int nfrit=0;
      int nqual=0;
      float f1=1500.0;
      float xlevel=0.0;
      float sigdb=0.0;
      float dfreq=0.0;
      float width=0.0;
      echocom_.nclearave=m_nclearave;
      int nDop=0;
      avecho_(dec_data.d2,&nDop,&nfrit,&nqual,&f1,&xlevel,&sigdb,
          &snr,&dfreq,&width);
      QString t;
      t.sprintf("%3d %7.1f %7.1f %7.1f %7.1f %3d",echocom_.nsum,xlevel,sigdb,
                dfreq,width,nqual);
      t=DriftingDateTime::currentDateTimeUtc().toString("hh:mm:ss  ") + t;
      if (ui) ui->decodedTextBrowser->appendText(t);
      if(m_echoGraph->isVisible()) m_echoGraph->plotSpec();
      m_nclearave=0;
//Don't restart Monitor after an Echo transmission
      if(m_bEchoTxed and !m_auto) {
        monitor(false);
        m_bEchoTxed=false;
      }
      return;
    }
    if(m_mode=="FreqCal") {
      return;
    }
    if( m_dialFreqRxWSPR==0) m_dialFreqRxWSPR=m_freqNominal;
    m_dataAvailable=true;
    dec_data.params.npts8=(m_ihsym*m_nsps)/16;
    dec_data.params.newdat=1;
    dec_data.params.nagain=0;
    dec_data.params.nzhsym=m_hsymStop;
    QDateTime now {DriftingDateTime::currentDateTimeUtc ()};
    m_dateTime = now.toString ("yyyy-MMM-dd hh:mm");
    if(!m_mode.startsWith ("WSPR")) decode(); //Start decoder

    if(!m_diskData) {                        //Always save; may delete later

      if(m_mode=="FT8") {
        int n=now.time().second() % m_TRperiod;
        if(n<(m_TRperiod/2)) n=n+m_TRperiod;
        auto const& period_start=now.addSecs(-n);
        m_fnameWE=m_config.save_directory().absoluteFilePath (period_start.toString("yyMMdd_hhmmss"));
      } else {
        auto const& period_start = now.addSecs (-(now.time ().minute () % (m_TRperiod / 60)) * 60);
        m_fnameWE=m_config.save_directory ().absoluteFilePath (period_start.toString ("yyMMdd_hhmm"));
      }
      m_fileToSave.clear ();

      if(m_saveAll or m_bAltV or (m_bDecoded and m_saveDecoded) or (m_mode!="MSK144" and m_mode!="FT8")) {
          m_bAltV=false;
          // the following is potential a threading hazard - not a good
          // idea to pass pointer to be processed in another thread
          m_saveWAVWatcher.setFuture (QtConcurrent::run (std::bind (&MainWindow::save_wave_file,
                this, m_fnameWE, &dec_data.d2[0], m_TRperiod, m_config.my_callsign(),
                m_config.my_grid(), m_mode, m_nSubMode, m_freqNominal, m_hisCall, m_hisGrid)));
      }
    }

    m_rxDone=true;
  }
}

void MainWindow::startP1()
{
  p1.start(m_cmndP1);
}

QString MainWindow::save_wave_file (QString const& name, short const * data, int seconds,
        QString const& my_callsign, QString const& my_grid, QString const& mode, qint32 sub_mode,
        Frequency frequency, QString const& his_call, QString const& his_grid) const
{
  //
  // This member function runs in a thread and should not access
  // members that may be changed in the GUI thread or any other thread
  // without suitable synchronization.
  //
  QAudioFormat format;
  format.setCodec ("audio/pcm");
  format.setSampleRate (12000);
  format.setChannelCount (1);
  format.setSampleSize (16);
  format.setSampleType (QAudioFormat::SignedInt);
  auto source = QString {"%1, %2"}.arg (my_callsign).arg (my_grid);
  auto comment = QString {"Mode=%1%2, Freq=%3%4"}
     .arg (mode)
     .arg (QString {mode.contains ('J') && !mode.contains ('+')
           ? QString {", Sub Mode="} + QChar {'A' + sub_mode}
         : QString {}})
        .arg (Radio::frequency_MHz_string (frequency))
     .arg (QString {!mode.startsWith ("WSPR") ? QString {", DXCall=%1, DXGrid=%2"}
         .arg (his_call)
         .arg (his_grid).toLocal8Bit () : ""});
  BWFFile::InfoDictionary list_info {
      {{{'I','S','R','C'}}, source.toLocal8Bit ()},
      {{{'I','S','F','T'}}, program_title (revision ()).simplified ().toLocal8Bit ()},
      {{{'I','C','R','D'}}, DriftingDateTime::currentDateTime ()
                          .toString ("yyyy-MM-ddTHH:mm:ss.zzzZ").toLocal8Bit ()},
      {{{'I','C','M','T'}}, comment.toLocal8Bit ()},
        };
  auto file_name = name + ".wav";
  qDebug() << "saving" << file_name;
  BWFFile wav {format, file_name, list_info};
  if (!wav.open (BWFFile::WriteOnly)
      || 0 > wav.write (reinterpret_cast<char const *> (data)
                        , sizeof (short) * seconds * format.sampleRate ()))
    {
      return file_name + ": " + wav.errorString ();
    }
  return QString {};
}

//-------------------------------------------------------------- fastSink()
void MainWindow::fastSink(qint64 frames)
{
}

void MainWindow::showSoundInError(const QString& errorMsg)
{
  if (m_splash && m_splash->isVisible ()) m_splash->hide ();
  MessageBox::critical_message (this, tr ("Error in Sound Input"), errorMsg);
}

void MainWindow::showSoundOutError(const QString& errorMsg)
{
  if (m_splash && m_splash->isVisible ()) m_splash->hide ();
  MessageBox::critical_message (this, tr ("Error in Sound Output"), errorMsg);
}

void MainWindow::showStatusMessage(const QString& statusMsg)
{
    statusBar()->showMessage(statusMsg);
}


/**
 * This function forces the menuBar to rebuild a QAction that has a submenu
 * on OSX fixing a weird bug where they aren't displayed correctly.
 */
void rebuildMacQAction(QMenu *menu, QAction *existingAction){
    auto dummyAction = new QAction("...", menu);
    menu->insertAction(existingAction, dummyAction);
    menu->insertAction(dummyAction, existingAction);
    menu->removeAction(dummyAction);
}

void MainWindow::on_menuModeJS8_aboutToShow(){
    bool canChangeMode = !m_transmitting && m_txFrameCount == 0 && m_txFrameQueue.isEmpty();
    ui->actionModeJS8Normal->setEnabled(canChangeMode);
    ui->actionModeJS8Fast->setEnabled(canChangeMode);
    ui->actionModeJS8Turbo->setEnabled(canChangeMode);
    ui->actionModeJS8Ultra->setEnabled(canChangeMode);
}

void MainWindow::on_menuControl_aboutToShow(){
    QMenu * freqMenu = new QMenu(this->menuBar());
    buildFrequencyMenu(freqMenu);
    ui->actionSetFrequency->setMenu(freqMenu);
#if __APPLE__
    rebuildMacQAction(ui->menuControl, ui->actionSetFrequency);
#endif

    ui->actionEnable_Spotting->setChecked(ui->spotButton->isChecked());

    QMenu * heartbeatMenu = new QMenu(this->menuBar());
    buildHeartbeatMenu(heartbeatMenu);
    ui->actionHeartbeat->setMenu(heartbeatMenu);
#if __APPLE__
    rebuildMacQAction(ui->menuControl, ui->actionHeartbeat);
#endif

    QMenu * cqMenu = new QMenu(this->menuBar());
    buildCQMenu(cqMenu);
    ui->actionCQ->setMenu(cqMenu);
#if __APPLE__
    rebuildMacQAction(ui->menuControl, ui->actionCQ);
#endif
}

void MainWindow::on_actionCheck_for_Updates_triggered(){
    checkVersion(true);
}

void MainWindow::on_actionEnable_Spotting_toggled(bool checked){
    ui->spotButton->setChecked(checked);
}

void MainWindow::on_menuWindow_aboutToShow(){
    ui->actionShow_Fullscreen->setChecked((windowState() & Qt::WindowFullScreen) == Qt::WindowFullScreen);

    ui->actionShow_Statusbar->setChecked(ui->statusBar && ui->statusBar->isVisible());

    auto hsizes = ui->textHorizontalSplitter->sizes();
    ui->actionShow_Band_Activity->setChecked(hsizes.at(0) > 0);
    ui->actionShow_Call_Activity->setChecked(hsizes.at(2) > 0);

    auto vsizes = ui->mainSplitter->sizes();
    ui->actionShow_Frequency_Clock->setChecked(vsizes.first() > 0);
    ui->actionShow_Waterfall->setChecked(vsizes.last() > 0);
    ui->actionShow_Waterfall_Controls->setChecked(m_wideGraph->controlsVisible());
    ui->actionShow_Waterfall_Controls->setEnabled(ui->actionShow_Waterfall->isChecked());
    ui->actionShow_Time_Drift_Controls->setChecked(ui->driftSyncFrame->isVisible());
    ui->actionShow_Time_Drift_Controls->setEnabled(ui->actionShow_Waterfall->isChecked());

    QMenu * sortBandMenu = new QMenu(this->menuBar()); //ui->menuWindow);
    buildBandActivitySortByMenu(sortBandMenu);
    ui->actionSort_Band_Activity->setMenu(sortBandMenu);
    ui->actionSort_Band_Activity->setEnabled(ui->actionShow_Band_Activity->isChecked());
#if __APPLE__
    rebuildMacQAction(ui->menuWindow, ui->actionSort_Band_Activity);
#endif

    QMenu * sortCallMenu = new QMenu(this->menuBar()); //ui->menuWindow);
    buildCallActivitySortByMenu(sortCallMenu);
    ui->actionSort_Call_Activity->setMenu(sortCallMenu);
    ui->actionSort_Call_Activity->setEnabled(ui->actionShow_Call_Activity->isChecked());
#if __APPLE__
    rebuildMacQAction(ui->menuWindow, ui->actionSort_Call_Activity);
#endif

    QMenu * showBandMenu = new QMenu(this->menuBar()); //ui->menuWindow);
    buildShowColumnsMenu(showBandMenu, "band");
    ui->actionShow_Band_Activity_Columns->setMenu(showBandMenu);
    ui->actionShow_Band_Activity_Columns->setEnabled(ui->actionShow_Band_Activity->isChecked());
#if __APPLE__
    rebuildMacQAction(ui->menuWindow, ui->actionShow_Band_Activity_Columns);
#endif

    QMenu * showCallMenu = new QMenu(this->menuBar()); //ui->menuWindow);
    buildShowColumnsMenu(showCallMenu, "call");
    ui->actionShow_Call_Activity_Columns->setMenu(showCallMenu);
    ui->actionShow_Call_Activity_Columns->setEnabled(ui->actionShow_Call_Activity->isChecked());
#if __APPLE__
    rebuildMacQAction(ui->menuWindow, ui->actionShow_Call_Activity_Columns);
#endif

    ui->actionShow_Band_Heartbeats_and_ACKs->setEnabled(ui->actionShow_Band_Activity->isChecked());
}

void MainWindow::on_actionFocus_Message_Receive_Area_triggered(){
    ui->textEditRX->setFocus();
}

void MainWindow::on_actionFocus_Message_Reply_Area_triggered(){
    ui->extFreeTextMsgEdit->setFocus();
}

void MainWindow::on_actionFocus_Band_Activity_Table_triggered(){
    ui->tableWidgetRXAll->setFocus();
}

void MainWindow::on_actionFocus_Call_Activity_Table_triggered(){
    ui->tableWidgetCalls->setFocus();
}

void MainWindow::on_actionSetOffset_triggered(){
    bool ok = false;
    auto currentFreq = currentFreqOffset();
    QString newFreq = QInputDialog::getText(this, tr("Set Frequency Offset"),
                                             tr("Offset in Hz:"), QLineEdit::Normal,
                                             QString("%1").arg(currentFreq), &ok).toUpper().trimmed();
    int offset = newFreq.toInt(&ok);
    if(!ok){
       return;
    }

    setFreqOffsetForRestore(offset, false);
}

void MainWindow::on_actionShow_Fullscreen_triggered(bool checked){
    auto state = windowState();
    if(checked){
        state |= Qt::WindowFullScreen;
    } else {
        state &= ~Qt::WindowFullScreen;
    }
    setWindowState(state);
}

void MainWindow::on_actionShow_Statusbar_triggered(bool checked){
    if(!ui->statusBar){
        return;
    }

    ui->statusBar->setVisible(checked);
}

void MainWindow::on_actionShow_Frequency_Clock_triggered(bool checked){
    auto vsizes = ui->mainSplitter->sizes();
    vsizes[0] = checked ? ui->logHorizontalWidget->minimumHeight() : 0;
    ui->logHorizontalWidget->setVisible(checked);
    ui->mainSplitter->setSizes(vsizes);
}

void MainWindow::on_actionShow_Band_Activity_triggered(bool checked){
    auto hsizes = ui->textHorizontalSplitter->sizes();

    if(m_bandActivityWidth == 0){
        m_bandActivityWidth = ui->textHorizontalSplitter->width()/4;
    }

    if(m_callActivityWidth == 0){
        m_callActivityWidth = ui->textHorizontalSplitter->width()/4;
    }

    if(m_textActivityWidth == 0){
        m_textActivityWidth = ui->textHorizontalSplitter->width()/2;
    }

    if(checked){
        hsizes[0] = m_bandActivityWidth;
        hsizes[1] = m_textActivityWidth;
        if(hsizes[2]) hsizes[2] = m_callActivityWidth;

    } else {
        if(hsizes[0]) m_bandActivityWidth = hsizes[0];
        if(hsizes[1]) m_textActivityWidth = hsizes[1];
        if(hsizes[2]) m_callActivityWidth = hsizes[2];
        hsizes[0] = 0;
    }

    ui->textHorizontalSplitter->setSizes(hsizes);
    ui->tableWidgetRXAll->setVisible(checked);
    m_bandActivityWasVisible = checked;

}

void MainWindow::on_actionShow_Band_Heartbeats_and_ACKs_triggered(bool checked){
    displayBandActivity();
}

void MainWindow::on_actionShow_Call_Activity_triggered(bool checked){
    auto hsizes = ui->textHorizontalSplitter->sizes();

    if(m_bandActivityWidth == 0){
        m_bandActivityWidth = ui->textHorizontalSplitter->width()/4;
    }

    if(m_callActivityWidth == 0){
        m_callActivityWidth = ui->textHorizontalSplitter->width()/4;
    }

    if(m_textActivityWidth == 0){
        m_textActivityWidth = ui->textHorizontalSplitter->width()/2;
    }

    if(checked){
        if(hsizes[0]) hsizes[0] = m_bandActivityWidth;
        hsizes[1] = m_textActivityWidth;
        hsizes[2] = m_callActivityWidth;

    } else {
        if(hsizes[0]) m_bandActivityWidth = hsizes[0];
        if(hsizes[1]) m_textActivityWidth = hsizes[1];
        if(hsizes[2]) m_callActivityWidth = hsizes[2];
        hsizes[2] = 0;
    }

    ui->textHorizontalSplitter->setSizes(hsizes);
    ui->tableWidgetCalls->setVisible(checked);
}

void MainWindow::on_actionShow_Waterfall_triggered(bool checked){
    auto vsizes = ui->mainSplitter->sizes();

    if(m_waterfallHeight == 0){
        m_waterfallHeight = ui->mainSplitter->height()/4;
    }

    if(checked){
        vsizes[vsizes.length() - 1] = m_waterfallHeight;

    } else {
        m_waterfallHeight = vsizes[vsizes.length() - 1];
        vsizes[1] += m_waterfallHeight;
        vsizes[vsizes.length() - 1] = 0;
    }

    ui->mainSplitter->setSizes(vsizes);
    ui->bandHorizontalWidget->setVisible(checked);
}

void MainWindow::on_actionShow_Waterfall_Controls_triggered(bool checked){
    m_wideGraph->setControlsVisible(checked);
}

void MainWindow::on_actionShow_Time_Drift_Controls_triggered(bool checked){
    ui->driftSyncFrame->setVisible(checked);
}

void MainWindow::on_actionReset_Window_Sizes_triggered(){
    auto size = this->centralWidget()->size();

    ui->mainSplitter->setSizes({
        ui->logHorizontalWidget->minimumHeight(),
        ui->mainSplitter->height()/2,
        ui->macroHorizonalWidget->minimumHeight(),
        ui->mainSplitter->height()/4
    });

    ui->textHorizontalSplitter->setSizes({
        ui->textHorizontalSplitter->width()/4,
        ui->textHorizontalSplitter->width()/2,
        ui->textHorizontalSplitter->width()/4
    });

    ui->textVerticalSplitter->setSizes({
        ui->textVerticalSplitter->height()/2,
        ui->textVerticalSplitter->height()/2
    });
}

void MainWindow::on_actionSettings_triggered(){
    openSettings();
}

void MainWindow::openSettings(int tab){
    m_config.select_tab(tab);

    // things that might change that we need know about
    auto callsign = m_config.my_callsign ();
    auto my_grid = m_config.my_grid ();
    if (QDialog::Accepted == m_config.exec ()) {
        if (m_config.my_callsign () != callsign) {
            m_baseCall = Radio::base_callsign (m_config.my_callsign ());
            morse_(const_cast<char *> (m_config.my_callsign ().toLatin1().constData()),
                    const_cast<int *> (icw), &m_ncw, m_config.my_callsign ().length());
        }
        if (m_config.my_callsign () != callsign || m_config.my_grid () != my_grid) {
            statusUpdate ();
        }

        enable_DXCC_entity (m_config.DXCC ());  // sets text window proportions and (re)inits the logbook

        prepareSpotting();

        if(m_config.restart_audio_input ()) {
            Q_EMIT startAudioInputStream (m_config.audio_input_device (),
                m_framesAudioInputBuffered, m_detector, m_downSampleFactor,
                m_config.audio_input_channel ());
        }

        if(m_config.restart_audio_output ()) {
            Q_EMIT initializeAudioOutputStream (m_config.audio_output_device (),
                AudioDevice::Mono == m_config.audio_output_channel () ? 1 : 2,
                m_msAudioOutputBuffered);
        }

        ui->bandComboBox->view ()->setMinimumWidth (ui->bandComboBox->view ()->sizeHintForColumn (FrequencyList_v2::frequency_mhz_column));

        displayDialFrequency ();
        displayActivity(true);

        bool vhf {m_config.enable_VHF_features()};
        m_wideGraph->setVHF(vhf);
        if (!vhf) ui->sbSubmode->setValue (0);

        setup_status_bar (vhf);
        bool b = vhf && (m_mode=="JT4" or m_mode=="JT65" or m_mode=="ISCAT" or
            m_mode=="JT9" or m_mode=="MSK144" or m_mode=="QRA64");
        if(b) VHF_features_enabled(b);
        if(m_mode=="FT8") on_actionJS8_triggered();
        if(b) VHF_features_enabled(b);

        m_config.transceiver_online ();
        if(!m_bFastMode) setXIT (ui->TxFreqSpinBox->value ());
        if(m_config.single_decode() or m_mode=="JT4") {
            ui->label_6->setText("Single-Period Decodes");
            ui->label_7->setText("Average Decodes");
        } else {
            //      ui->label_6->setText("Band Activity");
            //      ui->label_7->setText("Rx Frequency");
        }
        update_watchdog_label ();
        if(!m_splitMode) ui->cbCQTx->setChecked(false);
        if(!m_config.enable_VHF_features()) {
            ui->actionInclude_averaging->setVisible(false);
            ui->actionInclude_correlation->setVisible (false);
            ui->actionInclude_averaging->setChecked(false);
            ui->actionInclude_correlation->setChecked(false);
            ui->actionEnable_AP_JT65->setVisible(false);
        }
        m_opCall=m_config.opCall();
    }
}

void MainWindow::prepareSpotting(){
    if(m_config.spot_to_reporting_networks ()){
        spotSetLocal();
        pskSetLocal();
        aprsSetLocal();
        m_aprsClient->setServer(m_config.aprs_server_name(), m_config.aprs_server_port());
        m_aprsClient->setPaused(false);
        ui->spotButton->setChecked(true);
    } else {
        m_aprsClient->setPaused(true);
        ui->spotButton->setChecked(false);
    }
}

void MainWindow::on_spotButton_clicked(bool checked){
    // 1. save setting
    m_config.set_spot_to_reporting_networks(checked);

    // 2. prepare
    prepareSpotting();
}

void MainWindow::on_monitorButton_clicked (bool checked)
{
  if (!m_transmitting) {
    auto prior = m_monitoring;
    monitor (checked);
    if (checked && !prior) {
      if (m_config.monitor_last_used ()) {
              // put rig back where it was when last in control
        setRig (m_lastMonitoredFrequency);
        setXIT (ui->TxFreqSpinBox->value ());
      }
          // ensure FreqCal triggers
      on_RxFreqSpinBox_valueChanged (ui->RxFreqSpinBox->value ());
    }
      //Get Configuration in/out of strict split and mode checking
    Q_EMIT m_config.sync_transceiver (true, checked);
  } else {
    ui->monitorButton->setChecked (false); // disallow
  }
}

void MainWindow::monitor (bool state)
{
  ui->monitorButton->setChecked (state);
  if (state) {
    m_diskData = false; // no longer reading WAV files
    if (!m_monitoring) Q_EMIT resumeAudioInputStream ();
  } else {
    Q_EMIT suspendAudioInputStream ();
  }
  m_monitoring = state;
}

void MainWindow::on_actionAbout_triggered()                  //Display "About"
{
  CAboutDlg {this}.exec ();
}

void MainWindow::on_autoButton_clicked (bool checked)
{
  m_auto = checked;
  if (checked
      && ui->cbFirst->isVisible () && ui->cbFirst->isChecked()
      && CALLING == m_QSOProgress) {
    m_bAutoReply = false;         // ready for next
    m_bCallingCQ = true;        // allows tail-enders to be picked up
    ui->cbFirst->setStyleSheet ("QCheckBox{color:red}");
  } else {
    ui->cbFirst->setStyleSheet("");
  }
  if (!checked) m_bCallingCQ = false;
  statusUpdate ();
  m_bEchoTxOK=false;
  if(m_auto and (m_mode=="Echo")) {
    m_nclearave=1;
    echocom_.nsum=0;
  }
  if(m_mode.startsWith ("WSPR"))  {
    QPalette palette {ui->sbTxPercent->palette ()};
    if(m_auto or m_pctx==0) {
      palette.setColor(QPalette::Base,Qt::white);
    } else {
      palette.setColor(QPalette::Base,Qt::yellow);
    }
    ui->sbTxPercent->setPalette(palette);
  }
  m_tAutoOn=DriftingDateTime::currentMSecsSinceEpoch()/1000;

  // stop tx, reset the ui and message queue
  if(!checked){
      on_stopTxButton_clicked();
  }
}

void MainWindow::on_monitorButton_toggled(bool checked){
    resetPushButtonToggleText(ui->monitorButton);
}

void MainWindow::on_monitorTxButton_toggled(bool checked){
    resetPushButtonToggleText(ui->monitorTxButton);
}

void MainWindow::on_tuneButton_toggled(bool checked){
    resetPushButtonToggleText(ui->tuneButton);
}

void MainWindow::on_spotButton_toggled(bool checked){
    resetPushButtonToggleText(ui->spotButton);
}

void MainWindow::auto_tx_mode (bool state)
{
    ui->autoButton->setChecked (state);
    on_autoButton_clicked (state);
}

void MainWindow::keyPressEvent (QKeyEvent * e)
{
    switch (e->key()) {
        case Qt::Key_Escape:
            on_stopTxButton_clicked();
            stopTx();
            return;
        case Qt::Key_F5:
            on_logQSOButton_clicked();
            return;
    }

    QMainWindow::keyPressEvent (e);
}

void MainWindow::bumpFqso(int n)                                 //bumpFqso()
{
  int i;
  bool ctrl = (n>=100);
  n=n%100;
  i=ui->RxFreqSpinBox->value();
  bool bTrackTx=ui->TxFreqSpinBox->value() == i;
  if(n==11) i--;
  if(n==12) i++;
  if (ui->RxFreqSpinBox->isEnabled ()) {
    ui->RxFreqSpinBox->setValue (i);
  }
  if(ctrl and m_mode.startsWith ("WSPR")) {
    ui->WSPRfreqSpinBox->setValue(i);
  } else {
    if(ctrl and bTrackTx) {
      ui->TxFreqSpinBox->setValue (i);
    }
  }
}

Radio::Frequency MainWindow::dialFrequency() {
    return Frequency {m_rigState.ptt () && m_rigState.split () ?
        m_rigState.tx_frequency () : m_rigState.frequency ()};
}

QString MainWindow::currentMode(){
    if(m_nSubMode == Varicode::JS8CallNormal){
        return "NORMAL";
    }
    else if(m_nSubMode == Varicode::JS8CallFast){
        return "FAST";
    }
    else if(m_nSubMode == Varicode::JS8CallTurbo){
        return "TURBO";
    }
    else if(m_nSubMode == Varicode::JS8CallUltra){
#ifdef JS8D_IS_ULTRA
        return "ULTRA";
#else
        return "SLOW";
#endif
    }

    return "?";
}

void MainWindow::updateCurrentBand(){
    QVariant state = ui->readFreq->property("state");
    if(!state.isValid()){
        return;
    }

    auto dial_frequency = dialFrequency();
    auto const& band_name = m_config.bands ()->find(dial_frequency);

    if (m_lastBand == band_name){
        return;
    }

    cacheActivity(m_lastBand);

    // clear activity on startup if asked or on when the previous band is not empty
    if(m_config.reset_activity() || !m_lastBand.isEmpty()){
        clearActivity();
    }

    // only change this when necessary as we get called a lot and it
    // would trash any user input to the band combo box line edit
    ui->bandComboBox->setCurrentText (band_name);
    m_wideGraph->setRxBand (band_name);

    qDebug() << "setting band" << band_name;
    m_lastBand = band_name;

    band_changed(dial_frequency);
    restoreActivity(m_lastBand);
}

void MainWindow::displayDialFrequency (){
#if 0
    qDebug() << "rx nominal" << m_freqNominal;
    qDebug() << "tx nominal" << m_freqTxNominal;
    qDebug() << "offset set to" << ui->RxFreqSpinBox->value() << ui->TxFreqSpinBox->value();
#endif

    auto dial_frequency = dialFrequency();
    auto audio_frequency = currentFreqOffset();

    // lookup band
    auto const& band_name = m_config.bands ()->find (dial_frequency);
    bool valid = !band_name.isEmpty();

    update_dynamic_property (ui->labDialFreq, "oob", !valid);
    ui->labDialFreq->setText (Radio::pretty_frequency_MHz_string (dial_frequency));

    auto sFreq = Radio::pretty_frequency_MHz_string (dial_frequency);
    ui->currentFreq->setDigitCount(sFreq.length());
    ui->currentFreq->display(sFreq);

    if(m_splitMode && m_transmitting){
        audio_frequency -= m_XIT;
    }
    ui->labDialFreqOffset->setText(QString("%1 Hz").arg(audio_frequency));
}

void MainWindow::statusChanged()
{
  statusUpdate ();
}

bool MainWindow::eventFilter (QObject * object, QEvent * event)
{
  switch (event->type())
    {
    case QEvent::KeyPress:
      // fall through
    case QEvent::MouseButtonPress:
      // reset the Tx watchdog
      resetIdleTimer();
      tx_watchdog (false);
      break;

    case QEvent::ChildAdded:
      // ensure our child widgets get added to our event filter
      add_child_to_event_filter (static_cast<QChildEvent *> (event)->child ());
      break;

    case QEvent::ChildRemoved:
      // ensure our child widgets get d=removed from our event filter
      remove_child_from_event_filter (static_cast<QChildEvent *> (event)->child ());
      break;

    case QEvent::ToolTip:
      if(!ui->actionShow_Tooltips->isChecked()){
          return true;
      }

      break;

    default: break;
    }
  return QObject::eventFilter(object, event);
}

void MainWindow::createStatusBar()                           //createStatusBar
{
  tx_status_label.setAlignment (Qt::AlignHCenter);
  tx_status_label.setMinimumSize (QSize  {150, 18});
  tx_status_label.setStyleSheet ("QLabel{background-color: #22ff22}");
  tx_status_label.setFrameStyle (QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget (&tx_status_label);

  config_label.setAlignment (Qt::AlignHCenter);
  config_label.setMinimumSize (QSize {80, 18});
  config_label.setFrameStyle (QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget (&config_label);
  config_label.hide ();         // only shown for non-default configuration

  mode_label.setAlignment (Qt::AlignHCenter);
  mode_label.setMinimumSize (QSize {80, 18});
  mode_label.setFrameStyle (QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget (&mode_label);

  last_tx_label.setAlignment (Qt::AlignHCenter);
  last_tx_label.setMinimumSize (QSize {150, 18});
  last_tx_label.setFrameStyle (QFrame::Panel | QFrame::Sunken);
  statusBar()->addWidget (&last_tx_label);

  band_hopping_label.setAlignment (Qt::AlignHCenter);
  band_hopping_label.setMinimumSize (QSize {90, 18});
  band_hopping_label.setFrameStyle (QFrame::Panel | QFrame::Sunken);

  statusBar()->addPermanentWidget(&progressBar, 1);
  progressBar.setMinimumSize (QSize {100, 18});
  progressBar.setFormat ("%v/%m");

  statusBar()->addPermanentWidget(&wpm_label);
  wpm_label.setMinimumSize (QSize {120, 18});
  wpm_label.setFrameStyle (QFrame::Panel | QFrame::Sunken);
  wpm_label.setAlignment(Qt::AlignHCenter);

  statusBar ()->addPermanentWidget (&watchdog_label);
  update_watchdog_label ();
}

void MainWindow::setup_status_bar (bool vhf)
{
  auto submode = current_submode ();
  if (vhf && submode != QChar::Null)
    {
      mode_label.setText (m_mode + " " + submode);
    }
  else
    {
      if(m_mode == "FT8"){
        mode_label.setText("JS8");
      } else {
        mode_label.setText (m_mode);
      }
    }
  if ("ISCAT" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #ff9933}");
  } else if ("JT9" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #ff6ec7}");
  } else if ("JT4" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #cc99ff}");
  } else if ("Echo" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #66ffff}");
  } else if ("JT9+JT65" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #ffff66}");
  } else if ("JT65" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #66ff66}");
  } else if ("QRA64" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #99ff33}");
  } else if ("MSK144" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #ff6666}");
  } else if ("FT8" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #6699ff}");
  } else if ("FreqCal" == m_mode) {
    mode_label.setStyleSheet ("QLabel{background-color: #ff9933}");  }
  last_tx_label.setText (QString {});
  if (m_mode.contains (QRegularExpression {R"(^(Echo|ISCAT))"})) {
    if (band_hopping_label.isVisible ()) statusBar ()->removeWidget (&band_hopping_label);
  } else if (m_mode.startsWith ("WSPR")) {
    mode_label.setStyleSheet ("QLabel{background-color: #ff66ff}");
    if (!band_hopping_label.isVisible ()) {
      statusBar ()->addWidget (&band_hopping_label);
      band_hopping_label.show ();
    }
  } else {
    if (band_hopping_label.isVisible ()) statusBar ()->removeWidget (&band_hopping_label);
  }
}

void MainWindow::subProcessFailed (QProcess * process, int exit_code, QProcess::ExitStatus status)
{
  if (m_valid && (exit_code || QProcess::NormalExit != status))
    {
      QStringList arguments;
      for (auto argument: process->arguments ())
        {
          if (argument.contains (' ')) argument = '"' + argument + '"';
          arguments << argument;
        }
      if (m_splash && m_splash->isVisible ()) m_splash->hide ();
      MessageBox::critical_message (this, tr ("Subprocess Error")
                                    , tr ("Subprocess failed with exit code %1")
                                    .arg (exit_code)
                                    , tr ("Running: %1\n%2")
                                    .arg (process->program () + ' ' + arguments.join (' '))
                                    .arg (QString {process->readAllStandardError()}));
      QTimer::singleShot (0, this, SLOT (close ()));
      m_valid = false;          // ensures exit if still constructing
    }
}

void MainWindow::subProcessError (QProcess * process, QProcess::ProcessError)
{
  if (m_valid)
    {
      QStringList arguments;
      for (auto argument: process->arguments ())
        {
          if (argument.contains (' ')) argument = '"' + argument + '"';
          arguments << argument;
        }
      if (m_splash && m_splash->isVisible ()) m_splash->hide ();
      MessageBox::critical_message (this, tr ("Subprocess error")
                                    , tr ("Running: %1\n%2")
                                    .arg (process->program () + ' ' + arguments.join (' '))
                                    .arg (process->errorString ()));
      QTimer::singleShot (0, this, SLOT (close ()));
      m_valid = false;              // ensures exit if still constructing
    }
}

void MainWindow::closeEvent(QCloseEvent * e)
{
  m_valid = false;              // suppresses subprocess errors
  m_config.transceiver_offline ();
  writeSettings ();
  m_astroWidget.reset ();
  m_guiTimer.stop ();
  m_prefixes.reset ();
  m_shortcuts.reset ();
  m_mouseCmnds.reset ();
  if(m_mode!="MSK144" and m_mode!="FT8") killFile();
  float sw=0.0;
  int nw=400;
  int nh=100;
  int irow=-99;
  plotsave_(&sw,&nw,&nh,&irow);
  mem_js8->detach();
  QFile quitFile {m_config.temp_dir ().absoluteFilePath (".quit")};
  quitFile.open(QIODevice::ReadWrite);
  QFile {m_config.temp_dir ().absoluteFilePath (".lock")}.remove(); // Allow jt9 to terminate
  bool b=proc_js8.waitForFinished(1000);
  if(!b) proc_js8.close();
  quitFile.remove();

  Q_EMIT finished ();

  QMainWindow::closeEvent (e);
}

void MainWindow::on_labDialFreq_clicked()                       //dialFrequency
{
    ui->bandComboBox->setFocus();
}

void MainWindow::on_stopButton_clicked()                       //stopButton
{
  monitor (false);
  m_loopall=false;
  if(m_bRefSpec) {
    MessageBox::information_message (this, tr ("Reference spectrum saved"));
    m_bRefSpec=false;
  }
}

void MainWindow::on_actionAdd_Log_Entry_triggered(){
  on_logQSOButton_clicked();
}

void MainWindow::on_actionRelease_Notes_triggered ()
{
  QDesktopServices::openUrl (QUrl {"http://physics.princeton.edu/pulsar/k1jt/Release_Notes.txt"});
}

void MainWindow::on_actionFT8_DXpedition_Mode_User_Guide_triggered()
{
  QDesktopServices::openUrl (QUrl {"http://physics.princeton.edu/pulsar/k1jt/FT8_DXpedition_Mode.pdf"});
}
void MainWindow::on_actionOnline_User_Guide_triggered()      //Display manual
{
}

//Display local copy of manual
void MainWindow::on_actionLocal_User_Guide_triggered()
{
}

void MainWindow::on_actionWide_Waterfall_triggered()      //Display Waterfalls
{
  m_wideGraph->show();
}

void MainWindow::on_actionEcho_Graph_triggered()
{
  m_echoGraph->show();
}

void MainWindow::on_actionFast_Graph_triggered()
{
  m_fastGraph->show();
}

void MainWindow::on_actionSolve_FreqCal_triggered()
{
  QString dpath{QDir::toNativeSeparators(m_config.writeable_data_dir().absolutePath()+"/")};
  char data_dir[512];
  int len=dpath.length();
  int iz,irc;
  double a,b,rms,sigmaa,sigmab;
  strncpy(data_dir,dpath.toLatin1(),len);
  calibrate_(data_dir,&iz,&a,&b,&rms,&sigmaa,&sigmab,&irc,len);
  QString t2;
  if(irc==-1) t2="Cannot open " + dpath + "fmt.all";
  if(irc==-2) t2="Cannot open " + dpath + "fcal2.out";
  if(irc==-3) t2="Insufficient data in fmt.all";
  if(irc==-4) t2 = tr ("Invalid data in fmt.all at line %1").arg (iz);
  if(irc>0 or rms>1.0) t2="Check fmt.all for possible bad data.";
  if (irc < 0 || irc > 0 || rms > 1.) {
    MessageBox::warning_message (this, "Calibration Error", t2);
  }
  else if (MessageBox::Apply == MessageBox::query_message (this
                                                           , tr ("Good Calibration Solution")
                                                           , tr ("<pre>"
                                                                 "%1%L2 ±%L3 ppm\n"
                                                                 "%4%L5 ±%L6 Hz\n\n"
                                                                 "%7%L8\n"
                                                                 "%9%L10 Hz"
                                                                 "</pre>")
                                                           .arg ("Slope: ", 12).arg (b, 0, 'f', 3).arg (sigmab, 0, 'f', 3)
                                                           .arg ("Intercept: ", 12).arg (a, 0, 'f', 2).arg (sigmaa, 0, 'f', 2)
                                                           .arg ("N: ", 12).arg (iz)
                                                           .arg ("StdDev: ", 12).arg (rms, 0, 'f', 2)
                                                           , QString {}
                                                           , MessageBox::Cancel | MessageBox::Apply)) {
    m_config.set_calibration (Configuration::CalibrationParams {a, b});
    if (MessageBox::Yes == MessageBox::query_message (this
                                                      , tr ("Delete Calibration Measurements")
                                                      , tr ("The \"fmt.all\" file will be renamed as \"fmt.bak\""))) {
      // rename fmt.all as we have consumed the resulting calibration
      // solution
      auto const& backup_file_name = m_config.writeable_data_dir ().absoluteFilePath ("fmt.bak");
      QFile::remove (backup_file_name);
      QFile::rename (m_config.writeable_data_dir ().absoluteFilePath ("fmt.all"), backup_file_name);
    }
  }
}

void MainWindow::on_actionCopyright_Notice_triggered()
{
  auto const& message = tr("If you make fair use of any part of this program under terms of the GNU "
                           "General Public License, you must display the following copyright "
                           "notice prominently in your derivative work:\n\n"
                           "\"The algorithms, source code, look-and-feel of WSJT-X and related "
                           "programs, and protocol specifications for the modes FSK441, FT8, JT4, "
                           "JT6M, JT9, JT65, JTMS, QRA64, ISCAT, MSK144 are Copyright (C) "
                           "2001-2018 by one or more of the following authors: Joseph Taylor, "
                           "K1JT; Bill Somerville, G4WJS; Steven Franke, K9AN; Nico Palermo, "
                           "IV3NWV; Greg Beam, KI7MT; Michael Black, W9MDB; Edson Pereira, PY2SDR; "
                           "Philip Karn, KA9Q; and other members of the WSJT Development Group.\n\n"
                           "Further, the source code of JS8Call contains material Copyright (C) "
                           "2018-2019 by Jordan Sherer, KN4CRD.\"");
  MessageBox::warning_message(this, message);
}

// This allows the window to shrink by removing certain things
// and reducing space used by controls
void MainWindow::hideMenus(bool checked)
{
  int spacing = checked ? 1 : 6;
  if (checked) {
      statusBar ()->removeWidget (&auto_tx_label);
      minimumSize().setHeight(450);
      minimumSize().setWidth(700);
      restoreGeometry(m_geometryNoControls);
      updateGeometry();
  } else {
      m_geometryNoControls = saveGeometry();
      statusBar ()->addWidget(&auto_tx_label);
      minimumSize().setHeight(520);
      minimumSize().setWidth(770);
  }
  ui->menuBar->setVisible(!checked);
  if(m_mode!="FreqCal" and m_mode!="WSPR") {
    ui->label_6->setVisible(!checked);
    ui->label_7->setVisible(!checked);
    ui->decodedTextLabel2->setVisible(!checked);
//    ui->line_2->setVisible(!checked);
  }
//  ui->line->setVisible(!checked);
  ui->decodedTextLabel->setVisible(!checked);
  ui->gridLayout_5->layout()->setSpacing(spacing);
  ui->horizontalLayout->layout()->setSpacing(spacing);
  ui->horizontalLayout_2->layout()->setSpacing(spacing);
  ui->horizontalLayout_3->layout()->setSpacing(spacing);
  ui->horizontalLayout_5->layout()->setSpacing(spacing);
  ui->horizontalLayout_6->layout()->setSpacing(spacing);
  ui->horizontalLayout_7->layout()->setSpacing(spacing);
  ui->horizontalLayout_8->layout()->setSpacing(spacing);
  ui->horizontalLayout_9->layout()->setSpacing(spacing);
  ui->horizontalLayout_10->layout()->setSpacing(spacing);
  ui->horizontalLayout_11->layout()->setSpacing(spacing);
  ui->horizontalLayout_12->layout()->setSpacing(spacing);
  ui->horizontalLayout_13->layout()->setSpacing(spacing);
  ui->horizontalLayout_14->layout()->setSpacing(spacing);
  ui->verticalLayout->layout()->setSpacing(spacing);
  ui->verticalLayout_2->layout()->setSpacing(spacing);
  ui->verticalLayout_3->layout()->setSpacing(spacing);
  ui->verticalLayout_4->layout()->setSpacing(spacing);
  ui->verticalLayout_5->layout()->setSpacing(spacing);
  ui->verticalLayout_7->layout()->setSpacing(spacing);
  ui->verticalLayout_8->layout()->setSpacing(spacing);
  ui->tab->layout()->setSpacing(spacing);
}

void MainWindow::on_actionAstronomical_data_toggled (bool checked)
{
  if (checked)
    {
      m_astroWidget.reset (new Astro {m_settings, &m_config});

      // hook up termination signal
      connect (this, &MainWindow::finished, m_astroWidget.data (), &Astro::close);
      connect (m_astroWidget.data (), &Astro::tracking_update, [this] {
          m_astroCorrection = {};
          setRig ();
          setXIT (ui->TxFreqSpinBox->value ());
          displayDialFrequency ();
        });
      m_astroWidget->showNormal();
      m_astroWidget->raise ();
      m_astroWidget->activateWindow ();
      m_astroWidget->nominal_frequency (m_freqNominal, m_freqTxNominal);
    }
  else
    {
      m_astroWidget.reset ();
    }
}

void MainWindow::on_actionFox_Log_triggered()
{
  on_actionMessage_averaging_triggered();
  m_msgAvgWidget->foxLogSetup();
}

void MainWindow::on_actionMessage_averaging_triggered()
{
#if 0
  if (!m_msgAvgWidget)
    {
      m_msgAvgWidget.reset (new MessageAveraging {m_settings, m_config.decoded_text_font ()});

      // Connect signals from Message Averaging window
      connect (this, &MainWindow::finished, m_msgAvgWidget.data (), &MessageAveraging::close);
    }
  m_msgAvgWidget->showNormal();
  m_msgAvgWidget->raise ();
  m_msgAvgWidget->activateWindow ();
#endif
}

void MainWindow::on_actionOpen_triggered()                     //Open File
{
  monitor (false);

  QString fname;
  fname=QFileDialog::getOpenFileName(this, "Open File", m_path,
                                     "Audio Files (*.wav)");
  if(!fname.isEmpty ()) {
    m_path=fname;
    int i1=fname.lastIndexOf("/");
    QString baseName=fname.mid(i1+1);
    tx_status_label.setStyleSheet("QLabel{background-color: #99ffff}");
    tx_status_label.setText(" " + baseName + " ");
    on_stopButton_clicked();
    m_diskData=true;
    read_wav_file (fname);
  }
}

void MainWindow::read_wav_file (QString const& fname)
{
  // call diskDat() when done
  int i0=fname.lastIndexOf("_");
  int i1=fname.indexOf(".wav");
  m_nutc0=m_UTCdisk;
  m_UTCdisk=fname.mid(i0+1,i1-i0-1).toInt();
  m_wav_future_watcher.setFuture (QtConcurrent::run ([this, fname] {
        auto basename = fname.mid (fname.lastIndexOf ('/') + 1);
        auto pos = fname.indexOf (".wav", 0, Qt::CaseInsensitive);
        // global variables and threads do not mix well, this needs changing
        dec_data.params.nutc = 0;
        if (pos > 0)
          {
            if (pos == fname.indexOf ('_', -11) + 7)
              {
                dec_data.params.nutc = fname.mid (pos - 6, 6).toInt ();
              }
            else
              {
                dec_data.params.nutc = 100 * fname.mid (pos - 4, 4).toInt ();
              }
          }
        BWFFile file {QAudioFormat {}, fname};
        bool ok=file.open (BWFFile::ReadOnly);
        if(ok) {
          auto bytes_per_frame = file.format ().bytesPerFrame ();
          qint64 max_bytes = std::min (std::size_t (m_TRperiod * RX_SAMPLE_RATE),
              sizeof (dec_data.d2) / sizeof (dec_data.d2[0]))* bytes_per_frame;
          auto n = file.read (reinterpret_cast<char *> (dec_data.d2),
                            std::min (max_bytes, file.size ()));
          int frames_read = n / bytes_per_frame;
        // zero unfilled remaining sample space
          std::memset(&dec_data.d2[frames_read],0,max_bytes - n);
          if (11025 == file.format ().sampleRate ()) {
            short sample_size = file.format ().sampleSize ();
            wav12_ (dec_data.d2, dec_data.d2, &frames_read, &sample_size);
          }
          dec_data.params.kin = frames_read;
          dec_data.params.newdat = 1;
        } else {
          dec_data.params.kin = 0;
          dec_data.params.newdat = 0;
        }

        if(basename.mid(0,10)=="000000_000" && m_mode == "FT8") {
          dec_data.params.nutc=15*basename.mid(10,3).toInt();
        }

      }));
}

void MainWindow::on_actionOpen_next_in_directory_triggered()   //Open Next
{
  monitor (false);

  int i,len;
  QFileInfo fi(m_path);
  QStringList list;
  list= fi.dir().entryList().filter(".wav",Qt::CaseInsensitive);
  for (i = 0; i < list.size()-1; ++i) {
    len=list.at(i).length();
    if(list.at(i)==m_path.right(len)) {
      int n=m_path.length();
      QString fname=m_path.replace(n-len,len,list.at(i+1));
      m_path=fname;
      int i1=fname.lastIndexOf("/");
      QString baseName=fname.mid(i1+1);
      tx_status_label.setStyleSheet("QLabel{background-color: #99ffff}");
      tx_status_label.setText(" " + baseName + " ");
      m_diskData=true;
      read_wav_file (fname);
      if(m_loopall and (i==list.size()-2)) {
        m_loopall=false;
        m_bNoMoreFiles=true;
      }
      return;
    }
  }
}
//Open all remaining files
void MainWindow::on_actionDecode_remaining_files_in_directory_triggered()
{
  m_loopall=true;
  on_actionOpen_next_in_directory_triggered();
}

void MainWindow::diskDat()                                   //diskDat()
{
  if(dec_data.params.kin>0) {
    int k;
    int kstep=m_FFTSize;
    m_diskData=true;
    float db=m_config.degrade();
    float bw=m_config.RxBandwidth();
    if(db > 0.0) degrade_snr_(dec_data.d2,&dec_data.params.kin,&db,&bw);
    for(int n=1; n<=m_hsymStop; n++) {                      // Do the waterfall spectra
      k=(n+1)*kstep;
      if(k > dec_data.params.kin) break;
      dec_data.params.npts8=k/8;
      dataSink(k);
      qApp->processEvents();                                //Update the waterfall
    }
  } else {
    MessageBox::information_message(this, tr("No data read from disk. Wrong file format?"));
  }
}

//Delete ../save/*.wav
void MainWindow::on_actionDelete_all_wav_files_in_SaveDir_triggered()
{
  auto button = MessageBox::query_message (this, tr ("Confirm Delete"),
                                             tr ("Are you sure you want to delete all *.wav files in \"%1\"?")
                                             .arg (QDir::toNativeSeparators (m_config.save_directory ().absolutePath ())));
  if (MessageBox::Yes == button) {
    Q_FOREACH (auto const& file
               , m_config.save_directory ().entryList ({"*.wav", "*.c2"}, QDir::Files | QDir::Writable)) {
      m_config.save_directory ().remove (file);
    }
  }
}

void MainWindow::on_actionNone_triggered()                    //Save None
{
  m_saveDecoded=false;
  m_saveAll=false;
  ui->actionNone->setChecked(true);
}

void MainWindow::on_actionSave_decoded_triggered()
{
  m_saveDecoded=true;
  m_saveAll=false;
  ui->actionSave_decoded->setChecked(true);
}

void MainWindow::on_actionSave_all_triggered()                //Save All
{
  m_saveDecoded=false;
  m_saveAll=true;
  ui->actionSave_all->setChecked(true);
}

void MainWindow::on_actionKeyboard_shortcuts_triggered()
{
}

void MainWindow::on_actionSpecial_mouse_commands_triggered()
{
}

void MainWindow::on_DecodeButton_clicked (bool /* checked */) //Decode request
{
  if(m_mode=="MSK144") {
    ui->DecodeButton->setChecked(false);
  } else {
    if(!m_mode.startsWith ("WSPR") && !m_decoderBusy) {
      dec_data.params.newdat=0;
      dec_data.params.nagain=1;
      m_blankLine=false; // don't insert the separator again
      decode();
    }
  }
}

void MainWindow::freezeDecode(int n)                          //freezeDecode()
{
  if((n%100)==2) on_DecodeButton_clicked (true);
}

void MainWindow::on_ClrAvgButton_clicked()
{
  m_nclearave=1;
  if(m_msgAvgWidget != NULL) {
    if(m_msgAvgWidget->isVisible()) m_msgAvgWidget->displayAvg("");
  }
}

void MainWindow::msgAvgDecode2()
{
  on_DecodeButton_clicked (true);
}

void MainWindow::decode()                                       //decode()
{
  QDateTime now = DriftingDateTime::currentDateTime();
  if( m_dateTimeLastTX.isValid () ) {
    qint64 isecs_since_tx = m_dateTimeLastTX.secsTo(now);
    dec_data.params.lapcqonly= (isecs_since_tx > 600);
//    QTextStream(stdout) << "last tx " << isecs_since_tx << endl;
  } else {
    m_dateTimeLastTX = now.addSecs(-900);
    dec_data.params.lapcqonly=true;
  }
  if( m_diskData ) {
    dec_data.params.lapcqonly=false;
  }

  m_msec0=DriftingDateTime::currentMSecsSinceEpoch();
  if(!m_dataAvailable or m_TRperiod==0) return;
  ui->DecodeButton->setChecked (true);
  if(!dec_data.params.nagain && m_diskData && !m_bFastMode && m_mode!="FT8") {
    dec_data.params.nutc=dec_data.params.nutc/100;
  }
  if(dec_data.params.nagain==0 && dec_data.params.newdat==1 && (!m_diskData)) {
    qint64 ms = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
    int imin=ms/60000;
    int ihr=imin/60;
    imin=imin % 60;
    if(m_TRperiod>=60) imin=imin - (imin % (m_TRperiod/60));
    dec_data.params.nutc=100*ihr + imin;
    if(m_mode=="ISCAT" or m_mode=="MSK144" or m_bFast9 or m_mode=="FT8") {
      QDateTime t=DriftingDateTime::currentDateTimeUtc().addSecs(2-m_TRperiod);
      ihr=t.toString("hh").toInt();
      imin=t.toString("mm").toInt();
      int isec=t.toString("ss").toInt();
      isec=isec - isec%m_TRperiod;
      dec_data.params.nutc=10000*ihr + 100*imin + isec;
    }
  }

  if(m_nPick==1 and !m_diskData) {
    QDateTime t=DriftingDateTime::currentDateTimeUtc();
    int ihr=t.toString("hh").toInt();
    int imin=t.toString("mm").toInt();
    int isec=t.toString("ss").toInt();
    isec=isec - isec%m_TRperiod;
    dec_data.params.nutc=10000*ihr + 100*imin + isec;
  }
  if(m_nPick==2) dec_data.params.nutc=m_nutc0;
  dec_data.params.nQSOProgress = m_QSOProgress;
  dec_data.params.nfqso=m_wideGraph->rxFreq();
  dec_data.params.nftx = ui->TxFreqSpinBox->value ();
  qint32 depth {m_ndepth};
  if (!ui->actionInclude_averaging->isVisible ()) depth &= ~16;
  if (!ui->actionInclude_correlation->isVisible ()) depth &= ~32;
  if (!ui->actionEnable_AP_DXcall->isVisible ()) depth &= ~64;
  dec_data.params.ndepth=depth;
  dec_data.params.n2pass=1;
  if(m_config.twoPass()) dec_data.params.n2pass=2;
  dec_data.params.nranera=m_config.ntrials();
  dec_data.params.naggressive=m_config.aggressive();
  dec_data.params.nrobust=0;
  dec_data.params.ndiskdat=0;
  if(m_diskData) dec_data.params.ndiskdat=1;
  dec_data.params.nfa=m_wideGraph->nStartFreq();
  dec_data.params.nfSplit=m_wideGraph->Fmin();
  dec_data.params.nfb=m_wideGraph->Fmax();

  //if(m_mode=="FT8" and m_config.bHound() and !ui->cbRxAll->isChecked()) dec_data.params.nfb=1000;
  //if(m_mode=="FT8" and m_config.bFox()) dec_data.params.nfqso=200;

  dec_data.params.ntol=ui->sbFtol->value ();
  if(m_mode=="JT9+JT65" or !m_config.enable_VHF_features()) {
    dec_data.params.ntol=20;
    dec_data.params.naggressive=0;
  }
  if(dec_data.params.nutc < m_nutc0) m_RxLog = 1;       //Date and Time to ALL.TXT
  if(dec_data.params.newdat==1 and !m_diskData) m_nutc0=dec_data.params.nutc;
  dec_data.params.ntxmode=9;
  if(m_modeTx=="JT65") dec_data.params.ntxmode=65;
  dec_data.params.nmode=9;
  if(m_mode=="JT65") dec_data.params.nmode=65;
  if(m_mode=="JT65") dec_data.params.ljt65apon = ui->actionEnable_AP_JT65->isVisible () && ui->actionEnable_AP_JT65->isChecked ();
  if(m_mode=="QRA64") dec_data.params.nmode=164;
  if(m_mode=="QRA64") dec_data.params.ntxmode=164;
  if(m_mode=="JT9+JT65") dec_data.params.nmode=9+65;  // = 74
  if(m_mode=="JT4") {
    dec_data.params.nmode=4;
    dec_data.params.ntxmode=4;
  }
  if(m_mode=="FT8") dec_data.params.nmode=8;
  if(m_mode=="FT8") dec_data.params.lft8apon = ui->actionEnable_AP_FT8->isVisible () && ui->actionEnable_AP_FT8->isChecked ();
  if(m_mode=="FT8") dec_data.params.napwid=50;
  dec_data.params.ntrperiod=m_TRperiod;
  dec_data.params.nsubmode=m_nSubMode;
  if(m_mode=="QRA64") dec_data.params.nsubmode=100 + m_nSubMode;
  dec_data.params.minw=0;
  dec_data.params.nclearave=m_nclearave;
  if(m_nclearave!=0) {
    QFile f(m_config.temp_dir ().absoluteFilePath ("avemsg.txt"));
    f.remove();
  }
  dec_data.params.dttol=m_DTtol;
  dec_data.params.emedelay=0.0;
  if(m_config.decode_at_52s()) dec_data.params.emedelay=2.5;
  dec_data.params.minSync=ui->syncSpinBox->isVisible () ? m_minSync : 0;
  dec_data.params.nexp_decode=0;
  if(m_config.single_decode()) dec_data.params.nexp_decode += 32;
  if(m_config.enable_VHF_features()) dec_data.params.nexp_decode += 64;
  if(ui->cbVHFcontest->isChecked()) dec_data.params.nexp_decode += 128;

  strncpy(dec_data.params.datetime, m_dateTime.toLatin1(), 20);
  strncpy(dec_data.params.mycall, (m_config.my_callsign()+"            ").toLatin1(),12);
  strncpy(dec_data.params.mygrid, (m_config.my_grid()+"      ").toLatin1(),6);
  QString hisCall {ui->dxCallEntry->text ()};
  QString hisGrid {ui->dxGridEntry->text ()};
  strncpy(dec_data.params.hiscall,(hisCall + "            ").toLatin1 ().constData (), 12);
  strncpy(dec_data.params.hisgrid,(hisGrid + "      ").toLatin1 ().constData (), 6);

  //newdat=1  ==> this is new data, must do the big FFT
  //nagain=1  ==> decode only at fQSO +/- Tol

  char *to = (char*)mem_js8->data();
  char *from = (char*) dec_data.ss;
  int size=sizeof(struct dec_data);
  if(dec_data.params.newdat==0) {
    int noffset {offsetof (struct dec_data, params.nutc)};
    to += noffset;
    from += noffset;
    size -= noffset;
  }

  memcpy(to, from, qMin(mem_js8->size(), size));
  QFile {m_config.temp_dir ().absoluteFilePath (".lock")}.remove (); // Allow jt9 to start
  decodeBusy(true);
}

void MainWindow::writeAllTxt(QString message, int bits)
{
  // Write decoded text to file "ALL.TXT".
  QFile f {m_config.writeable_data_dir ().absoluteFilePath ("ALL.TXT")};
      if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream out(&f);
        if(m_RxLog==1) {
          out << DriftingDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss")
              << "  " << qSetRealNumberPrecision (12) << (m_freqNominal / 1.e6) << " MHz  "
              << "JS8" << endl;
          m_RxLog=0;
        }
        auto dt = DecodedText(message, bits, m_nSubMode);
        out << dt.message() << endl;
        f.close();
      } else {
        MessageBox::warning_message (this, tr ("File Open Error")
                                     , tr ("Cannot open \"%1\" for append: %2")
                                     .arg (f.fileName ()).arg (f.errorString ()));
      }
}

void MainWindow::writeMsgTxt(QString message, int snr)
{
  // Write decoded text to file "DIRECTED.TXT".
  QFile f {m_config.writeable_data_dir ().absoluteFilePath ("DIRECTED.TXT")};
  if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream out(&f);

        QStringList output = {
            DriftingDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss"),
            Radio::frequency_MHz_string(m_freqNominal),
            QString::number(currentFreqOffset()),
            Varicode::formatSNR(snr),
            message
        };

        out << output.join("\t") << endl;

        f.close();
    } else {
        MessageBox::warning_message (this, tr ("File Open Error")
                                     , tr ("Cannot open \"%1\" for append: %2")
                                    .arg (f.fileName ()).arg (f.errorString ()));
    }
}

QDateTime MainWindow::nextTransmitCycle(){
    auto timestamp = DriftingDateTime::currentDateTimeUtc();

    // remove milliseconds
    auto t = timestamp.time();
    t.setHMS(t.hour(), t.minute(), t.second());
    timestamp.setTime(t);

    // round to 15 second increment
    int secondsSinceEpoch = (timestamp.toMSecsSinceEpoch()/1000);
    int delta = roundUp(secondsSinceEpoch, 15) + 1 - secondsSinceEpoch;
    timestamp = timestamp.addSecs(delta);

    return timestamp;
}

void MainWindow::resetAutomaticIntervalTransmissions(bool stopCQ, bool stopHB){
    resetCQTimer(stopCQ);
    resetHeartbeatTimer(stopHB);
}

void MainWindow::resetCQTimer(bool stop){
    if(ui->cqMacroButton->isChecked() && m_cqInterval > 0){
        ui->cqMacroButton->setChecked(false);
        if(!stop){
            ui->cqMacroButton->setChecked(true);
        }
    }
}

void MainWindow::resetHeartbeatTimer(bool stop){
    // toggle the heartbeat timer if we have a repeating heartbeat
    if(ui->hbMacroButton->isChecked() && m_hbInterval > 0){
        ui->hbMacroButton->setChecked(false);
        if(!stop){
            ui->hbMacroButton->setChecked(true);
        }
    }
}

void MainWindow::decodeDone ()
{
  dec_data.params.nagain=0;
  dec_data.params.ndiskdat=0;
  m_nclearave=0;
  QFile {m_config.temp_dir ().absoluteFilePath (".lock")}.open(QIODevice::ReadWrite);
  ui->DecodeButton->setChecked (false);
  decodeBusy(false);
  m_RxLog=0;
  m_blankLine=true;
}

void MainWindow::readFromStdout()                             //readFromStdout
{
  while(proc_js8.canReadLine()) {
    QByteArray t=proc_js8.readLine();
    qDebug() << "JS8: " << QString(t);
    bool bAvgMsg=false;
    int navg=0;
    if(t.indexOf("<DecodeFinished>") >= 0) {
      if(m_mode=="QRA64") m_wideGraph->drawRed(0,0);
      m_bDecoded = t.mid(20).trimmed().toInt() > 0;
      int mswait=3*1000*m_TRperiod/4;
      if(!m_diskData) killFileTimer.start(mswait); //Kill in 3/4 period
      decodeDone ();
      m_startAnother=m_loopall;
      if(m_bNoMoreFiles) {
        MessageBox::information_message(this, tr("No more files to open."));
        m_bNoMoreFiles=false;
      }
      return;
    } else {
      if(m_mode=="JT4" or m_mode=="JT65" or m_mode=="QRA64" or m_mode=="FT8") {
        int n=t.indexOf("f");
        if(n<0) n=t.indexOf("d");
        if(n>0) {
          QString tt=t.mid(n+1,1);
          navg=tt.toInt();
          if(navg==0) {
            char c = tt.data()->toLatin1();
            if(int(c)>=65 and int(c)<=90) navg=int(c)-54;
          }
          if(navg>1 or t.indexOf("f*")>0) bAvgMsg=true;
        }
      }

      QFile f {m_config.writeable_data_dir ().absoluteFilePath ("ALL.TXT")};
      if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream out(&f);
        if(m_RxLog==1) {
          out << DriftingDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss")
              << "  " << qSetRealNumberPrecision (12) << (m_freqNominal / 1.e6) << " MHz  "
              << "JS8" << endl;
          m_RxLog=0;
        }
        int n=t.length();
        auto logText = t.mid(0, n-2);
        auto dt = DecodedText(logText, false, m_config.my_grid(), m_nSubMode);
        out << logText << "  " << dt.message() << endl;
        f.close();
      } else {
        MessageBox::warning_message (this, tr ("File Open Error")
                                     , tr ("Cannot open \"%1\" for append: %2")
                                     .arg (f.fileName ()).arg (f.errorString ()));
      }

      DecodedText decodedtext {QString::fromUtf8 (t.constData ()).remove (QRegularExpression {"\r|\n"}), "FT8" == m_mode &&
            ui->cbVHFcontest->isChecked(), m_config.my_grid (), m_nSubMode};

      bool bValidFrame = decodedtext.snr() > -24;

      qDebug() << "valid" << bValidFrame << "decoded text" << decodedtext.message();

      ActivityDetail d = {};
      CallDetail cd = {};
      CommandDetail cmd = {};
      CallDetail td = {};


      //Left (Band activity) window
      if(bValidFrame) {
          // Parse General Activity
#if 1
          bool shouldParseGeneralActivity = true;
          if(shouldParseGeneralActivity && !decodedtext.messageWords().isEmpty()){
            int offset = decodedtext.frequencyOffset();

            if(!m_bandActivity.contains(offset)){
                QList<int> offsets = {
                    // offset - 60, offset - 61, offset - 62, offset - 63, offset - 64, offset - 65, offset - 66, offset - 67, offset - 68, offset - 69,
                    // offset + 60, offset + 61, offset + 62, offset + 63, offset + 64, offset + 65, offset + 66, offset + 67, offset + 68, offset + 69,
                    offset - 1, offset - 2, offset - 3, offset - 4, offset - 5, offset - 6, offset - 7, offset - 8, offset - 9, offset - 10,
                    offset + 1, offset + 2, offset + 3, offset + 4, offset + 5, offset + 6, offset + 7, offset + 8, offset + 9, offset + 10
                };
                foreach(int prevOffset, offsets){
                    if(!m_bandActivity.contains(prevOffset)){ continue; }
                    m_bandActivity[offset] = m_bandActivity[prevOffset];
                    m_bandActivity.remove(prevOffset);
                    break;
                }
            }

            //ActivityDetail d = {};
            d.isLowConfidence = decodedtext.isLowConfidence();
            d.isFree = !decodedtext.isStandardMessage();
            d.isCompound = decodedtext.isCompound();
            d.isDirected = decodedtext.isDirectedMessage();
            d.bits = decodedtext.bits();
            d.freq = offset;
            d.text = decodedtext.message();
            d.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
            d.snr = decodedtext.snr();
            d.isBuffered = false;
            d.tdrift = decodedtext.dt();
            d.mode = currentMode();

            // if we have any "first" frame, and a buffer is already established, clear it...
            int prevBufferOffset = -1;
            if(((d.bits & Varicode::JS8CallFirst) == Varicode::JS8CallFirst) && hasExistingMessageBuffer(d.freq, true, &prevBufferOffset)){
                qDebug() << "first message encountered, clearing existing buffer" << prevBufferOffset;
                m_messageBuffer.remove(d.freq);
            }

            // if we have a data frame, and a message buffer has been established, buffer it...
            if(hasExistingMessageBuffer(d.freq, true, &prevBufferOffset) && !decodedtext.isCompound() && !decodedtext.isDirectedMessage()){
                qDebug() << "buffering data" << d.freq << d.text;
                d.isBuffered = true;
                m_messageBuffer[d.freq].msgs.append(d);
                // TODO: incremental display if it's "to" me.
            }

            m_rxActivityQueue.append(d);
            m_bandActivity[offset].append(d);
            while(m_bandActivity[offset].count() > 10){
                m_bandActivity[offset].removeFirst();
            }
          }
#endif

          // Process compound callsign commands (put them in cache)"
#if 1
          qDebug() << "decoded" << decodedtext.frameType() << decodedtext.isCompound() << decodedtext.isDirectedMessage() << decodedtext.isHeartbeat();
          bool shouldProcessCompound = true;
          if(shouldProcessCompound && decodedtext.isCompound() && !decodedtext.isDirectedMessage()){
            cd.call = decodedtext.compoundCall();
            cd.grid = decodedtext.extra(); // compound calls via pings may contain grid...
            cd.snr = decodedtext.snr();
            cd.freq = decodedtext.frequencyOffset();
            cd.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
            cd.bits = decodedtext.bits();
            cd.tdrift = decodedtext.dt();
            cd.mode = currentMode();

            // Only respond to HEARTBEATS...remember that CQ messages are "Alt" pings
            if(decodedtext.isHeartbeat()){
                if(decodedtext.isAlt()){
                    // this is a cq with a standard or compound call, ala "KN4CRD/P: CQCQCQ"
                    cd.cqTimestamp = DriftingDateTime::currentDateTimeUtc();

                    // it is not processed elsewhere, so we need to just log it here.
                    logCallActivity(cd, true);

                    // TODO: jsherer - notification for cq?

                } else {
                    // convert HEARTBEAT to a directed command and process...
                    cmd.from = cd.call;
                    cmd.to = "@ALLCALL";
                    cmd.cmd = " HB";
                    cmd.snr = cd.snr;
                    cmd.bits = cd.bits;
                    cmd.grid = cd.grid;
                    cmd.freq = cd.freq;
                    cmd.utcTimestamp = cd.utcTimestamp;
                    cmd.tdrift = cd.tdrift;
                    cmd.mode = cd.mode;
                    m_rxCommandQueue.append(cmd);
                }

            } else {
                qDebug() << "buffering compound call" << cd.freq << cd.call << cd.bits;

                hasExistingMessageBuffer(cd.freq, true, nullptr);
                m_messageBuffer[cd.freq].compound.append(cd);
            }
          }
#endif

          // Parse commands
          // KN4CRD K1JT ?
#if 1
          bool shouldProcessDirected = true;
          if(shouldProcessDirected && decodedtext.isDirectedMessage()){
              auto parts = decodedtext.directedMessage();

              cmd.from = parts.at(0);
              cmd.to = parts.at(1);
              cmd.cmd = parts.at(2);
              cmd.freq = decodedtext.frequencyOffset();
              cmd.snr = decodedtext.snr();
              cmd.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
              cmd.bits = decodedtext.bits();
              cmd.extra = parts.length() > 2 ? parts.mid(3).join(" ") : "";
              cmd.tdrift = decodedtext.dt();
              cmd.mode = currentMode();

              // if the command is a buffered command and its not the last frame OR we have from or to in a separate message (compound call)
              if((Varicode::isCommandBuffered(cmd.cmd) && (cmd.bits & Varicode::JS8CallLast) != Varicode::JS8CallLast) || cmd.from == "<....>" || cmd.to == "<....>"){
                qDebug() << "buffering cmd" << cmd.freq << cmd.cmd << cmd.from << cmd.to;

                // log complete buffered callsigns immediately
                if(cmd.from != "<....>" && cmd.to != "<....>"){
                    CallDetail cmdcd = {};
                    cmdcd.call = cmd.from;
                    cmdcd.bits = cmd.bits;
                    cmdcd.snr = cmd.snr;
                    cmdcd.freq = cmd.freq;
                    cmdcd.utcTimestamp = cmd.utcTimestamp;
                    cmdcd.ackTimestamp = cmd.to == m_config.my_callsign() ? cmd.utcTimestamp : QDateTime{};
                    cmdcd.tdrift = cmd.tdrift;
                    cmdcd.mode = currentMode();
                    logCallActivity(cmdcd, false);
                    logHeardGraph(cmd.from, cmd.to);
                }

                hasExistingMessageBuffer(cmd.freq, true, nullptr);

                if(cmd.to == m_config.my_callsign()){
                    d.shouldDisplay = true;
                }

                m_messageBuffer[cmd.freq].cmd = cmd;
                m_messageBuffer[cmd.freq].msgs.clear();
              } else {
                m_rxCommandQueue.append(cmd);
              }

              // check to see if this is a station we've heard 3rd party
              bool shouldCaptureThirdPartyCallsigns = false;
              if(shouldCaptureThirdPartyCallsigns && Radio::base_callsign(cmd.to) != Radio::base_callsign(m_config.my_callsign())){
                  QString relayCall = QString("%1|%2").arg(Radio::base_callsign(cmd.from)).arg(Radio::base_callsign(cmd.to));
                  int snr = -100;
                  if(parts.length() == 4){
                      snr = QString(parts.at(3)).toInt();
                  }

                  //CallDetail td = {};
                  td.through = cmd.from;
                  td.call = cmd.to;
                  td.grid = "";
                  td.snr = snr;
                  td.freq = cmd.freq;
                  td.utcTimestamp = cmd.utcTimestamp;
                  td.tdrift = cmd.tdrift;
                  td.mode = currentMode();
                  logCallActivity(td, true);
                  logHeardGraph(cmd.from, cmd.to);
              }
          }
#endif




          // Parse CQs
#if 0
          bool shouldParseCQs = true;
          if(shouldParseCQs && decodedtext.isStandardMessage()){
            QString theircall;
            QString theirgrid;
            decodedtext.deCallAndGrid(theircall, theirgrid);

            QStringList calls = Varicode::parseCallsigns(theircall);
            if(!calls.isEmpty() && !calls.first().isEmpty()){
                theircall = calls.first();

                CallDetail d = {};
                d.bits = decodedtext.bits();
                d.call = theircall;
                d.grid = theirgrid;
                d.snr = decodedtext.snr();
                d.freq = decodedtext.frequencyOffset();
                d.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
                m_callActivity[d.call] = d;
              }
          }
#endif

          // Parse standard message callsigns
          // K1JT KN4CRD EM73
          // KN4CRD K1JT -21
          // K1JT KN4CRD R-12
          // DE KN4CRD
          // KN4CRD
#if 0
          bool shouldParseCallsigns = false;
          if(shouldParseCallsigns){
              QStringList callsigns = Varicode::parseCallsigns(decodedtext.message());
              if(!callsigns.isEmpty()){
                  // one callsign
                  // de [from]
                  // cq [from]

                  // two callsigns
                  // [from]: [to] ...
                  // [to] [from] [grid|signal]

                  QStringList grids = Varicode::parseGrids(decodedtext.message());

                  // one callsigns are handled above... so we only need to handle two callsigns if it's a standard message
                  if(decodedtext.isStandardMessage()){
                      if(callsigns.length() == 2){
                          auto de_callsign = callsigns.last();

                          // TODO: jsherer - put this in a function to record a callsign...
                          CallDetail d;
                          d.call = de_callsign;
                          d.grid = !grids.empty() ? grids.first() : "";
                          d.snr = decodedtext.snr();
                          d.freq = decodedtext.frequencyOffset();
                          d.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
                          m_callActivity[Radio::base_callsign(de_callsign)] = d;
                      }
                  }
              }
          }
#endif
      }

#if 0
      //Right (Rx Frequency) window
      bool bDisplayRight=bAvgMsg;
      int audioFreq=decodedtext.frequencyOffset();

      if(abs(audioFreq - m_wideGraph->rxFreq()) <= 10){
          bDisplayRight=true;
      }

      if (bDisplayRight) {
        // This msg is within 10 hertz of our tuned frequency, or a JT4 or JT65 avg,
        // or contains MyCall
        ui->decodedTextBrowser2->displayDecodedText(decodedtext,m_baseCall,false,
               m_logBook,m_config.color_CQ(),m_config.color_MyCall(),
               m_config.color_DXCC(),m_config.color_NewCall(),m_config.ppfx());

        if(m_mode!="JT4") {
          bool b65=decodedtext.isJT65();
          if(b65 and m_modeTx!="JT65") on_pbTxMode_clicked();
          if(!b65 and m_modeTx=="JT65") on_pbTxMode_clicked();
        }
        m_QSOText = decodedtext.string ().trimmed ();
      }

      if(m_mode!="FT8" or !m_config.bHound()) {
        postDecode (true, decodedtext.string ());

        // find and extract any report for myCall, but save in m_rptRcvd only if it's from DXcall
        QString rpt;
        bool stdMsg = decodedtext.report(m_baseCall,
            Radio::base_callsign(ui->dxCallEntry->text()), rpt);
        QString deCall;
        QString grid;
        decodedtext.deCallAndGrid(/*out*/deCall,grid);
        {
          QString t=Radio::base_callsign(ui->dxCallEntry->text());
          if((t==deCall or t=="") and rpt!="") m_rptRcvd=rpt;
        }
        // extract details and send to PSKreporter
        int nsec=DriftingDateTime::currentMSecsSinceEpoch()/1000-m_secBandChanged;
        bool okToPost=(nsec>(4*m_TRperiod)/5);

        //if (stdMsg && okToPost) pskPost(decodedtext);

        if((m_mode=="JT4" or m_mode=="JT65" or m_mode=="QRA64") and m_msgAvgWidget!=NULL) {
          if(m_msgAvgWidget->isVisible()) {
            QFile f(m_config.temp_dir ().absoluteFilePath ("avemsg.txt"));
            if(f.open(QIODevice::ReadOnly | QIODevice::Text)) {
              QTextStream s(&f);
              QString t=s.readAll();
              m_msgAvgWidget->displayAvg(t);
            }
          }
        }
      }
#endif

    }
  }


  // See MainWindow::postDecode for displaying the latest decodes
}

void MainWindow::playSoundFile(const QString &path){
    if(path.isEmpty()){
        return;
    }

    qDebug() << "Trying to play sound file" << path;

    QSound::play(path);
}

bool MainWindow::hasExistingMessageBufferToMe(int *pOffset){
    foreach(auto offset, m_messageBuffer.keys()){
        auto buffer = m_messageBuffer[offset];

        // if this is a valid buffer and it's to me...
        if(buffer.cmd.utcTimestamp.isValid() && (buffer.cmd.to == m_config.my_callsign() || buffer.cmd.to == Radio::base_callsign(m_config.my_callsign()))){
            if(pOffset) *pOffset = offset;
            return true;
        }
    }

    return false;
}

bool MainWindow::hasExistingMessageBuffer(int offset, bool drift, int *pPrevOffset){
    if(m_messageBuffer.contains(offset)){
        if(pPrevOffset) *pPrevOffset = offset;
        return true;
    }

    QList<int> offsets = {
        //offset - 60, offset - 61, offset - 62, offset - 63, offset - 64, offset - 65, offset - 66, offset - 67, offset - 68, offset - 69,
        //offset + 60, offset + 61, offset + 62, offset + 63, offset + 64, offset + 65, offset + 66, offset + 67, offset + 68, offset + 69,
        offset - 1, offset - 2, offset - 3, offset - 4, offset - 5, offset - 6, offset - 7, offset - 8, offset - 9, offset - 10,
        offset + 1, offset + 2, offset + 3, offset + 4, offset + 5, offset + 6, offset + 7, offset + 8, offset + 9, offset + 10
    };
    foreach(int prevOffset, offsets){
        if(!m_messageBuffer.contains(prevOffset)){ continue; }

        if(drift){
            m_messageBuffer[offset] = m_messageBuffer[prevOffset];
            m_messageBuffer.remove(prevOffset);
        }

        if(pPrevOffset) *pPrevOffset = prevOffset;
        return true;
    }

    return false;
}

void MainWindow::logCallActivity(CallDetail d, bool spot){
    // don't log empty calls
    if(d.call.trimmed().isEmpty()){
        return;
    }

    // don't log relay calls
    if(d.call.contains(">")){
        return;
    }

    if(m_callActivity.contains(d.call)){
        // update (keep grid)
        CallDetail old = m_callActivity[d.call];
        if(d.grid.isEmpty() && !old.grid.isEmpty()){
            d.grid = old.grid;
        }
        if(!d.ackTimestamp.isValid() && old.ackTimestamp.isValid()){
            d.ackTimestamp = old.ackTimestamp;
        }
        if(!d.cqTimestamp.isValid() && old.cqTimestamp.isValid()){
            d.cqTimestamp = old.cqTimestamp;
        }
        m_callActivity[d.call] = d;
    } else {
        // create
        m_callActivity[d.call] = d;
    }

    // enqueue for spotting to psk reporter
    if(spot){
        m_rxCallQueue.append(d);
    }
}

void MainWindow::logHeardGraph(QString from, QString to){
    auto my_callsign = m_config.my_callsign();

    // hearing
    if(m_heardGraphOutgoing.contains(my_callsign)){
        m_heardGraphOutgoing[my_callsign].insert(from);
    } else {
        m_heardGraphOutgoing[my_callsign].insert(from);
    }

    // heard by
    if(m_heardGraphIncoming.contains(from)){
        m_heardGraphIncoming[from].insert(my_callsign);
    } else {
        m_heardGraphIncoming[from] = { my_callsign };
    }

    if(to == "@ALLCALL"){
        return;
    }

    // hearing
    if(m_heardGraphOutgoing.contains(from)){
        m_heardGraphOutgoing[from].insert(to);
    } else {
        m_heardGraphOutgoing[from] = { to };
    }

    // heard by
    if(m_heardGraphIncoming.contains(to)){
        m_heardGraphIncoming[to].insert(from);
    } else {
        m_heardGraphIncoming[to] = { from };
    }
}

QString MainWindow::lookupCallInCompoundCache(QString const &call){
    QString myBaseCall = Radio::base_callsign(m_config.my_callsign());
    if(call == myBaseCall){
        return m_config.my_callsign();
    }
    return m_compoundCallCache.value(call, call);
}

void MainWindow::spotReport(int offset, int snr, QString callsign, QString grid){
    if(!m_config.spot_to_reporting_networks()) return;

    Frequency frequency = m_freqNominal + offset;

    m_spotClient->enqueueSpot(callsign, grid, frequency, snr);
}

void MainWindow::spotCmd(CommandDetail cmd){
    if(!m_config.spot_to_reporting_networks()) return;

    QString cmdStr = cmd.cmd;
    if(!cmdStr.trimmed().isEmpty()){
        cmdStr = Varicode::lstrip(cmd.cmd);
    }

    m_spotClient->enqueueCmd(cmdStr, cmd.from, cmd.to, cmd.relayPath, cmd.text, cmd.grid, cmd.extra, m_freqNominal + cmd.freq, cmd.snr);
}

void MainWindow::spotAPRSMsg(CommandDetail d){
    if(!m_config.spot_to_reporting_networks()) return;
    if(!m_aprsClient->isPasscodeValid()) return;

    // ONLY MSG IS APPROPRIATE HERE, e.g.,
    // KN4CRD: @APRSIS MSG :EMAIL-2  :email@domain.com booya{
    if(d.cmd != " MSG") return;

    qDebug() << "enqueueing third party text" << d.from << d.text;
    m_aprsClient->enqueueThirdParty(Radio::base_callsign(d.from), d.text);
}

void MainWindow::pskLogReport(QString mode, int offset, int snr, QString callsign, QString grid){
    if(!m_config.spot_to_reporting_networks()) return;

    Frequency frequency = m_freqNominal + offset;

    psk_Reporter->addRemoteStation(
       callsign,
       grid,
       QString::number(frequency),
       mode,
       QString::number(snr),
       QString::number(DriftingDateTime::currentDateTime().toTime_t()));
}

void MainWindow::aprsLogReport(int offset, int snr, QString callsign, QString grid){
    if(!m_config.spot_to_reporting_networks()) return;

    Frequency frequency = m_freqNominal + offset;

    if(grid.length() < 6){
        qDebug() << "APRSISClient Spot Skipped:" << callsign << grid;
        return;
    }

    auto comment = QString("%1MHz %2dB").arg(Radio::frequency_MHz_string(frequency)).arg(Varicode::formatSNR(snr));
    if(callsign.contains("/")){
        comment = QString("%1 %2").arg(callsign).arg(comment);
    }

    auto base = Radio::base_callsign(callsign);
    callsign = APRSISClient::replaceCallsignSuffixWithSSID(callsign, base);

    if(m_aprsCallCache.contains(callsign)){
        qDebug() << "APRSISClient Spot Skipped For Cache:" << callsign << grid;
        return;
    }

    m_aprsClient->enqueueSpot(callsign, grid, comment);
    m_aprsCallCache.insert(callsign, DriftingDateTime::currentDateTimeUtc());
}

void MainWindow::killFile ()
{
  if (m_fnameWE.size () &&
      !(m_saveAll || (m_saveDecoded && m_bDecoded) || m_fnameWE == m_fileToSave)) {
    QFile f1 {m_fnameWE + ".wav"};
    if(f1.exists()) f1.remove();
    if(m_mode.startsWith ("WSPR")) {
      QFile f2 {m_fnameWE + ".c2"};
      if(f2.exists()) f2.remove();
    }
  }
}

void MainWindow::on_EraseButton_clicked ()
{
  qint64 ms=DriftingDateTime::currentMSecsSinceEpoch();
  ui->decodedTextBrowser2->erase ();
  if(m_mode.startsWith ("WSPR") or m_mode=="Echo" or m_mode=="ISCAT") {
    ui->decodedTextBrowser->erase ();
  } else {
    if((ms-m_msErase)<500) {
      ui->decodedTextBrowser->erase ();
    }
  }
  m_msErase=ms;
}

void MainWindow::decodeBusy(bool b)                             //decodeBusy()
{
  if (!b) m_optimizingProgress.reset ();
  m_decoderBusy=b;
  ui->DecodeButton->setEnabled(!b);
  ui->actionOpen->setEnabled(!b);
  ui->actionOpen_next_in_directory->setEnabled(!b);
  ui->actionDecode_remaining_files_in_directory->setEnabled(!b);

  statusUpdate ();
}

//------------------------------------------------------------- //guiUpdate()
void MainWindow::guiUpdate()
{
  static quint64 lastLoop;
  static char message[29];
  static char msgsent[29];
  static int msgibits;
  double txDuration;

  QString rt;

  quint64 thisLoop = QDateTime::currentMSecsSinceEpoch();
  if(lastLoop == 0){
      lastLoop = thisLoop;
  }
  quint64 delta = thisLoop - lastLoop;
  if(delta > (100 + 10)){
    qDebug() << "guiupdate overrun" << (delta-100);
  }
  lastLoop = thisLoop;

  if(m_TRperiod==0) m_TRperiod=60;
  txDuration=0.0;
  if(m_modeTx=="FT8"){
      if(m_nSubMode == Varicode::JS8CallNormal){
        txDuration=JS8A_START_DELAY_MS/1000.0 + JS8_NUM_SYMBOLS * (double)JS8A_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE;
      } else if(m_nSubMode == Varicode::JS8CallFast){
        txDuration=JS8B_START_DELAY_MS/1000.0 + JS8_NUM_SYMBOLS * (double)JS8B_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE;
      } else if(m_nSubMode == Varicode::JS8CallTurbo){
        txDuration=JS8C_START_DELAY_MS/1000.0 + JS8_NUM_SYMBOLS * (double)JS8C_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE;
      } else if(m_nSubMode == Varicode::JS8CallUltra){
        txDuration=JS8D_START_DELAY_MS/1000.0 + JS8_NUM_SYMBOLS * (double)JS8D_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE;
      }
  }

  double tx1=0.0;
  double tx2=txDuration;

  if(m_mode=="FT8") icw[0]=0;                                   //No CW ID in FT8 mode

  if((icw[0]>0) and (!m_bFast9)) tx2 += icw[0]*2560.0/48000.0;  //Full length including CW ID
  if(tx2>m_TRperiod) tx2=m_TRperiod;

  qint64 ms = DriftingDateTime::currentMSecsSinceEpoch() % 86400000;
  int nsec=ms/1000;
  double tsec=0.001*ms;
  double t2p=fmod(tsec, m_TRperiod);
  m_s6=fmod(tsec,6.0);
  m_nseq = nsec % m_TRperiod;
  m_tRemaining=m_TRperiod - fmod(tsec,double(m_TRperiod));

  // how long is the tx?
  m_bTxTime = (t2p >= tx1) and (t2p < tx2);

  if(m_tune) m_bTxTime=true;                 // "Tune" and tones take precedence

  if(m_transmitting or m_auto or m_tune) {
    m_dateTimeLastTX = DriftingDateTime::currentDateTime ();

// Don't transmit another mode in the 30 m WSPR sub-band
    Frequency onAirFreq = m_freqNominal + ui->TxFreqSpinBox->value();

    //qDebug() << "transmitting on" << onAirFreq;

    if ((onAirFreq > 10139900 and onAirFreq < 10140320) and
        !m_mode.startsWith ("WSPR")) {
      m_bTxTime=false;
      if (m_auto) auto_tx_mode (false);
      if(onAirFreq!=m_onAirFreq0) {
        m_onAirFreq0=onAirFreq;
        auto const& message = tr ("Please choose another Tx frequency."
                                  " The app will not knowingly transmit another"
                                  " mode in the WSPR sub-band on 30m.");
#if QT_VERSION >= 0x050400
        QTimer::singleShot (0, [=] { // don't block guiUpdate
            MessageBox::warning_message (this, tr ("WSPR Guard Band"), message);
          });
#else
        MessageBox::warning_message (this, tr ("WSPR Guard Band"), message);
#endif
      }
    }

    // watchdog!
    // if (m_config.watchdog() && !m_mode.startsWith ("WSPR")
    //     && m_idleMinutes >= m_config.watchdog ()) {
    //   tx_watchdog (true);       // disable transmit
    // }

    float fTR=float((ms%(1000*m_TRperiod)))/(1000*m_TRperiod);

    QString txMsg;
    if(m_ntx == 1) txMsg=ui->tx1->text();
    if(m_ntx == 2) txMsg=ui->tx2->text();
    if(m_ntx == 3) txMsg=ui->tx3->text();
    if(m_ntx == 4) txMsg=ui->tx4->text();
    if(m_ntx == 5) txMsg=ui->tx5->currentText();
    if(m_ntx == 6) txMsg=ui->tx6->text();
    if(m_ntx == 7) txMsg=ui->genMsg->text();
    if(m_ntx == 8) txMsg=ui->freeTextMsg->currentText();
    if(m_ntx == 9) txMsg=ui->nextFreeTextMsg->text();
    int msgLength=txMsg.trimmed().length();

    // TODO: stop
    if(msgLength==0 and !m_tune) on_stopTxButton_clicked();

    // 15.0 - 12.6
    double ratio = 1.0;
    if(m_nSubMode == Varicode::JS8CallNormal){
        ratio = (((double)m_TRperiod - (JS8_NUM_SYMBOLS*(double)JS8A_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE))/(double)m_TRperiod);
    }
    else if(m_nSubMode == Varicode::JS8CallFast){
        ratio = (((double)m_TRperiod - (JS8_NUM_SYMBOLS*(double)JS8B_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE))/(double)m_TRperiod);
    }
    else if(m_nSubMode == Varicode::JS8CallTurbo){
        ratio = (((double)m_TRperiod - (JS8_NUM_SYMBOLS*(double)JS8C_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE))/(double)m_TRperiod);
    }
    else if(m_nSubMode == Varicode::JS8CallUltra){
        ratio = (((double)m_TRperiod - (JS8_NUM_SYMBOLS*(double)JS8D_SYMBOL_SAMPLES/(double)RX_SAMPLE_RATE))/(double)m_TRperiod);
    }

    if(fTR > 1.0-ratio && fTR < 1.0){
        if(!m_deadAirTone){
            qDebug() << "should start dead air tone";
            m_deadAirTone = true;
        }
    } else {
        if(m_deadAirTone){
            qDebug() << "should stop dead air tone";
            m_deadAirTone = false;
        }
    }

    // the late threshold is the dead air time minus the tx delay time
    float lateThreshold = ratio - (m_config.txDelay() / m_TRperiod);
    if(m_nSubMode == Varicode::JS8CallFast){
        // for the faster mode, only allow 3/4 late threshold
        lateThreshold *= 0.75;
    }
    else if(m_nSubMode == Varicode::JS8CallTurbo){
        // for the turbo mode, only allow 1/2 late threshold
        lateThreshold *= 0.5;
    }
    if(g_iptt==0 and ((m_bTxTime and fTR<lateThreshold and msgLength>0) or m_tune)) {
      //### Allow late starts
      icw[0]=m_ncw;
      g_iptt = 1;
      setRig ();
      setXIT (ui->TxFreqSpinBox->value ());
      emitPTT(true);
      m_tx_when_ready = true;

      qDebug() << "start threshold" << fTR << lateThreshold << ms;
    }

    // TODO: stop
    if(!m_bTxTime and !m_tune) m_btxok=false;       //Time to stop transmitting
  }

  // Calculate Tx tones when needed
  if((g_iptt==1 && m_iptt0==0) || m_restart) {
//----------------------------------------------------------------------
    QByteArray ba;
    QByteArray ba0;

    if(m_ntx == 1) ba=ui->tx1->text().toLocal8Bit();
    if(m_ntx == 2) ba=ui->tx2->text().toLocal8Bit();
    if(m_ntx == 3) ba=ui->tx3->text().toLocal8Bit();
    if(m_ntx == 4) ba=ui->tx4->text().toLocal8Bit();
    if(m_ntx == 5) ba=ui->tx5->currentText().toLocal8Bit();
    if(m_ntx == 6) ba=ui->tx6->text().toLocal8Bit();
    if(m_ntx == 7) ba=ui->genMsg->text().toLocal8Bit();
    if(m_ntx == 8) ba=ui->freeTextMsg->currentText().toLocal8Bit();
    if(m_ntx == 9) ba=ui->nextFreeTextMsg->text().toLocal8Bit();

    ba2msg(ba,message);
    int ichk=0;

    if (m_lastMessageSent != m_currentMessage
        || m_lastMessageType != m_currentMessageType)
      {
        m_lastMessageSent = m_currentMessage;
        m_lastMessageType = m_currentMessageType;
      }

    m_currentMessageType = 0;

    if(m_tune) {
      itone[0]=0;
    } else if(m_modeTx=="FT8") {
      bool bcontest=false;
      char MyCall[6];
      char MyGrid[6];
      strncpy(MyCall, (m_config.my_callsign()+"      ").toLatin1(),6);
      strncpy(MyGrid, (m_config.my_grid()+"      ").toLatin1(),6);

      // 0:   [000] <- this is standard set
      // 1:   [001] <- this is fox/hound
      //m_i3bit=0;
      char ft8msgbits[75 + 12]; //packed 75 bit ft8 message plus 12-bit CRC

      if(m_nSubMode == Varicode::JS8CallNormal){
          qDebug() << "gen ft8";
          genft8_(message, MyGrid, &bcontest, &m_i3bit, msgsent, const_cast<char *> (ft8msgbits),
                  const_cast<int *> (itone), 22, 6, 22);
      } else if (m_nSubMode == Varicode::JS8CallFast || m_nSubMode == Varicode::JS8CallTurbo || m_nSubMode == Varicode::JS8CallUltra){
          qDebug() << "gen js8";
          genjs8_(message, MyGrid, &bcontest, &m_i3bit, msgsent, const_cast<char *> (ft8msgbits),
                  const_cast<int *> (itone), 22, 6, 22);
      }

      qDebug() << "-> msg:" << message;
      qDebug() << "-> bit:" << m_i3bit;

      msgibits = m_i3bit;
      msgsent[22]=0;

      m_currentMessage = QString::fromLatin1(msgsent).trimmed();
      m_currentMessageBits = msgibits;

      emitTones();

#if TEST_FOX_WAVE_GEN
      if(ui->turboButton->isChecked()) {

        foxcom_.nslots=1;

        foxcom_.nfreq=ui->TxFreqSpinBox->value();
        if(m_config.split_mode()) foxcom_.nfreq = foxcom_.nfreq - m_XIT;  //Fox Tx freq
        strncpy(&foxcom_.cmsg[0][0], QString::fromStdString(message).toLatin1(), 12);
        foxcom_.i3bit[0] = m_i3bit | Varicode::JS8CallFlag;
        int i = 1;
        while(!m_txFrameQueue.isEmpty() && foxcom_.nslots < TEST_FOX_WAVE_GEN_SLOTS){
            auto pair = m_txFrameQueue.dequeue();
            strncpy(&foxcom_.cmsg[i][0], pair.first.toLatin1(), 12);
            foxcom_.i3bit[i] = pair.second | Varicode::JS8CallFlag;
            foxcom_.nslots += 1;

            //m_currentMessage.append(pair.first);
            //m_currentMessageBits |= pair.second;

            i += 1;
        }

        if(i > 1){
            updateTxButtonDisplay();
        }

        foxgen_();
      }
#endif
    }

    m_bCallingCQ = CALLING == m_QSOProgress
      || m_currentMessage.contains (QRegularExpression {"^(CQ|QRZ) "});
    if(m_mode=="FT8") {
      if(m_bCallingCQ && ui->cbFirst->isVisible () && ui->cbFirst->isChecked ()) {
        ui->cbFirst->setStyleSheet("QCheckBox{color:red}");
      } else {
        ui->cbFirst->setStyleSheet("");
      }
    }

    if (m_tune) {
      m_currentMessage = "TUNE";
      m_currentMessageType = -1;
    }
    if(m_restart) {
      write_transmit_entry ("ALL.TXT");
      if (m_config.TX_messages ()) {
        ui->decodedTextBrowser2->displayTransmittedText(m_currentMessage,m_modeTx,
                     ui->TxFreqSpinBox->value(),m_config.color_rx_background(),m_bFastMode);
        }
    }

    auto t2 = DriftingDateTime::currentDateTimeUtc ().toString ("hhmm");
    icw[0] = 0;
    auto msg_parts = m_currentMessage.split (' ', QString::SkipEmptyParts);
    if (msg_parts.size () > 2) {
      // clean up short code forms
      msg_parts[0].remove (QChar {'<'});
      msg_parts[1].remove (QChar {'>'});
    }
    auto is_73 = m_QSOProgress >= ROGER_REPORT
      && message_is_73 (m_currentMessageType, msg_parts);
    m_sentFirst73 = is_73
      && !message_is_73 (m_lastMessageType, m_lastMessageSent.split (' ', QString::SkipEmptyParts));
    if (m_sentFirst73) {
      m_qsoStop=t2;
      if(m_config.id_after_73 ()) {
        icw[0] = m_ncw;
      }
      if (m_config.prompt_to_log () && !m_tune) {
        logQSOTimer.start (0);
      }
    }

    bool b=(m_mode=="FT8") and ui->cbAutoSeq->isChecked() and ui->cbFirst->isChecked();
    if(is_73 and (m_config.disable_TX_on_73() or b)) {
      auto_tx_mode (false);
      if(b) {
        m_ntx=6;
        ui->txrb6->setChecked(true);
        m_QSOProgress = CALLING;
      }
    }

    if(m_config.id_interval () >0) {
      int nmin=(m_sec0-m_secID)/60;
      if(m_sec0<m_secID) nmin=m_config.id_interval();
      if(nmin >= m_config.id_interval()) {
        icw[0]=m_ncw;
        m_secID=m_sec0;
      }
    }

    if ((m_currentMessageType < 6 || 7 == m_currentMessageType)
        && msg_parts.length() >= 3
        && (msg_parts[1] == m_config.my_callsign () ||
            msg_parts[1] == m_baseCall))
    {
      int i1;
      bool ok;
      i1 = msg_parts[2].toInt(&ok);
      if(ok and i1>=-50 and i1<50)
      {
        m_rptSent = msg_parts[2];
        m_qsoStart = t2;
      } else {
        if (msg_parts[2].mid (0, 1) == "R")
        {
          i1 = msg_parts[2].mid (1).toInt (&ok);
          if (ok and i1 >= -50 and i1 < 50)
          {
            m_rptSent = msg_parts[2].mid (1);
            m_qsoStart = t2;
          }
        }
      }
    }
    m_restart=false;
//----------------------------------------------------------------------
  }

  if (g_iptt == 1 && m_iptt0 == 0)
    {
      auto const& current_message = QString::fromLatin1 (msgsent);
      if(m_config.watchdog () && !m_mode.startsWith ("WSPR")
         && current_message != m_msgSent0) {
        // new messages don't reset the idle timer :|
        // tx_watchdog (false);  // in case we are auto sequencing
        m_msgSent0 = current_message;
      }

      if(!m_tune) {
        write_transmit_entry ("ALL.TXT");
      }

      // TODO: jsherer - perhaps an on_transmitting signal?
      m_lastTxTime = DriftingDateTime::currentDateTimeUtc();

      m_transmitting = true;
      transmitDisplay (true);
      statusUpdate ();
    }

  // TODO: stop
  if(!m_btxok && m_btxok0 && g_iptt==1) stopTx();

  if(m_startAnother) {
    if(m_mode=="MSK144") {
      m_wait++;
    }
    if(m_mode!="MSK144" or m_wait>=4) {
      m_wait=0;
      m_startAnother=false;
      on_actionOpen_next_in_directory_triggered();
    }
  }

  //Once per second:
  if(nsec != m_sec0) {
    if(m_freqNominal!=0 and m_freqNominal<50000000 and m_config.enable_VHF_features()) {
      if(!m_bVHFwarned) vhfWarning();
    } else {
      m_bVHFwarned=false;
    }

    if(m_monitoring or m_transmitting) {
        progressBar.setMaximum(m_TRperiod);
        int isec=int(fmod(tsec,m_TRperiod));
        progressBar.setValue(isec);
    } else {
        progressBar.setValue(0);
    }

    astroUpdate ();

    if(m_transmitting) {
      char s[41];
      auto dt = DecodedText(msgsent, msgibits, m_nSubMode);
      sprintf(s,"Tx: %s", dt.message().toLocal8Bit().mid(0, 41).data());
      m_nsendingsh=0;
      if(s[4]==64) m_nsendingsh=1;
      if(m_nsendingsh==1 or m_currentMessageType==7) {
        tx_status_label.setStyleSheet("QLabel{background-color: #ff2222; color:#000; }");
      } else if(m_nsendingsh==-1 or m_currentMessageType==6) {
        tx_status_label.setStyleSheet("QLabel{background-color: #ff2222; color:#000; }");
      } else {
        tx_status_label.setStyleSheet("QLabel{background-color: #ff2222; color:#000; }");
      }
      if(m_tune) {
        tx_status_label.setText("Tx: TUNE");
      } else {
        if(m_mode=="Echo") {
          tx_status_label.setText("Tx: ECHO");
        } else {
          s[40]=0;
          QString t{QString::fromLatin1(s)};
          tx_status_label.setText(t.trimmed());
        }
      }
      transmitDisplay(true);

    } else if(m_monitoring) {
      if (m_tx_watchdog) {
        tx_status_label.setStyleSheet ("QLabel{background-color: #000000; color:#ffffff}");
        tx_status_label.setText ("Idle timeout");
      } else {
        tx_status_label.setStyleSheet("QLabel{background-color: #22ff22}");
        QString t;
        t="Receiving";
        if(m_mode=="MSK144") {
          int npct=int(100.0*m_fCPUmskrtd/0.298667);
          if(npct>90) tx_status_label.setStyleSheet("QLabel{background-color: #ff2222; color:#000; }");
          t.sprintf("Receiving   %2d%%",npct);
        }
        tx_status_label.setText (t);
      }
      transmitDisplay(false);
    } else if (!m_diskData && !m_tx_watchdog) {
      tx_status_label.setStyleSheet("");
      tx_status_label.setText("");
    }

    auto drift = DriftingDateTime::drift();
    QDateTime t = DriftingDateTime::currentDateTimeUtc();
    QStringList parts;
    parts << (t.time().toString() + (!drift ? " " : QString(" (%1%2ms)").arg(drift > 0 ? "+" : "").arg(drift)));
    parts << t.date().toString("yyyy MMM dd");
    ui->labUTC->setText(parts.join("\n"));

#if 0
    auto delta = t.secsTo(m_nextHeartbeat);
    QString ping;
    if(heartbeatTimer.isActive()){
        if(delta > 0){
            ping = QString("%1 s").arg(delta);
        } else {
            ping = "queued!";
        }
    } else if (m_nextHeartPaused) {
        ping = "paused";
    } else {
        ping = "on demand";
    }
    ui->labHeartbeat->setText(QString("Next Heartbeat: %1").arg(ping));
#endif

    auto callLabel = m_config.my_callsign();
    if(m_config.use_dynamic_grid() && !m_config.my_grid().isEmpty()){
        callLabel = QString("%1 - %2").arg(callLabel).arg(m_config.my_grid());
    }
    ui->labCallsign->setText(callLabel);

    if(!m_monitoring and !m_diskData) {
      ui->signal_meter_widget->setValue(0,0);
    }

    m_sec0=nsec;

    // once per period
    if(m_sec0 % m_TRperiod == 0){
        tryBandHop();
    }

    // at the end of the period
    bool forceDirty = false;
    if(m_sec0 % (m_TRperiod-2) == 0 ||
       m_sec0 % (m_TRperiod) == 0   ||
       m_sec0 % (m_TRperiod+2) == 0 ){
        // force rx dirty at the end of the period
        forceDirty = true;
    }

    // update the dial frequency once per second..
    displayDialFrequency();

    // update repeat button text once per second..
    updateRepeatButtonDisplay();

    // once per second...but not when we're transmitting, unless it's in the first second...
    if(!m_transmitting || (m_sec0 % (m_TRperiod) == 0)){
        // process all received activity...
        processActivity(forceDirty);

        // process outgoing tx queue...
        processTxQueue();

        // once processed, lets update the display...
        displayActivity(forceDirty);
        updateButtonDisplay();
        updateTextDisplay();
    }
  }

  // once per 100ms
  displayTransmit();

  m_iptt0=g_iptt;
  m_btxok0=m_btxok;

  // compute the processing time and adjust loop to hit the next 100ms
  auto endLoop = QDateTime::currentMSecsSinceEpoch();
  auto processingTime = endLoop - thisLoop;
  auto nextLoopMs = 0;
  if(processingTime < 100){
      nextLoopMs = 100 - processingTime;
  }

  m_guiTimer.start(nextLoopMs);
}               //End of guiUpdate


void MainWindow::startTx()
{
#if IDLE_BLOCKS_TX
  if(m_tx_watchdog){
      return;
  }
#endif
  auto text = ui->extFreeTextMsgEdit->toPlainText();
  if(!ensureCreateMessageReady(text)){
    return;
  }

  if(!prepareNextMessageFrame()){
    return;
  }

  m_ntx=9;
  m_QSOProgress = CALLING;
  set_dateTimeQSO(-1);
  ui->rbNextFreeTextMsg->setChecked(true);
  if (m_transmitting) m_restart=true;

  // hack the auto button to kick off the transmit
  if(!ui->autoButton->isChecked()){
    ui->autoButton->setEnabled(true);
    ui->autoButton->click();
    ui->autoButton->setEnabled(false);
  }

  // disallow editing of the text while transmitting
  // ui->extFreeTextMsgEdit->setReadOnly(true);
  update_dynamic_property(ui->extFreeTextMsgEdit, "transmitting", true);

  // update the tx button display
  updateTxButtonDisplay();
}

void MainWindow::startTx2()
{
  if (!m_modulator->isActive ()) { // TODO - not thread safe
    double fSpread=0.0;
    double snr=99.0;
    QString t=ui->tx5->currentText();
    if(t.mid(0,1)=="#") fSpread=t.mid(1,5).toDouble();
    m_modulator->setSpread(fSpread); // TODO - not thread safe
    t=ui->tx6->text();
    if(t.mid(0,1)=="#") snr=t.mid(1,5).toDouble();
    if(snr>0.0 or snr < -50.0) snr=99.0;
    transmit (snr);
    ui->signal_meter_widget->setValue(0,0);
  }
}

void MainWindow::stopTx()
{
  Q_EMIT endTransmitMessage ();

  auto dt = DecodedText(m_currentMessage.trimmed(), m_currentMessageBits, m_nSubMode);
  last_tx_label.setText("Last Tx: " + dt.message()); //m_currentMessage.trimmed());

  // TODO: uncomment if we want to mark after the frame is sent.
  //// // start message marker
  //// // - keep track of the total message sent so far, and mark it having been sent
  //// m_totalTxMessage.append(dt.message());
  //// ui->extFreeTextMsgEdit->setCharsSent(m_totalTxMessage.length());
  //// qDebug() << "total sent:\n" << m_totalTxMessage;
  //// // end message marker

  m_btxok = false;
  m_transmitting = false;
  g_iptt=0;
  if (!m_tx_watchdog) {
    tx_status_label.setStyleSheet("");
    tx_status_label.setText("");
  }

#if IDLE_BLOCKS_TX
  bool shouldContinue = !m_tx_watchdog && prepareNextMessageFrame();
#else
  bool shouldContinue = prepareNextMessageFrame();
#endif
  if(!shouldContinue){
      // TODO: jsherer - split this up...
      ui->extFreeTextMsgEdit->clear();
      ui->extFreeTextMsgEdit->setReadOnly(false);
      update_dynamic_property(ui->extFreeTextMsgEdit, "transmitting", false);
      on_stopTxButton_clicked();
      tryRestoreFreqOffset();
  }

  ptt0Timer.start(200);                       //end-of-transmission sequencer delay stopTx2
  monitor (true);
  statusUpdate ();
}

/**
 *  stopTx2 is called from stopTx to open the PTT
 */
void MainWindow::stopTx2(){
    // GM8JCF: m_txFrameCount is set to the number of frames to be transmitted when the send button is pressed
    // and remains at that count until the last frame is transmitted.
    // So, we keep the PTT ON so long as m_txFrameCount is non-zero

    qDebug() << "stopTx2 frames left" << m_txFrameCount;

    // If we're holding the PTT and there are more frames to transmit, do not emit the PTT signal
    if(!m_tune && m_config.hold_ptt() && m_txFrameCount > 0){
        return;
    }

    // Otherwise, emit the PTT signal
    emitPTT(false);
}

void MainWindow::ba2msg(QByteArray ba, char message[])             //ba2msg()
{
  int iz=ba.length();
  for(int i=0;i<28; i++) {
    if(i<iz) {
      message[i]=ba[i];
    } else {
      message[i]=32;
    }
  }
  message[28]=0;
}

void MainWindow::set_dateTimeQSO(int m_ntx)
{
    // m_ntx = -1 resets to default time
    // Our QSO start time can be fairly well determined from Tx 2 and Tx 3 -- the grid reports
    // If we CQ'd and sending sigrpt then 2 minutes ago n=2
    // If we're on msg 3 then 3 minutes ago n=3 -- might have sat on msg1 for a while
    // If we've already set our time on just return.
    // This should mean that Tx2 or Tx3 has been repeated so don't update the start time
    // We reset it in several places
    if (m_ntx == -1) { // we use a default date to detect change
      m_dateTimeQSOOn = QDateTime {};
    }
    else if (m_dateTimeQSOOn.isValid ()) {
        return;
    }
    else { // we also take of m_TRperiod/2 to allow for late clicks
      auto now = DriftingDateTime::currentDateTimeUtc();
      m_dateTimeQSOOn = now.addSecs (-(m_ntx - 2) * m_TRperiod - (now.time ().second () % m_TRperiod));
    }
}

void MainWindow::set_ntx(int n)                                   //set_ntx()
{
  m_ntx=n;
}

void MainWindow::on_txrb1_toggled (bool status)
{
  if (status) {
    if (ui->tx1->isEnabled ()) {
      m_ntx = 1;
      set_dateTimeQSO (-1); // we reset here as tx2/tx3 is used for start times
    }
    else {
      QTimer::singleShot (0, ui->txrb2, SLOT (click ()));
    }
  }
}

void MainWindow::on_txrb1_doubleClicked ()
{
  if(m_mode=="FT8" and m_config.bHound()) return;
  // skip Tx1, only allowed if not a type 2 compound callsign
  auto const& my_callsign = m_config.my_callsign ();
  auto is_compound = my_callsign != m_baseCall;
  ui->tx1->setEnabled ((is_compound && shortList (my_callsign)) || !ui->tx1->isEnabled ());
  if (!ui->tx1->isEnabled ()) {
    // leave time for clicks to complete before setting txrb2
    QTimer::singleShot (500, ui->txrb2, SLOT (click ()));
  }
}

void MainWindow::on_txrb2_toggled (bool status)
{
  // Tx 2 means we already have CQ'd so good reference
  if (status) {
    m_ntx = 2;
    set_dateTimeQSO (m_ntx);
  }
}

void MainWindow::on_txrb3_toggled(bool status)
{
  // Tx 3 means we should havel already have done Tx 1 so good reference
  if (status) {
    m_ntx=3;
    set_dateTimeQSO(m_ntx);
  }
}

void MainWindow::on_txrb4_toggled (bool status)
{
  if (status) {
    m_ntx=4;
  }
}

void MainWindow::on_txrb4_doubleClicked ()
{
}

void MainWindow::on_txrb5_toggled (bool status)
{
  if (status) {
    m_ntx = 5;
  }
}

void MainWindow::on_txrb5_doubleClicked ()
{
}

void MainWindow::on_txrb6_toggled(bool status)
{
  if (status) {
    m_ntx=6;
    if (ui->txrb6->text().contains (QRegularExpression {"^(CQ|QRZ) "})) set_dateTimeQSO(-1);
  }
}

void MainWindow::on_txb1_clicked()
{
  if (ui->tx1->isEnabled ()) {
    m_ntx=1;
    m_QSOProgress = REPLYING;
    ui->txrb1->setChecked(true);
    if (m_transmitting) m_restart=true;
  }
  else {
    on_txb2_clicked ();
  }
}

void MainWindow::on_txb1_doubleClicked()
{
  if(m_mode=="FT8" and m_config.bHound()) return;
  // skip Tx1, only allowed if not a type 1 compound callsign
  auto const& my_callsign = m_config.my_callsign ();
  auto is_compound = my_callsign != m_baseCall;
  ui->tx1->setEnabled ((is_compound && shortList (my_callsign)) || !ui->tx1->isEnabled ());
}

void MainWindow::on_txb2_clicked()
{
    m_ntx=2;
    m_QSOProgress = REPORT;
    ui->txrb2->setChecked(true);
    if (m_transmitting) m_restart=true;
}

void MainWindow::on_txb3_clicked()
{
    m_ntx=3;
    m_QSOProgress = ROGER_REPORT;
    ui->txrb3->setChecked(true);
    if (m_transmitting) m_restart=true;
}

void MainWindow::on_txb4_clicked()
{
    m_ntx=4;
    m_QSOProgress = ROGERS;
    ui->txrb4->setChecked(true);
    if (m_transmitting) m_restart=true;
}

void MainWindow::on_txb4_doubleClicked()
{
}

void MainWindow::on_txb5_clicked()
{
    m_ntx=5;
    m_QSOProgress = SIGNOFF;
    ui->txrb5->setChecked(true);
    if (m_transmitting) m_restart=true;
}

void MainWindow::on_txb5_doubleClicked()
{
}

void MainWindow::on_txb6_clicked()
{
    m_ntx=6;
    m_QSOProgress = CALLING;
    set_dateTimeQSO(-1);
    ui->txrb6->setChecked(true);
    if (m_transmitting) m_restart=true;
}

void MainWindow::TxAgain()
{
  auto_tx_mode(true);
}

void MainWindow::clearDX ()
{
  if (m_QSOProgress != CALLING)
    {
      auto_tx_mode (false);
    }
  ui->dxCallEntry->clear ();
  ui->dxGridEntry->clear ();
  m_lastCallsign.clear ();
  m_rptSent.clear ();
  m_rptRcvd.clear ();
  m_qsoStart.clear ();
  m_qsoStop.clear ();

  if (ui->tabWidget->currentIndex() == 1) {
    ui->genMsg->setText(ui->tx6->text());
    m_ntx=7;
    m_gen_message_is_cq = true;
    ui->rbGenMsg->setChecked(true);
  } else {
    if(m_mode=="FT8" and m_config.bHound()) {
      m_ntx=1;
      ui->txrb1->setChecked(true);
    } else {
      m_ntx=6;
      ui->txrb6->setChecked(true);
    }
  }
  m_QSOProgress = CALLING;
}

void MainWindow::lookup()                                       //lookup()
{
  QString hisCall {ui->dxCallEntry->text()};
  if (!hisCall.size ()) return;
  QFile f {m_config.writeable_data_dir ().absoluteFilePath ("CALL3.TXT")};
  if (f.open (QIODevice::ReadOnly | QIODevice::Text))
    {
      char c[132];
      qint64 n=0;
      for(int i=0; i<999999; i++) {
        n=f.readLine(c,sizeof(c));
        if(n <= 0) {
          ui->dxGridEntry->clear ();
          break;
        }
        QString t=QString(c);
        if(t.indexOf(hisCall)==0) {
          int i1=t.indexOf(",");
          QString hisgrid=t.mid(i1+1,6);
          i1=hisgrid.indexOf(",");
          if(i1>0) {
            hisgrid=hisgrid.mid(0,4);
          } else {
            hisgrid=hisgrid.mid(0,4) + hisgrid.mid(4,2).toLower();
          }
          ui->dxGridEntry->setText(hisgrid);
          break;
        }
      }
      f.close();
    }
}

void MainWindow::on_lookupButton_clicked()                    //Lookup button
{
  lookup();
}

void MainWindow::on_addButton_clicked()                       //Add button
{
  if(!ui->dxGridEntry->text ().size ()) {
    MessageBox::warning_message (this, tr ("Add to CALL3.TXT")
                                 , tr ("Please enter a valid grid locator"));
    return;
  }
  m_call3Modified=false;
  QString hisCall=ui->dxCallEntry->text();
  QString hisgrid=ui->dxGridEntry->text();
  QString newEntry=hisCall + "," + hisgrid;

  //  int ret = MessageBox::query_message(this, tr ("Add to CALL3.TXT"),
  //       tr ("Is %1 known to be active on EME?").arg (newEntry));
  //  if(ret==MessageBox::Yes) {
  //    newEntry += ",EME,,";
  //  } else {
  newEntry += ",,,";
  //  }

  QFile f1 {m_config.writeable_data_dir ().absoluteFilePath ("CALL3.TXT")};
  if(!f1.open(QIODevice::ReadWrite | QIODevice::Text)) {
    MessageBox::warning_message (this, tr ("Add to CALL3.TXT")
                                 , tr ("Cannot open \"%1\" for read/write: %2")
                                 .arg (f1.fileName ()).arg (f1.errorString ()));
    return;
  }
  if(f1.size()==0) {
    QTextStream out(&f1);
    out << "ZZZZZZ" << endl;
    f1.close();
    f1.open(QIODevice::ReadOnly | QIODevice::Text);
  }
  QFile f2 {m_config.writeable_data_dir ().absoluteFilePath ("CALL3.TMP")};
  if(!f2.open(QIODevice::WriteOnly | QIODevice::Text)) {
    MessageBox::warning_message (this, tr ("Add to CALL3.TXT")
                                 , tr ("Cannot open \"%1\" for writing: %2")
                                 .arg (f2.fileName ()).arg (f2.errorString ()));
    return;
  }
  QTextStream in(&f1);          //Read from CALL3.TXT
  QTextStream out(&f2);         //Copy into CALL3.TMP
  QString hc=hisCall;
  QString hc1="";
  QString hc2="000000";
  QString s;
  do {
    s=in.readLine();
    hc1=hc2;
    if(s.mid(0,2)=="//") {
      out << s + QChar::LineFeed; //Copy all comment lines
    } else {
      int i1=s.indexOf(",");
      hc2=s.mid(0,i1);
      if(hc>hc1 && hc<hc2) {
        out << newEntry + QChar::LineFeed;
        out << s + QChar::LineFeed;
        m_call3Modified=true;
      } else if(hc==hc2) {
        QString t {tr ("%1\nis already in CALL3.TXT"
                       ", do you wish to replace it?").arg (s)};
        int ret = MessageBox::query_message (this, tr ("Add to CALL3.TXT"), t);
        if(ret==MessageBox::Yes) {
          out << newEntry + QChar::LineFeed;
          m_call3Modified=true;
        }
      } else {
        if(s!="") out << s + QChar::LineFeed;
      }
    }
  } while(!s.isNull());

  f1.close();
  if(hc>hc1 && !m_call3Modified) out << newEntry + QChar::LineFeed;
  if(m_call3Modified) {
    QFile f0 {m_config.writeable_data_dir ().absoluteFilePath ("CALL3.OLD")};
    if(f0.exists()) f0.remove();
    QFile f1 {m_config.writeable_data_dir ().absoluteFilePath ("CALL3.TXT")};
    f1.rename(m_config.writeable_data_dir ().absoluteFilePath ("CALL3.OLD"));
    f2.rename(m_config.writeable_data_dir ().absoluteFilePath ("CALL3.TXT"));
    f2.close();
  }
}

void MainWindow::msgtype(QString t, QLineEdit* tx)               //msgtype()
{
}

void MainWindow::on_tx1_editingFinished()                       //tx1 edited
{
}

void MainWindow::on_tx2_editingFinished()                       //tx2 edited
{
}

void MainWindow::on_tx3_editingFinished()                       //tx3 edited
{
}

void MainWindow::on_tx4_editingFinished()                       //tx4 edited
{
}

void MainWindow::on_tx5_currentTextChanged (QString const& text) //tx5 edited
{
}

void MainWindow::on_tx6_editingFinished()                       //tx6 edited
{
}

void MainWindow::cacheActivity(QString key){
    m_callActivityBandCache[key] = m_callActivity;
    m_bandActivityBandCache[key] = m_bandActivity;
    m_rxTextBandCache[key] = ui->textEditRX->toHtml();
    m_heardGraphIncomingBandCache[key] = m_heardGraphIncoming;
    m_heardGraphOutgoingBandCache[key] = m_heardGraphOutgoing;
}

void MainWindow::restoreActivity(QString key){
    if(m_callActivityBandCache.contains(key)){
        m_callActivity = m_callActivityBandCache[key];
    }

    if(m_bandActivityBandCache.contains(key)){
        m_bandActivity = m_bandActivityBandCache[key];
    }

    if(m_rxTextBandCache.contains(key)){
        ui->textEditRX->setHtml(m_rxTextBandCache[key]);
    }

    if(m_heardGraphIncomingBandCache.contains(key)){
        m_heardGraphIncoming = m_heardGraphIncomingBandCache[key];
    }

    if(m_heardGraphOutgoingBandCache.contains(key)){
        m_heardGraphOutgoing = m_heardGraphOutgoingBandCache[key];
    }

    displayActivity(true);
}

void MainWindow::clearActivity(){
    qDebug() << "clear activity";
    m_bandActivity.clear();
    m_callActivity.clear();
    m_callSeenHeartbeat.clear();
    m_compoundCallCache.clear();
    m_rxCallCache.clear();
    m_rxCallQueue.clear();
    m_rxRecentCache.clear();
    m_rxDirectedCache.clear();
    m_rxFrameBlockNumbers.clear();
    m_rxActivityQueue.clear();
    m_rxCommandQueue.clear();
    m_lastTxMessage.clear();
    m_heardGraphIncoming.clear();
    m_heardGraphOutgoing.clear();

    refreshInboxCounts();
    resetTimeDeltaAverage();

    clearTableWidget(ui->tableWidgetCalls);
    createAllcallTableRows(ui->tableWidgetCalls, "");

    clearTableWidget(ui->tableWidgetRXAll);

    ui->textEditRX->clear();
    ui->freeTextMsg->clear();
    ui->extFreeTextMsg->clear();

    // make sure to clear the read only and transmitting flags so there's always a "way out"
    ui->extFreeTextMsgEdit->clear();
    ui->extFreeTextMsgEdit->setReadOnly(false);
    update_dynamic_property(ui->extFreeTextMsgEdit, "transmitting", false);

    displayActivity(true);
}

void MainWindow::createAllcallTableRows(QTableWidget *table, QString const &selectedCall){
    int count = 0;
    auto now = DriftingDateTime::currentDateTimeUtc();
    int callsignAging = m_config.callsign_aging();

    int startCol = 1;

    foreach(auto cd, m_callActivity.values()){
        if (cd.call.trimmed().isEmpty()){
            continue;
        }
        if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
            continue;
        }
        count++;
    }

    table->horizontalHeaderItem(startCol)->setText(count == 0 ? "Callsigns" : QString("Callsigns (%1)").arg(count));

    if(!m_config.avoid_allcall()){
        table->insertRow(table->rowCount());

        auto emptyItem = new QTableWidgetItem("");
        emptyItem->setData(Qt::UserRole, QVariant("@ALLCALL"));
        table->setItem(table->rowCount() - 1, 0, emptyItem);

        auto item = new QTableWidgetItem(QString("@ALLCALL"));
        item->setData(Qt::UserRole, QVariant("@ALLCALL"));

        table->setItem(table->rowCount() - 1, startCol, item);
        table->setSpan(table->rowCount() - 1, startCol, 1, table->columnCount());
        if(selectedCall == "@ALLCALL"){
            table->item(table->rowCount()-1, 0)->setSelected(true);
            table->item(table->rowCount()-1, startCol)->setSelected(true);
        }
    }

    auto groups = m_config.my_groups().toList();
    qSort(groups);
    foreach(auto group, groups){
        table->insertRow(table->rowCount());

        auto emptyItem = new QTableWidgetItem("");
        emptyItem->setData(Qt::UserRole, QVariant(group));
        table->setItem(table->rowCount() - 1, 0, emptyItem);

        auto item = new QTableWidgetItem(group);
        item->setData(Qt::UserRole, QVariant(group));
        table->setItem(table->rowCount() - 1, startCol, item);
        table->setSpan(table->rowCount() - 1, startCol, 1, table->columnCount());

        if(selectedCall == group){
            table->item(table->rowCount()-1, 0)->setSelected(true);
            table->item(table->rowCount()-1, startCol)->setSelected(true);
        }
    }
}

void MainWindow::displayTextForFreq(QString text, int freq, QDateTime date, bool isTx, bool isNewLine, bool isLast){
    int lowFreq = freq/10*10;
    int highFreq = lowFreq + 10;

    int block = -1;

    if(m_rxFrameBlockNumbers.contains(freq)){
        block = m_rxFrameBlockNumbers[freq];
    } else if(m_rxFrameBlockNumbers.contains(lowFreq)){
        block = m_rxFrameBlockNumbers[lowFreq];
        freq = lowFreq;
    } else if(m_rxFrameBlockNumbers.contains(highFreq)){
        block = m_rxFrameBlockNumbers[highFreq];
        freq = highFreq;
    }

    qDebug() << "existing block?" << block << freq;

    if(isNewLine){
        m_rxFrameBlockNumbers.remove(freq);
        m_rxFrameBlockNumbers.remove(lowFreq);
        m_rxFrameBlockNumbers.remove(highFreq);
        block = -1;
    }

    block = writeMessageTextToUI(date, text.replace("\\n", "\n"), freq, isTx, block);

    // never cache tx or last lines
    if(/*isTx || */isLast) {
        // reset the cache so we're always progressing forward
        m_rxFrameBlockNumbers.clear();
    } else {
        m_rxFrameBlockNumbers.insert(freq, block);
        m_rxFrameBlockNumbers.insert(lowFreq, block);
        m_rxFrameBlockNumbers.insert(highFreq, block);
    }
}

void MainWindow::writeNoticeTextToUI(QDateTime date, QString text){
    auto c = ui->textEditRX->textCursor();
    c.movePosition(QTextCursor::End);
    if(c.block().length() > 1){
        c.insertBlock();
    }

    text = text.toHtmlEscaped();
    c.insertBlock();
    c.insertHtml(QString("<strong>%1 - %2</strong>").arg(date.time().toString()).arg(text));

    c.movePosition(QTextCursor::End);

    ui->textEditRX->ensureCursorVisible();
    ui->textEditRX->verticalScrollBar()->setValue(ui->textEditRX->verticalScrollBar()->maximum());
}

int MainWindow::writeMessageTextToUI(QDateTime date, QString text, int freq, bool isTx, int block){
    auto c = ui->textEditRX->textCursor();

    // find an existing block (that does not contain an EOT marker)
    bool found = false;
    if(block != -1){
        QTextBlock b = c.document()->findBlockByNumber(block);
        c.setPosition(b.position());
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

        auto blockText = c.selectedText();
        c.clearSelection();
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);

        if(!blockText.contains(m_config.eot())){
            found = true;
        }
    }

    if(!found){
        c.movePosition(QTextCursor::End);
        if(c.block().length() > 1){
            c.insertBlock();
        }
    }

    // fixup duplicate acks
    auto tc = c.document()->find(text);
    if(!tc.isNull() && tc.selectedText() == text && text.contains(" ACK ")){
        tc.select(QTextCursor::BlockUnderCursor);

        if(tc.selectedText().trimmed().startsWith(date.time().toString())){
            qDebug() << "found" << tc.selectedText() << "so not displaying...";
            return tc.blockNumber();
        }
    }

    if(found){
        c.clearSelection();
        c.insertText(text);
    } else {
        text = text.toHtmlEscaped();
        text = text.replace("  ", "&nbsp;&nbsp;");
        c.insertBlock();
        c.insertHtml(QString("%1 - (%2) - %3").arg(date.time().toString()).arg(freq).arg(text));
    }

    if(isTx){
        c.block().setUserState(STATE_TX);
        highlightBlock(c.block(), m_config.tx_text_font(), m_config.color_tx_foreground(), QColor(Qt::transparent));
    } else {
        c.block().setUserState(STATE_RX);
        highlightBlock(c.block(), m_config.rx_text_font(), m_config.color_rx_foreground(), QColor(Qt::transparent));
    }

    ui->textEditRX->ensureCursorVisible();
    ui->textEditRX->verticalScrollBar()->setValue(ui->textEditRX->verticalScrollBar()->maximum());

    return c.blockNumber();
}

bool MainWindow::isMessageQueuedForTransmit(){
    return m_transmitting || m_txFrameCount > 0;
}

void MainWindow::prependMessageText(QString text){
    // don't add message text if we already have a transmission queued...
    if(isMessageQueuedForTransmit()){
        return;
    }

    auto c = QTextCursor(ui->extFreeTextMsgEdit->textCursor());
    c.movePosition(QTextCursor::Start);
    c.insertText(text);
}

void MainWindow::addMessageText(QString text, bool clear, bool selectFirstPlaceholder){
    // don't add message text if we already have a transmission queued...
    if(isMessageQueuedForTransmit()){
        return;
    }

    if(clear){
        ui->extFreeTextMsgEdit->clear();
    }

    QTextCursor c = ui->extFreeTextMsgEdit->textCursor();
    if(c.hasSelection()){
        c.removeSelectedText();
    }

    int pos = c.position();
    c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);

    bool isSpace = c.selectedText().isEmpty() || c.selectedText().at(0).isSpace();
    c.clearSelection();

    c.setPosition(pos);

    if(!isSpace){
        c.insertText(" ");
    }

    c.insertText(text);

    if(selectFirstPlaceholder){
        auto match = QRegularExpression("(\\[[^\\]]+\\])").match(ui->extFreeTextMsgEdit->toPlainText());
        if(match.hasMatch()){
            c.setPosition(match.capturedStart());
            c.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
            ui->extFreeTextMsgEdit->setTextCursor(c);
        }
    }

    ui->extFreeTextMsgEdit->setFocus();
}

void MainWindow::enqueueMessage(int priority, QString message, int freq, Callback c){
    m_txMessageQueue.enqueue(
        PrioritizedMessage{
            DriftingDateTime::currentDateTimeUtc(), priority, message, freq, c
        }
    );
}

void MainWindow::resetMessage(){
    resetMessageUI();
    resetMessageTransmitQueue();
}

void MainWindow::resetMessageUI(){
    ui->nextFreeTextMsg->clear();
    ui->extFreeTextMsg->clear();
    ui->extFreeTextMsgEdit->clear();
    ui->extFreeTextMsgEdit->setReadOnly(false);
    update_dynamic_property (ui->extFreeTextMsgEdit, "transmitting", false);

    if(ui->startTxButton->isChecked()){
        ui->startTxButton->setChecked(false);
    }
}

bool MainWindow::ensureCallsignSet(bool alert){
    if(m_config.my_callsign().trimmed().isEmpty()){
        if(alert) MessageBox::warning_message(this, tr ("Please enter your callsign in the settings."));
        openSettings();
        return false;
    }

    if(m_config.my_grid().trimmed().isEmpty()){
        if(alert) MessageBox::warning_message(this, tr ("Please enter your grid locator in the settings."));
        openSettings();
        return false;
    }

    return true;
}

bool MainWindow::ensureKeyNotStuck(QString const& text){
    // be annoying and drop messages with all the same character to reduce spam...
    if(text.length() > 5 && QString(text).replace(text.at(0), "").trimmed().isEmpty()){
        return false;
    }

    return true;
}

bool MainWindow::ensureNotIdle(){
    if (!m_config.watchdog()){
        return true;
    }

    if(m_idleMinutes < m_config.watchdog ()){
        return true;
    }

    tx_watchdog (true);       // disable transmit and auto replies
    return false;
}

bool MainWindow::ensureCreateMessageReady(const QString &text){
    if(text.isEmpty()){
        return false;
    }

    if(!ensureCallsignSet()){
        on_stopTxButton_clicked();
        return false;
    }

    if(!ensureNotIdle()){
        on_stopTxButton_clicked();
        return false;
    }

    if(!ensureKeyNotStuck(text)){
        on_stopTxButton_clicked();

        ui->monitorButton->setChecked(false);
        on_monitorButton_clicked(false);

        foreach(auto obj, this->children()){
            if(obj->isWidgetType()){
                auto wid = qobject_cast<QWidget*>(obj);
                wid->setEnabled(false);
            }
        }

        return false;
    }

    if(text.contains("@APRSIS") && !m_aprsClient->isPasscodeValid()){
        MessageBox::warning_message(this, tr ("Please ensure a valid APRS passcode is set in the settings when sending to the @APRSIS group."));
        return false;
    }

    return true;
}

QString MainWindow::createMessage(QString const& text){
    return createMessageTransmitQueue(replaceMacros(text, buildMacroValues(), false), true);
}

QString MainWindow::appendMessage(QString const& text){
    return createMessageTransmitQueue(replaceMacros(text, buildMacroValues(), false), false);
}

QString MainWindow::createMessageTransmitQueue(QString const& text, bool reset){
  if(reset){
      resetMessageTransmitQueue();
  }

  auto frames = buildMessageFrames(text);

  QStringList lines;
  foreach(auto frame, frames){
      auto dt = DecodedText(frame.first, frame.second, m_nSubMode);
      lines.append(dt.message());
  }

  m_txFrameQueue.append(frames);
  m_txFrameCount += frames.length();

  // TODO: jsherer - move this outside of create message transmit queue
  // if we're transmitting a message to be displayed, we should bump the repeat buttons...
#if JS8HB_RESET_HB_TIMER_ON_TX
  resetAutomaticIntervalTransmissions(false, false);
#else
  resetCQTimer(false);
#endif

  // keep track of the last message text sent
  auto joined = lines.join("");

  m_lastTxMessage += joined;

  return joined;
}

void MainWindow::restoreMessage(){
    if(m_lastTxMessage.isEmpty()){
        return;
    }
    addMessageText(m_lastTxMessage, true);
}

void MainWindow::resetMessageTransmitQueue(){
  m_txFrameCount = 0;
  m_txFrameCountSent = 0;
  m_txFrameQueue.clear();
  m_txMessageQueue.clear();

  // reset the total message sent
  m_totalTxMessage.clear();

  // reset the last message sent
  m_lastTxMessage.clear();
}

QPair<QString, int> MainWindow::popMessageFrame(){
  if(m_txFrameQueue.isEmpty()){
      return QPair<QString, int>{};
  }
  return m_txFrameQueue.dequeue();
}

void MainWindow::on_textEditRX_mouseDoubleClicked(){
  auto c = ui->textEditRX->textCursor();
  auto text = c.selectedText();
  if(text.isEmpty()){
      return;
  }

  int start = c.selectionStart();
  int end = c.selectionEnd();

  c.clearSelection();
  c.setPosition(start);
  c.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor);
  c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 1 + end-start);

  auto prev = c.selectedText();
  if(prev.startsWith("-") || prev.startsWith("+")){
      ui->textEditRX->setTextCursor(c);
      text = prev;
  }

  m_logDlg->acceptText(text);
}

void MainWindow::on_nextFreeTextMsg_currentTextChanged (QString const& text)
{
}

void MainWindow::on_extFreeTextMsgEdit_currentTextChanged (QString const& text)
{
    // keep track of dirty flags
    m_txTextDirty = text != m_txTextDirtyLastText;
    m_txTextDirtyLastText = text;

    // immediately update the display
    updateButtonDisplay();
    updateTextDisplay();
}

int MainWindow::currentFreqOffset(){
    return ui->RxFreqSpinBox->value();
}

QList<QPair<QString, int>> MainWindow::buildMessageFrames(const QString &text){
    // prepare selected callsign for directed message
    QString selectedCall = callsignSelected();

    // prepare compound
    QString mycall = m_config.my_callsign();
    QString mygrid = m_config.my_grid().left(4);

    bool forceIdentify = !m_config.avoid_forced_identify();

    // TODO: might want to be more explicit?
    bool forceData = m_txFrameCountSent > 0;

    Varicode::MessageInfo info;
    auto frames = Varicode::buildMessageFrames(
        mycall,
        mygrid,
        selectedCall,
        text,
        forceIdentify,
        forceData,
        m_nSubMode,
        &info);

    if(!info.dirCmd.isEmpty()){
        qDebug() << "message contains cmd" << info.dirCmd;
        if(Varicode::isCommandBuffered(info.dirCmd)){
            // buffered commands should not allow typeahead
            // TODO: jsherer - i don't like setting this here, but it works for now...
            ui->extFreeTextMsgEdit->setReadOnly(true);
        }
    }

#if 0
    qDebug() << "frames:";
    foreach(auto frame, frames){
        auto dt = DecodedText(frame.frame, frame.bits);
        qDebug() << "->" << frame << dt.message() << Varicode::frameTypeString(dt.frameType());
    }
#endif

    return frames;
}

bool MainWindow::prepareNextMessageFrame()
{
  m_i3bit = Varicode::JS8Call;

  // typeahead
  if(ui->extFreeTextMsgEdit->isDirty() && !ui->extFreeTextMsgEdit->isEmpty()){
      auto sent = ui->extFreeTextMsgEdit->sentText();
      auto unsent = ui->extFreeTextMsgEdit->unsentText();
      qDebug() << "text dirty for typeahead\n" << sent << "\n" << unsent;
      m_txFrameQueue.clear();
      m_txFrameCount = 0;
      auto newText = appendMessage(unsent);
      qDebug () << "unsent replaced to" << "\n" << newText;
      ui->extFreeTextMsgEdit->replaceUnsentText(newText);
      ui->extFreeTextMsgEdit->setClean();
  }

  QPair<QString, int> f = popMessageFrame();
  auto frame = f.first;
  auto bits = f.second;

  // if not the first frame, ensure first bit is not set
  if(m_txFrameCountSent > 0){
      bits &= ~Varicode::JS8CallFirst;
  }

  // if last frame, ensure the last bit is set
  if(m_txFrameQueue.isEmpty()){
      bits |= Varicode::JS8CallLast;
  }

  if(frame.isEmpty()){
    ui->nextFreeTextMsg->clear();
    updateTxButtonDisplay();
    return false;
  }

  // append this frame to the total message sent so far
  auto dt = DecodedText(frame, bits, m_nSubMode);
  m_totalTxMessage.append(dt.message());
  ui->extFreeTextMsgEdit->setCharsSent(m_totalTxMessage.length());
  m_txFrameCountSent += 1;
  qDebug() << "total sent:" << m_txFrameCountSent << "\n" << m_totalTxMessage;

  // display the frame...
  auto freq = currentFreqOffset();
  if(m_txFrameQueue.isEmpty()){
    displayTextForFreq(QString("%1 %2 ").arg(dt.message()).arg(m_config.eot()), freq, DriftingDateTime::currentDateTimeUtc(), true, false, true);
  } else {
    bool newLine = m_txFrameCountSent == 1;
    displayTextForFreq(dt.message(), freq, DriftingDateTime::currentDateTimeUtc(), true, newLine, false);
  }

  ui->nextFreeTextMsg->setText(frame);
  m_i3bit = bits;

  updateTxButtonDisplay();

  // TODO: bump heartbeat

  return true;
}

bool MainWindow::isFreqOffsetFree(int f, int bw){
    // if this frequency is our current frequency, it's always "free"
    if(currentFreqOffset() == f){
        return true;
    }

    // if this frequency is in our directed cache, it's always "free"
    if(isDirectedOffset(f, nullptr)){
        return true;
    }

    foreach(int offset, m_bandActivity.keys()){
        auto d = m_bandActivity[offset];
        if(d.isEmpty()){
            continue;
        }

        // if we last received on this more than 30 seconds ago, it's free
        if(d.last().utcTimestamp.secsTo(DriftingDateTime::currentDateTimeUtc()) >= 30){
            continue;
        }

        // otherwise, if this is an occupied slot within our bw of where we'd like to transmit... it's not free...
        if(qAbs(offset - f) < bw){
            return false;
        }
    }

    return true;
}

int MainWindow::findFreeFreqOffset(int fmin, int fmax, int bw){
    int nslots = (fmax-fmin)/bw;

    int f = fmin;
    for(int i = 0; i < nslots; i++){
        f = fmin + bw * (qrand() % nslots);
        if(isFreqOffsetFree(f, bw)){
            return f;
        }
    }

    for(int i = 0; i < nslots; i++){
        f = fmin + (qrand() % (fmax-fmin));
        if(isFreqOffsetFree(f, bw)){
            return f;
        }
    }

    // return fmin if there's no free offset
    return fmin;
}

#if 0
// schedulePing
void MainWindow::scheduleHeartbeat(bool first){
    auto timestamp = DriftingDateTime::currentDateTimeUtc();

    // if we have the heartbeat interval disabled, return early, unless this is a "heartbeat now"
    if(!m_config.heartbeat() && !first){
        heartbeatTimer.stop();
        return;
    }

    // remove milliseconds
    auto t = timestamp.time();
    t.setHMS(t.hour(), t.minute(), t.second());
    timestamp.setTime(t);

    // round to 15 second increment
    int secondsSinceEpoch = (timestamp.toMSecsSinceEpoch()/1000);
    int delta = roundUp(secondsSinceEpoch, 15) + 1 + (first ? 0 : qMax(1, m_config.heartbeat()) * 60) - secondsSinceEpoch;
    timestamp = timestamp.addSecs(delta);

    // 25% of the time, switch intervals
    float prob = (float) qrand() / (RAND_MAX);
    if(prob < 0.25){
        timestamp = timestamp.addSecs(15);
    }

    m_nextHeartbeat = timestamp;
    m_nextHeartbeatQueued = false;
    m_nextHeartPaused = false;

    if(!heartbeatTimer.isActive()){
        heartbeatTimer.setInterval(1000);
        heartbeatTimer.start();
    }
}

// pausePing
void MainWindow::pauseHeartbeat(){
    m_nextHeartPaused = true;

    if(heartbeatTimer.isActive()){
        heartbeatTimer.stop();
    }
}

// unpausePing
void MainWindow::unpauseHeartbeat(){
    scheduleHeartbeat(false);
}

// checkPing
void MainWindow::checkHeartbeat(){
    if(m_config.heartbeat() <= 0){
        return;
    }
    auto secondsUntilHeartbeat = DriftingDateTime::currentDateTimeUtc().secsTo(m_nextHeartbeat);
    if(secondsUntilHeartbeat > 5 && m_txHeartbeatQueue.isEmpty()){
        return;
    }
    if(m_nextHeartbeatQueued){
        return;
    }
    if(m_tx_watchdog){
        return;
    }

    // idle heartbeat watchdog!
    if (m_config.watchdog() && m_idleMinutes >= m_config.watchdog ()){
      tx_watchdog (true);       // disable transmit
      return;
    }

    prepareHeartbeat();
}

// preparePing
void MainWindow::prepareHeartbeat(){
    QStringList lines;

    QString mycall = m_config.my_callsign();
    QString mygrid = m_config.my_grid().left(4);

    // JS8Call Style
    if(m_txHeartbeatQueue.isEmpty()){
        lines.append(QString("%1: HEARTBEAT %2").arg(mycall).arg(mygrid));
    } else {
        while(!m_txHeartbeatQueue.isEmpty() && lines.length() < 1){
            lines.append(m_txHeartbeatQueue.dequeue());
        }
    }

    // Choose a ping frequency
    auto f = m_config.heartbeat_anywhere() ? -1 : findFreeFreqOffset(500, 1000, 50);

    auto text = lines.join(QChar('\n'));
    if(text.isEmpty()){
        return;
    }

    // Queue the ping
    enqueueMessage(PriorityLow, text, f, [this](){
        m_nextHeartbeatQueued = false;
    });

    m_nextHeartbeatQueued = true;
}
#endif

void MainWindow::checkRepeat(){
    if(ui->hbMacroButton->isChecked() && m_hbInterval > 0 && m_nextHeartbeat.isValid()){
        if(DriftingDateTime::currentDateTimeUtc().secsTo(m_nextHeartbeat) <= 0){
            sendHeartbeat();
        }
    }

    if(ui->cqMacroButton->isChecked() && m_cqInterval > 0 && m_nextCQ.isValid()){
        if(DriftingDateTime::currentDateTimeUtc().secsTo(m_nextCQ) <= 0){
            sendCQ(true);
        }
    }
}

QString MainWindow::calculateDistance(QString const& value, int *pDistance, int *pAzimuth)
{
    QString grid = value.trimmed();
    if(grid.isEmpty() || grid.length() < 4){
        return QString{};
    }

    qint64 nsec = (DriftingDateTime::currentMSecsSinceEpoch()/1000) % 86400;
    double utch=nsec/3600.0;
    int nAz,nEl,nDmiles,nDkm,nHotAz,nHotABetter;
    azdist_(const_cast <char *> ((m_config.my_grid () + "      ").left (6).toLatin1().constData()),
            const_cast <char *> ((grid + "      ").left (6).toLatin1().constData()),&utch,
            &nAz,&nEl,&nDmiles,&nDkm,&nHotAz,&nHotABetter,6,6);

    if(pAzimuth) *pAzimuth = nAz;

    if(m_config.miles()){
        if(pDistance) *pDistance = nDmiles;
        return QString("%1 mi / %2°").arg(nDmiles).arg(nAz);
    }

    if(pDistance) *pDistance = nDkm;
    return QString("%1 km / %2°").arg(nDkm).arg(nAz);
}

// this function is called by auto_tx_mode, which is called by autoButton.clicked
void MainWindow::on_startTxButton_toggled(bool checked)
{
    if(checked){
        // auto text = ui->extFreeTextMsgEdit->toPlainText();
        // if(ensureCreateMessageReady(text)){
        //
        //     auto txText = createMessage(text);
        //     if(txText != text){
        //         ui->extFreeTextMsgEdit->setPlainText(txText);
        //     }
        //
        //     startTx();
        // }
        startTx();
    } else {
        resetMessage();
        on_stopTxButton_clicked();
        stopTx();
    }
}

void MainWindow::toggleTx(bool start){
    if(start && ui->startTxButton->isChecked()) { return; }
    if(!start && !ui->startTxButton->isChecked()) { return; }
    ui->startTxButton->setChecked(start);
}

void MainWindow::on_rbNextFreeTextMsg_toggled (bool status)
{
  if (status) {
    m_ntx = 9;
  }
}

void MainWindow::on_dxCallEntry_textChanged (QString const& call)
{
}

void MainWindow::on_dxCallEntry_returnPressed ()
{
}

void MainWindow::on_dxGridEntry_textChanged (QString const& grid)
{
}

void MainWindow::on_genStdMsgsPushButton_clicked()
{
}

void MainWindow::on_logQSOButton_clicked()                 //Log QSO button
{
  QString call = callsignSelected();
  if(m_callSelectedTime.contains(call)){
    m_dateTimeQSOOn = m_callSelectedTime[call];
  }
  if (!m_dateTimeQSOOn.isValid ()) {
    m_dateTimeQSOOn = DriftingDateTime::currentDateTimeUtc();
  }
  auto dateTimeQSOOff = DriftingDateTime::currentDateTimeUtc();
  if (dateTimeQSOOff < m_dateTimeQSOOn) dateTimeQSOOff = m_dateTimeQSOOn;

  if(call.startsWith("@")){
      call = "";
  }
  QString grid="";
  if(m_callActivity.contains(call)){
      grid = m_callActivity[call].grid;
  }
  QString opCall=m_opCall;
  if(opCall.isEmpty()){
      opCall = m_config.my_callsign();
  }

  QString comments = ui->textEditRX->textCursor().selectedText();

  // don't reset the log window if the call hasn't changed.
  if(!m_logDlg->currentCall().isEmpty() && call.trimmed() == m_logDlg->currentCall()){
      m_logDlg->show();
      return;
  }

  m_logDlg->initLogQSO (call.trimmed(), grid.trimmed(), m_modeTx == "FT8" ? "JS8" : m_modeTx, m_rptSent, m_rptRcvd,
                        m_dateTimeQSOOn, dateTimeQSOOff, m_freqNominal + ui->TxFreqSpinBox->value(),
                        m_config.my_callsign(), m_config.my_grid(),
                        m_config.log_as_DATA(), m_config.report_in_comments(),
                        m_config.bFox(), opCall, comments);
}

void MainWindow::acceptQSO (QDateTime const& QSO_date_off, QString const& call, QString const& grid
                            , Frequency dial_freq, QString const& mode, QString const &submode
                            , QString const& rpt_sent, QString const& rpt_received
                            , QString const& comments
                            , QString const& name, QDateTime const& QSO_date_on, QString const& operator_call
                            , QString const& my_call, QString const& my_grid, QByteArray const& ADIF, QMap<QString, QVariant> const &additionalFields)
{
  QString date = QSO_date_on.toString("yyyyMMdd");
  m_logBook.addAsWorked (m_hisCall, m_config.bands ()->find (m_freqNominal), mode, submode, grid, date, name, comments);

  // Log to JS8Call API
  if(m_config.udpEnabled()){
      sendNetworkMessage("LOG.QSO", QString(ADIF), {
          {"UTC.ON", QVariant(QSO_date_on.toMSecsSinceEpoch())},
          {"UTC.OFF", QVariant(QSO_date_off.toMSecsSinceEpoch())},
          {"CALL", QVariant(call)},
          {"GRID", QVariant(grid)},
          {"FREQ", QVariant(dial_freq)},
          {"MODE", QVariant(mode)},
          {"SUBMODE", QVariant(submode)},
          {"RPT.SENT", QVariant(rpt_sent)},
          {"RPT.RECV", QVariant(rpt_received)},
          {"NAME", QVariant(name)},
          {"COMMENTS", QVariant(comments)},
          {"STATION.OP", QVariant(operator_call)},
          {"STATION.CALL", QVariant(my_call)},
          {"STATION.GRID", QVariant(my_grid)},
          {"EXTRA", additionalFields}
      });
  }

  // Log to N1MM Logger
  if (m_config.broadcast_to_n1mm() && m_config.valid_n1mm_info())  {
    const QHostAddress n1mmhost = QHostAddress(m_config.n1mm_server_name());
    QUdpSocket _sock;
    auto rzult = _sock.writeDatagram (ADIF + " <eor>", n1mmhost, quint16(m_config.n1mm_server_port()));
    if (rzult == -1) {
      MessageBox::warning_message (this, tr ("Error sending log to N1MM"),
                                   tr ("Write returned \"%1\"").arg (rzult));
    }
  }

  // Log to N3FJP Logger
  if(m_config.broadcast_to_n3fjp() && m_config.valid_n3fjp_info()){
      QString data = QString(
        "<CMD>"
        "<ADDDIRECT>"
        "<EXCLUDEDUPES>TRUE</EXCLUDEDUPES>"
        "<STAYOPEN>FALSE</STAYOPEN>"
        "<fldDateStr>%1</fldDateStr>"
        "<fldTimeOnStr>%2</fldTimeOnStr>"
        "<fldCall>%3</fldCall>"
        "<fldGridR>%4</fldGridR>"
        "<fldBand>%5</fldBand>"
        "<fldFrequency>%6</fldFrequency>"
        "<fldMode>JS8</fldMode>"
        "<fldOperator>%7</fldOperator>"
        "<fldNameR>%8</fldNameR>"
        "<fldComments>%9</fldComments>"
        "<fldRstS>%10</fldRstS>"
        "<fldRstR>%11</fldRstR>"
        "%12"
        "</CMD>");

      data = data.arg(QSO_date_on.toString("yyyy/MM/dd"));
      data = data.arg(QSO_date_on.toString("H:mm"));
      data = data.arg(call);
      data = data.arg(grid);
      data = data.arg(m_config.bands ()->find(dial_freq).replace("m", ""));
      data = data.arg(Radio::frequency_MHz_string(dial_freq));
      data = data.arg(operator_call);
      data = data.arg(name);
      data = data.arg(comments);
      data = data.arg(rpt_sent);
      data = data.arg(rpt_received);

      int other = 0;
      QStringList additional;
      if(!additionalFields.isEmpty()){
          foreach(auto key, additionalFields.keys()){
              QString n3key;
              if(N3FJP_ADIF_MAP.contains(key)){
                  n3key = N3FJP_ADIF_MAP.value(key);
              } else {
                  other++;
                  n3key = N3FJP_ADIF_MAP.value(QString("*%1").arg(other));
              }

              if(n3key.isEmpty()){
                  break;
              }
              auto value = additionalFields[key].toString();
              additional.append(QString("<%1>%2</%1>").arg(n3key).arg(value));
          }
      }
      data = data.arg(additional.join(""));

      auto host = m_config.n3fjp_server_name();
      auto port = m_config.n3fjp_server_port();

      if(m_n3fjpClient->sendNetworkMessage(host, port, data.toLocal8Bit(), true, 500)){
          QTimer::singleShot(300, this, [this, host, port](){
            m_n3fjpClient->sendNetworkMessage(host, port, "<CMD><CHECKLOG></CMD>", true, 100);
            m_n3fjpClient->sendNetworkMessage(host, port, "\r\n", true, 100);
          });
      } else {
          MessageBox::warning_message (this, tr ("Error sending log to N3FJP"),
                                       tr ("Write failed for \"%1:%2\"").arg (host).arg(port));
      }
  }

  // reload the logbook data
  m_logBook.init();

  if (m_config.clear_callsign ()){
      clearCallsignSelected();
  }

  displayCallActivity();

  m_dateTimeQSOOn = QDateTime {};
}

qint64 MainWindow::nWidgets(QString t)
{
  Q_ASSERT(t.length()==N_WIDGETS);
  qint64 n=0;
  for(int i=0; i<N_WIDGETS; i++) {
    n=n + n + t.mid(i,1).toInt();
  }
  return n;
}

void MainWindow::displayWidgets(qint64 n)
{
  /* See text file "displayWidgets.txt" for widget numbers */
  qint64 j=qint64(1)<<(N_WIDGETS-1);
  bool b;
  for(int i=0; i<N_WIDGETS; i++) {
    b=(n&j) != 0;
    if(i==0) ui->txFirstCheckBox->setVisible(b);
    if(i==1) ui->TxFreqSpinBox->setVisible(b);
    if(i==2) ui->RxFreqSpinBox->setVisible(b);
    if(i==3) ui->sbFtol->setVisible(b);
    if(i==4) ui->rptSpinBox->setVisible(b);
    if(i==5) ui->sbTR->setVisible(b);
    if(i==6) {
      ui->sbCQTxFreq->setVisible (b);
      ui->cbCQTx->setVisible (b);
      auto is_compound = m_config.my_callsign () != m_baseCall;
      ui->cbCQTx->setEnabled (b && (!is_compound || shortList (m_config.my_callsign ())));
    }
    if(i==7) ui->cbShMsgs->setVisible(b);
    if(i==8) ui->cbFast9->setVisible(b);
    if(i==9) ui->cbAutoSeq->setVisible(b);
    if(i==10) ui->cbTx6->setVisible(b);
    if(i==11) ui->pbTxMode->setVisible(b);
    if(i==12) ui->pbR2T->setVisible(b);
    if(i==13) ui->pbT2R->setVisible(b);
    if(i==14) ui->cbHoldTxFreq->setVisible(b);
    if(i==14 and (!b)) ui->cbHoldTxFreq->setChecked(false);
    if(i==15) ui->sbSubmode->setVisible(b);
    if(i==16) ui->syncSpinBox->setVisible(b);
    if(i==17) ui->WSPR_controls_widget->setVisible(b);
    //if(i==18) ui->ClrAvgButton->setVisible(b);
    if(i==19) ui->actionQuickDecode->setEnabled(b);
    if(i==19) ui->actionMediumDecode->setEnabled(b);
    if(i==19) ui->actionDeepDecode->setEnabled(b);
    if(i==19) ui->actionDeepestDecode->setEnabled(b);
    if(i==20) ui->actionInclude_averaging->setVisible (b);
    if(i==21) ui->actionInclude_correlation->setVisible (b);
    if(i==22) {
      if(!b && m_echoGraph->isVisible())  m_echoGraph->hide();
    }
    if(i==23) ui->cbSWL->setVisible(b);
    //if(i==24) ui->actionEnable_AP_FT8->setVisible (b);
    //if(i==25) ui->actionEnable_AP_JT65->setVisible (b);
    //if(i==26) ui->actionEnable_AP_DXcall->setVisible (b);
    if(i==27) ui->cbFirst->setVisible(b);
    if(i==28) ui->cbVHFcontest->setVisible(b);
    if(i==29) ui->measure_check_box->setVisible(b);
    if(i==30) ui->labDXped->setVisible(b);
    if(i==31) ui->cbRxAll->setVisible(b);
    //if(i==32) ui->cbCQonly->setVisible(b);
    j=j>>1;
  }
  ui->tabWidget->setTabEnabled(3, "FT8" == m_mode);
  m_lastCallsign.clear ();     // ensures Tx5 is updated for new modes
}

void MainWindow::on_actionModeJS8HB_toggled(bool checked){
    // prep hb mode

    prepareHeartbeatMode(ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked());
    displayActivity(true);

    on_actionJS8_triggered();
}

void MainWindow::on_actionHeartbeatAcknowledgements_toggled(bool checked){
    // prep hb ack mode

    prepareHeartbeatMode(ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked());
    displayActivity(true);

    on_actionJS8_triggered();
}

void MainWindow::on_actionModeJS8Normal_triggered(){
    on_actionJS8_triggered();
}

void MainWindow::on_actionModeJS8Fast_triggered(){
    on_actionJS8_triggered();
}

void MainWindow::on_actionModeJS8Turbo_triggered(){
    on_actionJS8_triggered();
}

void MainWindow::on_actionModeJS8Ultra_triggered(){
    on_actionJS8_triggered();
}

void MainWindow::on_actionModeAutoreply_toggled(bool checked){
    // update the HB ack option (needs autoreply on)
    prepareHeartbeatMode(ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked());

    // then update the js8 mode
    on_actionJS8_triggered();
}

void MainWindow::prepareHeartbeatMode(bool enabled){
    // heartbeat is only available in HB mode
    ui->hbMacroButton->setVisible(enabled);
    if(!enabled){
        ui->hbMacroButton->setChecked(false);
    }
    ui->actionHeartbeat->setEnabled(enabled);
    ui->actionModeJS8HB->setEnabled(m_nSubMode == Varicode::JS8CallNormal);
    ui->actionHeartbeatAcknowledgements->setEnabled(ui->actionModeAutoreply->isChecked() && enabled);

#if 0
    //ui->actionCQ->setEnabled(!enabled);
    //ui->actionFocus_Message_Reply_Area->setEnabled(!enabled);

    // default to not displaying the other buttons
    // ui->cqMacroButton->setVisible(!enabled);
    // ui->replyMacroButton->setVisible(!enabled);
    // ui->snrMacroButton->setVisible(!enabled);
    // ui->infoMacroButton->setVisible(!enabled);
    // ui->macrosMacroButton->setVisible(!enabled);
    // ui->queryButton->setVisible(!enabled);
    // ui->extFreeTextMsgEdit->setVisible(!enabled);
    // if(enabled){
    //     ui->extFreeTextMsgEdit->clear();
    // }

    // show heartbeat and acks in hb mode only
    // ui->actionShow_Band_Heartbeats_and_ACKs->setChecked(enabled);
    // ui->actionShow_Band_Heartbeats_and_ACKs->setVisible(true);
    // ui->actionShow_Band_Heartbeats_and_ACKs->setEnabled(false);
#endif

    // update the HB button immediately
    updateRepeatButtonDisplay();
    updateButtonDisplay();
}

void MainWindow::on_actionJS8_triggered()
{
  m_mode="FT8";
  m_nSubMode=0;
  if(ui->actionModeJS8Normal->isChecked()){
      m_nSubMode=Varicode::JS8CallNormal;
  }
  else if(ui->actionModeJS8Fast->isChecked()){
      m_nSubMode=Varicode::JS8CallFast;
  }
  else if(ui->actionModeJS8Turbo->isChecked()){
      m_nSubMode=Varicode::JS8CallTurbo;
  }
  else if(ui->actionModeJS8Ultra->isChecked()){
      m_nSubMode=Varicode::JS8CallUltra;
  }

  // Only enable heartbeat for normal mode
  ui->actionModeJS8HB->setEnabled(m_nSubMode == Varicode::JS8CallNormal);
  prepareHeartbeatMode(ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked());

  //if(m_nSubMode != Varicode::JS8CallNormal){
  //  ui->actionModeJS8HB->setChecked(false);
  //}

  auto modeText = currentMode();
  if(ui->actionModeAutoreply->isChecked()){
      modeText += QString("+AUTO");
  }
  if(ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked()){
      if(ui->actionHeartbeatAcknowledgements->isChecked()){
          modeText += QString("+HB+ACK");
      } else {
          modeText += QString("+HB");
      }
  }

  ui->modeButton->setText(modeText);

  m_wideGraph->setSubMode(m_nSubMode);
  bool bVHF=m_config.enable_VHF_features();
  m_bFast9=false;
  m_bFastMode=false;
  WSPR_config(false);
  switch_mode (Modes::JS8);
  m_modeTx="FT8";
  m_nsps=6912;
  m_FFTSize = m_nsps / 2;
  Q_EMIT FFTSize (m_FFTSize);
  fixStop();
  setup_status_bar (bVHF);
  m_toneSpacing=0.0;                   //???
  ui->actionFT8->setChecked(true);     //???
  m_wideGraph->setMode(m_mode);
  m_wideGraph->setModeTx(m_modeTx);
  VHF_features_enabled(bVHF);
  ui->cbAutoSeq->setChecked(true);
  m_TRperiod = 0;
  if(m_nSubMode == Varicode::JS8CallNormal){
    m_TRperiod = JS8A_TX_SECONDS;
  }
  else if(m_nSubMode == Varicode::JS8CallFast){
    m_TRperiod = JS8B_TX_SECONDS;
  }
  else if(m_nSubMode == Varicode::JS8CallTurbo){
    m_TRperiod = JS8C_TX_SECONDS;
  }
  else if(m_nSubMode == Varicode::JS8CallUltra){
    m_TRperiod = JS8D_TX_SECONDS;
  }
  m_fastGraph->hide();
  m_wideGraph->show();
  ui->decodedTextLabel2->setText("  UTC   dB   DT Freq    Message");
  m_wideGraph->setPeriod(m_TRperiod,m_nsps);
  m_modulator->setTRPeriod(m_TRperiod); // TODO - not thread safe
  m_detector->setTRPeriod(m_TRperiod);  // TODO - not thread safe
  ui->label_7->setText("Rx Frequency");
  if(m_config.bFox()) {
    ui->label_6->setText("Stations calling DXpedition " + m_config.my_callsign());
    ui->decodedTextLabel->setText( "Call         Grid   dB  Freq   Dist Age Continent");
  } else {
    ui->label_6->setText("Band Activity");
    ui->decodedTextLabel->setText( "  UTC   dB   DT Freq    Message");
  }
  if(!bVHF) {
    displayWidgets(nWidgets("111010000100111000010000100100001"));
// Make sure that VHF contest mode is unchecked if VHF features is not enabled.
    ui->cbVHFcontest->setChecked(false);
  } else {
    displayWidgets(nWidgets("111010000100111000010000100110001"));
  }
  ui->txrb2->setEnabled(true);
  ui->txrb4->setEnabled(true);
  ui->txrb5->setEnabled(true);
  ui->txrb6->setEnabled(true);
  ui->txb2->setEnabled(true);
  ui->txb4->setEnabled(true);
  ui->txb5->setEnabled(true);
  ui->txb6->setEnabled(true);
  ui->txFirstCheckBox->setEnabled(true);
  ui->cbAutoSeq->setEnabled(true);

  updateTextDisplay();
  refreshTextDisplay();
  statusChanged();
}

void MainWindow::switch_mode (Mode mode)
{
  m_fastGraph->setMode(m_mode);
  m_config.frequencies ()->filter (m_config.region (), mode);

#if 0
  auto const& row = m_config.frequencies ()->best_working_frequency (m_freqNominal);
  if (row >= 0) {
    ui->bandComboBox->setCurrentIndex (row);
    on_bandComboBox_activated (row);
  }
#endif

  ui->rptSpinBox->setSingleStep(1);
  ui->rptSpinBox->setMinimum(-50);
  ui->rptSpinBox->setMaximum(49);
  ui->sbFtol->values ({10, 20, 50, 100, 200, 500, 1000});
  if(m_mode=="MSK144") {
    ui->RxFreqSpinBox->setMinimum(1400);
    ui->RxFreqSpinBox->setMaximum(1600);
    ui->RxFreqSpinBox->setSingleStep(25);
  } else {
    ui->RxFreqSpinBox->setMinimum(0);
    ui->RxFreqSpinBox->setMaximum(5000);
    ui->RxFreqSpinBox->setSingleStep(1);
  }
  m_bVHFwarned=false;
  bool b=m_mode=="FreqCal";
  ui->tabWidget->setVisible(!b);
  if(b) {
    ui->DX_controls_widget->setVisible(false);
    ui->decodedTextBrowser2->setVisible(false);
    ui->decodedTextLabel2->setVisible(false);
    ui->label_6->setVisible(false);
    ui->label_7->setVisible(false);
  }
}

void MainWindow::WSPR_config(bool b)
{
  ui->decodedTextBrowser2->setVisible(!b);
  ui->decodedTextLabel2->setVisible(!b and ui->cbMenus->isChecked());
  ui->controls_stack_widget->setCurrentIndex (b && m_mode != "Echo" ? 1 : 0);
  //ui->QSO_controls_widget->setVisible (!b);
  //ui->DX_controls_widget->setVisible (!b);
  ui->WSPR_controls_widget->setVisible (b);
  ui->label_6->setVisible(!b and ui->cbMenus->isChecked());
  ui->label_7->setVisible(!b and ui->cbMenus->isChecked());
  //ui->logQSOButton->setVisible(!b);
  ui->DecodeButton->setEnabled(!b);
  if(b and (m_mode!="Echo")) {
    QString t="UTC    dB   DT     Freq     Drift  Call          Grid    dBm    ";
    if(m_config.miles()) t += " mi";
    if(!m_config.miles()) t += " km";
    ui->decodedTextLabel->setText(t);
    if (m_config.is_transceiver_online ()) {
      Q_EMIT m_config.transceiver_tx_frequency (0); // turn off split
    }
    m_bSimplex = true;
  } else {
    ui->decodedTextLabel->setText("UTC   dB   DT Freq    Message");
    m_bSimplex = false;
  }
  enable_DXCC_entity (m_config.DXCC ());  // sets text window proportions and (re)inits the logbook
}

void MainWindow::fast_config(bool b)
{
  m_bFastMode=b;
  ui->TxFreqSpinBox->setEnabled(!b);
  ui->sbTR->setVisible(b);
  if(b and (m_bFast9 or m_mode=="MSK144" or m_mode=="ISCAT")) {
    m_wideGraph->hide();
    m_fastGraph->show();
  } else {
    m_wideGraph->show();
    m_fastGraph->hide();
  }
}

void MainWindow::on_TxFreqSpinBox_valueChanged(int n)
{
  m_wideGraph->setTxFreq(n);
//  if (ui->cbHoldTxFreq->isChecked ()) ui->RxFreqSpinBox->setValue(n);
  if(m_mode!="MSK144") {
    Q_EMIT transmitFrequency (n - m_XIT);
  }
  statusUpdate ();
}

void MainWindow::on_RxFreqSpinBox_valueChanged(int n)
{
  m_wideGraph->setRxFreq(n);
  if (m_mode == "FreqCal") {
    setRig ();
  }
  statusUpdate ();
}

void MainWindow::on_actionQuickDecode_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000001;
}

void MainWindow::on_actionMediumDecode_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000002;
}

void MainWindow::on_actionDeepDecode_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000003;
}

void MainWindow::on_actionDeepestDecode_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000004;
}

void MainWindow::on_actionInclude_averaging_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000010;
}

void MainWindow::on_actionInclude_correlation_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000020;
}

void MainWindow::on_actionEnable_AP_DXcall_toggled (bool checked)
{
  m_ndepth ^= (-checked ^ m_ndepth) & 0x00000040;
}

void MainWindow::on_actionErase_ALL_TXT_triggered()          //Erase ALL.TXT
{
  int ret = MessageBox::query_message (this, tr ("Confirm Erase"),
                                         tr ("Are you sure you want to erase file ALL.TXT?"));
  if(ret==MessageBox::Yes) {
    QFile f {m_config.writeable_data_dir ().absoluteFilePath ("ALL.TXT")};
    f.remove();
    m_RxLog=1;
  }
}

void MainWindow::on_actionErase_FoxQSO_txt_triggered()
{
  int ret = MessageBox::query_message(this, tr("Confirm Erase"),
                  tr("Are you sure you want to erase file FoxQSO.txt?"));
  if(ret==MessageBox::Yes) {
    QFile f{m_config.writeable_data_dir().absoluteFilePath("FoxQSO.txt")};
    f.remove();
  }
}

void MainWindow::on_actionErase_js8call_log_adi_triggered()
{
  int ret = MessageBox::query_message (this, tr ("Confirm Erase"),
                                       tr ("Are you sure you want to erase file js8call_log.adi?"));
  if(ret==MessageBox::Yes) {
    QFile f {m_config.writeable_data_dir ().absoluteFilePath ("js8call_log.adi")};
    f.remove();

    m_logBook.init();
  }
}

void MainWindow::on_actionOpen_log_directory_triggered ()
{
  QDesktopServices::openUrl (QUrl::fromLocalFile (m_config.writeable_data_dir ().absolutePath ()));
}

void MainWindow::on_actionOpen_Save_Directory_triggered(){
    QDesktopServices::openUrl (QUrl::fromLocalFile (m_config.save_directory().absolutePath ()));
}

void MainWindow::on_bandComboBox_currentIndexChanged (int index)
{
  auto const& frequencies = m_config.frequencies ();
  auto const& source_index = frequencies->mapToSource (frequencies->index (index, FrequencyList_v2::frequency_column));
  Frequency frequency {m_freqNominal};
  if (source_index.isValid ())
    {
      frequency = frequencies->frequency_list ()[source_index.row ()].frequency_;
    }

  // Lookup band
  auto const& band  = m_config.bands ()->find (frequency);
  if (!band.isEmpty ())
    {
      ui->bandComboBox->lineEdit ()->setStyleSheet ({});
      ui->bandComboBox->setCurrentText (band);
    }
  else
    {
      ui->bandComboBox->lineEdit ()->setStyleSheet ("QLineEdit {color: yellow; background-color : red;}");
      ui->bandComboBox->setCurrentText (m_config.bands ()->oob ());
    }
  displayDialFrequency ();
}

void MainWindow::on_bandComboBox_activated (int index)
{
  auto const& frequencies = m_config.frequencies ();
  auto const& source_index = frequencies->mapToSource (frequencies->index (index, FrequencyList_v2::frequency_column));
  Frequency frequency {m_freqNominal};
  if (source_index.isValid ())
    {
      frequency = frequencies->frequency_list ()[source_index.row ()].frequency_;
    }
  m_bandEdited = true;
  band_changed (frequency);
  m_wideGraph->setRxBand (m_config.bands ()->find (frequency));
}

void MainWindow::band_changed (Frequency f)
{
//  bool monitor_off=!m_monitoring;
  // Set the attenuation value if options are checked
  QString curBand = ui->bandComboBox->currentText();
  if (m_config.pwrBandTxMemory() && !m_tune) {
      if (m_pwrBandTxMemory.contains(curBand)) {
        ui->outAttenuation->setValue(m_pwrBandTxMemory[curBand].toInt());
      }
      else {
        m_pwrBandTxMemory[curBand] = ui->outAttenuation->value();
      }
  }

  if (m_bandEdited) {
    if (!m_mode.startsWith ("WSPR")) { // band hopping preserves auto Tx
      if (f + m_wideGraph->nStartFreq () > m_freqNominal + ui->TxFreqSpinBox->value ()
          || f + m_wideGraph->nStartFreq () + m_wideGraph->fSpan () <=
          m_freqNominal + ui->TxFreqSpinBox->value ()) {
//        qDebug () << "start f:" << m_wideGraph->nStartFreq () << "span:" << m_wideGraph->fSpan () << "DF:" << ui->TxFreqSpinBox->value ();
        // disable auto Tx if "blind" QSY outside of waterfall
        ui->stopTxButton->click (); // halt any transmission
        auto_tx_mode (false);       // disable auto Tx
        m_send_RR73 = false;        // force user to reassess on new band
      }
    }

    // TODO: jsherer - is this relied upon anywhere?
    //m_lastBand.clear ();
    m_bandEdited = false;

    psk_Reporter->sendReport();      // Upload any queued spots before changing band
    m_aprsClient->sendReports();
    if (!m_transmitting) monitor (true);
    if ("FreqCal" == m_mode)
      {
        m_frequency_list_fcal_iter = m_config.frequencies ()->find (f);
      }
    float r=m_freqNominal/(f+0.0001);
    if(r<0.9 or r>1.1) m_bVHFwarned=false;
    setRig (f);
    setXIT (ui->TxFreqSpinBox->value ());
//    if(monitor_off) monitor(false);
  }
}

void MainWindow::vhfWarning()
{
  MessageBox::warning_message (this, tr ("VHF features warning"),
     "VHF/UHF/Microwave features is enabled on a lower frequency band.");
  m_bVHFwarned=true;
}

void MainWindow::enable_DXCC_entity (bool /*on*/)
{
  m_logBook.init();                        // re-read the log and cty.dat files
  updateGeometry ();
}

void MainWindow::on_pbCallCQ_clicked()
{
  ui->genMsg->setText(ui->tx6->text());
  m_ntx=7;
  m_QSOProgress = CALLING;
  m_gen_message_is_cq = true;
  ui->rbGenMsg->setChecked(true);
  if(m_transmitting) m_restart=true;
  set_dateTimeQSO(-1);
}

void MainWindow::on_pbAnswerCaller_clicked()
{
  QString t=ui->tx3->text();
  int i0=t.indexOf(" R-");
  if(i0<0) i0=t.indexOf(" R+");
  t=t.mid(0,i0+1)+t.mid(i0+2,3);
  ui->genMsg->setText(t);
  m_ntx=7;
  m_QSOProgress = REPORT;
  m_gen_message_is_cq = false;
  ui->rbGenMsg->setChecked(true);
  if(m_transmitting) m_restart=true;
  set_dateTimeQSO(2);
}

void MainWindow::on_pbSendRRR_clicked()
{
  ui->genMsg->setText(ui->tx4->text());
  m_ntx=7;
  m_QSOProgress = ROGERS;
  m_gen_message_is_cq = false;
  ui->rbGenMsg->setChecked(true);
  if(m_transmitting) m_restart=true;
}

void MainWindow::on_pbAnswerCQ_clicked()
{
  ui->genMsg->setText(ui->tx1->text());
  QString t=ui->tx2->text();
  int i0=t.indexOf("/");
  int i1=t.indexOf(" ");
  if(i0>0 and i0<i1) ui->genMsg->setText(t);
  m_ntx=7;
  m_QSOProgress = REPLYING;
  m_gen_message_is_cq = false;
  ui->rbGenMsg->setChecked(true);
  if(m_transmitting) m_restart=true;
}

void MainWindow::on_pbSendReport_clicked()
{
  ui->genMsg->setText(ui->tx3->text());
  m_ntx=7;
  m_QSOProgress = ROGER_REPORT;
  m_gen_message_is_cq = false;
  ui->rbGenMsg->setChecked(true);
  if(m_transmitting) m_restart=true;
  set_dateTimeQSO(3);
}

void MainWindow::on_pbSend73_clicked()
{
  ui->genMsg->setText(ui->tx5->currentText());
  m_ntx=7;
  m_QSOProgress = SIGNOFF;
  m_gen_message_is_cq = false;
  ui->rbGenMsg->setChecked(true);
  if(m_transmitting) m_restart=true;
}

void MainWindow::on_rbGenMsg_clicked(bool checked)
{
  m_freeText=!checked;
  if(!m_freeText) {
    if(m_ntx != 7 && m_transmitting) m_restart=true;
    m_ntx=7;
    // would like to set m_QSOProgress but what to? So leave alone and
    // assume it is correct
  }
}

void MainWindow::on_rbFreeText_clicked(bool checked)
{
  m_freeText=checked;
  if(m_freeText) {
    m_ntx=8;
    // would like to set m_QSOProgress but what to? So leave alone and
    // assume it is correct. Perhaps should store old value to be
    // restored above in on_rbGenMsg_clicked
    if (m_transmitting) m_restart=true;
  }
}

void MainWindow::on_clearAction_triggered(QObject * sender){
    // TODO: jsherer - abstract this into a tableWidgetRXAllReset function
    if(sender == ui->tableWidgetRXAll){
        m_bandActivity.clear();
        clearTableWidget(ui->tableWidgetRXAll);
        resetTimeDeltaAverage();
        displayBandActivity();
    }

    // TODO: jsherer - abstract this into a tableWidgetCallsReset function
    if(sender == ui->tableWidgetCalls){
        m_callActivity.clear();
        m_heardGraphIncoming.clear();
        m_heardGraphOutgoing.clear();
        clearTableWidget((ui->tableWidgetCalls));
        createAllcallTableRows(ui->tableWidgetCalls, "");
        resetTimeDeltaAverage();
        displayCallActivity();
    }

    if(sender == ui->extFreeTextMsgEdit){
        resetMessage();
        m_lastTxMessage.clear();
    }

    if(sender == ui->textEditRX){
        // TODO: jsherer - move these
        ui->textEditRX->clear();
        m_rxFrameBlockNumbers.clear();
        m_rxActivityQueue.clear();
    }
}

void MainWindow::buildFrequencyMenu(QMenu *menu){
    auto custom = menu->addAction("Set a Custom Frequency...");

    connect(custom, &QAction::triggered, this, [this](){
        bool ok = false;
        auto currentFreq = Radio::frequency_MHz_string(dialFrequency());
        QString newFreq = QInputDialog::getText(this, tr("Set a Custom Frequency"),
                                                 tr("Frequency in MHz:"), QLineEdit::Normal,
                                                 currentFreq, &ok).toUpper().trimmed();
        if(!ok){
           return;
        }

        setRig(Radio::frequency(newFreq, 6));
    });

    menu->addSeparator();

    auto frequencies = m_config.frequencies()->frequency_list();
    qSort(frequencies.begin(), frequencies.end(), [](FrequencyList_v2::Item &a, FrequencyList_v2::Item &b) {
        return a.frequency_ < b.frequency_;
    });

    foreach(auto f, frequencies){
        auto freq = Radio::pretty_frequency_MHz_string(f.frequency_);
        auto const& band = m_config.bands ()->find (f.frequency_);

        auto a = menu->addAction(QString("%1:%2%2%3 MHz").arg(band).arg(QString(" ").repeated(5-band.length())).arg(freq));
        connect(a, &QAction::triggered, this, [this, f](){
            setRig(f.frequency_);
        });
    }
}

void MainWindow::buildHeartbeatMenu(QMenu *menu){
    if(m_hbInterval > 0){
        auto startStop = menu->addAction(ui->hbMacroButton->isChecked() ? "Stop Heartbeat Timer" : "Start Heartbeat Timer");
        connect(startStop, &QAction::triggered, this, [this](){ ui->hbMacroButton->toggle(); });
        menu->addSeparator();
    }

    buildRepeatMenu(menu, ui->hbMacroButton, false, &m_hbInterval);

    menu->addSeparator();
    auto now = menu->addAction("Send Heartbeat Now");
    connect(now, &QAction::triggered, this, &MainWindow::sendHeartbeat);
}

void MainWindow::buildCQMenu(QMenu *menu){
    if(m_cqInterval > 0){
        auto startStop = menu->addAction(ui->cqMacroButton->isChecked() ? "Stop CQ Timer" : "Start CQ Timer");
        connect(startStop, &QAction::triggered, this, [this](){ ui->cqMacroButton->toggle(); });
        menu->addSeparator();
    }

    buildRepeatMenu(menu, ui->cqMacroButton, true, &m_cqInterval);

    menu->addSeparator();
    auto now = menu->addAction("Send CQ Now");
    connect(now, &QAction::triggered, this, [this](){ sendCQ(true); });
}

void MainWindow::buildRepeatMenu(QMenu *menu, QPushButton * button, bool isLowInterval, int * interval){
    QList<QPair<QString, int>> items = {
        {"On demand / do not repeat",  0},
        {"Repeat every 1 minute",      1},
        {"Repeat every 5 minutes",     5},
        {"Repeat every 10 minutes",   10},
        {"Repeat every 15 minutes",   15},
        {"Repeat every 30 minutes",   30},
        {"Repeat every 60 minutes",   60},
        {"Repeat every N minutes (Custom Interval)", -1}, // this needs to be last because of isSet bool
    };

    if(isLowInterval){
        items.removeAt(5); // remove the thirty minute interval
        items.removeAt(5); // remove the sixty minute interval
    } else {
        items.removeAt(1); // remove the one minute interval
        items.removeAt(1); // remove the five minute interval
    }

    auto customFormat = QString("Repeat every %1 minutes (Custom Interval)");

    QActionGroup * group = new QActionGroup(menu);

    bool isSet = false;
    foreach(auto pair, items){
        int minutes = pair.second;
        bool isMatch = *interval == minutes;
        bool isCustom = (minutes == -1 && isSet == false);
        if(isMatch){
            isSet = true;
        }

        auto text = pair.first;
        if(isCustom){
            text = QString(customFormat).arg(*interval);
        }

        auto action = menu->addAction(text);
        action->setData(minutes);
        action->setCheckable(true);
        action->setChecked(isMatch || isCustom);
        group->addAction(action);

        connect(action, &QAction::toggled, this, [this, action, customFormat, minutes, interval, button](bool checked){
            int min = minutes;

            if(checked){

                if(minutes == -1){
                    bool ok = false;
                    min = QInputDialog::getInt(this, "Repeat every N minutes", "Minutes", 0, 1, 1440, 1, &ok);
                    if(!ok){
                        return;
                    }
                    action->setText(QString(customFormat).arg(*interval));
                }

                *interval = min;

                if(min > 0){
                    // force a re-toggle
                    button->setChecked(false);
                }
                button->setChecked(min > 0);
            }
        });
    }
}

void MainWindow::sendHeartbeat(){
    QString mycall = m_config.my_callsign();
    QString mygrid = m_config.my_grid().left(4);

    QStringList parts;

    parts.append(QString("%1:").arg(mycall));
    
    auto flags = generateStatusFlags();
    if(flags.isEmpty() || flags.first() != "HB"){
        parts.append("HB");
    }
    parts.append(flags);
    parts.append(mygrid);

    QString message = parts.join(" ").trimmed();

    auto f = findFreeFreqOffset(500, 1000, 50);

    if(currentFreqOffset() <= 1000){
        f = currentFreqOffset();
    }
    else if(m_config.heartbeat_anywhere()){
        f = -1;
    }

    enqueueMessage(PriorityLow + 1, message, f, [this](){ /* */ });
    processTxQueue();
}

#define SEND_SNR_IN_ACK 1
void MainWindow::sendHeartbeatAck(QString to, int snr, QString extra){
#if SEND_SNR_IN_ACK
    auto message = QString("%1 ACK %2 %3").arg(to).arg(Varicode::formatSNR(snr)).arg(extra).trimmed();
#else
    auto message = QString("%1 ACK %2").arg(to).arg(extra).trimmed();
#endif

    auto f = m_config.heartbeat_anywhere() ? -1 : findFreeFreqOffset(500, 1000, 50);

    enqueueMessage(PriorityLow + 1, message, f, [this](){ /* */ });
    processTxQueue();
}

void MainWindow::on_hbMacroButton_toggled(bool checked){
    if(checked){
        clearCallsignSelected();

        if(m_hbInterval){
            m_nextHeartbeat = nextTransmitCycle().addSecs(m_hbInterval * 60);

            if(!repeatTimer.isActive()){
                repeatTimer.start();
            }

        } else {
            sendHeartbeat();

            // make this button emulate a single press button
            ui->hbMacroButton->setChecked(false);
        }
    } else {
        m_nextHeartbeat = QDateTime{};
    }

    updateRepeatButtonDisplay();
}

void MainWindow::on_hbMacroButton_clicked(){
}

void MainWindow::sendCQ(bool repeat){
    auto message = m_config.cq_message();
    if(message.isEmpty()){
        QString mygrid = m_config.my_grid().left(4);
        message = QString("CQCQCQ %1").arg(mygrid).trimmed();
    }

    clearCallsignSelected();

    addMessageText(replaceMacros(message, buildMacroValues(), true));

    if(repeat || m_config.transmit_directed()) toggleTx(true);
}

void MainWindow::on_cqMacroButton_toggled(bool checked){
    if(checked){
        clearCallsignSelected();

        if(m_cqInterval){
            m_nextCQ = nextTransmitCycle().addSecs(m_cqInterval * 60);

            if(!repeatTimer.isActive()){
                repeatTimer.start();
            }

        } else {
            sendCQ();

            // make this button emulate a single press button
            ui->cqMacroButton->setChecked(false);
        }
    } else {
        m_nextCQ= QDateTime{};
    }

    updateRepeatButtonDisplay();
}

void MainWindow::on_cqMacroButton_clicked(){
}

void MainWindow::on_replyMacroButton_clicked(){
    QString call = callsignSelected();
    if(call.isEmpty()){
        return;
    }

    auto message = m_config.reply_message();
    message = replaceMacros(message, buildMacroValues(), true);
    addMessageText(QString("%1 %2").arg(call).arg(message));

    if(m_config.transmit_directed()) toggleTx(true);
}

void MainWindow::on_snrMacroButton_clicked(){
    QString call = callsignSelected();
    if(call.isEmpty()){
        return;
    }

    auto now = DriftingDateTime::currentDateTimeUtc();
    int callsignAging = m_config.callsign_aging();
    if(!m_callActivity.contains(call)){
        return;
    }

    auto cd = m_callActivity[call];
    if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
        return;
    }

    auto snr = Varicode::formatSNR(cd.snr);

    addMessageText(QString("%1 SNR %2").arg(call).arg(snr));

    if(m_config.transmit_directed()) toggleTx(true);
}

void MainWindow::on_infoMacroButton_clicked(){
    QString info = m_config.my_info();
    if(info.isEmpty()){
        return;
    }

    addMessageText(QString("INFO %1").arg(replaceMacros(info, buildMacroValues(), true)));

    if(m_config.transmit_directed()) toggleTx(true);
}

void MainWindow::setShowColumn(QString tableKey, QString columnKey, bool value){
    m_showColumnsCache[tableKey + columnKey] = QVariant(value);
    displayBandActivity();
    displayCallActivity();
}

bool MainWindow::showColumn(QString tableKey, QString columnKey, bool default_){
    return m_showColumnsCache.value(tableKey + columnKey, QVariant(default_)).toBool();
}

void MainWindow::buildShowColumnsMenu(QMenu *menu, QString tableKey){
    QList<QPair<QString, QString>> columnKeys = {
        {"Frequency Offset", "offset"},
        {"Last heard timestamp", "timestamp"},
        {"SNR", "snr"},
        {"Time Delta", "tdrift"},
        {"Mode Speed", "mode"},
    };

    QMap<QString, bool> defaultOverride = {
        {"mode", false},
        {"tdrift", false},
        {"grid", false},
        {"distance", false}
    };

    if(tableKey == "call"){
        columnKeys.prepend({"Callsign", "callsign"});
        columnKeys.append({
          {"Grid Locator", "grid"},
          {"Distance", "distance"},
          {"Worked Before", "log"},
          {"Logged Name", "logName"},
          {"Logged Comment", "logComment"},
        });
    }

    columnKeys.prepend({"Show Column Labels", "labels"});

    bool first = true;
    foreach(auto p, columnKeys){
        auto columnLabel = p.first;
        auto columnKey = p.second;

        auto a = menu->addAction(columnLabel);
        a->setCheckable(true);


        bool showByDefault = true;
        if(defaultOverride.contains(columnKey)){
            showByDefault = defaultOverride[columnKey];
        }
        a->setChecked(showColumn(tableKey, columnKey, showByDefault));

        connect(a, &QAction::triggered, this, [this, a, tableKey, columnKey](){
            setShowColumn(tableKey, columnKey, a->isChecked());
        });

        if(first){
            menu->addSeparator();
            first = false;
        }
    }
}

void MainWindow::setSortBy(QString key, QString value){
    m_sortCache[key] = QVariant(value);
    displayBandActivity();
    displayCallActivity();
}

QString MainWindow::getSortBy(QString key, QString defaultValue){
    return m_sortCache.value(key, QVariant(defaultValue)).toString();
}

void MainWindow::buildSortByMenu(QMenu * menu, QString key, QString defaultValue, QList<QPair<QString, QString>> values){
    auto currentSortBy = getSortBy(key, defaultValue);

    QActionGroup * g = new QActionGroup(menu);
    g->setExclusive(true);

    foreach(auto p, values){
        auto k = p.first;
        auto v = p.second;
        auto a = menu->addAction(k);
        a->setCheckable(true);
        a->setChecked(v == currentSortBy);
        a->setActionGroup(g);

        connect(a, &QAction::triggered, this, [this, a, key, v](){
            if(a->isChecked()){
                setSortBy(key, v);
            }
        });
    }
}

void MainWindow::buildBandActivitySortByMenu(QMenu * menu){
    buildSortByMenu(menu, "bandActivity", "offset", {
        {"Frequency offset", "offset"},
        {"Last heard timestamp (oldest first)", "timestamp"},
        {"Last heard timestamp (recent first)", "-timestamp"},
        {"SNR (weakest first)", "snr"},
        {"SNR (strongest first)", "-snr"}
    });
}

void MainWindow::buildCallActivitySortByMenu(QMenu * menu){
    buildSortByMenu(menu, "callActivity", "callsign", {
        {"Callsign", "callsign"},
        {"Callsigns Replied (recent first)", "ackTimestamp"},
        {"Frequency offset", "offset"},
        {"Distance (closest first)", "distance"},
        {"Distance (farthest first)", "-distance"},
        {"Last heard timestamp (oldest first)", "timestamp"},
        {"Last heard timestamp (recent first)", "-timestamp"},
        {"SNR (weakest first)", "snr"},
        {"SNR (strongest first)", "-snr"}
    });
}

void MainWindow::buildQueryMenu(QMenu * menu, QString call){
    bool isAllCall = isAllCallIncluded(call);

    // for now, we're going to omit displaying the call...delete this if we want the other functionality
    call = "";

    auto grid = m_config.my_grid();

    bool emptyInfo = m_config.my_info().isEmpty();
    bool emptyGrid = m_config.my_grid().isEmpty();

    auto callAction = menu->addAction(QString("Send a directed message to selected callsign"));
    connect(callAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 ").arg(selectedCall), true);
    });

    menu->addSeparator();

    auto sendReplyAction = menu->addAction(QString("%1 Reply - Send reply message to selected callsign").arg(call).trimmed());
    connect(sendReplyAction, &QAction::triggered, this, [this](){
        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        auto message = m_config.reply_message();
        message = replaceMacros(message, buildMacroValues(), true);
        addMessageText(QString("%1 %2").arg(selectedCall).arg(message), true);
    });

    auto sendSNRAction = menu->addAction(QString("%1 SNR - Send a signal report to the selected callsign").arg(call).trimmed());
    sendSNRAction->setEnabled(m_callActivity.contains(callsignSelected()));
    connect(sendSNRAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        if(!m_callActivity.contains(selectedCall)){
            return;
        }

        auto d = m_callActivity[selectedCall];
        addMessageText(QString("%1 SNR %2").arg(selectedCall).arg(Varicode::formatSNR(d.snr)), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto infoAction = menu->addAction(QString("%1 INFO - Send my station information").arg(call).trimmed());
    infoAction->setDisabled(emptyInfo);
    connect(infoAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 INFO %2").arg(selectedCall).arg(m_config.my_info()), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });


    auto gridAction = menu->addAction(QString("%1 GRID %2 - Send my current station Maidenhead grid locator").arg(call).arg(grid).trimmed());
    gridAction->setDisabled(emptyGrid);
    connect(gridAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 GRID %2").arg(selectedCall).arg(m_config.my_grid()), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    menu->addSeparator();

    auto snrQueryAction = menu->addAction(QString("%1 SNR? - What is my signal report?").arg(call).trimmed());
    snrQueryAction->setDisabled(isAllCall);
    connect(snrQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 SNR?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto infoQueryAction = menu->addAction(QString("%1 INFO? - What is your station information?").arg(call).trimmed());
    infoQueryAction->setDisabled(isAllCall);
    connect(infoQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 INFO?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto gridQueryAction = menu->addAction(QString("%1 GRID? - What is your current grid locator?").arg(call).trimmed());
    gridQueryAction->setDisabled(isAllCall);
    connect(gridQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 GRID?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto stationIdleQueryAction = menu->addAction(QString("%1 STATUS? - What is the status of your station (auto, hb, version, etc)?").arg(call).trimmed());
    stationIdleQueryAction->setDisabled(isAllCall);
    connect(stationIdleQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 STATUS?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto heardQueryAction = menu->addAction(QString("%1 HEARING? - What are the stations are you hearing? (Top 4 ranked by most recently heard)").arg(call).trimmed());
    heardQueryAction->setDisabled(isAllCall);
    connect(heardQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 HEARING?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

#if 0
    auto retransmitAction = menu->addAction(QString("%1|[MESSAGE] - Please ACK and retransmit the following message").arg(call).trimmed());
    retransmitAction->setDisabled(isAllCall);
    connect(retransmitAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1|[MESSAGE]").arg(selectedCall), true, true);
    });
#endif

    auto alertAction = menu->addAction(QString("%1>[MESSAGE] - Please relay this message to its destination").arg(call).trimmed());
    alertAction->setDisabled(isAllCall);
    connect(alertAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1>[MESSAGE]").arg(selectedCall), true, true);
    });

    auto msgAction = menu->addAction(QString("%1 MSG [MESSAGE] - Please store this message in your inbox").arg(call).trimmed());
    msgAction->setDisabled(isAllCall);
    connect(msgAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 MSG [MESSAGE]").arg(selectedCall), true, true);
    });

    auto msgToAction = menu->addAction(QString("%1 MSG TO:[CALLSIGN] [MESSAGE] - Please store this message at your station for later retreival by [CALLSIGN]").arg(call).trimmed());
    msgToAction->setDisabled(isAllCall);
    connect(msgToAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 MSG TO:[CALLSIGN] [MESSAGE]").arg(selectedCall), true, true);
    });

    auto qsoQueryAction = menu->addAction(QString("%1 QUERY CALL [CALLSIGN]? - Please acknowledge you can communicate directly with [CALLSIGN]").arg(call).trimmed());
    connect(qsoQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 QUERY CALL [CALLSIGN]?").arg(selectedCall), true, true);
    });

    auto qsoQueryMsgsAction = menu->addAction(QString("%1 QUERY MSGS - Do you have any messages for me?").arg(call).trimmed());
    connect(qsoQueryMsgsAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 QUERY MSGS").arg(selectedCall), true, true);
    });

    auto qsoQueryMsgAction = menu->addAction(QString("%1 QUERY MSG [ID] - Please deliver the complete message identified by ID").arg(call).trimmed());
    connect(qsoQueryMsgAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 QUERY MSG [ID]").arg(selectedCall), true, true);
    });

    menu->addSeparator();

    auto agnAction = menu->addAction(QString("%1 AGN? - Please repeat your last transmission").arg(call).trimmed());
    connect(agnAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 AGN?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto qslQueryAction = menu->addAction(QString("%1 QSL? - Did you receive my last transmission?").arg(call).trimmed());
    connect(qslQueryAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 QSL?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto qslAction = menu->addAction(QString("%1 QSL - I confirm I received your last transmission").arg(call).trimmed());
    connect(qslAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 QSL").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto yesAction = menu->addAction(QString("%1 YES - I confirm your last inquiry").arg(call).trimmed());
    connect(yesAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 YES").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto noAction = menu->addAction(QString("%1 NO - I do not confirm your last inquiry").arg(call).trimmed());
    connect(noAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 NO").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto hwAction = menu->addAction(QString("%1 HW CPY? - How do you copy?").arg(call).trimmed());
    connect(hwAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 HW CPY?").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto rrAction = menu->addAction(QString("%1 RR - Roger. Received. I copy.").arg(call).trimmed());
    connect(rrAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 RR").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto fbAction = menu->addAction(QString("%1 FB - Fine Business").arg(call).trimmed());
    connect(fbAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 FB").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto sevenThreeAction = menu->addAction(QString("%1 73 - I send my best regards").arg(call).trimmed());
    connect(sevenThreeAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 73").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });

    auto skAction = menu->addAction(QString("%1 SK - End of contact").arg(call).trimmed());
    connect(skAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 SK").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });


    auto ditDitAction = menu->addAction(QString("%1 DIT DIT - End of contact / Two bits").arg(call).trimmed());
    connect(ditDitAction, &QAction::triggered, this, [this](){

        QString selectedCall = callsignSelected();
        if(selectedCall.isEmpty()){
            return;
        }

        addMessageText(QString("%1 DIT DIT").arg(selectedCall), true);

        if(m_config.transmit_directed()) toggleTx(true);
    });
}

void MainWindow::buildRelayMenu(QMenu *menu){
    auto now = DriftingDateTime::currentDateTimeUtc();
    int callsignAging = m_config.callsign_aging();
    foreach(auto cd, m_callActivity.values()){
        if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
            continue;
        }

        menu->addAction(buildRelayAction(cd.call));
    }
}

QAction* MainWindow::buildRelayAction(QString call){
    QAction *a = new QAction(call, nullptr);
    connect(a, &QAction::triggered, this, [this, call](){
        prependMessageText(QString("%1>").arg(call));
    });
    return a;
}

void MainWindow::buildEditMenu(QMenu *menu, QTextEdit *edit){
    bool hasSelection = !edit->textCursor().selectedText().isEmpty();

    auto cut = menu->addAction("Cu&t");
    cut->setEnabled(hasSelection && !edit->isReadOnly());
    connect(edit, &QTextEdit::copyAvailable, this, [this, edit, cut](bool copyAvailable){
        cut->setEnabled(copyAvailable && !edit->isReadOnly());
    });
    connect(cut, &QAction::triggered, this, [this, edit](){
        edit->copy();
        edit->textCursor().removeSelectedText();
    });

    auto copy = menu->addAction("&Copy");
    copy->setEnabled(hasSelection);
    connect(edit, &QTextEdit::copyAvailable, this, [this, copy](bool copyAvailable){
        copy->setEnabled(copyAvailable);
    });
    connect(copy, &QAction::triggered, edit, &QTextEdit::copy);

    auto paste = menu->addAction("&Paste");
    paste->setEnabled(edit->canPaste());
    connect(paste, &QAction::triggered, edit, &QTextEdit::paste);
}

QMap<QString, QString> MainWindow::buildMacroValues(){
    QMap<QString, QString> values = {
        {"<MYCALL>", m_config.my_callsign()},
        {"<MYGRID4>", m_config.my_grid().left(4)},
        {"<MYGRID12>", m_config.my_grid().left(12)},
        {"<MYINFO>", m_config.my_info()},
        {"<MYCQ>", m_config.cq_message()},
        {"<MYREPLY>", m_config.reply_message()},
        {"<MYSTATUS>", generateStatus()},
    };

    auto selectedCall = callsignSelected();
    if(m_callActivity.contains(selectedCall)){
        auto cd = m_callActivity[selectedCall];

        values["<CALL>"] = selectedCall;
        values["<TDELTA>"] = QString("%1 ms").arg((int)(1000*cd.tdrift));

        if(cd.snr > -31){
            values["<SNR>"] = Varicode::formatSNR(cd.snr);
        }
    }

    // these macros can have recursive macros
    values["<MYINFO>"]   = replaceMacros(values["<MYINFO>"], values, false);
    values["<MYCQ>"]    = replaceMacros(values["<MYCQ>"], values, false);
    values["<MYREPLY>"] = replaceMacros(values["<MYREPLY>"], values, false);

    return values;
}

QString MainWindow::replaceMacros(QString const &text, QMap<QString, QString> values, bool prune){
    QString output = QString(text);

    foreach(auto key, values.keys()){
        output = output.replace(key, values[key].toUpper());
    }

    if(prune){
        output = output.replace(QRegularExpression("[<](?:[^>]+)[>]"), "");
    }

    return output;
}

void MainWindow::buildSuggestionsMenu(QMenu *menu, QTextEdit *edit, const QPoint &point){
    if(!m_config.spellcheck()){
        return;
    }

    bool found = false;

    auto c = edit->cursorForPosition(point);
    if(c.charFormat().underlineStyle() != QTextCharFormat::WaveUnderline){
        return;
    }

    c.movePosition(QTextCursor::StartOfWord);
    c.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);

    auto word = c.selectedText().toUpper().trimmed();
    if(word.isEmpty()){
        return;
    }

    QStringList suggestions = JSCChecker::suggestions(word, 5, &found);
    if(suggestions.isEmpty() && !found){
        return;
    }

    if(suggestions.isEmpty()){
        auto a = menu->addAction("No Suggestions");
        a->setDisabled(true);
    } else {
        foreach(auto suggestion, suggestions){
            auto a = menu->addAction(suggestion);

            connect(a, &QAction::triggered, this, [this, edit, point, suggestion](){
                auto c = edit->cursorForPosition(point);
                c.select(QTextCursor::WordUnderCursor);
                c.insertText(suggestion);
            });
        }
    }

    menu->addSeparator();
}

void MainWindow::buildSavedMessagesMenu(QMenu *menu){
    auto values = buildMacroValues();

    foreach(QString macro, m_config.macros()->stringList()){
        QAction *action = menu->addAction(replaceMacros(macro, values, false));
        connect(action, &QAction::triggered, this, [this, macro](){
            auto values = buildMacroValues();
            addMessageText(replaceMacros(macro, values, true));

            if(m_config.transmit_directed()) toggleTx(true);
        });
    }

    menu->addSeparator();

    auto editAction = new QAction(QString("&Edit Saved Messages"), menu);
    menu->addAction(editAction);
    connect(editAction, &QAction::triggered, this, [this](){
        openSettings(5);
    });

    auto saveAction = new QAction(QString("&Save Current Message"), menu);
    saveAction->setDisabled(ui->extFreeTextMsgEdit->toPlainText().isEmpty());
    menu->addAction(saveAction);
    connect(saveAction, &QAction::triggered, this, [this](){
        auto macros = m_config.macros();
        if(macros->insertRow(macros->rowCount())){
            auto index = macros->index(macros->rowCount()-1);
            macros->setData(index, ui->extFreeTextMsgEdit->toPlainText());
            writeSettings();
        }
    });
}

void MainWindow::on_queryButton_pressed(){
    QMenu *menu = ui->queryButton->menu();
    if(!menu){
        menu = new QMenu(ui->queryButton);
    }
    menu->clear();

    buildQueryMenu(menu, callsignSelected());

    ui->queryButton->setMenu(menu);
    ui->queryButton->showMenu();
}

void MainWindow::on_macrosMacroButton_pressed(){
    QMenu *menu = ui->macrosMacroButton->menu();
    if(!menu){
        menu = new QMenu(ui->macrosMacroButton);
    }
    menu->clear();

    buildSavedMessagesMenu(menu);

    ui->macrosMacroButton->setMenu(menu);
    ui->macrosMacroButton->showMenu();
}

void MainWindow::on_deselectButton_pressed(){
    clearCallsignSelected();
}

void MainWindow::on_tableWidgetRXAll_cellClicked(int /*row*/, int /*col*/){
    ui->tableWidgetCalls->selectionModel()->select(
        ui->tableWidgetCalls->selectionModel()->selection(),
        QItemSelectionModel::Deselect);

    displayCallActivity();
}

void MainWindow::on_tableWidgetRXAll_cellDoubleClicked(int row, int col){
    on_tableWidgetRXAll_cellClicked(row, col);

    // TODO: jsherer - could also parse the messages for the last callsign?
    auto item = ui->tableWidgetRXAll->item(row, 0);
    int offset = item->text().replace(" Hz", "").toInt();

    // switch to the offset of this row
    setFreqOffsetForRestore(offset, false);

    // TODO: prompt mode switch?

    // print the history in the main window...
    int activityAging = m_config.activity_aging();
    QDateTime now = DriftingDateTime::currentDateTimeUtc();
    QDateTime firstActivity = now;
    QString activityText;
    bool isLast = false;
    foreach(auto d, m_bandActivity[offset]){
        if(activityAging && d.utcTimestamp.secsTo(now)/60 >= activityAging){
            continue;
        }
        if(activityText.isEmpty()){
            firstActivity = d.utcTimestamp;
        }
        activityText.append(d.text);

        isLast = (d.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast;
        if(isLast){
            activityText = QString("%1 %2 ").arg(Varicode::rstrip(activityText)).arg(m_config.eot());
        }
    }
    if(!activityText.isEmpty()){
        displayTextForFreq(activityText, offset, firstActivity, false, true, isLast);
    }
}

void MainWindow::on_tableWidgetRXAll_selectionChanged(const QItemSelection &/*selected*/, const QItemSelection &/*deselected*/){
    on_extFreeTextMsgEdit_currentTextChanged(ui->extFreeTextMsgEdit->toPlainText());

    auto selectedCall = callsignSelected();
    if(selectedCall != m_prevSelectedCallsign){
        callsignSelectedChanged(m_prevSelectedCallsign, selectedCall);
    }
}

QString MainWindow::generateCallDetail(QString selectedCall){
    if(selectedCall.isEmpty() || selectedCall.contains("@")){
        return "";
    }

    // heard detail
    QString hearing = m_heardGraphOutgoing.value(selectedCall).values().join(", ");
    QString heardby = m_heardGraphIncoming.value(selectedCall).values().join(", ");
    QStringList detail = {
        QString("<h1>%1</h1>").arg(selectedCall.toHtmlEscaped()),
        QString("<p><strong>HEARING</strong>: %1</p>").arg(hearing.toHtmlEscaped()),
        QString("<p><strong>HEARD BY</strong>: %1</p>").arg(heardby.toHtmlEscaped()),
    };

    return detail.join("\n");
}

void MainWindow::on_tableWidgetCalls_cellClicked(int /*row*/, int /*col*/){
    ui->tableWidgetRXAll->selectionModel()->select(
        ui->tableWidgetRXAll->selectionModel()->selection(),
        QItemSelectionModel::Deselect);

    displayBandActivity();
}

void MainWindow::on_tableWidgetCalls_cellDoubleClicked(int row, int col){
    on_tableWidgetCalls_cellClicked(row, col);

    auto call = callsignSelected();
    addMessageText(call);

#if SHOW_MESSAGE_HISTORY_ON_DOUBLECLICK
    if(m_rxInboxCountCache.value(call, 0) > 0){

        // TODO:
        // CommandDetail d = m_rxCallsignInboxCountCache[call].first();
        // m_rxCallsignInboxCountCache[call].removeFirst();
        //
        // processAlertReplyForCommand(d, d.relayPath, d.cmd);

        Inbox i(inboxPath());
        if(i.open()){
            QList<Message> msgs;
            foreach(auto pair, i.values("UNREAD", "$.params.FROM", call, 0, 1000)){
                msgs.append(pair.second);
            }

            auto mw = new MessageWindow(this);
            mw->populateMessages(msgs);
            mw->show();

            auto pair = i.firstUnreadFrom(call);
            auto id = pair.first;
            auto msg = pair.second;
            auto params = msg.params();

            CommandDetail d;
            d.cmd = params.value("CMD").toString();
            d.extra = params.value("EXTRA").toString();
            d.freq = params.value("OFFSET").toInt();
            d.from = params.value("FROM").toString();
            d.grid = params.value("GRID").toString();
            d.relayPath = params.value("PATH").toString();
            d.snr = params.value("SNR").toInt();
            d.tdrift = params.value("TDRIFT").toFloat();
            d.text = params.value("TEXT").toString();
            d.to = params.value("TO").toString();
            d.utcTimestamp = QDateTime::fromString(params.value("UTC").toString(), "yyyy-MM-dd hh:mm:ss");
            d.utcTimestamp.setUtcOffset(0);

            msg.setType("READ");
            i.set(id, msg);

            m_rxInboxCountCache[call] = max(0, m_rxInboxCountCache.value(call) - 1);

            processAlertReplyForCommand(d, d.relayPath, d.cmd);
        }

    } else {
        addMessageText(call);
    }
#endif
}

void MainWindow::on_tableWidgetCalls_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected){
    on_tableWidgetRXAll_selectionChanged(selected, deselected);
}

void MainWindow::on_freeTextMsg_currentTextChanged (QString const& text)
{
}

void MainWindow::on_driftSpinBox_valueChanged(int n){
    if(n == DriftingDateTime::drift()){
        return;
    }

    setDrift(n);
}

void MainWindow::on_driftSyncButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();

    int n = 0;
    int nPos = 15 - now.time().second() % m_TRperiod;
    int nNeg = now.time().second() % m_TRperiod - 15;

    if(abs(nNeg) < nPos){
        n = nNeg;
    } else {
        n = nPos;
    }

    setDrift(n * 1000);
}

void MainWindow::on_driftSyncEndButton_clicked(){
    auto now = QDateTime::currentDateTimeUtc();

    int n = 0;
    int nPos = 15 - now.time().second() % m_TRperiod;
    int nNeg = now.time().second() % m_TRperiod - 15;

    if(abs(nNeg) < nPos){
        n = nNeg + 2;
    } else {
        n = nPos - 2;
    }

    setDrift(n * 1000);
}

void MainWindow::on_driftSyncResetButton_clicked(){
    setDrift(0);
    resetTimeDeltaAverage();
}

void MainWindow::setDrift(int n){
    DriftingDateTime::setDrift(n);

    qDebug() << qSetRealNumberPrecision(12) << "Average delta:" << m_timeDeltaMsMMA;
    qDebug() << qSetRealNumberPrecision(12) << "Drift milliseconds:" << n;
    qDebug() << qSetRealNumberPrecision(12) << "Clock time:" << QDateTime::currentDateTimeUtc();
    qDebug() << qSetRealNumberPrecision(12) << "Drifted time:" << DriftingDateTime::currentDateTimeUtc();

    if(ui->driftSpinBox->value() != n){
        ui->driftSpinBox->setValue(n);
    }
}

void MainWindow::on_rptSpinBox_valueChanged(int n)
{
  int step=ui->rptSpinBox->singleStep();
  if(n%step !=0) {
    n++;
    ui->rptSpinBox->setValue(n);
  }
  m_rpt=QString::number(n);
  int ntx0=m_ntx;
  m_ntx=ntx0;
  if(m_ntx==1) ui->txrb1->setChecked(true);
  if(m_ntx==2) ui->txrb2->setChecked(true);
  if(m_ntx==3) ui->txrb3->setChecked(true);
  if(m_ntx==4) ui->txrb4->setChecked(true);
  if(m_ntx==5) ui->txrb5->setChecked(true);
  if(m_ntx==6) ui->txrb6->setChecked(true);
  statusChanged();
}

void MainWindow::on_tuneButton_clicked (bool checked)
{
  static bool lastChecked = false;
  if (lastChecked == checked) return;
  lastChecked = checked;
  QString curBand = ui->bandComboBox->currentText();
  if (checked && m_tune==false) { // we're starting tuning so remember Tx and change pwr to Tune value
    if (m_config.pwrBandTuneMemory ()) {
      m_pwrBandTxMemory[curBand] = ui->outAttenuation->value(); // remember our Tx pwr
      m_PwrBandSetOK = false;
      if (m_pwrBandTuneMemory.contains(curBand)) {
        ui->outAttenuation->setValue(m_pwrBandTuneMemory[curBand].toInt()); // set to Tune pwr
      }
      m_PwrBandSetOK = true;
    }
  }
  else { // we're turning off so remember our Tune pwr setting and reset to Tx pwr
    if (m_config.pwrBandTuneMemory() || m_config.pwrBandTxMemory()) {
      m_pwrBandTuneMemory[curBand] = ui->outAttenuation->value(); // remember our Tune pwr
      m_PwrBandSetOK = false;
      ui->outAttenuation->setValue(m_pwrBandTxMemory[curBand].toInt()); // set to Tx pwr
      m_PwrBandSetOK = true;
    }
  }
  if (m_tune) {
    tuneButtonTimer.start(250);
  } else {
    m_sentFirst73=false;
    itone[0]=0;
    on_monitorButton_clicked (true);
    m_tune=true;
  }
  Q_EMIT tune (checked);
}

void MainWindow::stop_tuning ()
{
  on_tuneButton_clicked(false);
  ui->tuneButton->setChecked (false);
  m_bTxTime=false;
  m_tune=false;
  on_stopTxButton_clicked();
}

void MainWindow::stopTuneATU()
{
  on_tuneButton_clicked(false);
  m_bTxTime=false;
}

void MainWindow::resetPushButtonToggleText(QPushButton *btn){
    bool checked = btn->isChecked();
    auto style = btn->styleSheet();
    if(checked){
        style = style.replace("font-weight:normal;", "font-weight:bold;");
    } else {
        style = style.replace("font-weight:bold;", "font-weight:normal;");
    }
    btn->setStyleSheet(style);

#if PUSH_BUTTON_CHECKMARK
    auto on = "✓ ";
    auto text = btn->text();
    if(checked){
        btn->setText(on + text.replace(on, ""));
    } else {
        btn->setText(text.replace(on, ""));
    }
#endif

#if PUSH_BUTTON_MIN_WIDTH
    int width = 0;
    QList<QPushButton*> btns;
    foreach(auto child, ui->buttonGrid->children()){
        if(!child->isWidgetType()){
            continue;
        }

        if(!child->objectName().contains("Button")){
            continue;
        }

        auto b = qobject_cast<QPushButton*>(child);
        width = qMax(width, b->geometry().width());
        btns.append(b);
    }

    foreach(auto child, btns){
        child->setMinimumWidth(width);
    }
#endif
}

void MainWindow::on_monitorTxButton_clicked(){
    ui->monitorTxButton->setChecked(false);
    on_stopTxButton_clicked();
}

void MainWindow::on_stopTxButton_clicked()                    //Stop Tx
{
  if (m_tune) stop_tuning ();
  if (m_auto and !m_tuneup) auto_tx_mode (false);
  m_btxok=false;
  m_bCallingCQ = false;
  m_bAutoReply = false;         // ready for next
  ui->cbFirst->setStyleSheet ("");

  resetMessage();
  resetAutomaticIntervalTransmissions(false, false);
}

void MainWindow::rigOpen ()
{
  update_dynamic_property (ui->readFreq, "state", "warning");
  ui->readFreq->setText ("CAT");
  ui->readFreq->setEnabled (true);
  m_config.transceiver_online ();
  Q_EMIT m_config.sync_transceiver (true, true);
}

void MainWindow::on_pbR2T_clicked()
{
  ui->TxFreqSpinBox->setValue(ui->RxFreqSpinBox->value ());
}

void MainWindow::on_pbT2R_clicked()
{
  if (ui->RxFreqSpinBox->isEnabled ())
    {
      ui->RxFreqSpinBox->setValue (ui->TxFreqSpinBox->value ());
    }
}

void MainWindow::on_turboButton_clicked(){
  m_wideGraph->setTurbo(ui->turboButton->isChecked());
  m_txTextDirty = true;
  updateTextDisplay();
}

void MainWindow::on_readFreq_clicked()
{
  if (m_transmitting) return;

  if (m_config.transceiver_online ())
    {
      Q_EMIT m_config.sync_transceiver (true, true);
    }
}

void MainWindow::on_pbTxMode_clicked()
{
  if(m_modeTx=="JT9") {
    m_modeTx="JT65";
    ui->pbTxMode->setText("Tx JT65  #");
  } else {
    m_modeTx="JT9";
    ui->pbTxMode->setText("Tx JT9  @");
  }
  m_wideGraph->setModeTx(m_modeTx);
  statusChanged();
}

void MainWindow::setXIT(int n, Frequency base)
{
  if (m_transmitting && !m_config.tx_qsy_allowed ()) return;
  // If "CQ nnn ..." feature is active, set the proper Tx frequency
  if(m_config.split_mode () && ui->cbCQTx->isEnabled () && ui->cbCQTx->isVisible () &&
     ui->cbCQTx->isChecked())
    {
      if (6 == m_ntx || (7 == m_ntx && m_gen_message_is_cq))
        {
          // All conditions are met, use calling frequency
          base = m_freqNominal / 1000000 * 1000000 + 1000 * ui->sbCQTxFreq->value () + m_XIT;
        }
    }
  if (!base) base = m_freqNominal;
  m_XIT = 0;
  if (!m_bSimplex) {
    // m_bSimplex is false, so we can use split mode if requested
    if (m_config.split_mode () && (!m_config.enable_VHF_features () || m_mode == "FT8")) {
      // Don't use XIT for VHF & up
      m_XIT=(n/500)*500 - 1500;
    }

    if ((m_monitoring || m_transmitting)
        && m_config.is_transceiver_online ()
        && m_config.split_mode ())
      {
        // All conditions are met, reset the transceiver Tx dial
        // frequency
        m_freqTxNominal = base + m_XIT;
        if (m_astroWidget) m_astroWidget->nominal_frequency (m_freqNominal, m_freqTxNominal);
        Q_EMIT m_config.transceiver_tx_frequency (m_freqTxNominal + m_astroCorrection.tx);
      }
  }
  //Now set the audio Tx freq
  Q_EMIT transmitFrequency (ui->TxFreqSpinBox->value () - m_XIT);
}

void MainWindow::qsy(int hzDelta){
    setRig(m_freqNominal + hzDelta);
    setFreqOffsetForRestore(m_wideGraph->centerFreq(), false);

    // adjust band activity frequencies
    QMap<int, QList<ActivityDetail>> newActivity;
    foreach(auto offset, m_bandActivity.keys()){
        if(m_bandActivity[offset].isEmpty()){
            continue;
        }
        newActivity[offset - hzDelta] = m_bandActivity[offset];
        newActivity[offset - hzDelta].last().freq -= hzDelta;
    }
    m_bandActivity.clear();
    m_bandActivity.unite(newActivity);

    // adjust call activity frequencies
    foreach(auto call, m_callActivity.keys()){
        m_callActivity[call].freq -= hzDelta;
    }

    displayActivity(true);
}

void MainWindow::setFreqOffsetForRestore(int freq, bool shouldRestore){
    setFreq4(freq, freq);
    if(shouldRestore){
        m_shouldRestoreFreq = true;
    } else {
        m_previousFreq = 0;
        m_shouldRestoreFreq = false;
    }
}

bool MainWindow::tryRestoreFreqOffset(){
    if(!m_shouldRestoreFreq || m_previousFreq == 0){
        return false;
    }

    setFreqOffsetForRestore(m_previousFreq, false);
    return true;
}

void MainWindow::setFreq4(int rxFreq, int txFreq) {
  // don't allow QSY if we've already queued a transmission, unless we have that functionality enabled.
  if(isMessageQueuedForTransmit() && !m_config.tx_qsy_allowed()){
      return;
  }

  if(rxFreq != txFreq){
      txFreq = rxFreq;
  }

  // TODO: jsherer - here's where we'd set minimum frequency again (later?)
  rxFreq = max(0, rxFreq);
  txFreq = max(0, txFreq);

  m_previousFreq = currentFreqOffset();
  ui->RxFreqSpinBox->setValue(rxFreq);
  ui->TxFreqSpinBox->setValue(txFreq);

  displayDialFrequency();
}

void MainWindow::handle_transceiver_update (Transceiver::TransceiverState const& s)
{
  //qDebug () << "MainWindow::handle_transceiver_update:" << s;
  Transceiver::TransceiverState old_state {m_rigState};

  // GM8JCF: in stopTx2 we maintain PTT if there are still untransmitted JS8 frames and we are holding the PTT
  // KN4CRD: if we're not holding the PTT we need to check to ensure it's safe to transmit
  if (m_config.hold_ptt() || (s.ptt () && !m_rigState.ptt())) // safe to start audio (caveat - DX Lab Suite Commander)
  {
      if (m_tx_when_ready && g_iptt) // waiting to Tx and still needed
      {
          ptt1Timer.start(1000 * m_config.txDelay ()); //Start-of-transmission sequencer delay
      }
      m_tx_when_ready = false;
  }
  m_rigState = s;

  auto old_freqNominal = m_freqNominal;
  if (!old_freqNominal)
    {
      // always take initial rig frequency to avoid start up problems
      // with bogus Tx frequencies
      m_freqNominal = s.frequency ();
    }

  if (old_state.online () == false && s.online () == true)
    {
      // initializing
      on_monitorButton_clicked (!m_config.monitor_off_at_startup ());
    }

  if (s.frequency () != old_state.frequency () || s.split () != m_splitMode)
    {
      m_splitMode = s.split ();
      if (!s.ptt ())
        {
          m_freqNominal = s.frequency () - m_astroCorrection.rx;
          if (old_freqNominal != m_freqNominal)
            {
              m_freqTxNominal = m_freqNominal;
            }

          if (m_monitoring)
            {
              m_lastMonitoredFrequency = m_freqNominal;
            }
          if (m_lastDialFreq != m_freqNominal &&
              (m_mode != "MSK144"
               || !(ui->cbCQTx->isEnabled () && ui->cbCQTx->isVisible () && ui->cbCQTx->isChecked()))) {

            m_lastDialFreq = m_freqNominal;
            m_secBandChanged=DriftingDateTime::currentMSecsSinceEpoch()/1000;

            if(m_freqNominal != m_bandHoppedFreq){
                m_bandHopped = false;
            }

            if(s.frequency () < 30000000u && !m_mode.startsWith ("WSPR")) {
              // Write freq changes to ALL.TXT only below 30 MHz.
              QFile f2 {m_config.writeable_data_dir ().absoluteFilePath ("ALL.TXT")};
              if (f2.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
                QTextStream out(&f2);
                out << DriftingDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss")
                    << "  " << qSetRealNumberPrecision (12) << (m_freqNominal / 1.e6) << " MHz  "
                    << "JS8" << endl;
                f2.close();
              } else {
                MessageBox::warning_message (this, tr ("File Error")
                                             ,tr ("Cannot open \"%1\" for append: %2")
                                             .arg (f2.fileName ()).arg (f2.errorString ()));
              }
            }

            if (m_config.spot_to_reporting_networks ()) {
              spotSetLocal();
              pskSetLocal();
              aprsSetLocal();
            }
            statusChanged();
            m_wideGraph->setDialFreq(m_freqNominal / 1.e6);
          }
      } else {
        m_freqTxNominal = s.split () ? s.tx_frequency () - m_astroCorrection.tx : s.frequency ();
      }
      if (m_astroWidget) m_astroWidget->nominal_frequency (m_freqNominal, m_freqTxNominal);
  }
  // ensure frequency display is correct
  if (m_astroWidget && old_state.ptt () != s.ptt ()) setRig ();

  updateCurrentBand();
  displayDialFrequency ();
  update_dynamic_property (ui->readFreq, "state", "ok");
  ui->readFreq->setEnabled (false);
  ui->readFreq->setText (s.split () ? "CAT/S" : "CAT");
}

void MainWindow::handle_transceiver_failure (QString const& reason)
{
  update_dynamic_property (ui->readFreq, "state", "error");
  ui->readFreq->setEnabled (true);
  on_stopTxButton_clicked ();
  rigFailure (reason);
}

void MainWindow::rigFailure (QString const& reason)
{
  if (m_first_error)
    {
      // one automatic retry
      QTimer::singleShot (0, this, SLOT (rigOpen ()));
      m_first_error = false;
    }
  else
    {
      if (m_splash && m_splash->isVisible ()) m_splash->hide ();
      m_rigErrorMessageBox.setDetailedText (reason);

      // don't call slot functions directly to avoid recursion
      m_rigErrorMessageBox.exec ();
      auto const clicked_button = m_rigErrorMessageBox.clickedButton ();
      if (clicked_button == m_configurations_button)
        {
          ui->menuConfig->exec (QCursor::pos ());
        }
      else
        {
          switch (m_rigErrorMessageBox.standardButton (clicked_button))
            {
            case MessageBox::Ok:
              m_config.select_tab (1);
              QTimer::singleShot (0, this, SLOT (on_actionSettings_triggered ()));
              break;

            case MessageBox::Retry:
              QTimer::singleShot (0, this, SLOT (rigOpen ()));
              break;

            case MessageBox::Cancel:
              QTimer::singleShot (0, this, SLOT (close ()));
              break;

            default: break;     // squashing compile warnings
            }
        }
      m_first_error = true;     // reset
    }
}

void MainWindow::transmit (double snr)
{
  double toneSpacing=0.0;

  if (m_modeTx == "FT8") {
    double symbolSamples = 0.0;
    if(m_nSubMode == Varicode::JS8CallNormal){
        symbolSamples=(double)JS8A_SYMBOL_SAMPLES;
        toneSpacing=(double)RX_SAMPLE_RATE/(double)JS8A_SYMBOL_SAMPLES;
    }
    else if(m_nSubMode == Varicode::JS8CallFast){
        symbolSamples=(double)JS8B_SYMBOL_SAMPLES;
        toneSpacing=(double)RX_SAMPLE_RATE/(double)JS8B_SYMBOL_SAMPLES;
    }
    else if(m_nSubMode == Varicode::JS8CallTurbo){
        symbolSamples=(double)JS8C_SYMBOL_SAMPLES;
        toneSpacing=(double)RX_SAMPLE_RATE/(double)JS8C_SYMBOL_SAMPLES;
    }
    else if(m_nSubMode == Varicode::JS8CallUltra){
        symbolSamples=(double)JS8D_SYMBOL_SAMPLES;
        toneSpacing=(double)RX_SAMPLE_RATE/(double)JS8D_SYMBOL_SAMPLES;
    }
    if(m_config.x2ToneSpacing()) toneSpacing*=2.0;
    if(m_config.x4ToneSpacing()) toneSpacing*=4.0;
    if(m_config.bFox() and !m_tune) toneSpacing=-1;
    if(TEST_FOX_WAVE_GEN && ui->turboButton->isChecked() && !m_tune) toneSpacing=-1;

    Q_EMIT sendMessage (JS8_NUM_SYMBOLS,
           symbolSamples, ui->TxFreqSpinBox->value () - m_XIT,
           toneSpacing, m_soundOutput, m_config.audio_output_channel (),
           true, false, snr, m_TRperiod);
  }
}

void MainWindow::on_outAttenuation_valueChanged (int a)
{
  QString tt_str;
  qreal dBAttn {a / 10.};       // slider interpreted as dB / 100
  if (m_tune && m_config.pwrBandTuneMemory()) {
    tt_str = tr ("Tune digital gain ");
  } else {
    tt_str = tr ("Transmit digital gain ");
  }
  tt_str += (a ? QString::number (-dBAttn, 'f', 1) : "0") + "dB";
  if (!m_block_pwr_tooltip) {
    QToolTip::showText (QCursor::pos (), tt_str, ui->outAttenuation);
  }
  QString curBand = ui->bandComboBox->currentText();
  if (m_PwrBandSetOK && !m_tune && m_config.pwrBandTxMemory ()) {
    m_pwrBandTxMemory[curBand] = a; // remember our Tx pwr
  }
  if (m_PwrBandSetOK && m_tune && m_config.pwrBandTuneMemory()) {
    m_pwrBandTuneMemory[curBand] = a; // remember our Tune pwr
  }
  Q_EMIT outAttenuationChanged (dBAttn);
}

void MainWindow::on_actionShort_list_of_add_on_prefixes_and_suffixes_triggered()
{
}

bool MainWindow::shortList(QString callsign)
{
  int n=callsign.length();
  int i1=callsign.indexOf("/");
  Q_ASSERT(i1>0 and i1<n);
  QString t1=callsign.mid(0,i1);
  QString t2=callsign.mid(i1+1,n-i1-1);
  bool b=(m_pfx.contains(t1) or m_sfx.contains(t2));
  return b;
}

void MainWindow::spotSetLocal ()
{
    auto call = m_config.my_callsign();
    auto grid = m_config.my_grid();
    auto info = replaceMacros(m_config.my_info(), buildMacroValues(), true);
    auto ver = QString {"JS8Call v" + version() }.simplified ();
    qDebug() << "SpotClient Set Local Station:" << call << grid << info << ver;
    m_spotClient->setLocalStation(call, grid, info, ver);
}

void MainWindow::pskSetLocal ()
{
  auto info = replaceMacros(m_config.my_info(), buildMacroValues(), true);
  psk_Reporter->setLocalStation(m_config.my_callsign (), m_config.my_grid (), info, QString {"JS8Call v" + version() }.simplified ());
}

void MainWindow::aprsSetLocal ()
{
    auto call = m_config.my_callsign();
    auto base = Radio::base_callsign(call);
    auto grid = m_config.my_grid();
    auto passcode = m_config.aprs_passcode();

    call = APRSISClient::replaceCallsignSuffixWithSSID(call, base);

    qDebug() << "APRSISClient Set Local Station:" << call << grid << passcode;
    m_aprsClient->setLocalStation(call, grid, passcode);
}

void MainWindow::transmitDisplay (bool transmitting)
{
  ui->monitorTxButton->setChecked(transmitting);

  if (transmitting == m_transmitting) {
    if (transmitting) {
      ui->signal_meter_widget->setValue(0,0);
      if (m_monitoring) monitor (false);
      m_btxok=true;
    }

    auto QSY_allowed = !transmitting or m_config.tx_qsy_allowed () or
      !m_config.split_mode ();
    if (ui->cbHoldTxFreq->isChecked ()) {
      ui->RxFreqSpinBox->setEnabled (QSY_allowed);
      ui->pbT2R->setEnabled (QSY_allowed);
    }

    if (!m_mode.startsWith ("WSPR")) {
      if(m_config.enable_VHF_features ()) {
//### During tests, at least, allow use of Tx Freq spinner with VHF features enabled.
        // used fixed 1000Hz Tx DF for VHF & up QSO modes
//        ui->TxFreqSpinBox->setValue(1000);
//        ui->TxFreqSpinBox->setEnabled (false);
        ui->TxFreqSpinBox->setEnabled (true);
//###
      } else {
        ui->TxFreqSpinBox->setEnabled (QSY_allowed and !m_bFastMode);
        ui->pbR2T->setEnabled (QSY_allowed);
        ui->cbHoldTxFreq->setEnabled (QSY_allowed);
      }
    }

    // the following are always disallowed in transmit
    //ui->menuMode->setEnabled (!transmitting);
    //ui->bandComboBox->setEnabled (!transmitting);
    if (!transmitting) {
      if (m_mode == "JT9+JT65") {
        // allow mode switch in Rx when in dual mode
        ui->pbTxMode->setEnabled (true);
      }
    } else {
      ui->pbTxMode->setEnabled (false);
    }
  }

  updateTxButtonDisplay();
}

void MainWindow::on_sbFtol_valueChanged(int value)
{
  m_wideGraph->setTol (value);
}

void::MainWindow::VHF_features_enabled(bool b)
{
  if(m_mode!="JT4" and m_mode!="JT65") b=false;
  if(b and (ui->actionInclude_averaging->isChecked() or
             ui->actionInclude_correlation->isChecked())) {
    ui->actionDeepestDecode->setChecked (true);
  }
  ui->actionInclude_averaging->setVisible (b);
  ui->actionInclude_correlation->setVisible (b);
  ui->actionMessage_averaging->setEnabled(b);
  ui->actionEnable_AP_DXcall->setVisible (m_mode=="QRA64");
  ui->actionEnable_AP_JT65->setVisible (b && m_mode=="JT65");
  if(!b && m_msgAvgWidget and !m_config.bFox()) {
    if(m_msgAvgWidget->isVisible()) m_msgAvgWidget->close();
  }
}

void MainWindow::on_sbTR_valueChanged(int value)
{
//  if(!m_bFastMode and n>m_nSubMode) m_MinW=m_nSubMode;
  if(m_bFastMode or m_mode=="FreqCal") {
    m_TRperiod = value;
    m_fastGraph->setTRPeriod (value);
    m_modulator->setTRPeriod (value); // TODO - not thread safe
    m_detector->setTRPeriod (value);  // TODO - not thread safe
    m_wideGraph->setPeriod (value, m_nsps);
    progressBar.setMaximum (value);
  }
  if(m_monitoring) {
    on_stopButton_clicked();
    on_monitorButton_clicked(true);
  }
  if(m_transmitting) {
    on_stopTxButton_clicked();
  }
}

QChar MainWindow::current_submode () const
{
  QChar submode {0};
  if (m_mode.contains (QRegularExpression {R"(^(JT65|JT9|JT4|ISCAT|QRA64)$)"})
      && (m_config.enable_VHF_features () || "JT4" == m_mode || "ISCAT" == m_mode))
    {
      submode = m_nSubMode + 65;
    }
  return submode;
}

void MainWindow::on_sbSubmode_valueChanged(int n)
{
  m_nSubMode=n;
  m_wideGraph->setSubMode(m_nSubMode);
  auto submode = current_submode ();
  if (submode != QChar::Null)
    {
      mode_label.setText (m_mode + " " + submode);
    }
  else
    {
      mode_label.setText (m_mode);
    }
  if(m_mode=="ISCAT") {
    if(m_nSubMode==0) ui->TxFreqSpinBox->setValue(1012);
    if(m_nSubMode==1) ui->TxFreqSpinBox->setValue(560);
  }
  if(m_mode=="JT9") {
    if(m_nSubMode<4) {
      ui->cbFast9->setChecked(false);
      on_cbFast9_clicked(false);
      ui->cbFast9->setEnabled(false);
      ui->sbTR->setVisible(false);
      m_TRperiod=60;
    } else {
      ui->cbFast9->setEnabled(true);
    }
    ui->sbTR->setVisible(m_bFast9);
    if(m_bFast9) ui->TxFreqSpinBox->setValue(700);
  }
  if(m_transmitting and m_bFast9 and m_nSubMode>=4) transmit (99.0);
  statusUpdate ();
}

void MainWindow::on_cbFast9_clicked(bool b)
{
  if(m_mode=="JT9") {
    m_bFast9=b;
//    ui->cbAutoSeq->setVisible(b);
  }

  if(b) {
    m_TRperiod = ui->sbTR->value ();
  } else {
    m_TRperiod=60;
  }
  progressBar.setMaximum(m_TRperiod);
  m_wideGraph->setPeriod(m_TRperiod,m_nsps);
  fast_config(b);
  statusChanged ();
}


void MainWindow::on_cbShMsgs_toggled(bool b)
{
  ui->cbTx6->setEnabled(b);
  m_bShMsgs=b;
  if(b) ui->cbSWL->setChecked(false);
  if(m_bShMsgs and (m_mode=="MSK144")) ui->rptSpinBox->setValue(1);
  int itone0=itone[0];
  int ntx=m_ntx;
  m_lastCallsign.clear ();      // ensure Tx5 gets updated
  itone[0]=itone0;
  if(ntx==1) ui->txrb1->setChecked(true);
  if(ntx==2) ui->txrb2->setChecked(true);
  if(ntx==3) ui->txrb3->setChecked(true);
  if(ntx==4) ui->txrb4->setChecked(true);
  if(ntx==5) ui->txrb5->setChecked(true);
  if(ntx==6) ui->txrb6->setChecked(true);
}

void MainWindow::on_cbSWL_toggled(bool b)
{
  if(b) ui->cbShMsgs->setChecked(false);
}

void MainWindow::on_cbTx6_toggled(bool)
{
}

void MainWindow::locationChange (QString const& location)
{
  QString grid {location.trimmed ()};
  int len;

  // string 6 chars or fewer, interpret as a grid, or use with a 'GRID:' prefix
  if (grid.size () > 6) {
    if (grid.toUpper ().startsWith ("GRID:")) {
      grid = grid.mid (5).trimmed ();
    }
    else {
      // TODO - support any other formats, e.g. latlong? Or have that conversion done externally
      return;
    }
  }
  if (MaidenheadLocatorValidator::Acceptable == MaidenheadLocatorValidator ().validate (grid, len)) {
    qDebug() << "locationChange: Grid supplied is " << grid;
    if (m_config.my_grid () != grid) {
      m_config.set_dynamic_location (grid);
      statusUpdate ();
    }
  } else {
    qDebug() << "locationChange: Invalid grid " << grid;
  }
}

void MainWindow::replayDecodes ()
{
  // we accept this request even if the setting to accept UDP requests
  // is not checked

  // attempt to parse the decoded text
  Q_FOREACH (auto const& message
             , ui->decodedTextBrowser->toPlainText ().split (QChar::LineFeed,
                                                             QString::SkipEmptyParts))
    {
      if (message.size() >= 4 && message.left (4) != "----")
        {
          auto const& parts = message.split (' ', QString::SkipEmptyParts);
          if (parts.size () >= 5 && parts[3].contains ('.')) // WSPR
            {
              postWSPRDecode (false, parts);
            }
          else
            {
              auto eom_pos = message.indexOf (' ', 35);
              // we always want at least the characters to position 35
              if (eom_pos < 35)
                {
                  eom_pos = message.size () - 1;
                }
              // TODO - how to skip ISCAT decodes
              postDecode (false, message.left (eom_pos + 1));
            }
        }
    }
  statusChanged ();
}

void MainWindow::postDecode (bool is_new, QString const& message)
{
#if 0
  auto const& decode = message.trimmed ();
  auto const& parts = decode.left (22).split (' ', QString::SkipEmptyParts);
  if (parts.size () >= 5)
  {
      auto has_seconds = parts[0].size () > 4;
      m_messageClient->decode (is_new
                               , QTime::fromString (parts[0], has_seconds ? "hhmmss" : "hhmm")
                               , parts[1].toInt ()
                               , parts[2].toFloat (), parts[3].toUInt (), parts[4]
                               , decode.mid (has_seconds ? 24 : 22, 21)
                               , QChar {'?'} == decode.mid (has_seconds ? 24 + 21 : 22 + 21, 1)
                               , m_diskData);
  }
#endif

  if(is_new){
      m_rxDirty = true;
  }
}

void MainWindow::displayTransmit(){
    // Transmit Activity
    update_dynamic_property (ui->startTxButton, "transmitting", m_transmitting);
}

void MainWindow::updateButtonDisplay(){
    bool isTransmitting = m_transmitting || m_txFrameCount > 0;

    auto selectedCallsign = callsignSelected(true);
    bool emptyCallsign = selectedCallsign.isEmpty();
    bool emptyInfo = m_config.my_info().isEmpty();

    ui->hbMacroButton->setDisabled(isTransmitting);
    ui->cqMacroButton->setDisabled(isTransmitting);
    ui->replyMacroButton->setDisabled(isTransmitting || emptyCallsign);
    ui->snrMacroButton->setDisabled(isTransmitting || emptyCallsign);
    ui->infoMacroButton->setDisabled(isTransmitting || emptyInfo);
    ui->macrosMacroButton->setDisabled(isTransmitting);
    ui->queryButton->setDisabled(isTransmitting || emptyCallsign);
    ui->deselectButton->setDisabled(isTransmitting || emptyCallsign);
    ui->queryButton->setText(emptyCallsign ? "Directed" : QString("Directed to %1").arg(selectedCallsign));

    // refresh repeat button text too
    updateRepeatButtonDisplay();
}

void MainWindow::updateRepeatButtonDisplay(){
    auto selectedCallsign = callsignSelected();
    auto hbBase = ui->actionModeAutoreply->isChecked() && ui->actionHeartbeatAcknowledgements->isChecked() && selectedCallsign.isEmpty() ? "HB + ACK" : "HB";
    if(ui->hbMacroButton->isChecked() && m_hbInterval > 0 && m_nextHeartbeat.isValid()){
        auto secs = DriftingDateTime::currentDateTimeUtc().secsTo(m_nextHeartbeat);
        if(secs > 0){
            ui->hbMacroButton->setText(QString("%1 (%2)").arg(hbBase).arg(secs));
        } else {
            ui->hbMacroButton->setText(QString("%1 (now)").arg(hbBase));
        }
    } else {
        ui->hbMacroButton->setText(hbBase);
    }

    if(ui->cqMacroButton->isChecked() && m_cqInterval > 0 && m_nextCQ.isValid()){
        auto secs = DriftingDateTime::currentDateTimeUtc().secsTo(m_nextCQ);
        if(secs > 0){
            ui->cqMacroButton->setText(QString("CQ (%1)").arg(secs));
        } else {
            ui->cqMacroButton->setText(QString("CQ (now)"));
        }
    } else {
        ui->cqMacroButton->setText("CQ");
    }
}

void MainWindow::updateTextDisplay(){
    bool isTransmitting = m_transmitting || m_txFrameCount > 0;
    bool emptyText = ui->extFreeTextMsgEdit->toPlainText().isEmpty();

    ui->startTxButton->setDisabled(isTransmitting || emptyText);

    if(m_txTextDirty){
        // debounce frame and word count
        if(m_txTextDirtyDebounce.isActive()){
            m_txTextDirtyDebounce.stop();
        }
        m_txTextDirtyDebounce.setSingleShot(true);
        m_txTextDirtyDebounce.start(100);
        m_txTextDirty = false;
    }
}


#if __APPLE__
#define USE_SYNC_FRAME_COUNT 0
#else
#define USE_SYNC_FRAME_COUNT 0
#endif

void MainWindow::refreshTextDisplay(){
    qDebug() << "refreshing text display...";
    auto text = ui->extFreeTextMsgEdit->toPlainText();

#if USE_SYNC_FRAME_COUNT
    auto frames = buildMessageFrames(text);

    QStringList textList;
    qDebug() << "frames:";
    foreach(auto frame, frames){
        auto dt = DecodedText(frame.first, frame.second);
        qDebug() << "->" << frame << dt.message() << Varicode::frameTypeString(dt.frameType());
        textList.append(dt.message());
    }

    auto transmitText = textList.join("");
    auto count = frames.length();

    // ugh...i hate these globals
    m_txTextDirtyLastSelectedCall = callsignSelected(true);
    m_txTextDirtyLastText = text;
    m_txFrameCountEstimate = count;
    m_txTextDirty = false;

    updateTextWordCheckerDisplay();
    updateTextStatsDisplay(transmitText, count);
    updateTxButtonDisplay();

#else
    // prepare selected callsign for directed message
    QString selectedCall = callsignSelected();

    // prepare compound
    QString mycall = m_config.my_callsign();
    QString mygrid = m_config.my_grid().left(4);
    bool forceIdentify = !m_config.avoid_forced_identify();
    bool forceData = false;

    BuildMessageFramesThread *t = new BuildMessageFramesThread(
        mycall,
        mygrid,
        selectedCall,
        text,
        forceIdentify,
        forceData,
        m_nSubMode
    );

    connect(t, &BuildMessageFramesThread::finished, t, &QObject::deleteLater);
    connect(t, &BuildMessageFramesThread::resultReady, this, [this, text](QString transmitText, int frames){
        // ugh...i hate these globals
        m_txTextDirtyLastSelectedCall = callsignSelected(true);
        m_txTextDirtyLastText = text;
#if TEST_FOX_WAVE_GEN
        m_txFrameCountEstimate = ui->turboButton->isChecked() ? (int)ceil(float(frames)/TEST_FOX_WAVE_GEN_SLOTS) : frames;
#else
        m_txFrameCountEstimate = frames;
#endif
        m_txTextDirty = false;

        updateTextWordCheckerDisplay();
        updateTextStatsDisplay(transmitText, m_txFrameCountEstimate);
        updateTxButtonDisplay();

    });
    t->start();
#endif
}

void MainWindow::updateTextWordCheckerDisplay(){
    if(!m_config.spellcheck()){
        return;
    }

    JSCChecker::checkRange(ui->extFreeTextMsgEdit, 0, -1);
}

void MainWindow::updateTextStatsDisplay(QString text, int count){
    const double fpm = 60.0/m_TRperiod;
    if(count > 0){
        auto words = text.split(" ", QString::SkipEmptyParts).length();
        auto wpm = QString::number(words/(count/fpm), 'f', 1);
        auto cpm = QString::number(text.length()/(count/fpm), 'f', 1);
        wpm_label.setText(QString("%1wpm / %2cpm").arg(wpm).arg(cpm));
        wpm_label.setVisible(true);
    } else {
        wpm_label.setVisible(false);
        wpm_label.clear();
    }
}

void MainWindow::updateTxButtonDisplay(){
    // update transmit button
    if(m_tune || m_transmitting || m_txFrameCount > 0){
        int count = m_txFrameCount;

#if TEST_FOX_WAVE_GEN
        if(ui->turboButton->isChecked()){
            count = qMax(1, (int)ceil(float(count)/TEST_FOX_WAVE_GEN_SLOTS));
        }

        int left = m_txFrameQueue.count();
        if(ui->turboButton->isChecked()){
            left = (int)ceil(float(left)/TEST_FOX_WAVE_GEN_SLOTS);
        }
        int sent = qMax(1, count - left);
#else
        int left = m_txFrameQueue.count();
        int sent = count - left;
#endif

        QString buttonText;
        if(m_tune){
            buttonText = "Tuning";
        } else if(m_transmitting){
            auto secondsLeft = (((left + 1) * m_TRperiod) - ((m_sec0 + 1) % m_TRperiod));
            auto timeLeft = timeSince(secondsLeft);
            buttonText = QString("Sending (%1)").arg(timeLeft);
        } else {
            auto secondsLeft = sent == 1 ? ((left + 1) * m_TRperiod) :
                (((left + 2) * m_TRperiod) - ((m_sec0 + 1) % m_TRperiod));
            auto timeLeft = timeSince(secondsLeft);
            buttonText = QString("Ready (%1)").arg(timeLeft);
        }
        ui->startTxButton->setText(buttonText);
        ui->startTxButton->setEnabled(false);
        ui->startTxButton->setFlat(true);
    } else {
        QString buttonText;
        if(m_txFrameCountEstimate > 0){
            auto secondsLeft = m_txFrameCountEstimate * m_TRperiod;
            auto timeLeft = timeSince(secondsLeft);
            buttonText = QString("Send (%1)").arg(timeLeft);
        } else {
            buttonText = "Send";
        }
        ui->startTxButton->setText(buttonText);

        ui->startTxButton->setEnabled(m_txFrameCountEstimate > 0);
        ui->startTxButton->setFlat(false);
    }
}

QString MainWindow::callsignSelected(bool useInputText){
    if(!ui->tableWidgetCalls->selectedItems().isEmpty()){
        auto selectedCalls = ui->tableWidgetCalls->selectedItems();
        if(!selectedCalls.isEmpty()){
            auto call = selectedCalls.first()->data(Qt::UserRole).toString();
            if(!call.isEmpty()){
                return call;
            }
        }
    }

    if(!ui->tableWidgetRXAll->selectedItems().isEmpty()){
        int selectedOffset = -1;
        auto selectedItems = ui->tableWidgetRXAll->selectedItems();
        selectedOffset = selectedItems.first()->data(Qt::UserRole).toInt();

        auto keys = m_callActivity.keys();
        qStableSort(keys.begin(), keys.end(), [this](QString const &a, QString const &b){
            auto tA = m_callActivity[a].utcTimestamp;
            auto tB = m_callActivity[b].utcTimestamp;
            if(tA == tB){
                return a < b;
            }
            return tB < tA;
        });
        foreach(auto call, keys){
            auto d = m_callActivity[call];
            if(d.freq == selectedOffset){
                return d.call;
            }
        }
    }

#if ALLOW_USE_INPUT_TEXT_CALLSIGN
    if(useInputText){
        auto text = ui->extFreeTextMsgEdit->toPlainText().left(11); // Maximum callsign is 6 + / + 4 = 11 characters
        auto calls = Varicode::parseCallsigns(text);
        if(!calls.isEmpty() && text.startsWith(calls.first()) && calls.first() != m_config.my_callsign()){
            return calls.first();
        }
    }
#endif

    return QString();
}

void MainWindow::callsignSelectedChanged(QString /*old*/, QString selectedCall){
    auto placeholderText = QString("Type your outgoing messages here.");
    if(selectedCall.isEmpty()){
        // try to restore hb
        if(m_hbPaused){
            ui->hbMacroButton->setChecked(true);
            m_hbPaused = false;
        }
    } else {
        placeholderText = QString("Type your outgoing directed message to %1 here.").arg(selectedCall);

        // when we select a callsign, use it as the qso start time
        if(!m_callSelectedTime.contains(selectedCall)){
            m_callSelectedTime[selectedCall] = DriftingDateTime::currentDateTimeUtc();
        }

        if(m_config.heartbeat_qso_pause()){
            // TODO: jsherer - HB issue
            // don't hb if we select a callsign... (but we should keep track so if we deselect, we restore our hb)
            if(ui->hbMacroButton->isChecked()){
                ui->hbMacroButton->setChecked(false);
                m_hbPaused = true;
            }

            // don't cq if we select a callsign... (and it will not be restored otherwise)
            if(ui->cqMacroButton->isChecked()){
                ui->cqMacroButton->setChecked(false);
            }
        }
    }
    ui->extFreeTextMsgEdit->setPlaceholderText(placeholderText);

#if SHOW_CALL_DETAIL_BROWSER
    auto html = generateCallDetail(selectedCall);
    ui->callDetailTextBrowser->setHtml(html);
    ui->callDetailTextBrowser->setVisible(!selectedCall.isEmpty() && (!hearing.isEmpty() || !heardby.isEmpty()));
#endif

    // immediately update the display);
    updateButtonDisplay();
    updateTextDisplay();

    m_prevSelectedCallsign = selectedCall;
}

void MainWindow::clearCallsignSelected(){
    // remove the date cache
    m_callSelectedTime.remove(m_prevSelectedCallsign);

    // remove the callsign selection
    ui->tableWidgetCalls->clearSelection();
    ui->tableWidgetRXAll->clearSelection();
}

bool MainWindow::isRecentOffset(int offset){
    if(abs(offset - currentFreqOffset()) <= NEAR_THRESHOLD_RX){
        return true;
    }
    return (
        m_rxRecentCache.contains(offset/10*10) &&
        m_rxRecentCache[offset/10*10]->secsTo(DriftingDateTime::currentDateTimeUtc()) < 120
    );
}

void MainWindow::markOffsetRecent(int offset){
    m_rxRecentCache.insert(offset/10*10, new QDateTime(DriftingDateTime::currentDateTimeUtc()), 10);
    m_rxRecentCache.insert(offset/10*10+10, new QDateTime(DriftingDateTime::currentDateTimeUtc()), 10);
}

bool MainWindow::isDirectedOffset(int offset, bool *pIsAllCall){
    bool isDirected = (
        m_rxDirectedCache.contains(offset/10*10) &&
        m_rxDirectedCache[offset/10*10]->date.secsTo(DriftingDateTime::currentDateTimeUtc()) < 120
    );

    if(isDirected){
        if(pIsAllCall) *pIsAllCall = m_rxDirectedCache[offset/10*10]->isAllcall;
    }

    return isDirected;
}

void MainWindow::markOffsetDirected(int offset, bool isAllCall){
    CachedDirectedType *d1 = new CachedDirectedType{ isAllCall, DriftingDateTime::currentDateTimeUtc() };
    CachedDirectedType *d2 = new CachedDirectedType{ isAllCall, DriftingDateTime::currentDateTimeUtc() };
    m_rxDirectedCache.insert(offset/10*10,    d1, 10);
    m_rxDirectedCache.insert(offset/10*10+10, d2, 10);
}

void MainWindow::clearOffsetDirected(int offset){
    m_rxDirectedCache.remove(offset/10*10);
    m_rxDirectedCache.remove(offset/10*10+10);
}

bool MainWindow::isMyCallIncluded(const QString &text){
    QString myCall = Radio::base_callsign(m_config.my_callsign());

    if(myCall.isEmpty()){
        return false;
    }

    if(!text.contains(myCall)){
        return false;
    }

    return Varicode::parseCallsigns(text).contains(myCall);
}

bool MainWindow::isAllCallIncluded(const QString &text){
    return text.contains("@ALLCALL");
}

bool MainWindow::isGroupCallIncluded(const QString &text){
    return m_config.my_groups().contains(text);
}

void MainWindow::processActivity(bool force) {
    if (!m_rxDirty && !force) {
        return;
    }

    // Recent Rx Activity
    processRxActivity();

    // Process Idle Activity
    processIdleActivity();

    // Grouped Compound Activity
    processCompoundActivity();

    // Buffered Activity
    processBufferedActivity();

    // Command Activity
    processCommandActivity();

    // Process PSKReporter Spots
    processSpots();

    m_rxDirty = false;
}

void MainWindow::observeTimeDeltaForAverage(float delta){
    // delta can only be +/- the TR period
    delta = qMax(-(float)m_TRperiod, qMin(delta, (float)m_TRperiod));

    // compute average drift
    if(m_timeDeltaMsMMA_N == 0){
        m_timeDeltaMsMMA_N++;
        m_timeDeltaMsMMA = (int)(delta*1000);
    } else {
        m_timeDeltaMsMMA_N++;
        m_timeDeltaMsMMA = (((m_timeDeltaMsMMA_N-1)*m_timeDeltaMsMMA) + (int)(delta*1000))/ min(m_timeDeltaMsMMA_N, 100);
    }

    // display average
    if(m_timeDeltaMsMMA < -(float)m_TRperiod || m_timeDeltaMsMMA > (float)m_TRperiod){
        resetTimeDeltaAverage();
    }
    ui->driftAvgLabel->setText(QString("Avg Time Delta: %1 ms").arg((int)m_timeDeltaMsMMA));
}

void MainWindow::resetTimeDeltaAverage(){
    m_timeDeltaMsMMA = 0;
    m_timeDeltaMsMMA_N = 0;

    // observe zero for reset
    observeTimeDeltaForAverage(0);
}


void MainWindow::processIdleActivity() {
    auto now = DriftingDateTime::currentDateTimeUtc();

    // if we detect an idle offset, insert an ellipsis into the activity queue and band activity
    foreach(auto offset, m_bandActivity.keys()){
        auto details = m_bandActivity[offset];
        if(details.isEmpty()){
            continue;
        }

        auto last = details.last();
        if((last.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast){
            continue;
        }

        if(last.utcTimestamp.secsTo(now) < m_TRperiod){
            continue;
        }

        if(last.text == " . . . "){
            continue;
        }

        ActivityDetail d = {};
        d.text = " . . . ";
        d.isFree = true;
        d.utcTimestamp = last.utcTimestamp;
        d.snr = last.snr;
        d.tdrift = last.tdrift;
        d.freq = last.freq;

        if(hasExistingMessageBuffer(offset, false, nullptr)){
            m_messageBuffer[offset].msgs.append(d);
        }

        m_rxActivityQueue.append(d);
        m_bandActivity[offset].append(d);
    }
}

void MainWindow::processRxActivity() {
    if(m_rxActivityQueue.isEmpty()){
        return;
    }

    int freqOffset = currentFreqOffset();

    qDebug() << m_messageBuffer.count() << "message buffers open";

    while (!m_rxActivityQueue.isEmpty()) {
        ActivityDetail d = m_rxActivityQueue.dequeue();

        observeTimeDeltaForAverage(d.tdrift);

        // use the actual frequency and check its delta from our current frequency
        // meaning, if our current offset is 1502 and the d.freq is 1492, the delta is <= 10;
        bool shouldDisplay = abs(d.freq - freqOffset) <= NEAR_THRESHOLD_RX;

        int prevOffset = d.freq;
        if(hasExistingMessageBuffer(d.freq, false, &prevOffset) && (
                (m_messageBuffer[prevOffset].cmd.to == m_config.my_callsign()) ||
                // (isAllCallIncluded(m_messageBuffer[prevOffset].cmd.to))     || // uncomment this if we want to incrementally print allcalls
                (isGroupCallIncluded(m_messageBuffer[prevOffset].cmd.to))
            )
        ){
            d.isBuffered = true;
            shouldDisplay = true;

            if(!m_messageBuffer[prevOffset].compound.isEmpty()){
                //qDebug() << "should display compound too because at this point it hasn't been displayed" << m_messageBuffer[prevOffset].compound.last().call;

                auto lastCompound = m_messageBuffer[prevOffset].compound.last();

                // fixup compound call incremental text
                d.text = QString("%1: %2").arg(lastCompound.call).arg(d.text);
                d.utcTimestamp = qMin(d.utcTimestamp, lastCompound.utcTimestamp);
            }


        } else if(d.isDirected && d.text.contains("<....>")){
            // if this is a _partial_ directed message, skip until the complete call comes through.
            continue;
        } else if(d.isDirected && d.text.contains(": HB ")){ // TODO: HEARTBEAT
            // if this is a heartbeat, process elsewhere...
            continue;
        }

        // if this is the first data frame of a standard message, parse the first word callsigns and spot them :)
        if((d.bits & Varicode::JS8CallFirst) == Varicode::JS8CallFirst && !d.isDirected && !d.isCompound){
            auto calls = Varicode::parseCallsigns(d.text);
            if(!calls.isEmpty()){
                auto theirCall = calls.first();
                if(d.text.startsWith(theirCall) && d.text.mid(theirCall.length(), 1) == ":"){
                    CallDetail cd = {};
                    cd.call = theirCall;
                    cd.freq = d.freq;
                    cd.snr = d.snr;
                    cd.bits = d.bits;
                    cd.tdrift = d.tdrift;
                    cd.utcTimestamp = d.utcTimestamp;
                    cd.mode = currentMode();
                    logCallActivity(cd, true);
                }
            }
        }


        // TODO: incremental printing of directed messages
        // Display if:
        // 1) this is a directed message header "to" us and should be buffered...
        // 2) or, this is a buffered message frame for a buffer with us as the recipient.

        if(!shouldDisplay){
            continue;
        }

        bool isFirst = (d.bits & Varicode::JS8CallFirst) == Varicode::JS8CallFirst;
        bool isLast = (d.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast;

        // if we're the last message, let's display our EOT character
        if (isLast) {
            d.text = QString("%1 %2 ").arg(Varicode::rstrip(d.text)).arg(m_config.eot());
        }

        // log it to the display!
        displayTextForFreq(d.text, d.freq, d.utcTimestamp, false, isFirst, isLast);

        // if we've received a message to be displayed, we should bump the repeat buttons...
        resetAutomaticIntervalTransmissions(true, false);

        if(isLast){
            clearOffsetDirected(d.freq);
        }

        if(isLast && !d.isBuffered){
            // buffered commands need the rxFrameBlockNumbers cache so it can fixup its display
            // all other "last" data frames can clear the rxFrameBlockNumbers cache so the next message will be on a new line.
            m_rxFrameBlockNumbers.remove(d.freq);
        }
    }

#if 0
    // TODO: this works but should also print in the rx window.
    foreach(auto offset, m_bandActivity.keys()){
        if(seen.contains(offset)){
            continue;
        }

        if(m_bandActivity[offset].isEmpty()){
            continue;
        }

        auto last = m_bandActivity[offset].last();
        if((last.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast){
            continue;
        }

        auto now = DriftingDateTime::currentDateTimeUtc();
        if(last.utcTimestamp.secsTo(now) < m_TRperiod){
            continue;
        }

        ActivityDetail d = {};
        d.text = " . . . ";
        d.isFree = true;
        d.utcTimestamp = now;
        d.snr = -99;

        m_bandActivity[offset].append(d);
    }
#endif
}

void MainWindow::processCompoundActivity() {
    if(m_messageBuffer.isEmpty()){
        return;
    }

    // group compound callsign and directed commands together.
    foreach(auto freq, m_messageBuffer.keys()) {
        QMap < int, MessageBuffer > ::iterator i = m_messageBuffer.find(freq);

        MessageBuffer & buffer = i.value();

        qDebug() << "-> grouping buffer for freq" << freq;

        if (buffer.compound.isEmpty()) {
            qDebug() << "-> buffer.compound is empty...skip";
            continue;
        }

        // if we don't have an initialized command, skip...
        int bits = buffer.cmd.bits;
        bool validBits = (
            bits == Varicode::JS8Call                                         ||
            ((bits & Varicode::JS8CallFirst)    == Varicode::JS8CallFirst)    ||
            ((bits & Varicode::JS8CallLast)     == Varicode::JS8CallLast)     ||
            ((bits & Varicode::JS8CallData)     == Varicode::JS8CallData)
        );
        if (!validBits) {
            qDebug() << "-> buffer.cmd bits is invalid...skip";
            continue;
        }

        // if we need two compound calls, but less than two have arrived...skip
        if (buffer.cmd.from == "<....>" && buffer.cmd.to == "<....>" && buffer.compound.length() < 2) {
            qDebug() << "-> buffer needs two compound, but has less...skip";
            continue;
        }

        // if we need one compound call, but non have arrived...skip
        if ((buffer.cmd.from == "<....>" || buffer.cmd.to == "<....>") && buffer.compound.length() < 1) {
            qDebug() << "-> buffer needs one compound, but has less...skip";
            continue;
        }

        if (buffer.cmd.from == "<....>") {
            auto d = buffer.compound.dequeue();
            buffer.cmd.from = d.call;
            buffer.cmd.grid = d.grid;
            buffer.cmd.isCompound = true;
            buffer.cmd.utcTimestamp = qMin(buffer.cmd.utcTimestamp, d.utcTimestamp);

            if ((d.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast) {
                buffer.cmd.bits = d.bits;
            }
        }

        if (buffer.cmd.to == "<....>") {
            auto d = buffer.compound.dequeue();
            buffer.cmd.to = d.call;
            buffer.cmd.isCompound = true;
            buffer.cmd.utcTimestamp = qMin(buffer.cmd.utcTimestamp, d.utcTimestamp);

            if ((d.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast) {
                buffer.cmd.bits = d.bits;
            }
        }

        if ((buffer.cmd.bits & Varicode::JS8CallLast) != Varicode::JS8CallLast) {
            qDebug() << "-> still not last message...skip";
            continue;
        }

        // fixup the datetime with the "minimum" dt seen
        // this will allow us to delete the activity lines
        // when the compound buffered command comes in.
        auto dt = buffer.cmd.utcTimestamp;
        foreach(auto c, buffer.compound){
            dt = qMin(dt, c.utcTimestamp);
        }
        foreach(auto m, buffer.msgs){
            dt = qMin(dt, m.utcTimestamp);
        }
        buffer.cmd.utcTimestamp = dt;

        qDebug() << "buffered compound command ready" << buffer.cmd.from << buffer.cmd.to << buffer.cmd.cmd;

        m_rxCommandQueue.append(buffer.cmd);
        m_messageBuffer.remove(freq);
    }
}

void MainWindow::processBufferedActivity() {
    if(m_messageBuffer.isEmpty()){
        return;
    }

    foreach(auto freq, m_messageBuffer.keys()) {
        auto buffer = m_messageBuffer[freq];

        // check to make sure we empty old buffers by getting the latest timestamp
        // and checking to see if it's older than one minute.
        auto dt = DriftingDateTime::currentDateTimeUtc().addDays(-1);
        if(buffer.cmd.utcTimestamp.isValid()){
            dt = qMax(dt, buffer.cmd.utcTimestamp);
        }
        if(!buffer.compound.isEmpty()){
            dt = qMax(dt, buffer.compound.last().utcTimestamp);
        }
        if(!buffer.msgs.isEmpty()){
            dt = qMax(dt, buffer.msgs.last().utcTimestamp);
        }
        if(dt.secsTo(DriftingDateTime::currentDateTimeUtc()) > 60 && !buffer.msgs.isEmpty()){
            // if the buffer has messages older than 1 minute, and we still haven't closed it, let's just mark it as the last frame
            buffer.msgs.last().bits |= Varicode::JS8CallLast;
        }
        if(dt.secsTo(DriftingDateTime::currentDateTimeUtc()) > 90){
            // but, if the buffer is older than 2 minutes, and we still haven't closed it, just remove it
            m_messageBuffer.remove(freq);
            continue;
        }

        if (buffer.msgs.isEmpty()) {
            continue;
        }

        if ((buffer.msgs.last().bits & Varicode::JS8CallLast) != Varicode::JS8CallLast) {
            continue;
        }

        QString message;
        foreach(auto part, buffer.msgs) {
            message.append(part.text);
        }
        message = Varicode::rstrip(message);

        QString checksum;

        bool valid = false;

        if(Varicode::isCommandBuffered(buffer.cmd.cmd)){
            int checksumSize = Varicode::isCommandChecksumed(buffer.cmd.cmd);

            if(checksumSize == 32) {
                message = Varicode::lstrip(message);
                checksum = message.right(6);
                message = message.left(message.length() - 7);
                valid = Varicode::checksum32Valid(checksum, message);
            } else if(checksumSize == 16) {
                message = Varicode::lstrip(message);
                checksum = message.right(3);
                message = message.left(message.length() - 4);
                valid = Varicode::checksum16Valid(checksum, message);
            } else if (checksumSize == 0) {
                valid = true;
            }
        } else {
            valid = true;
        }


        if (valid) {
            buffer.cmd.bits |= Varicode::JS8CallLast;
            buffer.cmd.text = message;
            buffer.cmd.isBuffered = true;
            m_rxCommandQueue.append(buffer.cmd);
        } else {
            qDebug() << "Buffered message failed checksum...discarding";
            qDebug() << "Checksum:" << checksum;
            qDebug() << "Message:" << message;
        }

        // regardless of valid or not, remove the "complete" buffered message from the buffer cache
        m_messageBuffer.remove(freq);
    }
}

QString MainWindow::generateStatus() {
    auto lastActive = DriftingDateTime::currentDateTimeUtc().addSecs(-m_idleMinutes*60);
    QString lastActiveString = since(lastActive).toUpper().replace("NOW", "0M");

    QStringList status;
    status.append(generateStatusFlags());

    if(!lastActiveString.isEmpty()){
        status.append(lastActiveString.trimmed());
    }

    status.append("V" + version().replace("-devel", "").replace("-rc", ""));

    return status.join(" ").trimmed();
}

QStringList MainWindow::generateStatusFlags() {
    QStringList flags;

    if(ui->hbMacroButton->isChecked() && m_hbInterval > 0){
        flags.append("HB");
    }

    if(ui->actionModeAutoreply->isChecked()){
        flags.append("AUTO");
    }

    if(ui->actionModeAutoreply->isChecked() && !m_config.relay_off()){
        flags.append("RELAY");
    }

    if(m_config.spot_to_reporting_networks()){
        flags.append("SPOT");
    }

    return flags;
}

void MainWindow::processCommandActivity() {
#if 0
    if (!m_txFrameQueue.isEmpty()) {
        return;
    }
#endif

    if (m_rxCommandQueue.isEmpty()) {
        return;
    }

#if 0
    bool processed = false;

    int f = currentFreq();
#endif

    auto now = DriftingDateTime::currentDateTimeUtc();

    while (!m_rxCommandQueue.isEmpty()) {
        auto d = m_rxCommandQueue.dequeue();

        auto selectedCallsign = callsignSelected();
        bool isAllCall = isAllCallIncluded(d.to);
        bool isGroupCall = isGroupCallIncluded(d.to);

        qDebug() << "try processing command" << d.from << d.to << d.cmd << d.freq << d.grid << d.extra << isAllCall << isGroupCall;

        // if we need a compound callsign but never got one...skip
        if (d.from == "<....>" || d.to == "<....>") {
            continue;
        }

        // we're only processing a subset of queries at this point
        if (!Varicode::isCommandAllowed(d.cmd)) {
            continue;
        }

        // is this to me?
        bool toMe = d.to == m_config.my_callsign().trimmed() || d.to == Radio::base_callsign(m_config.my_callsign()).trimmed();

        // log call activity...
        CallDetail cd = {};
        cd.call = d.from;
        cd.grid = d.grid;
        cd.snr = d.snr;
        cd.freq = d.freq;
        cd.bits = d.bits;
        cd.ackTimestamp = d.text.contains(": ACK") || toMe ? d.utcTimestamp : QDateTime{};
        cd.utcTimestamp = d.utcTimestamp;
        cd.tdrift = d.tdrift;
        cd.mode = currentMode();
        logCallActivity(cd, true);
        logHeardGraph(d.from, d.to);

        // PROCESS BUFFERED HEARING FOR EVERYONE
        if (d.cmd == " HEARING"){
            // 1. parse callsigns
            // 2. log it to the heard graph
            auto calls = Varicode::parseCallsigns(d.text);
            foreach(auto call, calls){
                logHeardGraph(d.from, call);
            }
        }

        // PROCESS BUFFERED GRID FOR EVERYONE
        if(d.cmd == " GRID"){
            // 1. parse grids
            // 2. log it to our call activity
            auto grids = Varicode::parseGrids(d.text);
            foreach(auto grid, grids){
                CallDetail cd = {};
                cd.bits = d.bits;
                cd.call = d.from;
                cd.freq = d.freq;
                cd.grid = grid;
                cd.snr = d.snr;
                cd.utcTimestamp = d.utcTimestamp;
                cd.tdrift = d.tdrift;
                cd.mode = currentMode();

                if(d.to == "@APRSIS"){
                    m_aprsCallCache.remove(cd.call);
                    m_aprsCallCache.remove(APRSISClient::replaceCallsignSuffixWithSSID(cd.call, Radio::base_callsign(cd.call)));
                }

                logCallActivity(cd, true);
            }
        }

        // PROCESS @JS8NET SPOTS FOR EVERYONE
        if (d.to == "@JS8NET"){
            spotCmd(d);
        }

        // PROCESS @APRSIS MSG SPOTS FOR EVERYONE
        if (d.to == "@APRSIS"){
            spotAPRSMsg(d);
        }

        // PREPARE CMD TEXT STRING
        QStringList textList = {
            QString("%1: %2%3").arg(d.from).arg(d.to).arg(d.cmd)
        };

        if(!d.extra.isEmpty()){
            textList.append(d.extra);
        }

        if(!d.text.isEmpty()){
            textList.append(d.text);
        }

        QString text = textList.join(" ");
        bool isLast = (d.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast;
        if (isLast) {
            // append the eot character to the text
            text = QString("%1 %2 ").arg(Varicode::rstrip(text)).arg(m_config.eot());
        }

        // log the text to directed txt log
        writeMsgTxt(text, d.snr);


        // we're only responding to allcall, groupcalls, and our callsign at this point, so we'll end after logging the callsigns we've heard
        if (!isAllCall && !toMe && !isGroupCall) {
            continue;
        }

        // we're only responding to allcalls if we are participating in the allcall group
        // but, don't avoid for heartbeats...those are technically allcalls but are processed differently
        if(isAllCall && m_config.avoid_allcall() && d.cmd != " HB"){
            continue;
        }

        ActivityDetail ad = {};
        ad.isLowConfidence = false;
        ad.isFree = true;
        ad.isDirected = true;
        ad.bits = d.bits;
        ad.freq = d.freq;
        ad.snr = d.snr;
        ad.text = text;
        ad.utcTimestamp = d.utcTimestamp;

        // we'd be double printing here if were on frequency, so let's be "smart" about this...
        bool shouldDisplay = true;

        // don't display ping allcalls
        if(isAllCall && (d.cmd != " " || ad.text.contains(": HB "))){
            shouldDisplay = false;
        }

        if(shouldDisplay){
            auto c = ui->textEditRX->textCursor();
            c.movePosition(QTextCursor::End);
            ui->textEditRX->setTextCursor(c);

            // ACKs are the most likely source of items to be overwritten (multiple responses at once)...
            // so don't overwrite those (i.e., print each on a new line)
            bool shouldOverwrite = (!d.cmd.contains(" ACK")); /* && isRecentOffset(d.freq);*/

            if(shouldOverwrite && ui->textEditRX->find(d.utcTimestamp.time().toString(), QTextDocument::FindBackward)){
                // ... maybe we could delete the last line that had this message on this frequency...
                c = ui->textEditRX->textCursor();
                c.movePosition(QTextCursor::StartOfBlock);
                c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                qDebug() << "should display directed message, erasing last rx activity line..." << c.selectedText().toUpper();
                c.removeSelectedText();
                c.deletePreviousChar();
                c.deletePreviousChar();
                /*
                c.deleteChar();
                c.deleteChar();
                */
            }

            // log it to the display!
            displayTextForFreq(ad.text, ad.freq, ad.utcTimestamp, false, true, false);

            // and send it to the network in case we want to interact with it from an external app...
            sendNetworkMessage("RX.DIRECTED", ad.text, {
                {"FROM", QVariant(d.from)},
                {"TO", QVariant(d.to)},
                {"CMD", QVariant(d.cmd)},
                {"GRID", QVariant(d.grid)},
                {"EXTRA", QVariant(d.extra)},
                {"TEXT", QVariant(d.text)},
                {"FREQ", QVariant(ad.freq)},
                {"SNR", QVariant(ad.snr)},
                {"UTC", QVariant(ad.utcTimestamp.toMSecsSinceEpoch())}
            });

            if(!isAllCall){
                // if we've received a message to be displayed, we should bump the repeat buttons...
                resetAutomaticIntervalTransmissions(true, false);

                // TODO: jsherer - notification for direct message?
            }
        }

        // we're only responding to callsigns in our whitelist if we have one defined...
        // make sure the whitelist is empty (no restrictions) or the from callsign or its base callsign is on it
        auto whitelist = m_config.auto_whitelist();
        if(!whitelist.isEmpty() && !(whitelist.contains(d.from) || whitelist.contains(Radio::base_callsign(d.from)))){
            qDebug() << "skipping command for whitelist" << d.from;
            continue;
        }

        // we'll never reply to a blacklisted callsign or base callsign
        auto blacklist = m_config.auto_blacklist();
        if(!blacklist.isEmpty() && (blacklist.contains(d.from) || blacklist.contains(Radio::base_callsign(d.from)))){
            qDebug() << "skipping command for blacklist" << d.from;
            continue;
        }

        // if this is an allcall, check to make sure we haven't replied to their allcall recently (in the past ten minutes)
        // that way we never get spammed by allcalls at too high of a frequency
        if (isAllCall && m_txAllcallCommandCache.contains(d.from) && m_txAllcallCommandCache[d.from]->secsTo(now) / 60 < 15) {
            qDebug() << "skipping command for allcall timeout" << d.from;
            continue;
        }

        // don't actually process any automatic message replies while in idle
        if(m_tx_watchdog){
            qDebug() << "skipping command for idle timeout" << d.from;
            continue;
        }

        // HACK: if this is an autoreply cmd and relay path is populated and cmd is not MSG or MSG TO:, then swap out the relay path
        if(Varicode::isCommandAutoreply(d.cmd) && !d.relayPath.isEmpty() && !d.cmd.startsWith(" MSG") && !d.cmd.startsWith(" QUERY")){
            d.from = d.relayPath;
        }

        // construct a reply, if needed
        QString reply;
        int priority = PriorityNormal;
        int freq = -1;

        // QUERIED SNR
        if (d.cmd == " SNR?" && !isAllCall) {
            reply = QString("%1 SNR %2").arg(d.from).arg(Varicode::formatSNR(d.snr));
        }

        // QUERIED INFO
        else if (d.cmd == " INFO?" && !isAllCall) {
            QString info = m_config.my_info();
            if (info.isEmpty()) {
                continue;
            }

            reply = QString("%1 INFO %2").arg(d.from).arg(replaceMacros(info, buildMacroValues(), true));
        }

        // QUERIED ACTIVE
        else if (d.cmd == " STATUS?" && !isAllCall) {
            reply = QString("%1 STATUS %2").arg(d.from).arg(generateStatus());
        }

        // QUERIED GRID
        else if (d.cmd == " GRID?" && !isAllCall) {
            QString grid = m_config.my_grid();
            if (grid.isEmpty()) {
                continue;
            }

            reply = QString("%1 GRID %2").arg(d.from).arg(grid);
        }

        // QUERIED STATIONS HEARD
        else if (d.cmd == " HEARING?" && !isAllCall) {
            int i = 0;
            int maxStations = 4;
            auto calls = m_callActivity.keys();
            qStableSort(calls.begin(), calls.end(), [this](QString
                const & a, QString
                const & b) {
                auto left = m_callActivity[a];
                auto right = m_callActivity[b];
                return right.utcTimestamp < left.utcTimestamp;
            });

            QStringList lines;

            int callsignAging = m_config.callsign_aging();

            foreach(auto call, calls) {
                if (i >= maxStations) {
                    break;
                }

                if(call == d.from){
                    continue;
                }

                auto cd = m_callActivity[call];
                if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
                    continue;
                }

                lines.append(cd.call);

                i++;
            }

            lines.prepend(QString("%1 HEARING").arg(d.from));
            reply = lines.join(' ');
        }

        // PROCESS RELAY
        else if (d.cmd == ">" && !isAllCall && !m_config.relay_off()) {

            // 1. see if there are any more hops to process
            // 2. if so, forward
            // 3. otherwise, display alert & reply dialog

            QString callToPattern = {R"(^(?<callsign>\b(?<prefix>[A-Z0-9]{1,4}\/)?(?<base>([0-9A-Z])?([0-9A-Z])([0-9])([A-Z])?([A-Z])?([A-Z])?)(?<suffix>\/[A-Z0-9]{1,4})?(?<type>[> ]))\b)"};
            QRegularExpression re(callToPattern);
            auto text = d.text;
            auto match = re.match(text);

            // if the text starts with a callsign, and relay is not disabled, and this is not a group callsign, then relay.
            if(match.hasMatch() && !isGroupCall){
                // replace freetext with relayed free text
                if(match.captured("type") != ">"){
                    text = text.replace(match.capturedStart("type"), match.capturedLength("type"), ">");
                }
                reply = QString("%1 VIA %2").arg(text).arg(d.from);

            // otherwise, as long as we're not an ACK...alert the user and either send an ACK or Message
            } else if(!d.text.startsWith("ACK")) {

                // parse out the callsign path
                auto calls = parseRelayPathCallsigns(d.from, d.text);

                // put these third party calls in the heard list
                foreach(auto call, calls){
                    CallDetail cd = {};
                    cd.call = call;
                    cd.snr = -64;
                    cd.freq = d.freq;
                    cd.through = d.from;
                    cd.utcTimestamp = DriftingDateTime::currentDateTimeUtc();
                    cd.tdrift = d.tdrift;
                    cd.mode = currentMode();
                    logCallActivity(cd, false);
                }

                d.relayPath = calls.join('>');

                reply = QString("%1 ACK").arg(d.relayPath);

                // check to see if the relay text contains a command that should be replied to instead of an ack.
                QStringList relayedCmds = d.text.split(" ");
                if(!relayedCmds.isEmpty()){
                    auto first = relayedCmds.first();

                    auto valid = Varicode::isCommandAllowed(first);
                    if(!valid){
                        first = " " + first;
                        valid = Varicode::isCommandAllowed(first);
                        if(valid){
                            relayedCmds.removeFirst();
                        }
                    }

                    // HACK: "MSG TO:" should be supported but contains a space :(
                    if(!relayedCmds.isEmpty()){
                        if(first == " MSG"){
                            auto second = relayedCmds.first();
                            if(second == "TO:"){
                                first = " MSG TO:";
                                relayedCmds.removeFirst();
                            } else if(second.startsWith("TO:")){
                                first = " MSG TO:";
                                relayedCmds.replace(0, second.mid(3));
                            }
                        } else if (first == " QUERY"){
                            auto second = relayedCmds.first();
                            if(second == "MSGS" || second == "MSGS?"){
                                first = " QUERY MSGS";
                                relayedCmds.removeFirst();
                            }
                            else if(second == "CALL"){
                                first = " QUERY CALL";
                                relayedCmds.removeFirst();
                            }
                        }
                    }

                    if(Varicode::isCommandAllowed(first) && Varicode::isCommandAutoreply(first)){
                        CommandDetail rd = {};
                        rd.bits = d.bits;
                        rd.cmd = first;
                        rd.freq = d.freq;
                        rd.from = d.from; // note, MSG and QUERY commands are not set with from as the relay path.
                        rd.relayPath = d.relayPath;
                        rd.text = relayedCmds.join(" "); //d.text;
                        rd.to = d.to;
                        rd.utcTimestamp = d.utcTimestamp;

                        m_rxCommandQueue.insert(0, rd);
                        continue;
                    }
                }

#if STORE_RELAY_MSGS_TO_INBOX
                // if we make it here, this is a message
                addCommandToMyInbox(d);
#endif
            }
        }

        // PROCESS MESSAGE STORAGE
        else if (d.cmd == " MSG TO:" && !isAllCall && !isGroupCall && !m_config.relay_off()){

            // store message
            QStringList segs = d.text.split(" ");
            if(segs.isEmpty()){
                continue;
            }

            auto to = segs.first();
            segs.removeFirst();

            auto text = segs.join(" ").trimmed();

            auto calls = parseRelayPathCallsigns(d.from, text);
            d.relayPath = calls.join(">");

            CommandDetail cd = {};
            cd.bits = d.bits;
            cd.cmd = d.cmd;
            cd.extra = d.extra;
            cd.freq = d.freq;
            cd.from = d.from;
            cd.grid = d.grid;
            cd.relayPath = d.relayPath;
            cd.snr = d.snr;
            cd.tdrift = d.tdrift;
            cd.text = text;
            cd.to = Radio::base_callsign(to);
            cd.utcTimestamp = d.utcTimestamp;

            qDebug() << "storing message to" << to << ":" << text;

            addCommandToStorage("STORE", cd);

            // we haven't replaced the from with the relay path, so we have to use it for the ack if there is one
            reply = QString("%1 ACK").arg(calls.length() > 1 ? d.relayPath : d.from);
        }

        // PROCESS AGN
        else if (d.cmd == " AGN?" && !isAllCall && !isGroupCall && !m_lastTxMessage.isEmpty()) {
            reply = m_lastTxMessage;
        }

        // PROCESS ACTIVE HEARTBEAT
        // if we have hb mode enabled and auto reply enabled <del>and auto ack enabled and no callsign is selected</del> update: if we're in HB mode, doesn't matter if a callsign is selected.
        else if (d.cmd == " HB" && ui->actionModeJS8HB->isEnabled() && ui->actionModeJS8HB->isChecked() && ui->actionModeAutoreply->isChecked() && ui->actionHeartbeatAcknowledgements->isChecked()){
            // check to make sure we aren't pausing HB transmissions (ACKs) while a callsign is selected
            if(m_config.heartbeat_qso_pause() && !selectedCallsign.isEmpty()){
                qDebug() << "hb paused during qso";
                continue;
            }

            // check to make sure this callsign isn't blacklisted
            if(m_config.hb_blacklist().contains(d.from) || m_config.hb_blacklist().contains(Radio::base_callsign(d.from))){
                qDebug() << "hb blacklist blocking" << d.from;
                continue;
            }

            // check to see if we have a message for a station who is heartbeating
            QString extra;
            auto mid = getNextMessageIdForCallsign(d.from);
            if(mid != -1){
                extra = QString("MSG ID %1").arg(mid);
            }

            sendHeartbeatAck(d.from, d.snr, extra);

            if(isAllCall){
                // since all pings are technically @ALLCALL, let's bump the allcall cache here...
                m_txAllcallCommandCache.insert(d.from, new QDateTime(now), 5);
            }

            continue;
        }

        // PROCESS MSG
        else if (d.cmd == " MSG" && !isAllCall){

            auto text = d.text;

            qDebug() << "adding message to inbox" << text;

            auto calls = parseRelayPathCallsigns(d.from, text);

            d.cmd = " MSG ";
            d.relayPath = calls.join(">");
            d.text = text;

            addCommandToMyInbox(d);

            // we haven't replaced the from with the relay path, so we have to use it for the ack if there is one
            reply = QString("%1 ACK").arg(calls.length() > 1 ? d.relayPath : d.from);

#define SHOW_ALERT_FOR_MSG 1
#if SHOW_ALERT_FOR_MSG
            SelfDestructMessageBox * m = new SelfDestructMessageBox(300,
              "New Message Received",
              QString("A new message was received at %1 UTC from %2").arg(d.utcTimestamp.time().toString()).arg(d.from),
              QMessageBox::Information,
              QMessageBox::Ok,
              QMessageBox::Ok,
              false,
              this);

            m->show();
#endif
        }

        // PROCESS ACKS
        else if (d.cmd == " ACK" && !isAllCall){
            qDebug() << "skipping incoming ack" << d.text;

            // make sure this is explicit
            continue;
        }

        // PROCESS NACKS
        else if (d.cmd == " NACK" && !isAllCall){
            qDebug() << "skipping incoming nack" << d.text;

            // make sure this is explicit
            continue;
        }

        // PROCESS BUFFERED CMD
        else if (d.cmd == " CMD" && !isAllCall){
            qDebug() << "skipping incoming command" << d.text;

            // make sure this is explicit
            continue;
        }

        // PROCESS BUFFERED QUERY
        else if (d.cmd == " QUERY" && !isAllCall){
            auto who = d.from; // keep in mind, this is the sender, not the original requestor if relayed
            auto replyPath = d.from;

            if(d.relayPath.contains(">")){
                auto path = d.relayPath.split(">");
                who = path.last();
                replyPath = d.relayPath;
            }

            QStringList segs = d.text.split(" ");
            if(segs.isEmpty()){
                continue;
            }

            auto cmd = segs.first();
            segs.removeFirst();

            if(cmd == "MSG" && !segs.isEmpty()){
                auto inbox = Inbox(inboxPath());
                if(!inbox.open()){
                    continue;
                }

                bool ok = false;
                int mid = QString(segs.first()).toInt(&ok);
                if(!ok){
                    continue;
                }

                auto msg = inbox.value(mid);
                auto params = msg.params();
                if(params.isEmpty()){
                    continue;
                }

                auto from = params.value("FROM").toString().trimmed();

                auto to = params.value("TO").toString().trimmed();
                if(to != who && to != Radio::base_callsign(who)){
                    continue;
                }

                auto text = params.value("TEXT").toString().trimmed();
                if(text.isEmpty()){
                    continue;
                }

                // mark as delivered (so subsequent HBs and QUERY MSGS don't receive this message)
                msg.setType("DELIVERED");
                inbox.set(mid, msg);

                // and reply
                reply = QString("%1 MSG %2 FROM %3");
                reply = reply.arg(replyPath);
                reply = reply.arg(text);
                reply = reply.arg(from);
            }
        }

        // PROCESS BUFFERED QUERY MSGS
        else if (d.cmd == " QUERY MSGS" && ui->actionModeAutoreply->isChecked()){
            auto who = d.from; // keep in mind, this is the sender, not the original requestor if relayed
            auto replyPath = d.from;

            if(d.relayPath.contains(">")){
                auto path = d.relayPath.split(">");
                who = path.last();
                replyPath = d.relayPath;
            }

            // if this is an allcall or a directed call, check to see if we have a stored message for user.
            // we reply yes if the user would be able to retreive a stored message
            auto mid = getNextMessageIdForCallsign(who);
            if(mid != -1){
                reply = QString("%1 YES MSG ID %2").arg(replyPath).arg(mid);
            }

            // if this is not an allcall and we have no messages, reply no.
            if(!isAllCall && reply.isEmpty()){
                reply = QString("%1 NO").arg(replyPath);
            }
        }

        // PROCESS BUFFERED QUERY CALL
        else if (d.cmd == " QUERY CALL" && ui->actionModeAutoreply->isChecked()){
            auto replyPath = d.from;
            if(d.relayPath.contains(">")){
                replyPath = d.relayPath;
            }

            auto who = d.text;
            if(who.isEmpty()){
                continue;
            }

            auto callsigns = Varicode::parseCallsigns(who);
            if(callsigns.isEmpty()){
                continue;
            }

            QStringList replies;
            int callsignAging = m_config.callsign_aging();
            auto baseCall = callsigns.first();
            foreach(auto cd, m_callActivity.values()){
                if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
                    continue;
                }

                if(baseCall == cd.call || baseCall == Radio::base_callsign(cd.call)){
                    auto r = QString("%1 (%2)").arg(Varicode::formatSNR(cd.snr)).arg(since(cd.utcTimestamp)).trimmed();
                    replies.append(r);
                    break;
                }
            }

            if(!replies.isEmpty()){
                replies.prepend(QString("%1 YES").arg(replyPath));
            }

            reply = replies.join(" ");

            if(!reply.isEmpty()){
                if(isAllCall){
                    m_txAllcallCommandCache.insert(d.from, new QDateTime(now), 25);
                }
            }
        }

#if 0
        // PROCESS ALERT
        else if (d.cmd == "!" && !isAllCall) {

            // create alert dialog
            processAlertReplyForCommand(d, d.from, " ");

            // make sure this is explicit
            continue;
        }
#endif

        // well, if there's no reply, don't do anything...
        if (reply.isEmpty()) {
            continue;
        }

        // do not queue @ALLCALL replies if auto-reply is not checked
        if(!ui->actionModeAutoreply->isChecked() && isAllCall){
            continue;
        }

#if 0
        // TODO: jsherer - HB issue here
        // do not queue a reply if it's a HB and HB is not active
        // if((!ui->hbMacroButton->isChecked() || m_hbInterval <= 0) && d.cmd.contains("HB")){
        //     continue;
        // }
#endif

        // do not queue for reply if there's text in the window
        if(!ui->extFreeTextMsgEdit->toPlainText().isEmpty()){
            continue;
        }

        // do not queue for reply if there's a buffer open to us
        int bufferOffset = 0;
        if(hasExistingMessageBufferToMe(&bufferOffset)){

            qDebug() << "skipping reply due to open buffer" << bufferOffset << m_messageBuffer.count();

            continue;
        }

        // add @ALLCALLs to the @ALLCALL cache
        if(isAllCall){
            m_txAllcallCommandCache.insert(d.from, new QDateTime(now), 25);
        }

        // queue the reply here to be sent when a free interval is available on the frequency that was sent
        // unless, this is an allcall, to which we should be responding on a clear frequency offset
        // we always want to make sure that the directed cache has been updated at this point so we have the
        // most information available to make a frequency selection.
        enqueueMessage(priority, reply, freq, nullptr);
    }
}

QString MainWindow::inboxPath(){
    return QDir::toNativeSeparators(m_config.writeable_data_dir().absoluteFilePath("inbox.db3"));
}

void MainWindow::refreshInboxCounts(){
    auto inbox = Inbox(inboxPath());
    if(inbox.open()){
        // reset inbox counts
        m_rxInboxCountCache.clear();

        // compute new counts from db
        auto v = inbox.values("UNREAD", "$", "%", 0, 10000);
        foreach(auto pair, v){
            auto params = pair.second.params();
            auto to = params.value("TO").toString();
            if(to.isEmpty() || (to != m_config.my_callsign() && to != Radio::base_callsign(m_config.my_callsign()))){
                continue;
            }
            auto from = params.value("FROM").toString();
            if(from.isEmpty()){
                continue;
            }

            m_rxInboxCountCache[from] = m_rxInboxCountCache.value(from, 0) + 1;

            if(!m_callActivity.contains(from)){
                auto utc = params.value("UTC").toString();
                auto snr = params.value("SNR").toInt();
                auto freq = params.value("OFFSET").toInt();
                auto tdrift = params.value("TDRIFT").toInt();

                CallDetail cd;
                cd.call = from;
                cd.snr = snr;
                cd.freq = freq;
                cd.tdrift = tdrift;
                cd.utcTimestamp = QDateTime::fromString(utc, "yyyy-MM-dd hh:mm:ss");
                cd.utcTimestamp.setUtcOffset(0);
                cd.ackTimestamp = cd.utcTimestamp;
                cd.mode = currentMode();
                logCallActivity(cd, false);
            }
        }
    }
}

bool MainWindow::hasMessageHistory(QString call){
    auto inbox = Inbox(inboxPath());
    if(!inbox.open()){
        return false;
    }

    int store = inbox.count("STORE", "$.params.TO", call);
    int unread = inbox.count("UNREAD", "$.params.FROM", call);
    int read = inbox.count("READ", "$.params.FROM", call);
    return (store + unread + read) > 0;
}

int MainWindow::addCommandToMyInbox(CommandDetail d){
    // local cache for inbox count
    m_rxInboxCountCache[d.from] = m_rxInboxCountCache.value(d.from, 0) + 1;

    // add it to my unread inbox
    return addCommandToStorage("UNREAD", d);
}

int MainWindow::addCommandToStorage(QString type, CommandDetail d){
    // inbox:
    auto inbox = Inbox(inboxPath());
    if(!inbox.open()){
        return -1;
    }

    auto df = dialFrequency();

    QMap<QString, QVariant> v = {
        {"UTC", QVariant(d.utcTimestamp.toString("yyyy-MM-dd hh:mm:ss"))},
        {"TO", QVariant(d.to)},
        {"FROM", QVariant(d.from)},
        {"PATH", QVariant(d.relayPath)},
        {"DIAL", QVariant((quint64)df)},
        {"TDRIFT", QVariant(d.tdrift)},
        {"OFFSET", QVariant(d.freq)},
        {"CMD", QVariant(d.cmd)},
        {"SNR", QVariant(d.snr)},
    };

    if(!d.grid.isEmpty()){
        v["GRID"] = QVariant(d.grid);
    }

    if(!d.extra.isEmpty()){
        v["EXTRA"] = QVariant(d.extra);
    }

    if(!d.text.isEmpty()){
        v["TEXT"] = QVariant(d.text);
    }

    auto m = Message(type, "", v);

    return inbox.append(m);
}

int MainWindow::getNextMessageIdForCallsign(QString callsign){
    auto inbox = Inbox(inboxPath());
    if(!inbox.open()){
        return -1;
    }

    auto v1 = inbox.values("STORE", "$.params.TO", callsign, 0, 10);
    foreach(auto pair, v1){
        auto params = pair.second.params();
        auto text = params.value("TEXT").toString().trimmed();
        if(!text.isEmpty()){
            return pair.first;
        }
    }

    auto v2 = inbox.values("STORE", "$.params.TO", Radio::base_callsign(callsign), 0, 10);
    foreach(auto pair, v2){
        auto params = pair.second.params();
        auto text = params.value("TEXT").toString().trimmed();
        if(!text.isEmpty()){
            return pair.first;
        }
    }

    return -1;
}

QStringList MainWindow::parseRelayPathCallsigns(QString from, QString text){
    QStringList calls;
    QString callDePattern = {R"(\sVIA\s(?<callsign>\b(?<prefix>[A-Z0-9]{1,4}\/)?(?<base>([0-9A-Z])?([0-9A-Z])([0-9])([A-Z])?([A-Z])?([A-Z])?)(?<suffix>\/[A-Z0-9]{1,4})?)\b)"};
    QRegularExpression re(callDePattern);
    auto iter = re.globalMatch(text);
    while(iter.hasNext()){
        auto match = iter.next();
        calls.prepend(match.captured("callsign"));
    }
    calls.prepend(from);
    return calls;
}

void MainWindow::processAlertReplyForCommand(CommandDetail d, QString from, QString cmd){
    QMessageBox * msgBox = new QMessageBox(this);
    msgBox->setIcon(QMessageBox::Information);

    QList<QString> calls = listCopyReverse<QString>(from.split(">"));
    auto fromLabel = calls.join(" via ");

    calls.removeLast();

    QString fromReplace = QString{};
    foreach(auto call, calls){
        fromReplace.append(" VIA ");
        fromReplace.append(call);
    }

    auto text = d.text;
    if(!fromReplace.isEmpty()){
        text = text.replace(fromReplace, "");
    }

    auto header = QString("Message from %3 at %1 UTC (%2):");
    header = header.arg(d.utcTimestamp.time().toString());
    header = header.arg(d.freq);
    header = header.arg(fromLabel);
    msgBox->setText(header);
    msgBox->setInformativeText(text);

    auto rb = msgBox->addButton("Reply", QMessageBox::AcceptRole);
    auto db = msgBox->addButton("Discard", QMessageBox::NoRole);

    connect(msgBox, &QMessageBox::buttonClicked, this, [this, cmd, from, fromLabel, d, db, rb](QAbstractButton * btn) {
        if (btn == db) {
            displayCallActivity();
            return;
        }

        if(btn == rb){
#if USE_RELAY_REPLY_DIALOG
            auto diag = new MessageReplyDialog(this);
            diag->setWindowTitle("Message Reply");
            diag->setLabel(QString("Message to send to %1:").arg(fromLabel));

            connect(diag, &MessageReplyDialog::accepted, this, [this, diag, from, cmd, d](){
                enqueueMessage(PriorityHigh, QString("%1%2%3").arg(from).arg(cmd).arg(diag->textValue()), -1, nullptr);
            });

            diag->show();
#else
            addMessageText(QString("%1%2[MESSAGE]").arg(from).arg(cmd), true, true);
#endif
        }
    });

    // TODO: jsherer - notification for alert?

    msgBox->setModal(false);
    msgBox->show();
}

void MainWindow::processSpots() {
    if(!m_config.spot_to_reporting_networks()){
        m_rxCallQueue.clear();
        return;
    }

    if(m_rxCallQueue.isEmpty()){
        return;
    }

    // Is it ok to post spots to PSKReporter?
    int nsec = DriftingDateTime::currentMSecsSinceEpoch() / 1000 - m_secBandChanged;
    bool okToPost = (nsec > (4 * m_TRperiod) / 5);
    if (!okToPost) {
        return;
    }

    // Get the dial frequency to report
    auto dial = dialFrequency();

    while(!m_rxCallQueue.isEmpty()){
        CallDetail d = m_rxCallQueue.dequeue();
        if(d.call.isEmpty()){
            continue;
        }

        qDebug() << "spotting call to reporting networks" << d.call << d.snr << d.freq;

        spotReport(d.freq, d.snr, d.call, d.grid);
        pskLogReport("JS8", d.freq, d.snr, d.call, d.grid);
        aprsLogReport(d.freq, d.snr, d.call, d.grid);

        sendNetworkMessage("RX.SPOT", "", {
            {"DIAL", QVariant(dial)},
            {"OFFSET", QVariant(d.freq)},
            {"CALL", QVariant(d.call)},
            {"SNR", QVariant(d.snr)},
            {"GRID", QVariant(d.grid)},
        });
    }
}

void MainWindow::processTxQueue(){
#if IDLE_BLOCKS_TX
    if(m_tx_watchdog){
        return;
    }
#endif

    if(m_txMessageQueue.isEmpty()){
        return;
    }

    // grab the next message...
    auto head = m_txMessageQueue.head();

    // decide if it's ok to transmit...
    int f = head.freq;
    if(f == -1){
        f = currentFreqOffset();
    }

    // we need a valid frequency...
    if(f <= 0){
        return;
    }

    // tx frame queue needs to be empty...
    if(!m_txFrameQueue.isEmpty()){
        return;
    }

    // our message box needs to be empty...
    if(!ui->extFreeTextMsgEdit->toPlainText().isEmpty()){
        return;
    }

    // and if we are a low priority message, we need to have not transmitted in the past 30 seconds...
    if(head.priority <= PriorityLow && m_lastTxTime.secsTo(DriftingDateTime::currentDateTimeUtc()) <= 30){
        return;
    }

    // if so... dequeue the next message from the queue...
    auto message = m_txMessageQueue.dequeue();

    // add the message to the outgoing message text box
    addMessageText(message.message, true);

    // check to see if this is a high priority message, or if we have autoreply enabled, or if this is a ping and the ping button is enabled
    if(message.priority >= PriorityHigh   ||
       message.message.contains(" HB ")   ||
       message.message.contains(" ACK ")  ||
       ui->actionModeAutoreply->isChecked()
    ){
        // then try to set the frequency...
        setFreqOffsetForRestore(f, true);

        // then prepare to transmit...
        toggleTx(true);
    }

    if(message.callback){
        message.callback();
    }
}

void MainWindow::displayActivity(bool force) {
    if (!m_rxDisplayDirty && !force) {
        return;
    }

    // Band Activity
    displayBandActivity();

    // Call Activity
    displayCallActivity();

    m_rxDisplayDirty = false;
}

// updateBandActivity
void MainWindow::displayBandActivity() {
    auto now = DriftingDateTime::currentDateTimeUtc();

    ui->tableWidgetRXAll->setFont(m_config.table_font());

    // Selected Offset
    int selectedOffset = -1;
    auto selectedItems = ui->tableWidgetRXAll->selectedItems();
    if (!selectedItems.isEmpty()) {
        selectedOffset = selectedItems.first()->data(Qt::UserRole).toInt();
    }

    ui->tableWidgetRXAll->setUpdatesEnabled(false);
    {
        // Scroll Position
        auto currentScrollPos = ui->tableWidgetRXAll->verticalScrollBar()->value();

        // Clear the table
        clearTableWidget(ui->tableWidgetRXAll);

        // Sort!
        QList < int > keys = m_bandActivity.keys();

        auto sortBy = getSortBy("bandActivity", "offset");
        bool reverse = false;
        if(sortBy.startsWith("-")){
            sortBy = sortBy.mid(1);
            reverse = true;
        }

        auto compareTimestamp = [this](const int left, int right) {
            auto leftItems = m_bandActivity[left];
            auto rightItems = m_bandActivity[right];

            if(leftItems.isEmpty()){
                return false;
            }

            if(rightItems.isEmpty()){
                return true;
            }

            auto leftLast = leftItems.last();
            auto rightLast = rightItems.last();

            return leftLast.utcTimestamp < rightLast.utcTimestamp;
        };

        auto compareSNR = [this, reverse](const int left, int right) {
            auto leftItems = m_bandActivity[left];
            auto rightItems = m_bandActivity[right];

            if(leftItems.isEmpty()){
                return false;
            }

            if(rightItems.isEmpty()){
                return true;
            }

            auto leftActivity = leftItems.last();
            auto rightActivity = rightItems.last();

            if(leftActivity.snr < -60 || leftActivity.snr > 60) {
                leftActivity.snr *= reverse ? 1 : -1;
            }
            if(rightActivity.snr < -60 || rightActivity.snr > 60) {
                rightActivity.snr *= reverse ? 1 : -1;
            }

            return leftActivity.snr < rightActivity.snr;
        };

        // compare offset
        qStableSort(keys.begin(), keys.end());

        if(sortBy == "timestamp"){
            qStableSort(keys.begin(), keys.end(), compareTimestamp);
        } else if(sortBy == "snr"){
            qStableSort(keys.begin(), keys.end(), compareSNR);
        }

        if(reverse){
            keys = listCopyReverse(keys);
        }

        // Build the table
        foreach(int offset, keys) {
            bool isOffsetSelected = (offset == selectedOffset);

            QList < ActivityDetail > items = m_bandActivity[offset];
            if (items.length() > 0) {
                QDateTime timestamp;
                QStringList text;
                QString age;
                int snr = 0;
                float tdrift = 0;
                QString mode;

                int activityAging = m_config.activity_aging();

                // hide items that shouldn't appear

                for(int i = 0; i < items.length(); i++){
                    auto item = items[i];

                    bool shouldDisplay = true;

                    // hide aged items
                    if (!isOffsetSelected && activityAging && item.utcTimestamp.secsTo(now) / 60 >= activityAging) {
                        shouldDisplay = false;
                    }

                    // hide heartbeat items
                    if (!ui->actionShow_Band_Heartbeats_and_ACKs->isChecked()){
                        // hide heartbeats and acks if we have heartbeating hidden
                        if(item.text.contains(" HB ") || item.text.contains(" ACK ")){
                            shouldDisplay = false;
                        }

                        // if our previous item should not be displayed and we have a MSG ID, then don't display it either.
                        if(
                           (i == 0 || (i > 0 && !items[i-1].shouldDisplay)) &&
                           (item.text.contains(" MSG ID "))
                        ){
                            shouldDisplay = false;
                        }
                    }

                    // hide empty items
                    if (item.text.isEmpty()) {
                        shouldDisplay = false;
                    }

                    // set the visibility of the item
                    items[i].shouldDisplay = shouldDisplay;
                }

                // show the items that should appear
                foreach(ActivityDetail item, items) {
                    if(!item.shouldDisplay){
                        continue;
                    }

                    if (item.isLowConfidence) {
                        item.text = QString("[%1]").arg(item.text);
                    }

                    if ((item.bits & Varicode::JS8CallLast) == Varicode::JS8CallLast) {
                        // append the eot character to the text
                        item.text = QString("%1 %2 ").arg(Varicode::rstrip(item.text)).arg(m_config.eot());
                    }
                    text.append(item.text);
                    snr = item.snr;
                    age = since(item.utcTimestamp);
                    timestamp = item.utcTimestamp;
                    tdrift = item.tdrift;
                    mode = item.mode;
                }

                auto joined = Varicode::rstrip(text.join(""));
                if (joined.isEmpty()) {
                    continue;
                }

                ui->tableWidgetRXAll->insertRow(ui->tableWidgetRXAll->rowCount());
                int row = ui->tableWidgetRXAll->rowCount() - 1;
                int col = 0;

                auto offsetItem = new QTableWidgetItem(QString("%1 Hz").arg(offset));
                offsetItem->setData(Qt::UserRole, QVariant(offset));
                ui->tableWidgetRXAll->setItem(row, col++, offsetItem);

                auto ageItem = new QTableWidgetItem(age);
                ageItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
                ageItem->setToolTip(timestamp.toString());
                ui->tableWidgetRXAll->setItem(row, col++, ageItem);

                auto snrText = Varicode::formatSNR(snr);
                auto snrItem = new QTableWidgetItem(snrText.isEmpty() ? "" : QString("%1 dB").arg(snrText));
                snrItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
                ui->tableWidgetRXAll->setItem(row, col++, snrItem);

                auto tdriftItem = new QTableWidgetItem(QString("%1 ms").arg((int)(1000*tdrift)));
                tdriftItem->setData(Qt::UserRole, QVariant(tdrift));
                ui->tableWidgetRXAll->setItem(row, col++, tdriftItem);

                auto modeItem = new QTableWidgetItem(mode.left(1).replace("H", "N"));
                modeItem->setToolTip(mode);
                modeItem->setData(Qt::UserRole, QVariant(mode));
                modeItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                ui->tableWidgetRXAll->setItem(row, col++, modeItem);

                // align right if eliding...
                int colWidth = ui->tableWidgetRXAll->columnWidth(3);
                auto textItem = new QTableWidgetItem(joined);
                auto html = QString("<qt/>%1").arg(joined.toHtmlEscaped());
                html = html.replace(m_config.eot(), m_config.eot() + "<br/><br/>");
                html = html.replace(QRegularExpression("([<]br[/][>])+$"), "");
                textItem->setToolTip(html);

                QFontMetrics fm(textItem->font());
                auto elidedText = fm.elidedText(joined, Qt::ElideLeft, colWidth);
                auto flag = Qt::AlignLeft | Qt::AlignVCenter;
                if (elidedText != joined) {
                    flag = Qt::AlignRight | Qt::AlignVCenter;
                    textItem->setText(joined);
                }
                textItem->setTextAlignment(flag);

                ui->tableWidgetRXAll->setItem(row, col++, textItem);

                if (isOffsetSelected) {
                    for(int i = 0; i < ui->tableWidgetRXAll->columnCount(); i++){
                        ui->tableWidgetRXAll->item(row, i)->setSelected(true);
                    }
                }

                bool isDirectedAllCall = false;
                if(
                    (isDirectedOffset(offset, &isDirectedAllCall) && !isDirectedAllCall) || isMyCallIncluded(text.last())
                ){
                    for(int i = 0; i < ui->tableWidgetRXAll->columnCount(); i++){
                        ui->tableWidgetRXAll->item(row, i)->setBackground(QBrush(m_config.color_MyCall()));
                    }
                }

                if(!text.isEmpty()){
                    auto words = QSet<QString>::fromList(joined.replace(":", " ").replace(">"," ").split(" "));

                    if(words.contains("CQ")){
                        for(int i = 0; i < ui->tableWidgetRXAll->columnCount(); i++){
                            ui->tableWidgetRXAll->item(row, i)->setBackground(QBrush(m_config.color_CQ()));
                        }
                    }

                    auto matchingSecondaryWords = m_config.secondary_highlight_words() & words;
                    if (!matchingSecondaryWords.isEmpty()){
                        for(int i = 0; i < ui->tableWidgetRXAll->columnCount(); i++){
                            ui->tableWidgetRXAll->item(row, i)->setBackground(QBrush(m_config.color_secondary_highlight()));
                        }
                    }

                    auto matchingPrimaryWords = m_config.primary_highlight_words() & words;
                    if (!matchingPrimaryWords.isEmpty()){
                        for(int i = 0; i < ui->tableWidgetRXAll->columnCount(); i++){
                            ui->tableWidgetRXAll->item(row, i)->setBackground(QBrush(m_config.color_primary_highlight()));
                        }
                    }
                }
            }
        }

        // Set table color
        auto style = QString("QTableWidget { background:%1; selection-background-color:%2; alternate-background-color:%1; color:%3; }");
        style = style.arg(m_config.color_table_background().name());
        style = style.arg(m_config.color_table_highlight().name());
        style = style.arg(m_config.color_table_foreground().name());
        ui->tableWidgetRXAll->setStyleSheet(style);

        // Set the table palette for inactive selected row
        auto p = ui->tableWidgetRXAll->palette();
        p.setColor(QPalette::Inactive, QPalette::Highlight, p.color(QPalette::Active, QPalette::Highlight));
        ui->tableWidgetRXAll->setPalette(p);

        // Set item fonts
        for(int row = 0; row < ui->tableWidgetRXAll->rowCount(); row++){
            for(int col = 0; col < ui->tableWidgetRXAll->columnCount(); col++){
                auto item = ui->tableWidgetRXAll->item(row, col);
                if(item){
                    item->setFont(m_config.table_font());
                }
            }
        }

        // Column labels
        ui->tableWidgetRXAll->horizontalHeader()->setVisible(showColumn("band", "labels"));

        // Hide columns
        ui->tableWidgetRXAll->setColumnHidden(0, !showColumn("band", "offset"));
        ui->tableWidgetRXAll->setColumnHidden(1, !showColumn("band", "timestamp"));
        ui->tableWidgetRXAll->setColumnHidden(2, !showColumn("band", "snr"));
        ui->tableWidgetRXAll->setColumnHidden(3, !showColumn("band", "tdrift", false));
        ui->tableWidgetRXAll->setColumnHidden(4, !showColumn("band", "mode", false));

        // Resize the table columns
        ui->tableWidgetRXAll->resizeColumnToContents(0);
        ui->tableWidgetRXAll->resizeColumnToContents(1);
        ui->tableWidgetRXAll->resizeColumnToContents(2);
        ui->tableWidgetRXAll->resizeColumnToContents(3);
        ui->tableWidgetRXAll->resizeColumnToContents(4);

        // Reset the scroll position
        ui->tableWidgetRXAll->verticalScrollBar()->setValue(currentScrollPos);
    }
    ui->tableWidgetRXAll->setUpdatesEnabled(true);
}

// updateCallActivity
void MainWindow::displayCallActivity() {
    auto now = DriftingDateTime::currentDateTimeUtc();

    ui->tableWidgetCalls->setFont(m_config.table_font());

    // Selected callsign
    QString selectedCall = callsignSelected();

    auto currentScrollPos = ui->tableWidgetCalls->verticalScrollBar()->value();

    ui->tableWidgetCalls->setUpdatesEnabled(false);
    {
        // Clear the table
        clearTableWidget(ui->tableWidgetCalls);
        createAllcallTableRows(ui->tableWidgetCalls, selectedCall); // isAllCallIncluded(selectedCall)); // || isGroupCallIncluded(selectedCall));

        // Build the table
        QList < QString > keys = m_callActivity.keys();

        auto sortBy = getSortBy("callActivity", "callsign");
        bool reverse = false;
        if(sortBy.startsWith("-")){
            sortBy = sortBy.mid(1);
            reverse = true;
        }

        auto compareOffset = [this](const QString left, QString right) {
            auto leftActivity = m_callActivity[left];
            auto rightActivity = m_callActivity[right];

            return leftActivity.freq < rightActivity.freq;
        };

        auto compareDistance = [this, reverse](const QString left, QString right) {
            auto leftActivity = m_callActivity[left];
            auto rightActivity = m_callActivity[right];

            int leftDistance = reverse ? -100000 : 100000;
            int rightDistance = reverse ? -100000 : 100000;
            if(!leftActivity.grid.isEmpty()){
                calculateDistance(leftActivity.grid, &leftDistance);
            }
            if(!rightActivity.grid.isEmpty()){
                calculateDistance(rightActivity.grid, &rightDistance);
            }

            return leftDistance < rightDistance;
        };

        auto compareTimestamp = [this](const QString left, QString right) {
            auto leftActivity = m_callActivity[left];
            auto rightActivity = m_callActivity[right];

            return leftActivity.utcTimestamp < rightActivity.utcTimestamp;
        };

        auto compareAckTimestamp = [this, reverse](const QString left, QString right) {
            auto leftActivity = m_callActivity[left];
            auto rightActivity = m_callActivity[right];

            return rightActivity.ackTimestamp < leftActivity.ackTimestamp;
        };

        auto compareSNR = [this, reverse](const QString left, QString right) {
            auto leftActivity = m_callActivity[left];
            auto rightActivity = m_callActivity[right];

            if(leftActivity.snr < -60 || leftActivity.snr > 60) {
                leftActivity.snr *= reverse ? 1 : -1;
            }
            if(rightActivity.snr < -60 || rightActivity.snr > 60) {
                rightActivity.snr *= reverse ? 1 : -1;
            }

            return leftActivity.snr < rightActivity.snr;
        };


        // compare callsign
        qStableSort(keys.begin(), keys.end());

        if(sortBy == "offset"){
            qStableSort(keys.begin(), keys.end(), compareOffset);
        } else if(sortBy == "distance"){
            qStableSort(keys.begin(), keys.end(), compareDistance);
        } else if(sortBy == "timestamp"){
            qStableSort(keys.begin(), keys.end(), compareTimestamp);
        } else if(sortBy == "ackTimestamp"){
            qStableSort(keys.begin(), keys.end(), compareAckTimestamp);
        } else if(sortBy == "snr"){
            qStableSort(keys.begin(), keys.end(), compareSNR);
        }

        if(reverse){
            keys = listCopyReverse(keys);
        }

        // pin messages to the top
        qStableSort(keys.begin(), keys.end(), [this](const QString left, QString right){
            int leftHas = (int)!(m_rxInboxCountCache.value(left, 0) > 0);
            int rightHas = (int)!(m_rxInboxCountCache.value(right, 0) > 0);

            return leftHas < rightHas;
        });

        bool showIconColumn = false;

        int callsignAging = m_config.callsign_aging();
        foreach(QString call, keys) {
            if(call.trimmed().isEmpty()){
                continue;
            }

            CallDetail d = m_callActivity[call];
            if(d.call.trimmed().isEmpty()){
                continue;
            }

            bool isCallSelected = (call == selectedCall);

            // icon flags (flag -> star -> empty)
            bool hasMessage = m_rxInboxCountCache.value(d.call, 0) > 0;

            // display telephone icon if called cq in the past 5 minutes
            bool hasCQ = d.cqTimestamp.isValid() && d.cqTimestamp.secsTo(now) / 60 < 5;

            // display star if they've acked a message from us
            bool hasACK = d.ackTimestamp.isValid();

            if (!isCallSelected && !hasMessage && callsignAging && d.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
                continue;
            }

            ui->tableWidgetCalls->insertRow(ui->tableWidgetCalls->rowCount());
            int row = ui->tableWidgetCalls->rowCount() - 1;
            int col = 0;

            // icon column
#if SHOW_THROUGH_CALLS
            QString displayCall = d.through.isEmpty() ? d.call : QString("%1>%2").arg(d.through).arg(d.call);
#else
            // unicode star
            QString displayCall = d.call;
#endif



            auto iconItem = new QTableWidgetItem(hasMessage ? "\u2691" : hasACK ? "\u2605" : hasCQ ? "\u260E" : "");
            iconItem->setData(Qt::UserRole, QVariant(d.call));
            iconItem->setToolTip(hasMessage ? "Message Available" : hasACK ? "Hearing Your Station" : hasCQ ? "Calling CQ" : "");
            iconItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            ui->tableWidgetCalls->setItem(row, col++, iconItem);
            if(hasMessage || hasACK || hasCQ){
                showIconColumn = true;
            }

            auto displayItem = new QTableWidgetItem(displayCall);
            displayItem->setData(Qt::UserRole, QVariant(d.call));
            displayItem->setToolTip(generateCallDetail(displayCall));
            ui->tableWidgetCalls->setItem(row, col++, displayItem);

#if ONLY_SHOW_HEARD_CALLSIGNS
            if(d.utcTimestamp.isValid()){
#else
            if(true){
#endif
                auto ageItem = new QTableWidgetItem(since(d.utcTimestamp));
                ageItem->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
                ageItem->setToolTip(d.utcTimestamp.toString());
                ui->tableWidgetCalls->setItem(row, col++, ageItem);

                auto snrText = Varicode::formatSNR(d.snr);
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem(snrText.isEmpty() ? "" : QString("%1 dB").arg(snrText)));

                auto offsetItem = new QTableWidgetItem(QString("%1 Hz").arg(d.freq));
                offsetItem->setData(Qt::UserRole, QVariant(d.freq));
                ui->tableWidgetCalls->setItem(row, col++, offsetItem);

                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem(QString("%1 ms").arg((int)(1000*d.tdrift))));

                auto modeItem = new QTableWidgetItem(d.mode.left(1).replace("H", "N"));
                modeItem->setToolTip(d.mode);
                modeItem->setData(Qt::UserRole, QVariant(d.mode));
                modeItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                ui->tableWidgetCalls->setItem(row, col++, modeItem);

                auto gridItem = new QTableWidgetItem(QString("%1").arg(d.grid.trimmed().left(4)));
                gridItem->setToolTip(d.grid.trimmed());
                ui->tableWidgetCalls->setItem(row, col++, gridItem);

                auto distanceItem = new QTableWidgetItem(calculateDistance(d.grid));
                distanceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->tableWidgetCalls->setItem(row, col++, distanceItem);

                QString flag;
                if(m_logBook.hasWorkedBefore(d.call, "")){
                    // unicode checkmark
                    flag = "\u2713";
                }
                auto workedBeforeItem = new QTableWidgetItem(flag);
                workedBeforeItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                ui->tableWidgetCalls->setItem(row, col++, workedBeforeItem);

                QString logDetailGrid;
                QString logDetailDate;
                QString logDetailName;
                QString logDetailComment;
                bool gridItemEmpty = gridItem->text().isEmpty();

                if((gridItemEmpty && showColumn("call", "grid")) || showColumn("call", "log") || showColumn("call", "logName") || showColumn("call", "logComment")){
                    m_logBook.findCallDetails(d.call, logDetailGrid, logDetailDate, logDetailName, logDetailComment);
                }

                if(gridItemEmpty && !logDetailGrid.isEmpty()){
                    gridItem->setText(logDetailGrid.trimmed().left(4));
                    gridItem->setToolTip(logDetailGrid.trimmed());
                    distanceItem->setText(calculateDistance(logDetailGrid.trimmed()));

                    // update the call activity cache with the loaded grid
                    if(m_callActivity.contains(d.call)){
                        m_callActivity[call].grid = logDetailGrid.trimmed();
                    }
                }

                if(!logDetailDate.isEmpty()){
                    auto lastLogged = QDate::fromString(logDetailDate, "yyyyMMdd");

                    workedBeforeItem->setToolTip(QString("Last Logged: %1").arg(lastLogged.toString()));
                }

                auto logNameItem = new QTableWidgetItem(logDetailName);
                logNameItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                ui->tableWidgetCalls->setItem(row, col++, logNameItem);

                auto logCommentItem = new QTableWidgetItem(logDetailComment);
                logCommentItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                ui->tableWidgetCalls->setItem(row, col++, logCommentItem);

            } else {
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // age
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // snr
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // freq
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // tdrift
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // mode
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // grid
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // distance
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // worked before
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // log name
                ui->tableWidgetCalls->setItem(row, col++, new QTableWidgetItem("")); // log comment
            }

            if (isCallSelected) {
                for(int i = 0; i < ui->tableWidgetCalls->columnCount(); i++){
                    ui->tableWidgetCalls->item(row, i)->setSelected(true);
                }
            }

            if(hasCQ){
                for(int i = 0; i < ui->tableWidgetCalls->columnCount(); i++){
                    ui->tableWidgetCalls->item(row, i)->setBackground(QBrush(m_config.color_CQ()));
                }
            }

            if (m_config.secondary_highlight_words().contains(call)){
                for(int i = 0; i < ui->tableWidgetCalls->columnCount(); i++){
                    ui->tableWidgetCalls->item(row, i)->setBackground(QBrush(m_config.color_secondary_highlight()));
                }
            }

            if (m_config.primary_highlight_words().contains(call)){
                for(int i = 0; i < ui->tableWidgetCalls->columnCount(); i++){
                    ui->tableWidgetCalls->item(row, i)->setBackground(QBrush(m_config.color_primary_highlight()));
                }
            }
        }

        // Set table color
        auto style = QString("QTableWidget { background:%1; selection-background-color:%2; alternate-background-color:%1; color:%3; }");
        style = style.arg(m_config.color_table_background().name());
        style = style.arg(m_config.color_table_highlight().name());
        style = style.arg(m_config.color_table_foreground().name());
        ui->tableWidgetCalls->setStyleSheet(style);

        // Set the table palette for inactive selected row
        auto p = ui->tableWidgetCalls->palette();
        p.setColor(QPalette::Inactive, QPalette::Highlight, p.color(QPalette::Active, QPalette::Highlight));
        ui->tableWidgetCalls->setPalette(p);

        // Set item fonts
        for(int row = 0; row < ui->tableWidgetCalls->rowCount(); row++){
            auto bold = ui->tableWidgetCalls->item(row,  0)->text() == "\u2691";
            for(int col = 0; col < ui->tableWidgetCalls->columnCount(); col++){
                auto item = ui->tableWidgetCalls->item(row, col);
                if(item){
                    auto f = m_config.table_font();
                    if(bold){
                        f.setBold(true);
                    }
                    item->setFont(f);
                }
            }
        }

        // Column labels
        ui->tableWidgetCalls->horizontalHeader()->setVisible(showColumn("call", "labels"));

        // Hide columns
        ui->tableWidgetCalls->setColumnHidden(0, !showIconColumn);
        ui->tableWidgetCalls->setColumnHidden(1, !showColumn("call", "callsign"));
        ui->tableWidgetCalls->setColumnHidden(2, !showColumn("call", "timestamp"));
        ui->tableWidgetCalls->setColumnHidden(3, !showColumn("call", "snr"));
        ui->tableWidgetCalls->setColumnHidden(4, !showColumn("call", "offset"));
        ui->tableWidgetCalls->setColumnHidden(5, !showColumn("call", "tdrift", false));
        ui->tableWidgetCalls->setColumnHidden(6, !showColumn("call", "mode", false));
        ui->tableWidgetCalls->setColumnHidden(7, !showColumn("call", "grid", false));
        ui->tableWidgetCalls->setColumnHidden(8, !showColumn("call", "distance", false));
        ui->tableWidgetCalls->setColumnHidden(9, !showColumn("call", "log"));
        ui->tableWidgetCalls->setColumnHidden(10, !showColumn("call", "logName"));
        ui->tableWidgetCalls->setColumnHidden(11, !showColumn("call", "logComment"));

        // Resize the table columns
        ui->tableWidgetCalls->resizeColumnToContents(0);
        ui->tableWidgetCalls->resizeColumnToContents(1);
        ui->tableWidgetCalls->resizeColumnToContents(2);
        ui->tableWidgetCalls->resizeColumnToContents(3);
        ui->tableWidgetCalls->resizeColumnToContents(4);
        ui->tableWidgetCalls->resizeColumnToContents(5);
        ui->tableWidgetCalls->resizeColumnToContents(6);
        ui->tableWidgetCalls->resizeColumnToContents(7);
        ui->tableWidgetCalls->resizeColumnToContents(8);
        ui->tableWidgetCalls->resizeColumnToContents(9);
        ui->tableWidgetCalls->resizeColumnToContents(10);

        // Reset the scroll position
        ui->tableWidgetCalls->verticalScrollBar()->setValue(currentScrollPos);
    }
    ui->tableWidgetCalls->setUpdatesEnabled(true);
}

void MainWindow::postWSPRDecode (bool is_new, QStringList parts)
{
#if 0
    if (parts.size () < 8)
    {
      parts.insert (6, "");
    }

  m_messageClient->WSPR_decode (is_new, QTime::fromString (parts[0], "hhmm"), parts[1].toInt ()
                                , parts[2].toFloat (), Radio::frequency (parts[3].toFloat (), 6)
                                , parts[4].toInt (), parts[5], parts[6], parts[7].toInt ()
                                , m_diskData);
#endif
}

void MainWindow::emitPTT(bool on){
    qDebug() << "PTT:" << on;

    Q_EMIT m_config.transceiver_ptt(on);

    // emit to network
    sendNetworkMessage("RIG.PTT", on ? "on" : "off", {
        {"PTT", QVariant(on)},
        {"UTC", QVariant(DriftingDateTime::currentDateTimeUtc().toMSecsSinceEpoch())},
    });
}

void MainWindow::emitTones(){
    if(!m_config.udpEnabled()){
        return;
    }

    // emit tone numbers to network
    QVariantList t;
    for(int i = 0; i < JS8_NUM_SYMBOLS; i++){
        //qDebug() << "tone" << i << "=" << itone[i];
        t.append(QVariant((int)itone[i]));
    }

    sendNetworkMessage("TX.FRAME", "", {
        {"TONES", t}
    });
}

void MainWindow::networkMessage(Message const &message)
{
    if(!m_config.udpEnabled()){
        return;
    }

    if(!m_config.accept_udp_requests()){
        return;
    }

    auto type = message.type();

    if(type == "PING"){
        return;
    }

    qDebug() << "try processing network message" << type;

    auto params = message.params();
    auto id = params.value("_ID", QVariant(0));

    // Inspired by FLDigi
    // TODO: MAIN.RX - Turn on RX
    // TODO: MAIN.TX - Transmit
    // TODO: MAIN.PTT - PTT
    // TODO: MAIN.TUNE - Tune
    // TODO: MAIN.HALT - Halt
    // TODO: MAIN.AUTO - Auto
    // TODO: MAIN.SPOT - Spot
    // TODO: MAIN.HB - HB

    // RIG.GET_FREQ - Get the current Frequency
    // RIG.SET_FREQ - Set the current Frequency
    if(type == "RIG.GET_FREQ"){
        sendNetworkMessage("RIG.FREQ", "", {
            {"_ID", id},
            {"DIAL", QVariant((quint64)dialFrequency())},
            {"OFFSET", QVariant((quint64)currentFreqOffset())}
        });
        return;
    }

    if(type == "RIG.SET_FREQ"){
        auto params = message.params();
        if(params.contains("DIAL")){
            bool ok = false;
            auto f = params["DIAL"].toInt(&ok);
            if(ok){
                setRig(f);
                displayDialFrequency();
            }
        }
        if(params.contains("OFFSET")){
            bool ok = false;
            auto f = params["OFFSET"].toInt(&ok);
            if(ok){
                setFreqOffsetForRestore(f, false);
            }
        }
    }

    // STATION.GET_CALLSIGN - Get the current callsign
    // STATION.GET_GRID - Get the current grid locator
    // STATION.SET_GRID - Set the current grid locator
    // STATION.GET_INFO - Get the current station qth
    // STATION.SET_INFO - Set the current station qth
    if(type == "STATION.GET_CALLSIGN"){
        sendNetworkMessage("STATION.CALLSIGN", m_config.my_callsign(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "STATION.GET_GRID"){
        sendNetworkMessage("STATION.GRID", m_config.my_grid(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "STATION.SET_GRID"){
        m_config.set_dynamic_location(message.value());
        sendNetworkMessage("STATION.GRID", m_config.my_grid(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "STATION.GET_INFO"){
        sendNetworkMessage("STATION.INFO", m_config.my_info(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "STATION.SET_INFO"){
        m_config.set_dynamic_station_info(message.value());
        sendNetworkMessage("STATION.INFO", m_config.my_info(), {
            {"_ID", id},
        });
        return;
    }

    // RX.GET_CALL_ACTIVITY
    // RX.GET_CALL_SELECTED
    // RX.GET_BAND_ACTIVITY
    // RX.GET_TEXT

    if(type == "RX.GET_CALL_ACTIVITY"){
        auto now = DriftingDateTime::currentDateTimeUtc();
        int callsignAging = m_config.callsign_aging();
        QMap<QString, QVariant> calls = {
            {"_ID", id},
        };

        foreach(auto cd, m_callActivity.values()){
            if (callsignAging && cd.utcTimestamp.secsTo(now) / 60 >= callsignAging) {
                continue;
            }
            QMap<QString, QVariant> detail;
            detail["SNR"] = QVariant(cd.snr);
            detail["GRID"] = QVariant(cd.grid);
            detail["UTC"] = QVariant(cd.utcTimestamp.toMSecsSinceEpoch());
            calls[cd.call] = QVariant(detail);
        }

        sendNetworkMessage("RX.CALL_ACTIVITY", "", calls);
        return;
    }

    if(type == "RX.GET_CALL_SELECTED"){
        sendNetworkMessage("RX.CALL_SELECTED", callsignSelected(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "RX.GET_BAND_ACTIVITY"){
        QMap<QString, QVariant> offsets = {
            {"_ID", id},
        };
        foreach(auto offset, m_bandActivity.keys()){
            auto activity = m_bandActivity[offset];
            if(activity.isEmpty()){
                continue;
            }

            auto d = activity.last();

            QMap<QString, QVariant> detail;
            detail["FREQ"] = QVariant(d.freq);
            detail["TEXT"] = QVariant(d.text);
            detail["SNR"] = QVariant(d.snr);
            detail["UTC"] = QVariant(d.utcTimestamp.toMSecsSinceEpoch());
            offsets[QString("%1").arg(offset)] = QVariant(detail);
        }

        sendNetworkMessage("RX.BAND_ACTIVITY", "", offsets);
        return;
    }

    if(type == "RX.GET_TEXT"){
        sendNetworkMessage("RX.TEXT", ui->textEditRX->toPlainText(), {
            {"_ID", id},
        });
        return;
    }

    // TX.GET_TEXT
    // TX.SET_TEXT
    // TX.SEND_MESSAGE

    if(type == "TX.GET_TEXT"){
        sendNetworkMessage("TX.TEXT", ui->extFreeTextMsgEdit->toPlainText(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "TX.SET_TEXT"){
        addMessageText(message.value(), true);
        sendNetworkMessage("TX.TEXT", ui->extFreeTextMsgEdit->toPlainText(), {
            {"_ID", id},
        });
        return;
    }

    if(type == "TX.SEND_MESSAGE"){
        auto text = message.value();
        if(!text.isEmpty()){
            enqueueMessage(PriorityNormal, text, -1, nullptr);
            return;
        }
    }

    // WINDOW.RAISE

    if(type == "WINDOW.RAISE"){
        setWindowState(Qt::WindowActive);
        activateWindow();
        raise();
        return;
    }

    qDebug() << "Unable to process networkMessage:" << type;
}

void MainWindow::sendNetworkMessage(QString const &type, QString const &message){
    if(!m_config.udpEnabled()){
        return;
    }

    m_messageClient->send(Message(type, message));
}

void MainWindow::sendNetworkMessage(QString const &type, QString const &message, QMap<QString, QVariant> const &params)
{
    if(!m_config.udpEnabled()){
        return;
    }

    m_messageClient->send(Message(type, message, params));
}

void MainWindow::networkError (QString const& e)
{
  if(!m_config.udpEnabled()){
    return;
  }

  if(!m_config.accept_udp_requests()){
    return;
  }
  if (m_splash && m_splash->isVisible ()) m_splash->hide ();
  if (MessageBox::Retry == MessageBox::warning_message (this, tr ("Network Error")
                                                        , tr ("Error: %1\nUDP server %2:%3")
                                                        .arg (e)
                                                        .arg (m_config.udp_server_name ())
                                                        .arg (m_config.udp_server_port ())
                                                        , QString {}
                                                        , MessageBox::Cancel | MessageBox::Retry
                                                        , MessageBox::Cancel))
    {
      // retry server lookup
      m_messageClient->set_server (m_config.udp_server_name ());
    }
}

void MainWindow::on_syncSpinBox_valueChanged(int n)
{
  m_minSync=n;
}

void MainWindow::p1ReadFromStdout()                        //p1readFromStdout
{
  QString t1;
  while(p1.canReadLine()) {
    QString t(p1.readLine());
    if(t.indexOf("<DecodeFinished>") >= 0) {
      m_bDecoded = m_nWSPRdecodes > 0;
      if(!m_diskData) {
        WSPR_history(m_dialFreqRxWSPR, m_nWSPRdecodes);
        if(m_nWSPRdecodes==0 and ui->band_hopping_group_box->isChecked()) {
          t = " Receiving " + m_mode + " ----------------------- " +
              m_config.bands ()->find (m_dialFreqRxWSPR);
          t=WSPR_hhmm(-60) + ' ' + t.rightJustified (66, '-');
          ui->decodedTextBrowser->appendText(t);
        }
        killFileTimer.start (45*1000); //Kill in 45s (for slow modes)
      }
      m_nWSPRdecodes=0;
      ui->DecodeButton->setChecked (false);
      if(m_uploadSpots
         && m_config.is_transceiver_online ()) { // need working rig control
        float x=qrand()/((double)RAND_MAX + 1.0);
        int msdelay=20000*x;
        uploadTimer.start(msdelay);                         //Upload delay
      } else {
        QFile f(QDir::toNativeSeparators(m_config.writeable_data_dir ().absolutePath()) + "/wspr_spots.txt");
        if(f.exists()) f.remove();
      }
      m_RxLog=0;
      m_startAnother=m_loopall;
      m_blankLine=true;
      m_decoderBusy = false;
      statusUpdate ();
    } else {

      int n=t.length();
      t=t.mid(0,n-2) + "                                                  ";
      t.remove(QRegExp("\\s+$"));
      QStringList rxFields = t.split(QRegExp("\\s+"));
      QString rxLine;
      QString grid="";
      if ( rxFields.count() == 8 ) {
          rxLine = QString("%1 %2 %3 %4 %5   %6  %7  %8")
                  .arg(rxFields.at(0), 4)
                  .arg(rxFields.at(1), 4)
                  .arg(rxFields.at(2), 5)
                  .arg(rxFields.at(3), 11)
                  .arg(rxFields.at(4), 4)
                  .arg(rxFields.at(5).leftJustified (12))
                  .arg(rxFields.at(6), -6)
                  .arg(rxFields.at(7), 3);
          postWSPRDecode (true, rxFields);
          grid = rxFields.at(6);
      } else if ( rxFields.count() == 7 ) { // Type 2 message
          rxLine = QString("%1 %2 %3 %4 %5   %6  %7  %8")
                  .arg(rxFields.at(0), 4)
                  .arg(rxFields.at(1), 4)
                  .arg(rxFields.at(2), 5)
                  .arg(rxFields.at(3), 11)
                  .arg(rxFields.at(4), 4)
                  .arg(rxFields.at(5).leftJustified (12))
                  .arg("", -6)
                  .arg(rxFields.at(6), 3);
          postWSPRDecode (true, rxFields);
      } else {
          rxLine = t;
      }
      if(grid!="") {
        double utch=0.0;
        int nAz,nEl,nDmiles,nDkm,nHotAz,nHotABetter;
        azdist_(const_cast <char *> (m_config.my_grid ().toLatin1().constData()),
                const_cast <char *> (grid.toLatin1().constData()),&utch,
                &nAz,&nEl,&nDmiles,&nDkm,&nHotAz,&nHotABetter,6,6);
        QString t1;
        if(m_config.miles()) {
          t1.sprintf("%7d",nDmiles);
        } else {
          t1.sprintf("%7d",nDkm);
        }
        rxLine += t1;
      }

      if (m_config.insert_blank () && m_blankLine) {
        QString band;
        Frequency f=1000000.0*rxFields.at(3).toDouble()+0.5;
        band = ' ' + m_config.bands ()->find (f);
        ui->decodedTextBrowser->appendText(band.rightJustified (71, '-'));
        m_blankLine = false;
      }
      m_nWSPRdecodes += 1;
      ui->decodedTextBrowser->appendText(rxLine);
    }
  }
}

QString MainWindow::WSPR_hhmm(int n)
{
  QDateTime t=DriftingDateTime::currentDateTimeUtc().addSecs(n);
  int m=t.toString("hhmm").toInt()/2;
  QString t1;
  t1.sprintf("%04d",2*m);
  return t1;
}

void MainWindow::WSPR_history(Frequency dialFreq, int ndecodes)
{
  QDateTime t=DriftingDateTime::currentDateTimeUtc().addSecs(-60);
  QString t1=t.toString("yyMMdd");
  QString t2=WSPR_hhmm(-60);
  QString t3;
  t3.sprintf("%13.6f",0.000001*dialFreq);
  if(ndecodes<0) {
    t1=t1 + " " + t2 + t3 + "  T";
  } else {
    QString t4;
    t4.sprintf("%4d",ndecodes);
    t1=t1 + " " + t2 + t3 + "  R" + t4;
  }
  QFile f {m_config.writeable_data_dir ().absoluteFilePath ("WSPR_history.txt")};
  if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
    QTextStream out(&f);
    out << t1 << endl;
    f.close();
  } else {
    MessageBox::warning_message (this, tr ("File Error")
                                 , tr ("Cannot open \"%1\" for append: %2")
                                 .arg (f.fileName ()).arg (f.errorString ()));
  }
}


void MainWindow::uploadSpots()
{
  // do not spot replays or if rig control not working
  if(m_diskData || !m_config.is_transceiver_online ()) return;
  if(m_uploading) {
    qDebug() << "Previous upload has not completed, spots were lost";
    wsprNet->abortOutstandingRequests ();
    m_uploading = false;
  }
  QString rfreq = QString("%1").arg(0.000001*(m_dialFreqRxWSPR + 1500), 0, 'f', 6);
  QString tfreq = QString("%1").arg(0.000001*(m_dialFreqRxWSPR +
                        ui->TxFreqSpinBox->value()), 0, 'f', 6);
  wsprNet->upload(m_config.my_callsign(), m_config.my_grid(), rfreq, tfreq,
                  m_mode, QString::number(ui->autoButton->isChecked() ? m_pctx : 0),
                  QString::number(m_dBm), version(),
                  QDir::toNativeSeparators(m_config.writeable_data_dir ().absolutePath()) + "/wspr_spots.txt");
  m_uploading = true;
}

void MainWindow::uploadResponse(QString response)
{
  if (response == "done") {
    m_uploading=false;
  } else {
    if (response.startsWith ("Upload Failed")) {
      m_uploading=false;
    }
    qDebug () << "WSPRnet.org status:" << response;
  }
}

void MainWindow::on_TxPowerComboBox_currentIndexChanged(const QString &arg1)
{
  int i1=arg1.indexOf(" ");
  m_dBm=arg1.mid(0,i1).toInt();
}

void MainWindow::on_sbTxPercent_valueChanged(int n)
{
  m_pctx=n;
  if(m_pctx>0) {
    ui->pbTxNext->setEnabled(true);
  } else {
    m_txNext=false;
    ui->pbTxNext->setChecked(false);
    ui->pbTxNext->setEnabled(false);
  }
}

void MainWindow::on_cbUploadWSPR_Spots_toggled(bool b)
{
  m_uploadSpots=b;
  if(m_uploadSpots) ui->cbUploadWSPR_Spots->setStyleSheet("");
  if(!m_uploadSpots) ui->cbUploadWSPR_Spots->setStyleSheet(
        "QCheckBox{background-color: yellow}");
}

void MainWindow::on_WSPRfreqSpinBox_valueChanged(int n)
{
  ui->TxFreqSpinBox->setValue(n);
}

void MainWindow::on_pbTxNext_clicked(bool b)
{
  m_txNext=b;
}

void MainWindow::WSPR_scheduling ()
{
  m_WSPR_tx_next = false;
  if (m_config.is_transceiver_online () // need working rig control for hopping
      && !m_config.is_dummy_rig ()
      && ui->band_hopping_group_box->isChecked ()) {
    auto hop_data = m_WSPR_band_hopping.next_hop (m_auto);
    qDebug () << "hop data: period:" << hop_data.period_name_
              << "frequencies index:" << hop_data.frequencies_index_
              << "tune:" << hop_data.tune_required_
              << "tx:" << hop_data.tx_next_;
    m_WSPR_tx_next = hop_data.tx_next_;
    if (hop_data.frequencies_index_ >= 0) { // new band
      ui->bandComboBox->setCurrentIndex (hop_data.frequencies_index_);
      on_bandComboBox_activated (hop_data.frequencies_index_);
      m_cmnd.clear ();
      QStringList prefixes {".bat", ".cmd", ".exe", ""};
      for (auto const& prefix : prefixes)
        {
          auto const& path = m_appDir + "/user_hardware" + prefix;
          QFile f {path};
          if (f.exists ()) {
            m_cmnd = QDir::toNativeSeparators (f.fileName ()) + ' ' +
              m_config.bands ()->find (m_freqNominal).remove ('m');
          }
        }
      if(m_cmnd!="") p3.start(m_cmnd);     // Execute user's hardware controller

      // Produce a short tuneup signal
      m_tuneup = false;
      if (hop_data.tune_required_) {
        m_tuneup = true;
        on_tuneButton_clicked (true);
        tuneATU_Timer.start (2500);
      }
    }

    // Display grayline status
    band_hopping_label.setText (hop_data.period_name_);
  }
  else {
    m_WSPR_tx_next = m_WSPR_band_hopping.next_is_tx ("WSPR-LF" == m_mode);
  }
}

void MainWindow::astroUpdate ()
{
  if (m_astroWidget)
    {
      // no Doppler correction while CTRL pressed allows manual tuning
      if (Qt::ControlModifier & QApplication::queryKeyboardModifiers ()) return;

      auto correction = m_astroWidget->astroUpdate(DriftingDateTime::currentDateTimeUtc (),
                                                   m_config.my_grid(), m_hisGrid,
                                                   m_freqNominal,
                                                   "Echo" == m_mode, m_transmitting,
                                                   !m_config.tx_qsy_allowed (), m_TRperiod);
      // no Doppler correction in Tx if rig can't do it
      if (m_transmitting && !m_config.tx_qsy_allowed ()) return;
      if (!m_astroWidget->doppler_tracking ()) return;
      if ((m_monitoring || m_transmitting)
          // no Doppler correction below 6m
          && m_freqNominal >= 50000000
          && m_config.split_mode ())
        {
          // adjust for rig resolution
          if (m_config.transceiver_resolution () > 2)
            {
              correction.rx = (correction.rx + 50) / 100 * 100;
              correction.tx = (correction.tx + 50) / 100 * 100;
            }
          else if (m_config.transceiver_resolution () > 1)
            {
              correction.rx = (correction.rx + 10) / 20 * 20;
              correction.tx = (correction.tx + 10) / 20 * 20;
            }
          else if (m_config.transceiver_resolution () > 0)
            {
              correction.rx = (correction.rx + 5) / 10 * 10;
              correction.tx = (correction.tx + 5) / 10 * 10;
            }
          else if (m_config.transceiver_resolution () < -2)
            {
              correction.rx = correction.rx / 100 * 100;
              correction.tx = correction.tx / 100 * 100;
            }
          else if (m_config.transceiver_resolution () < -1)
            {
              correction.rx = correction.rx / 20 * 20;
              correction.tx = correction.tx / 20 * 20;
            }
          else if (m_config.transceiver_resolution () < 0)
            {
              correction.rx = correction.rx / 10 * 10;
              correction.tx = correction.tx / 10 * 10;
            }
          m_astroCorrection = correction;
        }
      else
        {
          m_astroCorrection = {};
        }

      setRig ();
    }
}

void MainWindow::setRig (Frequency f)
{
  if (f)
    {
      m_freqNominal = f;
      m_freqTxNominal = m_freqNominal;
      if (m_astroWidget) m_astroWidget->nominal_frequency (m_freqNominal, m_freqTxNominal);
    }
  if (m_mode == "FreqCal"
      && m_frequency_list_fcal_iter != m_config.frequencies ()->end ()) {
    m_freqNominal = m_frequency_list_fcal_iter->frequency_ - ui->RxFreqSpinBox->value ();
  }
  if(m_transmitting && !m_config.tx_qsy_allowed ()) return;
  if ((m_monitoring || m_transmitting) && m_config.transceiver_online ())
    {
      if (m_transmitting && m_config.split_mode ())
        {
          Q_EMIT m_config.transceiver_tx_frequency (m_freqTxNominal + m_astroCorrection.tx);
        }
      else
        {
          Q_EMIT m_config.transceiver_frequency (m_freqNominal + m_astroCorrection.rx);
        }
    }
}

void MainWindow::fastPick(int x0, int x1, int y)
{
  float pixPerSecond=12000.0/512.0;
  if(m_TRperiod<30) pixPerSecond=12000.0/256.0;
  if(m_mode!="ISCAT" and m_mode!="MSK144") return;
  if(!m_decoderBusy) {
    dec_data.params.newdat=0;
    dec_data.params.nagain=1;
    m_blankLine=false;                 // don't insert the separator again
    m_nPick=1;
    if(y > 120) m_nPick=2;
    m_t0Pick=x0/pixPerSecond;
    m_t1Pick=x1/pixPerSecond;
    m_dataAvailable=true;
    decode();
  }
}

void MainWindow::on_actionMeasure_reference_spectrum_triggered()
{
  if(!m_monitoring) on_monitorButton_clicked (true);
  m_bRefSpec=true;
}

void MainWindow::on_actionMeasure_phase_response_triggered()
{
  if(m_bTrain) {
    m_bTrain=false;
    MessageBox::information_message (this, tr ("Phase Training Disabled"));
  } else {
    m_bTrain=true;
    MessageBox::information_message (this, tr ("Phase Training Enabled"));
  }
}

void MainWindow::on_actionErase_reference_spectrum_triggered()
{
  m_bClearRefSpec=true;
}

void MainWindow::freqCalStep()
{
  if (m_frequency_list_fcal_iter == m_config.frequencies ()->end ()
      || ++m_frequency_list_fcal_iter == m_config.frequencies ()->end ()) {
    m_frequency_list_fcal_iter = m_config.frequencies ()->begin ();
  }

  // allow for empty list
  if (m_frequency_list_fcal_iter != m_config.frequencies ()->end ()) {
    setRig (m_frequency_list_fcal_iter->frequency_ - ui->RxFreqSpinBox->value ());
  }
}

void MainWindow::on_sbCQTxFreq_valueChanged(int)
{
  setXIT (ui->TxFreqSpinBox->value ());
}

void MainWindow::on_cbCQTx_toggled(bool b)
{
  ui->sbCQTxFreq->setEnabled(b);
  if(b) {
    ui->txrb6->setChecked(true);
    m_ntx=6;
    m_QSOProgress = CALLING;
  }
  setRig ();
  setXIT (ui->TxFreqSpinBox->value ());
}

void MainWindow::statusUpdate () const
{
#if 0
  if (!ui) return;
  auto submode = current_submode ();

  m_messageClient->status_update (m_freqNominal, m_mode, m_hisCall,
                                  QString::number (ui->rptSpinBox->value ()),
                                  m_modeTx, ui->autoButton->isChecked (),
                                  m_transmitting, m_decoderBusy,
                                  ui->RxFreqSpinBox->value (), ui->TxFreqSpinBox->value (),
                                  m_config.my_callsign (), m_config.my_grid (),
                                  m_hisGrid, m_tx_watchdog,
                                  submode != QChar::Null ? QString {submode} : QString {}, m_bFastMode);
#endif
}

void MainWindow::childEvent (QChildEvent * e)
{
  if (e->child ()->isWidgetType ())
    {
      switch (e->type ())
        {
        case QEvent::ChildAdded: add_child_to_event_filter (e->child ()); break;
        case QEvent::ChildRemoved: remove_child_from_event_filter (e->child ()); break;
        default: break;
        }
    }
  QMainWindow::childEvent (e);
}

// add widget and any child widgets to our event filter so that we can
// take action on key press ad mouse press events anywhere in the main window
void MainWindow::add_child_to_event_filter (QObject * target)
{
  if (target && target->isWidgetType ())
    {
      target->installEventFilter (this);
    }
  auto const& children = target->children ();
  for (auto iter = children.begin (); iter != children.end (); ++iter)
    {
      add_child_to_event_filter (*iter);
    }
}

// recursively remove widget and any child widgets from our event filter
void MainWindow::remove_child_from_event_filter (QObject * target)
{
  auto const& children = target->children ();
  for (auto iter = children.begin (); iter != children.end (); ++iter)
    {
      remove_child_from_event_filter (*iter);
    }
  if (target && target->isWidgetType ())
    {
      target->removeEventFilter (this);
    }
}

void MainWindow::resetIdleTimer(){
    if(m_idleMinutes){
      m_idleMinutes = 0;
      qDebug() << "idle" << m_idleMinutes << "minutes";
    }
}

void MainWindow::incrementIdleTimer(){
    m_idleMinutes++;
    qDebug() << "increment idle to" << m_idleMinutes << "minutes";
}

void MainWindow::tx_watchdog (bool triggered)
{
  auto prior = m_tx_watchdog;
  m_tx_watchdog = triggered;
  if (triggered)
    {
      m_bTxTime=false;
      if (m_tune) stop_tuning ();
      if (m_auto) auto_tx_mode (false);
      stopTx();
      tx_status_label.setStyleSheet ("QLabel{background-color: #000000; color:#ffffff; }");
      tx_status_label.setText ("Idle timeout");

      // if the watchdog is triggered...we're no longer active
      bool wasAuto = ui->actionModeAutoreply->isChecked();
      bool wasHB = ui->hbMacroButton->isChecked();
      bool wasCQ = ui->cqMacroButton->isChecked();

      // save the button states
      ui->actionModeAutoreply->setChecked(false);
      ui->hbMacroButton->setChecked(false);
      ui->cqMacroButton->setChecked(false);

      // clear the tx queues
      resetMessageTransmitQueue();

      QMessageBox * msgBox = new QMessageBox(this);
      msgBox->setIcon(QMessageBox::Information);
      msgBox->setWindowTitle("Idle Timeout");
      msgBox->setInformativeText(QString("You have been idle for more than %1 minutes.").arg(m_config.watchdog()));
      msgBox->addButton(QMessageBox::Ok);

      connect(msgBox, &QMessageBox::finished, this, [this, wasAuto, wasHB, wasCQ](int /*result*/) {
          // restore the button states
          ui->actionModeAutoreply->setChecked(wasAuto);
          ui->hbMacroButton->setChecked(wasHB);
          ui->cqMacroButton->setChecked(wasCQ);

          this->tx_watchdog(false);
      });
      msgBox->setModal(true);
      msgBox->show();



#if 0
      MessageBox::warning_message(this, QString("You have been inactive for more than %1 minutes.").arg(m_config.watchdog()));

      // clear the tx queues
      resetMessageTransmitQueue();

      // restore the button states
      ui->actionModeAutoreply->setChecked(wasAuto);
      ui->hbMacroButton->setChecked(wasHB);
      ui->cqMacroButton->setChecked(wasCQ);
#endif
    }
  else
    {
      // m_idleMinutes = 0;
      update_watchdog_label ();
    }
  if (prior != triggered) statusUpdate ();
}

void MainWindow::update_watchdog_label ()
{
    watchdog_label.setVisible (false);

#if 0
  if (m_config.watchdog () && !m_mode.startsWith ("WSPR"))
    {
      watchdog_label.setText (QString {"WD:%1m"}.arg (m_config.watchdog () - m_idleMinutes));
      watchdog_label.setVisible (true);
    }
  else
    {
      watchdog_label.setText (QString {});
      watchdog_label.setVisible (false);
    }
#endif
}

void MainWindow::on_cbMenus_toggled(bool b)
{
  hideMenus(!b);
}

void MainWindow::on_cbCQonly_toggled(bool)
{
  QFile {m_config.temp_dir().absoluteFilePath(".lock")}.remove(); // Allow jt9 to start
  decodeBusy(true);
}

void MainWindow::on_cbFirst_toggled(bool b)
{
  if (b) {
    if (m_auto && CALLING == m_QSOProgress) {
      ui->cbFirst->setStyleSheet ("QCheckBox{color:red}");
    }
  } else {
    ui->cbFirst->setStyleSheet ("");
  }
}

void MainWindow::on_cbAutoSeq_toggled(bool b)
{
  if(!b) ui->cbFirst->setChecked(false);
  ui->cbFirst->setVisible((m_mode=="FT8") and b);
}

void MainWindow::on_measure_check_box_stateChanged (int state)
{
  m_config.enable_calibration (Qt::Checked != state);
}

void MainWindow::write_transmit_entry (QString const& file_name)
{
  QFile f {m_config.writeable_data_dir ().absoluteFilePath (file_name)};
  if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
      QTextStream out(&f);
      auto time = DriftingDateTime::currentDateTimeUtc ();
      time = time.addSecs (-(time.time ().second () % m_TRperiod));
      auto dt = DecodedText(m_currentMessage, m_currentMessageBits, m_nSubMode);
      out << time.toString("yyyy-MM-dd hh:mm:ss")
          << "  Transmitting " << qSetRealNumberPrecision (12) << (m_freqNominal / 1.e6)
          << " MHz  " << "JS8"
          << ":  " << dt.message() << endl;
      f.close();
    }
  else
    {
      auto const& message = tr ("Cannot open \"%1\" for append: %2")
        .arg (f.fileName ()).arg (f.errorString ());
#if QT_VERSION >= 0x050400
      QTimer::singleShot (0, [=] {                   // don't block guiUpdate
          MessageBox::warning_message (this, tr ("Log File Error"), message);
        });
#else
      MessageBox::warning_message (this, tr ("Log File Error"), message);
#endif
    }
}
