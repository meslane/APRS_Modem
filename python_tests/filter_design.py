import numpy as np
import scipy

from scipy.io import wavfile
import scipy.io
from scipy import signal

import matplotlib.pyplot as plt
from matplotlib.ticker import EngFormatter

formatter = EngFormatter(unit='Hz')
fig = plt.figure()

#sampling rate = 22050 Hz

'''
Input bandpass filter
'''
print("Bandpass Filter:")

BP_ORDER = 1
BP_LOW = 600
BP_HIGH = 4000
BP_RS = 60
BP_TYPE = 'butter'

b, a = signal.iirfilter(BP_ORDER, [BP_LOW, BP_HIGH], fs=22050, rs=BP_RS,
                        btype='band', analog=False, ftype=BP_TYPE, output='ba')
                        
z,p,k = signal.iirfilter(BP_ORDER, [BP_LOW, BP_HIGH], fs=22050, rs=BP_RS,
                        btype='band', analog=False, ftype=BP_TYPE, output='zpk')

print("Numerator (b) coeffs. = {}".format(b))
print("Denominator (a) coeffs. = {}".format(a))

print("Zeros = {}".format(z))
print("Poles = {}".format(p))

for pole in p:
    if (abs(pole) > 1):
        print("UNSTABLE FILTER")
        break

w, h = signal.freqz(b, a, fs=22050, worN=2048)
ax = fig.add_subplot(1, 2, 1)
ax.semilogx(w, 20 * np.log10(np.maximum(abs(h), 1e-5)))
ax.set_title('Input Bandpass Frequency response')
ax.set_xlabel('Frequency [Hz]')
ax.set_ylabel('Amplitude [dB]')
ax.grid(which='both', axis='both')
ax.xaxis.set_major_formatter(formatter)

print()

'''
Demodulator lowpass filter
'''
print("Lowpass Filter:")

LP_ORDER = 2
LP_CUTOFF = 1500
LP_RS = 60
LP_TYPE = 'butter'

b, a = signal.iirfilter(LP_ORDER, LP_CUTOFF, fs=22050, rs=LP_RS,
                        btype='lowpass', analog=False, ftype=LP_TYPE, output='ba')
                        
z,p,k = signal.iirfilter(LP_ORDER, LP_CUTOFF, fs=22050, rs=LP_RS,
                        btype='lowpass', analog=False, ftype=LP_TYPE, output='zpk')

print("Numerator (b) coeffs. = {}".format(b))
print("Denominator (a) coeffs. = {}".format(a))

print("Zeros = {}".format(z))
print("Poles = {}".format(p))

for pole in p:
    if (abs(pole) > 1):
        print("UNSTABLE FILTER")
        break
        
w, h = signal.freqz(b, a, fs=22050, worN=2048)
ax2 = fig.add_subplot(1, 2, 2)
ax2.semilogx(w, 20 * np.log10(np.maximum(abs(h), 1e-5)))
ax2.set_title('Lowpass Frequency response')
ax2.set_xlabel('Frequency [Hz]')
ax2.set_ylabel('Amplitude [dB]')
ax2.grid(which='both', axis='both')
ax2.xaxis.set_major_formatter(formatter)

plt.show()

