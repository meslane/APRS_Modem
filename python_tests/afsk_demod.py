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
#deleted bit counter
#deleted a bunch of other stuff too (call me Madman Muntz)
class PLL:
    def __init__(self, sample_rate, symbol_rate):
        self.sample_rate = sample_rate
        self.symbol_rate = symbol_rate
        self.samples_per_symbol = sample_rate / symbol_rate
        self.limit = (self.samples_per_symbol / 2) * 0.0625 #apply float scaling since MSP430 is limited to +/-2
        
        #self.loop_filter = IIR_filter([0.145, 0.145], [1.0, -0.71])
        
        self.count = 0
        self.last = 0
        
    def loop(self, sample):
        pulse = 0
        
        if (sample != self.last): #detect transition and nudge pll
            self.last = sample
            
            if (self.count > self.limit):
                self.count -= (self.samples_per_symbol * 0.0625)
            
            #can we delete the loop filter? BIG IF TRUE
            #self.count -= self.loop_filter.filter(self.count) * (self.samples_per_symbol * 0.1) #* 0.012
            self.count -= self.count * (self.samples_per_symbol * 0.0625) 
        else:
            if (self.count > self.limit):
                pulse = 1
                self.count -= (self.samples_per_symbol * 0.0625)
        
        self.count += 0.0625
        return pulse
        
filename = str(input("filename> "))

samplerate, data = wavfile.read(filename)

data = data.astype('float64')

print("Sample Rate: {} Hz".format(samplerate)) #should be 22050
print(data)

'''
DSP SECTION
'''
sample_rate = 9600

#bp_filter = IIR_filter([0.34,  0, -0.34], [1, -1.17, 0.31])
#bp_filter = IIR_filter([0.9929, -0.9929], [1, -0.9859]) #22050
bp_filter = IIR_filter([0.9839, -0.9839], [1, -0.9678])

#lp_filter = IIR_filter([0.035, 0.070, 0.035], [1, -1.41, 0.55])
#lp_filter = IIR_filter([0.1472, 0.1472], [1, -0.7055]) #22050
lp_filter = IIR_filter([0.2929, 0.2929], [1, -0.4142])

pll = PLL(9600.0, 1200.0)

#mixer delay queue
DELAY_NUM = 4 #(9600/1200)/2 = 4
delay_queue = []
for i in range(DELAY_NUM):
    delay_queue.append(0) #pre-populate delay queue

#start flag search queue
flag_queue = bitarray()
for i in range(16):
    flag_queue.append(1)

output = []
intermed = []
input = []
clk = []
bits = []
message = []
offset = []
count = []

for i in range(len(data)):
    data[i] += (2 ** 15) #add fake DC offset

'''
MAIN DSP LOOP
'''
samples_per_symbol = 18

phase = samples_per_symbol - 1

bit_index = 0
output_byte = 0

index = 0
indices = []
dsp_state = 'FLAG_SEARCH' #states = 'FLAG_SEARCH', 'START_FLAG', 'MESSAGE', 'END_FLAG'
for sample in data:
    sample /= (2 ** 15) #normalize

    input.append(sample)
    '''
    IIR bandpass filter
    '''
    bp_output = bp_filter.filter(sample)

    '''
    Mixer and delay line
    '''
    mixer_out = bp_output * delay_queue[-1]

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
    if (lp_output > 0):
        bit = 0
    
    output.append(bit)

    '''
    PLL clock recovery
    '''
    pll_out = pll.loop(bit)
    count.append(pll.count)
    clk.append(pll_out * 0.5) #currently takes a couple of start flags to gain lock
    
    '''
    Bitstream Decoding (only on clock edges)
    '''
    if (pll_out == 1): #sample
        bits.append(bit)
       
        #==state transitions==#
        if (dsp_state == 'FLAG_SEARCH'):
            if ba2int(flag_queue) == 0x0101: #detect start flag (and make sure it's not a fluke)
                dsp_state = 'START_FLAG'
                print("LOCKED")
        elif (dsp_state == 'START_FLAG'):
            if (bit_index == 0 and output_byte != 0x01):
                dsp_state = 'MESSAGE'
                print("IN MESSAGE")
        elif (dsp_state == 'MESSAGE'):
            if (bit_index == 0 and output_byte == 0x7F):
                dsp_state = 'END_FLAG'
                print("END FLAG REACHED")
        elif (dsp_state == 'END_FLAG'):
            if (bit_index == 0 and output_byte != 0x7F):
                dsp_state = 'FLAG_SEARCH'
                print("END FLAG COMPLETE")
        
        #==state actions==#
        if (dsp_state == 'FLAG_SEARCH'):
            flag_queue.append(bit)
            flag_queue.pop(0)
        elif (dsp_state == 'START_FLAG' or dsp_state == 'MESSAGE' or dsp_state == 'END_FLAG'):
            if (bit_index == 0):
                print(hex(output_byte))
                
                if (dsp_state == 'MESSAGE'):
                    message.append(output_byte)
                
                output_byte = 0
        
            output_byte |= (bit << (7 - bit_index))
            
            #leave at end
            if (bit_index == 7):
                bit_index = 0
            else: 
                bit_index += 1
            

    #end code
    index += 1
    indices.append(index)

#print(bits)
print([hex(x) for x in message])

plt.plot(indices, input, label='Input')
plt.plot(indices, intermed, label='Mixer output')
plt.plot(indices, output, label='Comparator output')
plt.plot(indices, clk, label='Sample clock')
plt.plot(indices, count, label='PLL count')
plt.legend()
plt.show()
