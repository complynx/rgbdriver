#!/usr/bin/env python3
# coding=utf-8

import socket
import struct
import time
import netifaces
import zlib
import threading

PORT = 7867

def recv(sock):
	while True:
		data, addr = sock.recvfrom(1024)  # buffer size is 1024 bytes
		print("received message:", data, " from ", addr)


def send(msg, port, baddr, saddr):
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
	sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	sock.bind((baddr, PORT))

	t = threading.Thread(target=recv, args=(sock,))
	t.daemon = True
	t.start()

	sock.sendto(msg, (saddr, port))

def sendall(msg, port):
	ifs = netifaces.interfaces()
	for _if in ifs:
		# print(_if)
		if_addrs = netifaces.ifaddresses(_if)
		# print(if_addrs)
		if netifaces.AF_INET in if_addrs:
			for if_addr in if_addrs[netifaces.AF_INET]:
				# print(if_addr)
				if if_addr['addr'].startswith('127.'):
					print(f"-- sending to {if_addr['addr']}-{if_addr['addr']}")
					send(msg, port, if_addr['addr'], if_addr['addr'])
				if 'broadcast' in if_addr:
					print(f"-- sending to {if_addr['addr']}-{if_addr['broadcast']}")
					send(msg, port, if_addr['addr'], if_addr['broadcast'])



# sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32)
# sock.bind((MY_IP, PORT))

# sock.setsockopt(socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton(MY_IP))
# 42:A3:6B:00:87:DB
# sock.sendto(b"\x02\x42\xA3\x6B\x00\x87\xDB\x00\x00\xff\x00\x00\x00\x01\x00\x00\x00\x00\x01", ("255.255.255.255", PORT))
#			  set hw_addr 				  R   G   B   sec             usec            trans
# time.sleep(0.1)
# sock.sendto(b"\x01", ("255.255.255.255", PORT))

# myself = b"testb\xED"
myself = b"python"
omega = b"\x42\xA3\x6B\x00\x87\xDB"
noexistent = b"n\x0E\x15\x7Ent"
null_address = b"\x00\x00\x00\x00\x00\x00"

# 02 04 = request set_color => 03 02 = response status
# buf = myself + b"\x02\x04" + omega + b"\xff\xff\xff"
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
buf = myself + b"\x01\x02" + null_address
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

sendall(buf, PORT)

time.sleep(3)

# buf = b'\xa4l\xf1[\xed\xde\x01\x02\x00\x00\x00\x00\x00\x00' # \x08\xf1\x88l
# buf += socket.htonl(zlib.crc32(buf, 0xCA7ADDED)).to_bytes(4,'little')
# print(buf)
