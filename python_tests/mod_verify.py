import matplotlib.pyplot as plt
import numpy as np
from bitarray import bitarray
import sounddevice as sd

import scipy.io.wavfile
from scipy import signal
import wavio

ccitt_lookup = [ # uint16_t, 256 entries
   0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
   0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
   0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
   0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
   0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
   0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
   0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
   0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
   0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
   0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
   0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
   0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
   0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
   0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
   0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
   0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
   0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
   0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
   0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
   0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
   0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
   0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
   0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
   0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
   0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
   0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
   0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
   0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
   0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
   0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
   0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
   0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
]

def updateFCS(crc, b):
    return (crc >> 8) ^ ccitt_lookup[(crc ^ b) & 0xFF]

def insertByte(bits, stuffCount, currNRZI, b):
    for _ in range(8): # add byte, little-endian, w/ bit stuffing
        if b & 0x01:
            stuffCount += 1
            bits.append(currNRZI) # add 1
        else:
            stuffCount = 0
            bits.append(not currNRZI) # add 0
            currNRZI = not currNRZI
            
        if stuffCount == 5:
            stuffCount = 0
            bits.append(not currNRZI) # add 0
            currNRZI = not currNRZI
        
        b >>= 1
    
    return stuffCount, currNRZI

def insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, b):
    stuffCount, currNRZI = insertByte(bits, stuffCount, currNRZI, b)
    crc = updateFCS(crc, b)
    return stuffCount, crc, currNRZI

def insertCallsign(bits, stuffCount, crc, currNRZI, sign, end=False):
    c, ssid = 0, False
    for b in sign:
        if b == ord('-'):
            ssid = True
            break
        stuffCount, crc, currNRZI = insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, b << 1)
        c += 1
    for _ in range(6 - c): # pad to 6 bytes
        stuffCount, crc, currNRZI = insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, ord(' ') << 1)
    # http://www.ax25.net/AX25.2.2-Jul%2098-2.pdf for how to encode SSID
    ssid = int(sign[c+1:]) if ssid else 0 # need to parse int
    return insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, ((0b0110000 | ssid) << 1) | end)

def encodeAPRS(dest=b'APCAL', callsign=b'KN6IEK-15', digi=[b'WIDE1-1', b'WIDE2-2'], info=b'>Hello World!'):
    # pre-allocate bitarray in C++
    # max APRS packet size (w/o flags) is 7+7+56+1+1+256+2 = 330 bytes
    # APRS packet can be bit-stuffed, so can expand by 20% (so max 3300 bits)
    # pre and post flags are 8 bits each (no stuffing)
    # bits to allocate == 8*(pre + post flags + 2) + 3300
    # in this case need 3956 bits (494.5 bytes)
    
    bits = bitarray('00000001') * 3#* (40 + 1) # 40 preflags, 1 flag
    stuffCount = 0
    crc = 0xFFFF
    currNRZI = True
        
    stuffCount, crc, currNRZI = insertCallsign(bits, stuffCount, crc, currNRZI, dest) # destination
    stuffCount, crc, currNRZI = insertCallsign(bits, stuffCount, crc, currNRZI, callsign) # source
    for d in range(len(digi) - 1):
        stuffCount, crc, currNRZI = insertCallsign(bits, stuffCount, crc, currNRZI, digi[d]) # digipeaters
    stuffCount, crc, currNRZI = insertCallsign(bits, stuffCount, crc, currNRZI, digi[-1], end=True)
    
    stuffCount, crc, currNRZI = insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, 0x03) # control field
    stuffCount, crc, currNRZI = insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, 0xF0) # id
    
    for b in info:
        stuffCount, crc, currNRZI = insertByteUpdateFCS(bits, stuffCount, crc, currNRZI, b) # info
    
    print(hex(np.uint16(~crc)))
    
    for b in np.uint16(~crc).tobytes(): # FCS, LSB
        stuffCount, currNRZI = insertByte(bits, stuffCount, currNRZI, b)
    
    if currNRZI:
        bits += bitarray('00000001') * 3 #* (40 + 1) # 40 postflags, 1 flag
    else:
        bits += bitarray('11111110') * 3 #* (40 + 1) # 40 postflags, 1 flag
    return bits
    

