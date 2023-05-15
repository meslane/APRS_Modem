import numpy as np
#import scipy

from scipy.io import wavfile
import scipy.io
from scipy import signal
import matplotlib.pyplot as plt

from bitarray import bitarray
from bitarray.util import ba2int

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
        
#adapted from: https://github.com/mobilinkd/afsk-demodulator/blob/18914893d0853070788a37d986bbd58db08721aa//DigitalPLL.py
#deleted the lock detector because we don't need it if there are enough start flags
class PLL:
    def __init__(self, sample_rate, symbol_rate):
        self.sample_rate = sample_rate
        self.symbol_rate = symbol_rate
        self.samples_per_symbol = sample_rate / symbol_rate
        self.limit = self.samples_per_symbol / 2
        
        self.loop_filter = IIR_filter([0.145, 0.145], [1.0, -0.71])
        
        self.count = 0
        self.last = 0
        self.bits = 1.0
        
    def loop(self, sample):
        pulse = 0
        
        if (sample != self.last or self.bits > 127): #detect transition and nudge pll
            self.last = sample
            
            if (self.count > self.limit):
                self.count -= self.samples_per_symbol
                
            offset = self.count / self.bits
            
            j = self.loop_filter.filter(offset)
            
            self.count -= j * self.samples_per_symbol * 0.012
            
            self.bits = 1
        else:
            if (self.count > self.limit):
                pulse = 1
                self.count -= self.samples_per_symbol
                self.bits += 1
        
        self.count += 1.0
        return pulse
        
filename = str(input("filename> "))

samplerate, data = wavfile.read(filename)

data = data.astype('float64')

print("Sample Rate: {} Hz".format(samplerate)) #should be 22050
print(data)

'''
DSP SECTION
'''
bp_filter = IIR_filter([0.34,  0, -0.34], [1, -1.17, 0.31])
lp_filter = IIR_filter([0.035, 0.070, 0.035], [1, -1.41, 0.55])

pll = PLL(22050.0, 1200.0)

DELAY_NUM = 9 #(22050/1200)/2 = 9
delay_queue = []
for i in range(DELAY_NUM):
    delay_queue.append(0) #pre-populate delay queue
    
edge_queue = []
for i in range(2):
    edge_queue.append(0)

output = []
intermed = []
input = []
clk = []
bits = []

'''
MAIN DSP LOOP
'''
samples_per_symbol = 18

phase = samples_per_symbol - 1

clock_index = 0 #sample every (22050/1200) = ~18 samples
edge_period = 0

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
    bit = 1
    comp_out = 30000
    if (lp_output > 0):
        bit = 0
        comp_out = 0 #1 = 1200 Hz
    
    output.append(comp_out)

    '''
    PLL clock recovery
    '''
    pll_out = pll.loop(bit)
    clk.append(pll_out * 10000) #currently takes a couple of start flags to gain lock
    
    if (pll_out == 1): #sample
        bits.append(bit)

    #end code
    index += 1
    indices.append(index)

print(bits)

flag_queue = bitarray()
for i in range(16):
    flag_queue.append(1)

locked = False
start = True
end = False
bit_index = 0
output_byte = 0
message = []
for bit in bits:
    if not locked:
        print(hex(ba2int(flag_queue)))
        if ba2int(flag_queue) == 0x0101: #detect start flag (and make sure it's not a fluke)
            locked = True
            print("LOCKED")
            
        flag_queue.append(bit)
        flag_queue.pop(0)
    
    if locked:
        output_byte |= (bit << (7 - bit_index))
        
        if (bit_index == 7):
            print(hex(output_byte))
            
            if (start and output_byte != 0x01): #if out of flags
                start = False
                
            if not start:
                if (output_byte == 0x01 or output_byte == 0x7F): #detect end flag
                    end = True
                    
                if not end:
                    message.append(output_byte)
            
            output_byte = 0
            bit_index = 0
        else: 
            bit_index += 1

print([hex(x) for x in message])

plt.plot(indices, input)
plt.plot(indices, intermed)
plt.plot(indices, output)
plt.plot(indices, clk)
plt.show()
