! LDPC (174,87) code

!parameter (NSPS=480)                  !Samples per symbol at 12000 S/s
!parameter (NTXDUR=5)                  !TX Duration in Seconds
!parameter (NDOWNSPS=16)               !Downsampled samples per symbol
!parameter (AZ=6.0)                    !Near dupe sync spacing
!parameter (NDD=136)                   !Downconverted FFT Bins - 100 Bins

! parameter (NSPS=480,  NTXDUR=5,  NDOWNSPS=16, NDD=136) ! 200 Hz
! parameter (NSPS=600,  NTXDUR=6,  NDOWNSPS=24, NDD=120) ! 160 Hz
! parameter (NSPS=1200, NTXDUR=10, NDOWNSPS=24, NDD=100) !  80 Hz
  parameter (NSPS=1920, NTXDUR=15, NDOWNSPS=32, NDD=100) !  50 Hz
! parameter (NSPS=3840, NTXDUR=30, NDOWNSPS=32, NDD=100) !  25 Hz

parameter (JZ=62)                     !Sync Search Space over +/- 2.5s relative to 0.5s TX start time. 
parameter (AZ=12000.0/(1.0*NSPS)*0.64d0)

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
