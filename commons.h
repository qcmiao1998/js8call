#ifndef COMMONS_H
#define COMMONS_H

#define NSMAX 6827
#define NTMAX 300

#define RX_SAMPLE_RATE 12000

#define JS8_NUM_SYMBOLS    79

#ifndef JS8_80HZ
#define JS8_80HZ 0
#endif

#if JS8_80HZ
#define JS8_SYMBOL_SAMPLES 1200 // 480 // 600 // 1200 // 1920
#define JS8_TX_SECONDS     10 // 5 // 6 // 10 // 15
#define JS8_SYMBOL_STOP    32 // 12 // 16 // 32   // 50 // ??? no idea why this works...
#else
#define JS8_SYMBOL_SAMPLES 1920
#define JS8_TX_SECONDS     15
#define JS8_SYMBOL_STOP    50
#endif

#ifndef TEST_FOX_WAVE_GEN
#define TEST_FOX_WAVE_GEN 0
#endif

#ifndef TEST_FOX_WAVE_GEN_SLOTS
#if TEST_FOX_WAVE_GEN
    #define TEST_FOX_WAVE_GEN_SLOTS 2
#else
    #define TEST_FOX_WAVE_GEN_SLOTS 1
#endif
#endif

#ifndef TEST_FOX_WAVE_GEN_OFFSET
#if TEST_FOX_WAVE_GEN
    #define TEST_FOX_WAVE_GEN_OFFSET 25
#else
    #define TEST_FOX_WAVE_GEN_OFFSET 0
#endif
#endif

#ifdef __cplusplus
#include <cstdbool>
extern "C" {
#else
#include <stdbool.h>
#endif

  /*
   * This structure is shared with Fortran code, it MUST be kept in
   * sync with lib/jt9com.f90
   */
extern struct dec_data {
  float ss[184*NSMAX];
  float savg[NSMAX];
  float sred[5760];
  short int d2[NTMAX*RX_SAMPLE_RATE];
  struct
  {
    int nutc;                   //UTC as integer, HHMM
    bool ndiskdat;              //true ==> data read from *.wav file
    int ntrperiod;              //TR period (seconds)
    int nQSOProgress;           /* QSO state machine state */
    int nfqso;                  //User-selected QSO freq (kHz)
    int nftx;                   /* Transmit audio offset where
                                   replies might be expected */
    bool newdat;                //true ==> new data, must do long FFT
    int npts8;                  //npts for c0() array
    int nfa;                    //Low decode limit (Hz)
    int nfSplit;                //JT65 | JT9 split frequency
    int nfb;                    //High decode limit (Hz)
    int ntol;                   //+/- decoding range around fQSO (Hz)
    int kin;
    int nzhsym;
    int nsubmode;
    bool nagain;
    int ndepth;
    bool lft8apon;
    bool lapcqonly;
    bool ljt65apon;
    int napwid;
    int ntxmode;
    int nmode;
    int minw;
    bool nclearave;
    int minSync;
    float emedelay;
    float dttol;
    int nlist;
    int listutc[10];
    int n2pass;
    int nranera;
    int naggressive;
    bool nrobust;
    int nexp_decode;
    char datetime[20];
    char mycall[12];
    char mygrid[6];
    char hiscall[12];
    char hisgrid[6];
  } params;
} dec_data;

extern struct {
  float syellow[NSMAX];
  float ref[3457];
  float filter[3457];
} spectra_;

extern struct {
  int   nclearave;
  int   nsum;
  float blue[4096];
  float red[4096];
} echocom_;

extern struct {
  float wave[606720];
  int   nslots;
  int   nfreq;
  int   i3bit[5];
  char  cmsg[5][40];
  char  mycall[12];
} foxcom_;

#ifdef __cplusplus
}
#endif

#endif // COMMONS_H
