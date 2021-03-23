#!/usr/bin/env python3
# coding=utf-8

import socket
import struct
import time
import netifaces
import zlib
import threading
import colorsys
import random

PORT = 7868
# ADDR = '127.0.0.1'
ADDR = '192.168.1.143'

# sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32)
# sock.bind((MY_IP, PORT))

# sock.setsockopt(socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton(MY_IP))
# 42:A3:6B:00:87:DB
# sock.sendto(b"\x02\x42\xA3\x6B\x00\x87\xDB\x00\x00\xff\x00\x00\x00\x01\x00\x00\x00\x00\x01", ("255.255.255.255", PORT))
#             set hw_addr                 R   G   B   sec             usec            trans
# time.sleep(0.1)
# sock.sendto(b"\x01", ("255.255.255.255", PORT))

myself = b"testb\xED"
omega = b"\x42\xA3\x6B\x00\x87\xDB"
noexistent = b"n\x0E\x15\x7Ent"
null_address = b"\x00\x00\x00\x00\x00\x00"


def hsv2rgb(h,s,v):
    return tuple(round(i * 255) for i in colorsys.hsv_to_rgb(h/360.,s/100.,v/100.))


class OmegaException(Exception):
    pass

class QueryParseException(OmegaException):
    pass

class ResponseException(OmegaException):
    def __init__(self, code, message=None):
        self.code = code
        self.message = message
        super().__init__(f"Response Error ({code})" + (f": {message}" if message is not None else ""))

