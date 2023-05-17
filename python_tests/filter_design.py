import numpy as np
import scipy

from scipy.io import wavfile
import scipy.io
from scipy import signal

import matplotlib.pyplot as plt
from matplotlib.ticker import EngFormatter

formatter = EngFormatter(unit='Hz')
fig = plt.figure()

sampling_rate = 9600 #Hz

'''
Input bandpass filter
'''
print("Bandpass Filter:")

BP_ORDER = 1
BP_LOW = 600
BP_HIGH = 4000
BP_RS = 60
BP_RP = 1
BP_TYPE = 'butter'

b, a = signal.iirfilter(BP_ORDER, [BP_LOW, BP_HIGH], fs=sampling_rate, rs=BP_RS, rp=BP_RP,
                        btype='band', analog=False, ftype=BP_TYPE, output='ba')
                        
z,p,k = signal.iirfilter(BP_ORDER, [BP_LOW, BP_HIGH], fs=sampling_rate, rs=BP_RS, rp=BP_RP,
                        btype='band', analog=False, ftype=BP_TYPE, output='zpk')

print("Numerator (b) coeffs. = {}".format(b))
print("Denominator (a) coeffs. = {}".format(a))

print("Zeros = {}".format(z))
print("Poles = {}".format(p))

for pole in p:
    if (abs(pole) > 1):
        print("UNSTABLE FILTER")
        break

w, h = signal.freqz(b, a, fs=sampling_rate, worN=2048)
ax = fig.add_subplot(1, 3, 1)
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

LP_ORDER = 1
LP_CUTOFF = 1200
LP_RS = 60
LP_TYPE = 'butter'

b, a = signal.iirfilter(LP_ORDER, LP_CUTOFF, fs=sampling_rate, rs=LP_RS,
                        btype='lowpass', analog=False, ftype=LP_TYPE, output='ba')
                        
z,p,k = signal.iirfilter(LP_ORDER, LP_CUTOFF, fs=sampling_rate, rs=LP_RS,
                        btype='lowpass', analog=False, ftype=LP_TYPE, output='zpk')

print("Numerator (b) coeffs. = {}".format(b))
print("Denominator (a) coeffs. = {}".format(a))

print("Zeros = {}".format(z))
print("Poles = {}".format(p))

for pole in p:
    if (abs(pole) > 1):
        print("UNSTABLE FILTER")
        break
        
w, h = signal.freqz(b, a, fs=sampling_rate, worN=2048)
ax2 = fig.add_subplot(1, 3, 2)
ax2.semilogx(w, 20 * np.log10(np.maximum(abs(h), 1e-5)))
ax2.set_title('Lowpass Frequency response')
ax2.set_xlabel('Frequency [Hz]')
ax2.set_ylabel('Amplitude [dB]')
ax2.grid(which='both', axis='both')
ax2.xaxis.set_major_formatter(formatter)

print()

'''
Alt input highpass filter
'''
print("Highpass filter")

HP_ORDER = 1
HP_CUTOFF = 50
HP_RS = 60
HP_TYPE = 'butter'

b, a = signal.iirfilter(HP_ORDER, HP_CUTOFF, fs=sampling_rate, rs=HP_RS,
                        btype='highpass', analog=False, ftype=HP_TYPE, output='ba')
                        
z,p,k = signal.iirfilter(HP_ORDER, HP_CUTOFF, fs=sampling_rate, rs=HP_RS,
                        btype='highpass', analog=False, ftype=HP_TYPE, output='zpk')

print("Numerator (b) coeffs. = {}".format(b))
print("Denominator (a) coeffs. = {}".format(a))

print("Zeros = {}".format(z))
print("Poles = {}".format(p))

for pole in p:
    if (abs(pole) > 1):
        print("UNSTABLE FILTER")
        break

w, h = signal.freqz(b, a, fs=sampling_rate, worN=2048)
ax3 = fig.add_subplot(1, 3, 3)
ax3.semilogx(w, 20 * np.log10(np.maximum(abs(h), 1e-5)))
ax3.set_title('Highpass Frequency response')
ax3.set_xlabel('Frequency [Hz]')
ax3.set_ylabel('Amplitude [dB]')
ax3.grid(which='both', axis='both')
ax3.xaxis.set_major_formatter(formatter)

plt.show()

