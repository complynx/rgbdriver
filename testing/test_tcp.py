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


class Omega(object):
    @staticmethod
    def gen_id():
        return bytes([random.randint(0,255) for _ in range(6)])

    def __init__(self, host_id=None, omega_id=None, address='127.0.0.1', port=7868):
        self.host_id = host_id if host_id is not None else self.gen_id()
        self.omega_id = omega_id if omega_id is not None else self.gen_id()
        self.address = address
        self.port = port

    def __enter__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.address, self.port))
        return self

    def __exit__(self):
        self.sock.close()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((ADDR, PORT))

    allstart = time.time()
    NUM = 500
    for i in range(0,NUM):
        start = time.time()

        # 02 04 = request set_color => 03 02 = response status
        buf = myself + b"\x02\x04" + omega + bytes(hsv2rgb(i%360, 100, 100))
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

        crc = socket.htonl(zlib.crc32(buf, 0xCA7ADDED))
        # print(crc.to_bytes(4,'little'))
        buf += crc.to_bytes(4,'little')

        s.sendall(buf)
        _ = s.recv(1024)
        end = time.time()
        time.sleep(max(1/30. - (end-start), 0))

        # buf = b'\xa4l\xf1[\xed\xde\x01\x02\x00\x00\x00\x00\x00\x00' # \x08\xf1\x88l
        # buf += socket.htonl(zlib.crc32(buf, 0xCA7ADDED)).to_bytes(4,'little')
        # print(buf)
    elapsed = time.time() - allstart
    print(f"took {elapsed} seconds, rate: {NUM/elapsed}")
