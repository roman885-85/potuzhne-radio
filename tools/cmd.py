#!/usr/bin/env python3
"""Надіслати кілька команд у пристрій одним з'єднанням і показати відповідь."""
import os, sys, time, termios, fcntl, struct, select
PORT=os.environ.get("PORT") or (sorted(__import__("glob").glob("/dev/cu.usbmodem*"))[0] if __import__("glob").glob("/dev/cu.usbmodem*") else "/dev/cu.usbmodem1442101")
def main(cmds, wait=3.0):
    fd=os.open(PORT, os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
    a=termios.tcgetattr(fd); a[0]=a[1]=a[3]=0
    a[2]=termios.CS8|termios.CREAD|termios.CLOCAL; a[4]=a[5]=termios.B115200
    a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    fcntl.ioctl(fd, 0x8004746d, struct.pack('I', 0x002|0x004))
    time.sleep(0.3)
    out=b''
    for c in cmds:
        os.write(fd, (c+"\n").encode()); time.sleep(0.15)
        end=time.time()+wait
        while time.time()<end:
            r,_,_=select.select([fd],[],[],0.2)
            if r:
                try: out+=os.read(fd,65536)
                except OSError: pass
    os.close(fd)
    return out.decode('utf-8','replace')
if __name__=="__main__":
    w=float(os.environ.get("W","3"))
    print(main(sys.argv[1:], w))
