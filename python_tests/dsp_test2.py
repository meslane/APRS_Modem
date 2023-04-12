import numpy as np
import scipy

from scipy.io import wavfile
import scipy.io

import matplotlib.pyplot as plt

filename = str(input("filename> "))

samplerate, data = wavfile.read(filename)

data = data.astype('float64')

print(samplerate)
print(data)

def mixer(data, k):
    delay_queue = []
    output = []
    
    for i in range(k): #pre-populate delay queue
        delay_queue.append(0)
        
    for i in range(len(data)):
        current = data[i] * delay_queue[0]
        delay_queue.pop(0)
        delay_queue.append(data[i])
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
    
def sub_avg(data, n):
    output = []
    
    for i in range(len(data)):
        sum = 0
        for j in range(i, i+n):
            if (j < len(data)):
                sum += data[j]
        
        avg = sum/n
        
        output.append(data[i] - avg)
    
    return output
    
def normalize(data):
    output = []

    for i in range(len(data)): #normalize
        output.append(data[i] / max(data))
        
    return output

def comparate(data, thresh):
    output = []
    
    for i in range(len(data)):
        if data[i] > thresh:
            output.append(1)
        else:
            output.append(0)
            
    return output

start = 10000
end = 20000
time = []
for i in range(start, end):
    time.append(i)
    
data = sub_avg(data, 20)
mix = normalize(sub_avg(moving_average(mixer(data[start:end], 19), 50), 20))

plt.plot(time, normalize(data[start:end]))
plt.plot(time, mix)
plt.plot(time, comparate(mix, 0))
plt.legend()
plt.show()