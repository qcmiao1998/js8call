subroutine syncjs8d(cd0,i0,ctwk,itwk,sync)
! Compute sync power for a complex, downsampled FT8 signal.

  !include 'js8_params.f90'

  parameter(NP=NMAX/NDOWN,NSS=NSPS/NDOWN)
  complex cd0(0:NP-1)
  complex csynca(4*NSS),csyncb(4*NSS),csyncc(4*NSS)
  complex csync2(4*NSS)
  complex ctwk(4*NSS)
  complex z1,z2,z3
  logical first
  data first/.true./
  save first,twopi,csynca,csyncb,csyncc

  p(z1)=(real(z1)**2 + aimag(z1)**2)          !Statement function for power

  integer icos7a(0:6), icos7b(0:6), icos7c(0:6)
  if(NCOSTAS.eq.1) then
    icos7a = (/4,2,5,6,1,3,0/)                  !Beginning Costas 7x7 tone pattern
    icos7b = (/4,2,5,6,1,3,0/)                  !Middle Costas 7x7 tone pattern
    icos7c = (/4,2,5,6,1,3,0/)                  !End Costas 7x7 tone pattern
  else
    icos7a = (/0,6,2,3,5,4,1/)                  !Beginning Costas 7x7 tone pattern
    icos7b = (/1,5,0,2,3,6,4/)                  !Middle Costas 7x7 tone pattern
    icos7c = (/2,5,0,6,4,1,3/)                  !End Costas 7x7 tone pattern
  endif

! Set some constants and compute the csync array.  
  if( first ) then
    twopi=8.0*atan(1.0)

    k=1
    phia=0.0
    phib=0.0
    phic=0.0

    !fs2=12000.0/NDOWN                       !Sample rate after downsampling
    !dt2=1/fs2                               !Corresponding sample interval
    !taus=NDOWNSPS*dt2                       !Symbol duration
    !baud=1.0/taus                           !Keying rate

    do i=0,6

      dphia=2*twopi*icos7a(i)/real(NSS) 
      dphib=2*twopi*icos7b(i)/real(NSS) 
      dphic=2*twopi*icos7c(i)/real(NSS) 

      do j=1,NSS/2

        csynca(k)=cmplx(cos(phia),sin(phia)) !Waveform for Beginning 7x7 Costas array
        csyncb(k)=cmplx(cos(phib),sin(phib)) !Waveform for Middle 7x7 Costas array
        csyncc(k)=cmplx(cos(phic),sin(phic)) !Waveform for End 7x7 Costas array
        phia=mod(phia+dphia,twopi)
        phib=mod(phib+dphib,twopi)
        phic=mod(phia+dphic,twopi)

        k=k+1
      
      enddo
    
    enddo
    first=.false.
  endif

  i1=i0                            !four Costas arrays
  i2=i0+36*NSS
  i3=i0+72*NSS

  z1=0.
  z2=0.
  z3=0.

  csync2=csynca
  if(itwk.eq.1) csync2=ctwk*csynca      !Tweak the frequency
  if(i1.ge.0 .and. i1+8*NSS-1.le.NP-1) z1=sum(cd0(i1:i1+8*NSS-1:2)*conjg(csync2))

  csync2=csyncb
  if(itwk.eq.1) csync2=ctwk*csyncb      !Tweak the frequency
  if(i2.ge.0 .and. i2+8*NSS-1.le.NP-1) z2=sum(cd0(i2:i2+8*NSS-1:2)*conjg(csync2))

  csync2=csyncc
  if(itwk.eq.1) csync2=ctwk*csyncc      !Tweak the frequency
  if(i3.ge.0 .and. i3+8*NSS-1.le.NP-1) z3=sum(cd0(i3:i3+8*NSS-1:2)*conjg(csync2))

  sync = p(z1) + p(z2) + p(z3)

  return
end subroutine syncjs8d
