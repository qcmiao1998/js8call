from __future__ import print_function
from socket import socket, AF_INET, SOCK_DGRAM

listen = ('127.0.0.1', 2237)

def main():
    sock = socket(AF_INET, SOCK_DGRAM)
    print("listening on", ':'.join(map(str, listen)))
    sock.bind(listen)
    try:
        while True:
            content, addr = sock.recvfrom(65500)
            print("from:", ":".join(map(str, addr)))

            typ, msg = content.split('|')
            print("->", typ)
            print("->", msg)

            if typ == "PING":
                print("sending pong reply...", end="")
                sock.sendto("PONG|hello world|asdf", addr)
                print("done")
    finally:
        sock.close()


if __name__ == '__main__':
    main()
