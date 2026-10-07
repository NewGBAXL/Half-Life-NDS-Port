#!/usr/bin/env python3
"""Scripted input for an emulator running under Xvfb, through XTest.

usage: xin.py "<cmd>;<cmd>;..."   (commands are listed in README.md)
"""
import ctypes
import os
import subprocess
import sys
import threading
import time

x11 = ctypes.CDLL('libX11.so.6')
xtst = ctypes.CDLL('libXtst.so.6')
x11.XInitThreads()
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XStringToKeysym.restype = ctypes.c_ulong
x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
x11.XFlush.argtypes = [ctypes.c_void_p]
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
xtst.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                      ctypes.c_ulong]
display = x11.XOpenDisplay(None)


def move(x, y):
    xtst.XTestFakeMotionEvent(display, -1, x, y, 0)
    x11.XFlush(display)


def button(down):
    xtst.XTestFakeButtonEvent(display, 1, 1 if down else 0, 0)
    x11.XFlush(display)


def key(name, down):
    code = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name.encode()))
    xtst.XTestFakeKeyEvent(display, code, 1 if down else 0, 0)
    x11.XFlush(display)


def shot(path):
    subprocess.run(['import', '-window', 'root', path], check=False)


def sweep(seconds, step):
    """Stylus down, swept left and right at a steady rate, for `seconds`."""
    move(30, 340)
    time.sleep(0.1)
    button(True)
    time.sleep(0.2)
    x, direction, end = 30.0, 1, time.time() + seconds
    while time.time() < end:
        x += direction * step
        if x > 240 or x < 30:
            direction = -direction
        move(int(x), 340)
        time.sleep(0.008)
    button(False)


for command in sys.argv[1].split(';'):
    a = command.split()
    if not a:
        continue
    if a[0] == 'sleep':
        time.sleep(float(a[1]))
    elif a[0] == 'move':
        move(int(a[1]), int(a[2]))
    elif a[0] == 'key':
        key(a[1], True)
        time.sleep(float(a[2]))
        key(a[1], False)
    elif a[0] == 'tap':
        # tap <x> <y>: the stylus down and up again, quickly
        move(int(a[1]), int(a[2]))
        time.sleep(0.03)
        button(True)
        time.sleep(0.06)
        button(False)
    elif a[0] == 'drag':
        x0, y0, x1, y1 = map(int, a[1:5])
        move(x0, y0)
        time.sleep(0.1)
        button(True)
        time.sleep(0.15)
        for i in range(1, 21):
            move(x0 + (x1 - x0) * i // 20, y0 + (y1 - y0) * i // 20)
            time.sleep(0.03)
        time.sleep(0.1)
        button(False)
    elif a[0] == 'sweep':
        sweep(float(a[1]), float(a[2]))
    elif a[0] == 'shot':
        shot(a[1])
    elif a[0] == 'burst':
        os.makedirs(a[1], exist_ok=True)
        for i in range(int(a[2])):
            shot('%s/t%02d.png' % (a[1], i))
    elif a[0] == 'sweepburst':
        # sweepburst <dir> <n> <px per step>: screenshots WHILE the view turns
        os.makedirs(a[1], exist_ok=True)
        worker = threading.Thread(target=sweep, args=(600.0, float(a[3])), daemon=True)
        worker.start()
        time.sleep(0.5)
        for i in range(int(a[2])):
            shot('%s/t%02d.png' % (a[1], i))
    else:
        sys.exit('unknown command: ' + command)
