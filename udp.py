from __future__ import print_function
from socket import socket, AF_INET, SOCK_DGRAM

listen = ('127.0.0.1', 2237)

def main():
    sock = socket(AF_INET, SOCK_DGRAM)
    print("listening on", ':'.join(map(str, listen)))
    sock.bind(listen)
    try:
        while True:
            content, addr = sock.recvfrom(1024)
            print("from:", ":".join(map(str, addr)))
            print("->", repr(content))
            print("sending test reply...", end="")
            sock.sendto("test", addr)
            print("done")
    finally:
        sock.close()


if __name__ == '__main__':
    main()
