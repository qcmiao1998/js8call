#ifndef COMMONS_H
#define COMMONS_H

#define NSMAX 6827
#define NTMAX 300

#define RX_SAMPLE_RATE 12000

#define JS8_NUM_SYMBOLS    79
#define JS8_ENABLE_JS8B    1
#define JS8_ENABLE_JS8C    1
#define JS8_ENABLE_JS8D    0

#define JS8A_SYMBOL_SAMPLES 1920
#define JS8A_TX_SECONDS     15
#define JS8A_START_DELAY_MS 500

#define JS8B_SYMBOL_SAMPLES 1200
#define JS8B_TX_SECONDS     10
#define JS8B_START_DELAY_MS 200

#define JS8C_SYMBOL_SAMPLES 600
#define JS8C_TX_SECONDS     6
#define JS8C_START_DELAY_MS 100

#define JS8D_IS_ULTRA 1
#if JS8D_IS_ULTRA
#define JS8D_SYMBOL_SAMPLES 384
#define JS8D_TX_SECONDS     4
#define JS8D_START_DELAY_MS 100
#else
#define JS8D_SYMBOL_SAMPLES 4000
#define JS8D_TX_SECONDS     30
#define JS8D_START_DELAY_MS 100
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
  float ss[184*NSMAX]; // symbol spectra
  float savg[NSMAX];
  float sred[5760];
  short int d1[NTMAX*RX_SAMPLE_RATE]; // sample frame buffer
  short int d2[NTMAX*RX_SAMPLE_RATE]; // sample frame buffer
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
    int knum;                   // maximum number of frames per period in d2
    int kpos;                   // number of frames already processed in d2
    int kin;                    // number of frames available in d2
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
