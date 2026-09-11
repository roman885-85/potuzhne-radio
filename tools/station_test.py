#!/usr/bin/env python3
"""Прохід по плейлисту на самому пристрої: вмикає кожну станцію й дивиться,
чи декодер отримав бітрейт (тобто звук реально пішов), чи повідомив помилку.
Одне з'єднання з портом на весь прохід — без перевідкриттів."""
import os, sys, time, termios, fcntl, struct, select, re
PORT="/dev/cu.usbmodem1442101"
def open_port():
    fd=os.open(PORT, os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
    a=termios.tcgetattr(fd); a[0]=a[1]=a[3]=0
    a[2]=termios.CS8|termios.CREAD|termios.CLOCAL; a[4]=a[5]=termios.B115200
    a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    fcntl.ioctl(fd, 0x8004746d, struct.pack('I', 0x002|0x004))
    return fd
def read_for(fd, secs):
    out=b''; end=time.time()+secs
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.2)
        if r:
            try: out+=os.read(fd,65536)
            except OSError: pass
    return out.decode('utf-8','replace')
names=[l.split('\t')[0] for l in open(sys.argv[1],encoding='utf-8') if l.strip()]
first=int(sys.argv[2]) if len(sys.argv)>2 else 1
fd=open_port(); time.sleep(0.3)
ok=0; bad=[]
for i in range(first, len(names)+1):
    os.write(fd, f"play {i}\n".encode())
    txt=read_for(fd, 9.0)
    br=re.findall(r'BITRATE#: *(\d+)', txt)
    err=[l for l in txt.splitlines() if re.search(r'ERROR|error|failed|Failed|помилк|не вдалося|timed? ?out|refused', l)]
    meta=re.findall(r'CLI\.META#: *(.*)', txt)
    good = bool(br) and int(br[-1])>0
    ok+=good
    if not good: bad.append(i)
    print(f"{'OK ' if good else '-- '}{i:2} {names[i-1][:30]:30} бітрейт={br[-1] if br else '-':>6}  {('помилка: '+err[-1][:60]) if (err and not good) else (meta[-1][:40] if meta else '')}", flush=True)
os.close(fd)
print(f"\nпрацюють {ok} з {len(names)-first+1}; не заграли: {bad}")