class Omega(object):
    @staticmethod
    def gen_id():
        return bytes([random.randint(0,255) for _ in range(6)])

    def __init__(self, host_id=None, omega_id=None, address='127.0.0.1', port=7868, crc_magic=0xCA7ADDED):
        self.host_id = host_id if host_id is not None else self.gen_id()
        self.omega_id = bytes([0 for _ in range(6)])
        self.address = address
        self.port = port
        self.crc_magic = crc_magic
        self.color = b'\0\0\0'
        self.status_time = 0
        self.position = 0
        self.transition = 0
        self.transition_duration = 0
        self.main_light = 0
        self.target_color = b'\0\0\0'

    def __enter__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.address, self.port))

        self.get_status()
        return self

    def __exit__(self, exception_type, exception_value, exception_traceback):
        self.sock.close()

    def set_current_status(self, status):
        if status is None:
            return
        color, time_of_query, position, transition_duration, transition, main, target = status
        self.color = color
        self.status_time = time_of_query
        self.position = position
        self.transition = transition
        self.transition_duration = transition_duration
        self.main_light = main
        self.target_color = target

    def calc_crc(self, buf):
        crc = socket.htonl(zlib.crc32(buf, self.crc_magic))
        return crc.to_bytes(4, 'little')

    def send_query_raw(self, type, command, payload=None):
        buf = self.host_id + bytes([type, command]) + self.omega_id
        if payload is not None:
            buf += payload
        buf += self.calc_crc(buf)

        self.sock.send(buf)
        ret = self.sock.recv(1024)
        status = self.parse_response(ret)
        self.set_current_status(status)
        return status

    def parse_query(self, buf):
        # |0      |8       |16      |24      |32
        # SENDER_I|D_______|________|________|
        # SENDER_I|D_______|QER_TYPE|COMMAND_|
        # RECPIPEN|T_ID_IF_|ANY_OR_Z|EROS____|
        # ________|________|~~~~~~~~~~~~~~~~~PAYLOAD?
        # CRC_32__|________|________|________|EOQ
        if len(buf) < 18:
            raise QueryParseException("message is less than 18 bytes")
        query_main = buf[0:-4]
        query_crc = buf[-4:]
        if query_crc != self.calc_crc(query_main):
            raise QueryParseException("CRC does not match")

        sender = query_main[0:6]
        query_type = query_main[6]
        command = query_main[7]
        recipient = query_main[8:14]
        payload = query_main[14:] if len(query_main) > 14 else None
        return sender, query_type, command, recipient, payload

    @staticmethod
    def error_message_by_code(code):
        if code == Omega.QUERY_ERROR_UNSPECIFIED:
            return "Unspecified Error"
        elif code == Omega.QUERY_ERROR_WRONG_TYPE:
            return "Wrong request type"
        elif code == Omega.QUERY_ERROR_WRONG_REQUEST:
            return "Wrong request command"
        elif code == Omega.QUERY_ERROR_BAD_REQUEST:
            return "Bad request"

    @staticmethod
    def parse_status(status):
        # |0      |8       |16      |24      |32
        # DEV_TYPE|R_______|G_______|B_______|
        # Time_Of_|Query___|________|________|
        # Time_Of_|Query_uS|ec______|________|
        # Transiti|on_durat|ion_____|________|
        # Transiti|on_durat|ion_uSec|________|
        # Position|________|________|________|
        # Position|_uFrac__|________|________|
        # Tran_Typ|Main____|Target_R|Target_G|
        # Target_B|EOQ

        if status is None:
            raise ResponseException(Omega.QUERY_ERROR_BAD_REQUEST, "No status received")
        if status[0] != Omega.DEVICE_TYPE_LED:
            raise OmegaException("Device is not LED")
        color = status[1:4]
        time_of_query = int.from_bytes(status[4:8], "big") + int.from_bytes(status[8:12], "big")/1000000.
        transition_duration = int.from_bytes(status[12:16], "big") + int.from_bytes(status[16:20], "big")/1000000.
        position = int.from_bytes(status[20:24], "big") + int.from_bytes(status[24:28], "big")/1000000.
        transition = status[28]
        main = status[29]
        target = status[30:]
        return color, time_of_query, position, transition_duration, transition, main, target

    def get_status(self):
        return self.send_query_raw(Omega.QUERY_TYPE_DISCOVERY, Omega.DISCOVERY_STATUS)

    @staticmethod
    def color_to_bytes(color):
        if not isinstance(color, bytes):
            color = bytes(color)
        return color

    def set_color(self, color):
        color = self.color_to_bytes(color)
        return self.send_query_raw(Omega.QUERY_TYPE_REQUEST, Omega.CMD_SET_COLOR, color)

    def enable_main_light(self):
        return self.send_query_raw(Omega.QUERY_TYPE_REQUEST, Omega.CMD_SET_MAIN_LIGHT, b'\xff')

    def disable_main_light(self):
        return self.send_query_raw(Omega.QUERY_TYPE_REQUEST, Omega.CMD_SET_MAIN_LIGHT, b'\x00')

    def toggle_main_light(self):
        return self.send_query_raw(Omega.QUERY_TYPE_REQUEST, Omega.CMD_TOGGLE_MAIN_LIGHT)

    def start_transition(self, target=b'\x00\x00\x00', transition=1, duration=1):
        target = self.color_to_bytes(target)
        intg, frac = divmod(duration, 1)
        prog = target + int(intg).to_bytes(4, "big") + int(frac).to_bytes(4, "big") + bytes([transition])
        return self.send_query_raw(Omega.QUERY_TYPE_REQUEST, Omega.CMD_SET_PROG, prog)

    def parse_response(self, query):
        if isinstance(query, bytes):
            query = self.parse_query(query)

        self.omega_id = query[0]
        if query[1] == Omega.QUERY_TYPE_RESPONSE:
            if query[2] == Omega.RESPONSE_STATUS:
                return self.parse_status(query[4])
        elif query[1] == Omega.QUERY_TYPE_ERROR:
            raise ResponseException(query[2], self.error_message_by_code(query[2]))
        else:
            raise ResponseException(Omega.QUERY_ERROR_WRONG_TYPE, "Received wrong response type")
        return None


Omega.QUERY_TYPE_DISCOVERY = 0x01
Omega.QUERY_TYPE_REQUEST = 0x02
Omega.QUERY_TYPE_RESPONSE = 0x03
Omega.QUERY_TYPE_ERROR = 0xfe

