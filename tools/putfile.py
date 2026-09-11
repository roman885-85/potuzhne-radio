#!/usr/bin/env python3
"""Покласти файл у SPIFFS пристрою, не стираючи розділ.

Розділ цілком стирати не можна: поруч із веб-сторінками там лежать
збережена мережа й список станцій. Тому файли заливаються по одному
через послідовний порт, шістнадцятковим текстом.

  ./putfile.py <локальний файл> <шлях на пристрої>
"""
import os, sys, time, termios, fcntl, struct, select

PORT = "/dev/cu.usbmodem1442101"
PER_LINE = 48                      # байт на рядок; приймальний буфер порту всього 256

def open_port():
    fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd); a[0]=a[1]=a[3]=0
    a[2] = termios.CS8|termios.CREAD|termios.CLOCAL; a[4]=a[5]=termios.B115200
    a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    fcntl.ioctl(fd, 0x8004746d, struct.pack('I', 0x002|0x004))
    return fd

def drain(fd, secs):
    out=b''; end=time.time()+secs
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.1)
        if r:
            try: out += os.read(fd, 65536)
            except OSError: pass
    return out.decode('utf-8','replace')

def send_once(data, remote, pace):
    fd = open_port(); time.sleep(0.3); drain(fd, 0.2)
    os.write(fd, (f"put {remote}\n").encode())
    if "готовий" not in drain(fd, 1.0):
        os.close(fd); return -1
    for i in range(0, len(data), PER_LINE):
        os.write(fd, (data[i:i+PER_LINE].hex() + "\n").encode())
        time.sleep(pace)           # головний цикл зайнятий звуком, інакше буфер переповнюється
    ans = drain(fd, 0.3)
    os.write(fd, b".\n")
    ans += drain(fd, 1.5)
    os.close(fd)
    nums = [int(t) for t in ans.replace("\n"," ").split() if t.isdigit()]
    return nums[-1] if nums else -1

def main(local, remote):
    """Перевіряємо самі: рядок може загубитись, якщо головний цикл пристрою
       надовго відволікся на звук. Тоді пробуємо ще раз, повільніше."""
    data = open(local,'rb').read()
    pace = 0.045
    for attempt in range(4):
        got = send_once(data, remote, pace)
        if got == len(data):
            print(f"записано {got} байт"); return 0
        print(f"спроба {attempt+1}: {got} з {len(data)} — повторюю повільніше")
        pace += 0.02
    print("не вдалося записати повністю")
    return 1

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
