#!/usr/bin/env python3

import sys
import serial
import io
import binascii
import struct
import time
import argparse
import traceback

# def SerialPortTextWrapper():
#     def __init__(self, port, lf="\r\n"):
#         self.port = port
#         self.lf = lf
#         self.rx_buffer = ""
#         self.tx_buffer = ""

#     def get_line(self):
#         idx = self.rx_buffer.find(self.lf):
#         if idx == -1:
#             return None

#     def readline(self):
#         idx = self.rx_buffer.find(self.lf)
#         if idx == -1:
#             s = self.port.read()
#             if len(s) > 0:
#                 self.rx_buffer += s


    

def main():
    parser = argparse.ArgumentParser(description='BLE parser')
    parser.add_argument("-p", "--port", type=str, default="COM10")
    parser.add_argument("-b", "--baud", type=int, default=115200)

    args = parser.parse_args()

    port = serial.Serial(args.port, args.baud, timeout=0.1)

    sio = io.TextIOWrapper(io.BufferedRWPair(port, port), encoding='ascii', newline='\n')

    cmd = "LYWSD03MMC,a4:c1:38:5e:de:26\n"
    sio.write(cmd)
    sio.flush()

    while True:
        line = sio.readline()
        line = line.rstrip()
        if len(line) > 0:
            print(line)

if __name__ == "__main__":
    main()