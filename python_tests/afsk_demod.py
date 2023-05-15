import numpy as np
#import scipy

from scipy.io import wavfile
import scipy.io
from scipy import signal
import matplotlib.pyplot as plt

class IIR_filter:
    def __init__(self, b, a):
        self.delay = len(b) - 1
        
        self.b = b #num. coeffs.
        self.a = a #denom. coeffs.
        
        self.d_b = [] #delay queues
        self.d_a = []
        
        for i in range(self.delay): #prepopulate with 0s
            self.d_b.append(0)
            self.d_a.append(0)

    #shift in next sample
    def filter(self, sample):
        #compute output
        output = sample * self.b[0]
        
        for i in range(self.delay):
            output += self.b[i + 1] * self.d_b[i]
            output -= self.a[i + 1] * self.d_a[i]
        
        #shift delay queues
        self.d_b.insert(0, sample)
        self.d_b.pop()

        self.d_a.insert(0, output)
        self.d_a.pop()
        
        return output

filename = str(input("filename> "))

samplerate, data = wavfile.read(filename)

data = data.astype('float64')

print("Sample Rate: {} Hz".format(samplerate)) #should be 22050
print(data)

'''
DSP SECTION
'''
DELAY_NUM = 9 #(22050/1200)/2 = 9
delay_queue = []
for i in range(DELAY_NUM):
    delay_queue.append(0) #pre-populate delay queue

BANDPASS_ORDER = 2
delay_in_f1 = []
delay_out_f1 = []
for i in range(BANDPASS_ORDER):
    delay_in_f1.append(0)
    delay_out_f1.append(0)

bp_filter = IIR_filter([0.34,  0, -0.34], [1, -1.17, 0.31])
lp_filter = IIR_filter([0.035, 0.070, 0.035], [1, -1.41, 0.55])

output = []
intermed = []
input = []

'''
MAIN DSP LOOP
'''
index = 0
indices = []
for sample in data:
    input.append(sample)
    '''
    IIR bandpass filter
    '''
    bp_output = bp_filter.filter(sample)

    '''
    Mixer and delay line
    '''
    mixer_out = 6e-5 * bp_output * delay_queue[-1]

    delay_queue.insert(0, bp_output) #insert new values
    delay_queue.pop()
    
    '''
    IIR lowpass filter
    '''
    lp_output = lp_filter.filter(mixer_out)
    
    intermed.append(lp_output)
    
    '''
    Comparator
    '''
    comp_out = 0
    if (lp_output > 0):
        comp_out = 30000
    
    output.append(comp_out)

    index += 1
    indices.append(index)

plt.plot(indices, input)
plt.plot(indices, intermed)
plt.plot(indices, output)
plt.show()