sine_lookup = np.array([
 128, 131, 134, 137, 140, 143, 146, 149, 152, 156, 159, 162, 165, 168, 171, 174,
 176, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 206, 209, 211, 213, 216,
 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 237, 239, 240, 242, 243, 245,
 246, 247, 248, 249, 250, 251, 252, 252, 253, 254, 254, 255, 255, 255, 255, 255,
 255, 255, 255, 255, 255, 255, 254, 254, 253, 252, 252, 251, 250, 249, 248, 247,
 246, 245, 243, 242, 240, 239, 237, 236, 234, 232, 230, 228, 226, 224, 222, 220,
 218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 191, 188, 185, 182, 179,
 176, 174, 171, 168, 165, 162, 159, 156, 152, 149, 146, 143, 140, 137, 134, 131,
 128, 124, 121, 118, 115, 112, 109, 106, 103,  99,  96,  93,  90,  87,  84,  81,
  79,  76,  73,  70,  67,  64,  62,  59,  56,  54,  51,  49,  46,  44,  42,  39,
  37,  35,  33,  31,  29,  27,  25,  23,  21,  19,  18,  16,  15,  13,  12,  10,
   9,   8,   7,   6,   5,   4,   3,   3,   2,   1,   1,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   1,   1,   2,   3,   3,   4,   5,   6,   7,   8,
   9,  10,  12,  13,  15,  16,  18,  19,  21,  23,  25,  27,  29,  31,  33,  35,
  37,  39,  42,  44,  46,  49,  51,  54,  56,  59,  62,  64,  67,  70,  73,  76,
  79,  81,  84,  87,  90,  93,  96,  99, 103, 106, 109, 112, 115, 118, 121, 124
]).astype(np.uint8)

def afsk(bits, num_samples, index, prev_ph):
    # lots of precomputable stuff
    freq_mul = 5 # fs = baud * 2^freq_mul (samples_per_bit == 2^freq_mul)
    baud = 1200
    f_mark = 1200 # 1
    f_space = 2200 # 0

    fs = baud << freq_mul
    dph_mark = int(f_mark / fs * (1 << 32)) # using 2^32 == 2*pi
    dph_space = int(f_space / fs * (1 << 32))
    
    if ((num_samples >> freq_mul) << freq_mul) != num_samples:
        raise Exception(f'num_samples needs to be multiple of {1 << freq_mul}')
    sig = np.zeros(num_samples).astype(np.uint8) # make sure to pre-allocate in C++
    
    # actual computation
    sig_index = 0 # use register for this
    for i in range(num_samples >> freq_mul):
        dph = dph_mark if bits[index] else dph_space
        for j in range(1 << freq_mul):
            prev_ph += dph
            prev_ph %= (1 << 32) # done automagically with 32-bit
            sig_index = (i << freq_mul) + j
            sig[sig_index] = prev_ph >> 24 # storing as 8 bit value
        
        index += 1
        if index >= len(bits):
            break
            
    for i in range(sig_index + 1, num_samples):
        sig[i] = 127 # no more bits to transfer, just writing "0s" to end
    
    sig = np.array([sine_lookup[x] for x in sig])
    
    return sig, prev_ph, fs, index

def print_bitarray(b):
    i = 0
    temp = 0

    for bit in b:
        #print(bit)
        temp |= (bit << (7 - i))
        
        if (i < 7):
            i += 1
        else:
            print(hex(temp), end=' ')
            
            i=0
            temp = 0

msg = str(input('>'))
print(bytes(msg,'UTF-8'))
bits = encodeAPRS(dest=b'APRS', callsign=b'W6NXP', digi=[b'WIDE1-1',b'WIDE2-1'], info=bytes(msg,'UTF-8'))
print(bits)
print_bitarray(bits)

sig, ph, fs, i = [], 0, 0, 0
while i < len(bits):
    sig_frag, ph, fs, i = afsk(bits, 4096*2, i, ph) # generate 4kB at a time
    sig = np.concatenate((sig, sig_frag)) # send using DMA in C++
sig = sig.astype(np.uint8)

scipy.io.wavfile.write('test.wav', fs, sig)
sd.play((sig / 128 - 1)*.01, fs, blocking=True)
