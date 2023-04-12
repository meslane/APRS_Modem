import numpy as np
import scipy

from scipy.io import wavfile
import scipy.io

import matplotlib.pyplot as plt

filename = str(input("filename> "))
#delay_low = int(input("lowest delay> "))
#delay_high = int(input("highest delay> "))

samplerate, data = wavfile.read(filename)

print(samplerate)
print(data)

def comb_filter(data, delay, a):
    delay_queue = []
    output = []
    
    for i in range(delay): #pre-populate delay queue
        delay_queue.append(0)
        
    for i in range(len(data)):
        current = data[i] + (a * delay_queue[0]) #feedback
        delay_queue.pop(0)
        delay_queue.append(current)
        output.append(current)
    
    return output
    
def moving_average(data, n):
    output = []

    for i in range(len(data)):
        sum = 0
        for j in range(i, i+n):
            if (j < len(data)):
                sum += data[j]
        
        output.append(sum/n)
        
    return output
    
def envelope(data, d):
    output = []
    
    last_peak = 0
    for i in range(len(data)):
        if (i > d and i < len(data) - d):
            if (data[i] > data[i-d] and data[i] > data[i+d]):
                last_peak = data[i]
                
        output.append(last_peak)
        
    return output
    
data = moving_average(data, 12)    
   
start = 10000
end = 20000
time = []
for i in range(start, end):
    time.append(i)
    
comb_2200 = np.abs(comb_filter(data, 20, 0.8))
#comb_2200_env = moving_average(envelope(comb_2200, 5), 5)

comb_1200 = np.abs(comb_filter(data, 37, 0.8))
#comb_1200_env = moving_average(envelope(comb_1200, 5), 5)

comb_diff = []
for i in range(len(comb_2200)):
    comb_diff.append(comb_2200[i] - comb_1200[i])

#comb_diff = np.abs(comb_diff)

comb_diff_env = envelope(comb_diff, 2)

plt.plot(time, comb_2200[start:end])
#plt.plot(time, comb_2200_env[start:end])

plt.plot(time, comb_1200[start:end])
#plt.plot(time, comb_1200_env[start:end])

plt.plot(time, comb_diff[start:end])
plt.plot(time, comb_diff_env[start:end])

plt.plot(time, data[start:end])
plt.legend()
plt.show()

'''
freq = []
mag = []

for i in range(delay_low,delay_high):
    print("Delay={}, Freq={}Hz".format(i, round(samplerate/i)))
    filter_data = comb_filter(data, i, 0.9) #2200 Hz
    magnitude = sum(np.abs(filter_data))/len(filter_data)
    print(magnitude)
    
    freq.append(round(samplerate/i))
    mag.append(magnitude)
    
plt.xscale("log")
plt.plot(freq, (mag))
plt.show()
'''