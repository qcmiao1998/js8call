subroutine genft8refsig(itone,cref,f0)
  include 'ft8_params.f90'

  complex cref(NN*NSPS)
  integer itone(NN)
  real*8 twopi,phi,dphi,dt,xnsps
  data twopi/0.d0/
  save twopi
  if( twopi .lt. 0.1 ) twopi=8.d0*atan(1.d0)

  xnsps=NSPS*1.0d0
  dt=1.d0/12000.d0
  phi=0.d0
  k=1
  do i=1,NN
    dphi=twopi*(f0*dt+itone(i)/xnsps)
    do is=1,NSPS
      cref(k)=cmplx(cos(phi),sin(phi))
      phi=mod(phi+dphi,twopi)
      k=k+1
    enddo
  enddo
  return
end subroutine genft8refsig
