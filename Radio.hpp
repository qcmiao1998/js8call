#ifndef RADIO_HPP__
#define RADIO_HPP__

#include <QObject>
#include <QLocale>
#include <QList>

class QVariant;
class QString;

//
// Declarations common to radio software.
//

namespace Radio
{
  //
  // Frequency types
  //
  using Frequency = quint64;
  using Frequencies = QList<Frequency>;
  using FrequencyDelta = qint64;

  //
  // Qt type registration
  //
  void register_types ();

  //
  // Frequency type conversion.
  //
  //	QVariant argument is convertible to double and is assumed to
  //	be scaled by (10 ** -scale).
  //
  Frequency frequency (QVariant const&, int scale,
                                  bool * ok = nullptr, QLocale const& = QLocale ());
  FrequencyDelta frequency_delta (QVariant const&, int scale,
                                             bool * ok = nullptr, QLocale const& = QLocale ());

  //
  // Frequency type formatting
  //
  QString frequency_MHz_string (Frequency, QLocale const& = QLocale ());
  QString frequency_MHz_string (FrequencyDelta, QLocale const& = QLocale ());
  QString pretty_frequency_MHz_string (Frequency, QLocale const& = QLocale ());
  QString pretty_frequency_MHz_string (double, int scale, QLocale const& = QLocale ());
  QString pretty_frequency_MHz_string (FrequencyDelta, QLocale const& = QLocale ());

  //
  // Callsigns
  //
  bool is_callsign (QString const&);
  bool is_compound_callsign (QString const&);
  QString base_callsign (QString);
  QString effective_prefix (QString);
}

Q_DECLARE_METATYPE (Radio::Frequency);
Q_DECLARE_METATYPE (Radio::Frequencies);
Q_DECLARE_METATYPE (Radio::FrequencyDelta);

#endif
