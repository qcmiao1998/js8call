// -*- Mode: C++ -*-
#ifndef WIDEGRAPH_H
#define WIDEGRAPH_H


#include <random>
#include <iterator>
#include <iostream>

#include <QDialog>
#include <QScopedPointer>
#include <QDir>
#include <QHash>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QVariant>
#include "WFPalette.hpp"

#define MAX_SCREENSIZE 2048

namespace Ui {
  class WideGraph;
}

class QSettings;
class Configuration;

class WideGraph : public QDialog
{
  Q_OBJECT

public:
  explicit WideGraph(QSettings *, QWidget *parent = 0);
  ~WideGraph ();

  void   dataSink2(float s[], float df3, int ihsym, int ndiskdata);
  void   setRxFreq(int n);
  int    rxFreq();
  int    centerFreq();
  int    nStartFreq();
  int    Fmin();
  int    Fmax();
  int    filterMinimum();
  int    filterMaximum();
  bool   filterEnabled();
  void   setFilterCenter(int n);
  void   setFilter(int a, int b);
  void   setFilterMinimumBandwidth(int width);
  void   setFilterEnabled(bool enabled);
  void   setFilterOpacityPercent(int n);
  int    fSpan();
  void   saveSettings();
  void   setFsample(int n);
  void   setPeriod(int ntrperiod, int nsps);
  void   setTxFreq(int n);
  void   setMode(QString mode);
  void   setSubMode(int n);
  void   setModeTx(QString modeTx);
  bool   flatten();
  bool   useRef();
  void   setTol(int n);
  int    smoothYellow();
  void   setRxBand (QString const& band);
  void   setWSPRtransmitted();
  void   drawRed(int ia, int ib);
  void   setVHF(bool bVHF);
  void   setRedFile(QString fRed);
  void   setTurbo(bool turbo);

signals:
  void freezeDecode2(int n);
  void f11f12(int n);
  void setXIT2(int n);
  void setFreq3(int rxFreq, int txFreq);
  void qsy(int hzDelta);

public slots:
  void wideFreezeDecode(int n);
  void setFreq2(int rxFreq, int txFreq);
  void setDialFreq(double d);
  void setControlsVisible(bool visible);
  bool controlsVisible();
  void setDrift(int n);
  void setQSYEnabled(bool enabled);
  void setPaused(bool paused){ m_paused = paused; }

protected:
  void keyPressEvent (QKeyEvent *e) override;
  void closeEvent (QCloseEvent *) override;

private slots:
  void draw();
  void drawSwide();

  void on_qsyPushButton_clicked();
  void on_offsetSpinBox_valueChanged(int n);
  void on_waterfallAvgSpinBox_valueChanged(int arg1);
  void on_bppSpinBox_valueChanged(int arg1);
  void on_spec2dComboBox_currentIndexChanged(const QString &arg1);
  void on_fStartSpinBox_valueChanged(int n);
  void on_paletteComboBox_activated(const QString &palette);
  void on_cbFlatten_toggled(bool b);
  void on_cbRef_toggled(bool b);
  void on_cbControls_toggled(bool b);
  void on_adjust_palette_push_button_clicked (bool);
  void on_gainSlider_valueChanged(int value);
  void on_zeroSlider_valueChanged(int value);
  void on_gain2dSlider_valueChanged(int value);
  void on_zero2dSlider_valueChanged(int value);
  void on_smoSpinBox_valueChanged(int n);  
  void on_sbPercent2dPlot_valueChanged(int n);
  void on_filterMinSpinBox_valueChanged(int n);
  void on_filterCenterSpinBox_valueChanged(int n);
  void on_filterWidthSpinBox_valueChanged(int n);
  void on_filterCenterSyncButton_clicked();
  void on_filterCheckBox_toggled(bool b);
  void on_filterOpacitySpinBox_valueChanged(int n);

  void on_driftSpinBox_valueChanged(int n);
  void on_driftSyncButton_clicked();
  void on_driftSyncEndButton_clicked();
  void on_driftSyncMinuteButton_clicked();
  void on_driftSyncResetButton_clicked();


private:
  void readPalette ();
  void setRxRange ();
  void replot();

  QScopedPointer<Ui::WideGraph> ui;

  QSettings * m_settings;
  QDir m_palettes_path;
  WFPalette m_userPalette;
  QHash<QString, QVariant> m_fMinPerBand;

  bool m_filterEnabled;

  qint32 m_filterMinWidth;
  qint32 m_filterMinimum;
  qint32 m_filterMaximum;
  qint32 m_filterCenter;
  qint32 m_waterfallAvg;
  qint32 m_TRperiod;
  qint32 m_nsps;
  qint32 m_ntr0;
  qint32 m_fMax;
  qint32 m_nSubMode;
  qint32 m_nsmo;
  qint32 m_Percent2DScreen;
  qint32 m_jz=MAX_SCREENSIZE;
  qint32 m_n;

  bool   m_paused;
  bool   m_bFlatten;
  bool   m_bRef;
  bool   m_bHaveTransmitted;    //Set true at end of a WSPR transmission

  QTimer m_drawTimer;
  QMutex m_drawLock;

  QString m_rxBand;
  QString m_mode;
  QString m_modeTx;
  QString m_waterfallPalette;  

  std::default_random_engine m_gen;
  std::normal_distribution<double> m_dist;
};

#endif // WIDEGRAPH_H
