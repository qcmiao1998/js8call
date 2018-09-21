import socket

KKEY = 0x73e2

def do_hash(callsign):
    rootCall = callsign.split("-")[0].upper() + '\0'
    
    hash = KKEY
    i = 0
    length = len(rootCall)
    
    while (i+1 < length):
        hash ^= ord(rootCall[i])<<8
        hash ^= ord(rootCall[i+1])
        i += 2
    
    return int(hash & 0x7fff)

HOST = 'rotate.aprs2.net'
#HOST = 'rotate.aprs.net'
PORT = 14580

print "Connecting..."

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))

print "Connected..."

data = s.recv(1024)
print data

print "Callsign: "
call = raw_input().strip()
pw = do_hash(call)
ver = "FT8Call"

login = "user {} pass {} ver {}\n".format(call, pw, ver)
s.send(login)

print "Login sent...", login

data = s.recv(1024)
print data

if 0:
    message = "KN4CRD>APRS,TCPIP*::EMAIL-2  :KN4CRD@GMAIL.COM TESTING!{04}\n"
    s.send(message)

if 0:
    message = "{}>APRS,TCPIP*::EMAIL-2  :kn4crd@gmail.com testing456{{01}}\n".format(call)
    message = "KN4CRD>APRS,TCPIP*::EMAIL-2  :KN4CRD@GMAIL.COM TESTING!{02}\n"
    s.send(message)

if 0:
    payload = ":This is a test message"
    message = "{}>APRS,TCPIP*::{}   {}\n".format(call, call, payload)
    s.send(message)
if 1:
    position = "=3352.45N/08422.71Wn"
    status = "FT8CALL VIA XX9XXX/XXXX 14.082500MHz -20dB"
    payload = "".join((position, status))
    message = "{}-16>APRS,TCPIP*:{}\n".format(call, payload)
    s.send(message)

print "Spot sent...", message

data = s.recv(1024)
print data

s.close()
