#!/usr/bin/env python3
# coding=utf-8

import socket
import struct

import json

UDP_IP = "239.255.255.250"
UDP_PORT = 1982
if_ip = "192.168.0.108"
MULTICAST_TTL = 32

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
# sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)
sock.sendto(b"robot", (UDP_IP, UDP_PORT))
sock.close()