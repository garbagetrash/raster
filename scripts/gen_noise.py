#!/usr/bin/env python
import sys

import numpy as np

def real_noise(n):
    while True:
        somebytes = np.random.randn(n).astype(np.float32)
        sys.stdout.buffer.write(somebytes)

def noise_psd(n):
    while True:
        somebytes = 20 * np.log10(np.abs(np.fft.fft(np.random.randn(n), n)))
        somebytes = somebytes.astype(np.float32)
        try:
            sys.stdout.buffer.write(somebytes)
        except:
            break

if __name__ == "__main__":
    noise_psd(1024)
