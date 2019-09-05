!parameter (NSPS=480)                  !Samples per symbol at 12000 S/s
!parameter (NTXDUR=5)                  !TX Duration in Seconds
!parameter (NDOWNSPS=16)               !Downsampled samples per symbol
!parameter (AZ=6.0)                    !Near dupe sync spacing
!parameter (NDD=136)                   !Downconverted FFT Bins - 100 Bins
!parameter (JZ=62)                     !Sync Search Space over +/- 2.5s relative to 0.5s TX start time. 2.48 = 62/4/(12000/1920) ?


! parameter (NSPS=384,  NTXDUR=4,  NDOWNSPS=16, NDD=150, JZ=116) ! 250 Hz  31.25 baud 60 wpm -18.0dB (1.0Eb/N0)  2.52s
! parameter (NSPS=384,  NTXDUR=5,  NDOWNSPS=16, NDD=160, JZ=116) ! 250 Hz  31.25 baud 48 wpm -18.0dB (1.0Eb/N0)  2.52s
! parameter (NSPS=480,  NTXDUR=5,  NDOWNSPS=16, NDD=136, JZ=116) ! 200 Hz     25 baud 48 wpm -19.0dB (1.0Eb/N0)  3.16s
! parameter (NSPS=480,  NTXDUR=6,  NDOWNSPS=20, NDD=150, JZ=116) ! 200 Hz     25 baud 40 wpm -19.0dB (1.0Eb/N0)  3.16s
! parameter (NSPS=500,  NTXDUR=6,  NDOWNSPS=20, NDD=144, JZ=116) ! 192 Hz     24 baud 40 wpm -19.4dB (1.0Eb/N0)  3.29s
  parameter (NSPS=600,  NTXDUR=6,  NDOWNSPS=12, NDD=120, JZ=172) ! 160 Hz     20 baud 40 wpm -20.0dB (1.0Eb/N0)  3.95s
! parameter (NSPS=768,  NTXDUR=8,  NDOWNSPS=24, NDD=125, JZ=116) ! 125 Hz 15.625 baud 32 wpm -21.0dB (1.0Eb/N0)  5.05s
! parameter (NSPS=800,  NTXDUR=8,  NDOWNSPS=24, NDD=100, JZ=116) ! 120 Hz     15 baud 32 wpm -21.2dB (1.0Eb/N0)  5.26s
! parameter (NSPS=960,  NTXDUR=8,  NDOWNSPS=24, NDD=100, JZ=116) ! 100 Hz  12.50 baud 32 wpm -22.0dB (1.0Eb/N0)  5.92s
! parameter (NSPS=1200, NTXDUR=10, NDOWNSPS=20, NDD=100, JZ=116) !  80 Hz     10 baud 24 wpm -23.0dB (1.0Eb/N0)  7.90s
! parameter (NSPS=1920, NTXDUR=15, NDOWNSPS=32, NDD=100, JZ=116) !  50 Hz  6.250 baud 16 wpm -25.0dB (1.0Eb/N0) 12.64s
! parameter (NSPS=4000, NTXDUR=30, NDOWNSPS=20, NDD=90,  JZ=62)  !  24 Hz      3 baud  8 wpm -28.2dB (1.0Eb/N0) 26.33s

parameter (AZ=12000.0/(1.0*NSPS)*0.64d0) !Dedupe overlap in Hz
parameter (ASTART=0.1)                   !Start delay in seconds
parameter (ASYNCMIN=1.5)                 !Minimum Sync

parameter (KK=87)                     !Information bits (75 + CRC12)
parameter (ND=58)                     !Data symbols
parameter (NS=21)                     !Sync symbols (3 @ Costas 7x7)
parameter (NN=NS+ND)                  !Total channel symbols (79)
parameter (NZ=NSPS*NN)                !Samples in full 15 s waveform (151,680)
parameter (NMAX=NTXDUR*12000)         !Samples in iwave (180,000)
parameter (NFFT1=2*NSPS, NH1=NFFT1/2) !Length of FFTs for symbol spectra
parameter (NSTEP=NSPS/4)              !Rough time-sync step size
parameter (NHSYM=NMAX/NSTEP-3)        !Number of symbol spectra (1/4-sym steps)
parameter (NDOWN=NSPS/NDOWNSPS)       !Downsample factor to 32 samples per symbol
parameter (NQSYMBOL=NDOWNSPS/4)       !Downsample factor of a quarter symbol
