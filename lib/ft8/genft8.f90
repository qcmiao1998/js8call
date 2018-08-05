subroutine genft8(msg,mygrid,bcontest,i3bit,msgsent,msgbits,itone)

! Encode an FT8 message, producing array itone().
  
  use crc
  use packjt
  include 'ft8_params.f90'
  character*68 alphabet
  character*22 msg,msgsent
  character*6 mygrid
  character*87 cbits
  logical bcontest,checksumok
  integer*4 i4Msg6BitWords(12)                !72-bit message as 6-bit words
  integer*1 msgbits(KK),codeword(3*ND)
  integer*1, target:: i1Msg8BitBytes(11)
  integer itone(NN)
  integer icos7(0:6)
  data icos7/2,5,6,0,4,1,3/                   !Costas 7x7 tone pattern

  alphabet='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-+/?. abcdefghijklmnopqrstuvwxyz'

  itype=6
  do i=1,12
    v=index(alphabet, msg(i:i)) - 1
    i4Msg6BitWords(i)=v
  enddo
  !i4Msg6BitWords( 1)=index(alphabet, msg( 1: 2))
  !i4Msg6BitWords( 2)=index(alphabet, msg( 2: 3))
  !i4Msg6BitWords( 3)=index(alphabet, msg( 3: 4))
  !i4Msg6BitWords( 4)=index(alphabet, msg( 4: 5))
  !i4Msg6BitWords( 5)=index(alphabet, msg( 5: 6))
  !i4Msg6BitWords( 6)=index(alphabet, msg( 6: 7))
  !i4Msg6BitWords( 7)=index(alphabet, msg( 7: 8))
  !i4Msg6BitWords( 8)=index(alphabet, msg( 8: 9))
  !i4Msg6BitWords( 9)=index(alphabet, msg( 9:10))
  !i4Msg6BitWords(10)=index(alphabet, msg(10:11))
  !i4Msg6BitWords(11)=index(alphabet, msg(11:12))
  !i4Msg6BitWords(12)=index(alphabet, msg(12:13))
  msgsent='                      '
  msgsent(1:12)=msg(1:12)

  ! call packmsg(msg,i4Msg6BitWords,itype,bcontest) !Pack into 12 6-bit bytes
  ! call unpackmsg(i4Msg6BitWords,msgsent,bcontest,mygrid) !Unpack to get msgsent

  write(cbits,1000) i4Msg6BitWords,32*i3bit
1000 format(12b6.6,b8.8)
  read(cbits,1001) i1Msg8BitBytes(1:10)
1001 format(10b8)
  i1Msg8BitBytes(10)=iand(i1Msg8BitBytes(10),128+64+32)
  i1Msg8BitBytes(11)=0
  icrc12=crc12(c_loc(i1Msg8BitBytes),11)
  icrc12=xor(icrc12, 42)

! For reference, here's how to check the CRC
!  i1Msg8BitBytes(10)=icrc12/256
!  i1Msg8BitBytes(11)=iand (icrc12,255)
!  checksumok = crc12_check(c_loc (i1Msg8BitBytes), 11)
!  if( checksumok ) write(*,*) 'Good checksum'

  write(cbits,1003) i4Msg6BitWords,i3bit,icrc12
1003 format(12b6.6,b3.3,b12.12)
  read(cbits,1004) msgbits
1004 format(87i1)

  call encode174(msgbits,codeword)      !Encode the test message

! Message structure: S7 D29 S7 D29 S7
  itone(1:7)=icos7
  itone(36+1:36+7)=icos7
  itone(NN-6:NN)=icos7
  k=7
  do j=1,ND
     i=3*j -2
     k=k+1
     if(j.eq.30) k=k+7
     itone(k)=codeword(i)*4 + codeword(i+1)*2 + codeword(i+2)
  enddo

  return
end subroutine genft8
