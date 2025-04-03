#!/usr/bin/env python
import sys

import numpy as np

def noise(n):
    return (np.random.randn(n) + 1j * np.random.randn(n)) / np.sqrt(2)

def tone(f, n):
    return np.exp(2j*np.pi*f*np.arange(n))

def psd(x):
    return 20 * np.log10(np.abs(np.fft.fft(x, len(x))))

if __name__ == "__main__":
    n = 1024
    if len(sys.argv) >= 2:
        n = int(sys.argv[1])

    while True:
        x = psd(noise(n) + tone(0.01, n))
        try:
            sys.stdout.buffer.write(np.real(x).astype(np.float32))
            sys.stdout.flush()
        except:
            break
