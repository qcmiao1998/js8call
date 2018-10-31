#include "logbook.h"
#include <QDebug>
#include <QFontMetrics>
#include <QStandardPaths>
#include <QDir>

namespace
{
  auto logFileName = "js8call_log.adi";
  auto countryFileName = "cty.dat";
}

void LogBook::init()
{
  QDir dataPath {QStandardPaths::writableLocation (QStandardPaths::DataLocation)};
  QString countryDataFilename;
  if (dataPath.exists (countryFileName))
    {
      // User override
      countryDataFilename = dataPath.absoluteFilePath (countryFileName);
    }
  else
    {
      countryDataFilename = QString {":/"} + countryFileName;
    }

  _countries.init(countryDataFilename);
  _countries.load();

  _worked.init(_countries.getCountryNames());

  _log.init(dataPath.absoluteFilePath (logFileName));
  _log.load();

  _setAlreadyWorkedFromLog();
}


void LogBook::_setAlreadyWorkedFromLog()
{
  QList<QString> calls = _log.getCallList();
  QString c;
  foreach(c,calls)
    {
      QString countryName = _countries.find(c);
      if (countryName.length() > 0)
        {
          _worked.setAsWorked(countryName);
        }
    }
}

bool LogBook::hasWorkedBefore(const QString &call, const QString &band, const QString &mode){
    return _log.match(call, band, mode);
}

void LogBook::match(/*in*/const QString call,
                    /*out*/ QString &countryName,
                    bool &callWorkedBefore,
                    bool &countryWorkedBefore) const
{
  if (call.length() > 0)
    {
      QString currentMode = "";  // match any mode
      QString currentBand = "";  // match any band
      callWorkedBefore = _log.match(call,currentBand,currentMode);
      countryName = _countries.find(call);

      if (countryName.length() > 0)  //  country was found
        countryWorkedBefore = _worked.getHasWorked(countryName);
      else
        {
          countryName = "where?"; //error: prefix not found
          countryWorkedBefore = false;
        }
    }
}

void LogBook::addAsWorked(const QString call, const QString band, const QString mode, const QString date)
{
  _log.add(call,band,mode,date);
  QString countryName = _countries.find(call);
  if (countryName.length() > 0)
    _worked.setAsWorked(countryName);
}



