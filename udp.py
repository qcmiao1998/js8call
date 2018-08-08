from __future__ import print_function

import json
from socket import socket, AF_INET, SOCK_DGRAM
import time

listen = ('127.0.0.1', 2237)


def from_message(content):
    try:
        return json.loads(content)
    except ValueError:
        return {}


def to_message(typ, value='', params=None):
    if params is None:
        params = {}
    return json.dumps({'type': typ, 'value': value, 'params': params})


class Server(object):

    def process(self, message):
        typ = message.get('type', '')
        value = message.get('value', '')
        params = message.get('params', {})
        if not typ:
            return

        print('->', typ)

        if value:
            print('-> value', value)

        if params:
            print('-> params: ', params)

        if typ == 'PING':
            self.send('GET_GRID')
            self.send('GET_FREQ')
            self.send('GET_CALLSIGN')
            self.send('GET_CALL_ACTIVITY')

        #### elif typ == 'GRID':
        ####     if value != 'EM73TU49TQ':
        ####         self.send('SET_GRID', 'EM73TU49TQ')

        #### elif typ == 'FREQ':
        ####     if params.get('DIAL', 0) != 14064000:
        ####         self.send('SET_FREQ', '', {"DIAL": 14064000, "OFFSET": 977})
        ####         self.send('SEND_MESSAGE', 'HELLO WORLD')

        elif typ == 'CLOSE':
            self.close()

    def send(self, *args, **kwargs):
        message = to_message(*args, **kwargs)
        print('outgoing message:', message)
        self.sock.sendto(message, self.reply_to)
    
    def listen(self):
        print('listening on', ':'.join(map(str, listen)))
        self.sock = socket(AF_INET, SOCK_DGRAM)
        self.sock.bind(listen)
        self.listening = True
        try:
            while self.listening:
                content, addr = self.sock.recvfrom(65500)
                print('incoming message:', ':'.join(map(str, addr)))

                try:
                    message = json.loads(content)
                except ValueError:
                    message = {}

                if not message:
                    continue

                self.reply_to = addr
                self.process(message)

        finally:
            self.sock.close()

    def close(self):
        self.listening = False



def main():
    s = Server()
    s.listen()

if __name__ == '__main__':
    main()
