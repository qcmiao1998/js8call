subroutine syncjs8d(cd0,icos,i0,ctwk,itwk,sync)
! Compute sync power for a complex, downsampled JS8 signal.

  !include 'js8_params.f90'

  parameter(NP=NMAX/NDOWN, NP2=NN*NDOWNSPS)
  complex cd0(0:NP-1)
  complex csynca(7*NDOWNSPS),csyncb(7*NDOWNSPS),csyncc(7*NDOWNSPS)
  complex csync2(7*NDOWNSPS)
  complex ctwk(7*NDOWNSPS)
  complex z1,z2,z3
  logical first
  data first/.true./
  save first,twopi,csynca,csyncb,csyncc

  p(z1)=(real(z1)**2 + aimag(z1)**2)          !Statement function for power

  integer icos7a(0:6), icos7b(0:6), icos7c(0:6)

  if(icos.eq.1) then
    icos7a = (/4,2,5,6,1,3,0/)                  !Beginning Costas 7x7 tone pattern
    icos7b = (/4,2,5,6,1,3,0/)                  !Middle Costas 7x7 tone pattern
    icos7c = (/4,2,5,6,1,3,0/)                  !End Costas 7x7 tone pattern
  else
    icos7a = (/0,6,2,3,5,4,1/)                  !Beginning Costas 7x7 tone pattern
    icos7b = (/1,5,0,2,3,6,4/)                  !Middle Costas 7x7 tone pattern
    icos7c = (/2,5,0,6,4,1,3/)                  !End Costas 7x7 tone pattern
  endif

  if(NWRITELOG.eq.1) then
    write(*,*) '<DecodeDebug> syncjs8d costas', icos7a, icos7b, icos7c
    flush(6)
  endif

! Set some constants and compute the csync array.  
  if( first ) then
    twopi=8.0*atan(1.0)

    fs2=12000.0/NDOWN                       !Sample rate after downsampling
    dt2=1/fs2                               !Corresponding sample interval
    taus=NDOWNSPS*dt2                       !Symbol duration
    baud=1.0/taus                           !Keying rate

    phia=0.0
    phib=0.0
    phic=0.0

    do i=0,6
      dphia=twopi*icos7a(i)*baud*dt2
      dphib=twopi*icos7b(i)*baud*dt2
      dphic=twopi*icos7c(i)*baud*dt2

      do j=1,NDOWNSPS
        k=i*NDOWNSPS+j
        csynca(k)=cmplx(cos(phia),sin(phia)) !Waveform for Beginning 7x7 Costas array
        csyncb(k)=cmplx(cos(phib),sin(phib)) !Waveform for Middle 7x7 Costas array
        csyncc(k)=cmplx(cos(phic),sin(phic)) !Waveform for End 7x7 Costas array
        phia=mod(phia+dphia,twopi)
        phib=mod(phib+dphib,twopi)
        phic=mod(phia+dphic,twopi)

        if(NWRITELOG.eq.1) then
            write(*,*) '<DecodeDebug> computing costas waveforms', k, i, j, phia, phib, phic, dphia, dphib, dphic
            flush(6)
        endif
      enddo
    
    enddo
    first=.false.
  endif

  sync=0

  do i=0,6
    i1=i0+i*NDOWNSPS
    i2=i1+36*NDOWNSPS
    i3=i1+72*NDOWNSPS

    z1=0.
    z2=0.
    z3=0.

    csync2=csynca
    if(itwk.eq.1) csync2=ctwk*csynca
    if(i1.ge.0 .and. i1+NDOWNSPS-1.le.NP2-1) z1=sum(cd0(i1:i1+7*NDOWNSPS-1)*conjg(csync2))

    csync2=csyncb
    if(itwk.eq.1) csync2=ctwk*csyncb
    if(i2.ge.0 .and. i2+NDOWNSPS-1.le.NP2-1) z2=sum(cd0(i2:i2+7*NDOWNSPS-1)*conjg(csync2))

    csync2=csyncc
    if(itwk.eq.1) csync2=ctwk*csyncc
    if(i3.ge.0 .and. i3+NDOWNSPS-1.le.NP2-1) z3=sum(cd0(i3:i3+7*NDOWNSPS-1)*conjg(csync2))

    sync = sync + p(z1) + p(z2) + p(z3)

    if(NWRITELOG.eq.1) then
        write(*,*) '<DecodeDebug> sync computation', i, sync
        flush(6)
    endif
  enddo

  return
end subroutine syncjs8d