Omega.QUERY_ERROR_UNSPECIFIED = 0x00
Omega.QUERY_ERROR_WRONG_TYPE = 0x01
Omega.QUERY_ERROR_WRONG_REQUEST = 0x02
Omega.QUERY_ERROR_BAD_REQUEST = 0x03

Omega.DISCOVERY_PING = 0x01
Omega.DISCOVERY_STATUS = 0x02

Omega.RESPONSE_DISCOVERY_PING = 0x01
Omega.RESPONSE_STATUS = 0x02

Omega.DEVICE_TYPE_LED = 0x01

Omega.CMD_SET_PROG = 0x02
Omega.CMD_SET_MAIN_LIGHT = 0x03
Omega.CMD_SET_COLOR = 0x04
Omega.CMD_TOGGLE_MAIN_LIGHT = 0x05

if __name__ == "__main__":
    from pprint import pprint
    with Omega(address=ADDR, host_id=b'python') as o:
        allstart = time.time()
        NUM = 100
        for i in range(0,NUM):
            start = time.time()

            # 02 04 = request set_color => 03 02 = response status
            # buf = myself + b"\x02\x04" + omega + bytes(hsv2rgb(i%360, 100, 100))
            # 02 05 = request switch_main_light => 03 02 = response status
            # buf = myself + b"\x02\x05" + omega
            # 02 05 = request switch_main_light [zero address] => [ignored]
            # buf = myself + b"\x02\x05" + null_address
            # 02 05 = request switch_main_light [wrong address] => [ignored]
            # buf = myself + b"\x02\x05" + noexistent
            # 02 03 = request enable_main_light [right params] => 03 02 = response status
            # buf = myself + b"\x02\x03" + omega + b"\xff"
            # 02 03 = request enable_main_light [wrong params] => fe 03 = error bad_request
            # buf = myself + b"\x02\x03" + omega
            # 01 02 = discovery with_status [discovery does not need device name, hence 6×00] => 03 02 = response status
            # buf = myself + b"\x01\x02" + null_address
            # 01 02 = discovery with_status [wrong address] => [ignored]
            # buf = myself + b"\x01\x02" + noexistent
            # 43 02 = wrong 02 [wrong query type, right address] => fe 01 = error wrong_query_type
            # buf = myself + b"\x43\x02" + omega
            # 43 02 = wrong 02 [wrong query type, zero address] => [ignored]
            # buf = myself + b"\x43\x02" + null_address
            # 02 43 = request wrong [wrong command, right address] => fe 02 = error wrong_command
            # buf = myself + b"\x02\x43" + omega

            # crc = socket.htonl(zlib.crc32(buf, 0xCA7ADDED))
            # # print(crc.to_bytes(4,'little'))
            # buf += crc.to_bytes(4,'little')
            #
            # s.sendall(buf)
            # _ = s.recv(1024)

            pprint(o.set_color(hsv2rgb(i%360, 100, 100)))
            end = time.time()
            time.sleep(max(1/30. - (end-start), 0))

            # buf = b'\xa4l\xf1[\xed\xde\x01\x02\x00\x00\x00\x00\x00\x00' # \x08\xf1\x88l
            # buf += socket.htonl(zlib.crc32(buf, 0xCA7ADDED)).to_bytes(4,'little')
            # print(buf)
        elapsed = time.time() - allstart
        print(f"took {elapsed} seconds, rate: {NUM/elapsed}")
        pprint(o.toggle_main_light())
        pprint(o.toggle_main_light())
        pprint(o.disable_main_light())
        pprint(o.enable_main_light())
        pprint(o.enable_main_light())
        pprint(o.disable_main_light())
        pprint(o.disable_main_light())
        print("before transition")
        pprint(o.get_status())
        pprint(o.start_transition([0,0,0], duration=3))
        time.sleep(1)
        pprint(o.get_status())
        time.sleep(1)
        pprint(o.get_status())
        time.sleep(1)
        pprint(o.get_status())
        time.sleep(1)
        pprint(o.get_status())

