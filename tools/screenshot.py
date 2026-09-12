#!/usr/bin/env python3
"""Знімок екрана пристрою через послідовний порт -> PNG.

Плата читає відеопам'ять ILI9341 назад по SPI і віддає її рядками в base64.
Тут це збирається у справжню картинку, щоб відмальовку можна було перевіряти
очима, а не здогадками.

  ./screenshot.py [команда] [файл.png]
"""
import os, sys, time, termios, fcntl, struct, select, base64, zlib, binascii

PORT = os.environ.get("PORT") or (sorted(__import__("glob").glob("/dev/cu.usbmodem*"))[0] if __import__("glob").glob("/dev/cu.usbmodem*") else "/dev/cu.usbmodem1442101")

def open_port():
    fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd); a[0]=a[1]=a[3]=0
    a[2] = termios.CS8|termios.CREAD|termios.CLOCAL; a[4]=a[5]=termios.B115200
    a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    fcntl.ioctl(fd, 0x8004746d, struct.pack('I', 0x002|0x004))   # DTR|RTS
    return fd

def talk(cmd, secs, want_end=None):
    fd = open_port()
    time.sleep(0.25)
    os.write(fd, (cmd+"\n").encode())
    buf=b''; end=time.time()+secs
    while time.time()<end:
        r,_,_ = select.select([fd],[],[],0.2)
        if r:
            try: buf += os.read(fd, 65536)
            except OSError: pass
            if want_end and want_end in buf: break
    os.close(fd)
    return buf.decode('utf-8','replace')

def png(pixels, w, h, path):
    raw=b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            v = pixels[y*w+x]
            raw += bytes((((v>>11)&31)*255//31, ((v>>5)&63)*255//63, (v&31)*255//31))
    def chunk(t,d):
        c=t+d
        return struct.pack('>I',len(d))+c+struct.pack('>I', binascii.crc32(c)&0xffffffff)
    out=b'\x89PNG\r\n\x1a\n'
    out+=chunk(b'IHDR', struct.pack('>IIBBBBB', w,h,8,2,0,0,0))
    out+=chunk(b'IDAT', zlib.compress(raw,6))
    out+=chunk(b'IEND', b'')
    open(path,'wb').write(out)

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv)>1 else "scr"
    if cmd != "scr":
        print(talk(cmd, 6)); sys.exit(0)
    path = sys.argv[2] if len(sys.argv)>2 else "screen.png"
    txt = talk("scr", 40, b"SCR_END")
    if "SCR_BEGIN" not in txt:
        print("знімок не прийшов:\n"+txt[-400:]); sys.exit(1)
    body = txt.split("SCR_BEGIN",1)[1].split("\n",1)[1].split("SCR_END")[0]
    rows=[l.strip() for l in body.split("\n") if len(l.strip())>800]
    px=[]
    for l in rows:
        d=base64.b64decode(l+"="*(-len(l)%4))
        px += list(struct.unpack('<%dH'%(len(d)//2), d[:len(d)//2*2]))
    h=len(rows); w=320
    if h==0: print("порожньо"); sys.exit(1)
    png(px[:w*h], w, h, path)
    print(f"збережено {path}: {w}x{h}, рядків {h}")
