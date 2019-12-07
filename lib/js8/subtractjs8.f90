subroutine subtractjs8(dd,itone,f0,dt)

! Subtract an ft8 signal
!
! Measured signal  : dd(t)    = a(t)cos(2*pi*f0*t+theta(t))
! Reference signal : cref(t)  = exp( j*(2*pi*f0*t+phi(t)) )
! Complex amp      : cfilt(t) = LPF[ dd(t)*CONJG(cref(t)) ]
! Subtract         : dd(t)    = dd(t) - 2*REAL{cref*cfilt}

  use timer_module, only: timer

  !include 'js8_params.f90'
  
  parameter (NFRAME=NSPS*NN)
  parameter (NFFT=NMAX, NFILT=1400)

  real*4  dd(NMAX), window(-NFILT/2:NFILT/2)
  complex cref,camp,cfilt,cw
  integer itone(NN)
  logical first
  data first/.true./
  common/heap8/cref(NFRAME),camp(NMAX),cfilt(NMAX),cw(NMAX)
  save first

  nstart=dt*12000+1

  if(NWRITELOG.eq.1) then
      write(*,*) '<DecodeDebug> generating reference signal', nstart
      flush(6)
  endif

  call genjs8refsig(itone,cref,f0)
  camp=0.
  do i=1,nframe
    id=nstart-1+i 
    if(id.ge.1.and.id.le.NMAX) camp(i)=dd(id)*conjg(cref(i))
  enddo

  if(NWRITELOG.eq.1) then
      write(*,*) '<DecodeDebug> filtering', nfft
      flush(6)
  endif

  if(first) then
! Create and normalize the filter
     pi=4.0*atan(1.0)
     fac=1.0/float(nfft)
     sum=0.0
     do j=-NFILT/2,NFILT/2
        window(j)=cos(pi*j/NFILT)**2
        sum=sum+window(j)
     enddo
     cw=0.
     cw(1:NFILT+1)=window/sum
     cw=cshift(cw,NFILT/2+1)
     call four2a(cw,nfft,1,-1,1)
     cw=cw*fac
     first=.false.
  endif

  cfilt=0.0
  cfilt(1:nframe)=camp(1:nframe)
  call four2a(cfilt,nfft,1,-1,1)
  cfilt(1:nfft)=cfilt(1:nfft)*cw(1:nfft)
  call four2a(cfilt,nfft,1,1,1)

  if(NWRITELOG.eq.1) then
      write(*,*) '<DecodeDebug> subtracting filtered reference', nfft
      flush(6)
  endif

! Subtract the reconstructed signal
  do i=1,nframe
     j=nstart+i-1
     if(j.ge.1 .and. j.le.NMAX) dd(j)=dd(j)-2*REAL(cfilt(i)*cref(i))
  enddo

  return
end subroutine subtractjs8

