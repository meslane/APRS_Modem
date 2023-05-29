import numpy
import wave
import math
import struct

import winsound
import pyaudio
import bitstring

import threading
import queue

import scipy.io
from scipy import signal
import matplotlib.pyplot as plt

from bitarray import bitarray
from bitarray.util import ba2int

import time

source_call = "W6NXP"
dest_call = "APRS"
repeaters = ["WIDE1-1", "WIDE2-1"]

#2200 Hz -> 360/12 = 30
#1200 Hz -> 360/22 = 16.3636

audio = pyaudio.PyAudio()

def convert_callsign(callsign):
    call_bytes = bytearray(callsign, 'utf-8')
    output_bytes = bytearray()
    ID = 0
    
    for i, char in enumerate(call_bytes):
        if (char != 0x2D): #'-'
            output_bytes.append(char << 1)
        else:
            if (call_bytes[i + 1] - 0x30 == 1) and (len(call_bytes) > (i + 2)): #if > 10
                ID = 10 + (call_bytes[i + 2] - 0x30)
            else:
                ID = (call_bytes[i + 1] - 0x30)
            break
                
    #pad with spaces
    while (len(output_bytes) < 6):
        output_bytes.append(0x40)
            
    output_bytes.append(0x60 | ((ID & 0x0F) << 1))
    return output_bytes

def flip_bit_order(message):
    for i, octet in enumerate(message):
        temp = octet
        message[i] = 0x00
        
        for j in range(8):
            message[i] |= ((temp >> (7-j)) & 0x01) << j

def crc_16(buf):
    poly = 0x1021;
    crc = 0xFFFF;

    for i in range(len(buf)):
        crc ^= buf[i]  << 8
        
        for j in range(8):
            if (crc & 0x8000 != 0x0000):
                crc = (crc << 1) ^ poly
            else:
                crc = crc << 1

    return crc & 0xFFFF;

def stuff_bits(packet_bits):
    output_bits = bitstring.BitArray()
    ones = 0
    
    for bit in packet_bits.bin:
        output_bits.bin += bit
    
        if (bit == '1'):
            ones += 1
        else:
            ones = 0
            
        if (ones == 5):
            output_bits.bin += '0'
            ones = 0
    
    return output_bits
    
def unstuff_bits(packet_bits):
    output_bits = bitstring.BitArray()
    ones = 0
    skip = False
    
    for bit in packet_bits.bin:
        if bit == '1':
            ones += 1
        else:
            ones = 0
            
        if skip:
            skip = False
        else:
            output_bits.bin += bit
            
        if ones == 5:
            skip = True
            ones = 0
            
    return output_bits
    
def NRZ_to_NRZI(packet_bits):
    output_bits = bitstring.BitArray()
    current = '1'
    
    for i, bit in enumerate(packet_bits.bin):
        if (bit == '1'):
            output_bits.bin += current
        else:
            if current == '1':
                current = '0'
            elif current == '0':
                current = '1'
        
            output_bits.bin += current
            
    return output_bits
    
def NRZI_to_NRZ(packet_bits):
    output_bits = bitstring.BitArray()
    prev = '1'
    
    for i, bit in enumerate(packet_bits.bin):
        if (bit != prev):
            output_bits.bin += '0'
        else:
            output_bits.bin += '1'
            
        prev = bit
        
    return output_bits
    
def add_flags(packet_bits, num_start, num_end):
    output_bits = bitstring.BitArray()
    
    for i in range(num_start): #start flags
        output_bits.bin += '00000001'
        
    output_bits += packet_bits
    
    if (output_bits.bin[-1] == '1'):
        end_flag = '00000001'
    else:
        end_flag = '11111110'
        
    for i in range(num_end):
        output_bits.bin += end_flag
        
    return output_bits
        
'''
demod code
'''
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
        
class PLL:
    def __init__(self, sample_rate, symbol_rate):
        self.sample_rate = sample_rate
        self.symbol_rate = symbol_rate
        self.samples_per_symbol = sample_rate / symbol_rate
        self.limit = (self.samples_per_symbol / 2) * 0.0625 #apply float scaling since MSP430 is limited to +/-2
        
        self.count = 0
        self.last = 0
        
    def loop(self, sample):
        pulse = 0
        
        if (sample != self.last): #detect transition and nudge pll
            self.last = sample
            
            if (self.count > self.limit):
                self.count -= (self.samples_per_symbol * 0.0625)
            
            self.count -= self.count * (self.samples_per_symbol * 0.0625) 
        else:
            if (self.count > self.limit):
                pulse = 1
                self.count -= (self.samples_per_symbol * 0.0625)
        
        self.count += 0.0625
        return pulse

def num_ones(input, bits): #count number of ones in int
    sum = 0
    
    for i in range(bits):
        sum += (input >> i) & 0x01
        
    return bits

#this works if not threaded
def rx_thread(input_device):
    sample_rate = 9600
    
    samples_per_symbol = 18
    phase = samples_per_symbol - 1
    bit_index = 0
    output_byte = 0
    samples = []
    
    index = 0
    dsp_state = 'FLAG_SEARCH'

    bp_filter = IIR_filter([0.9839, -0.9839], [1, -0.9678])
    lp_filter = IIR_filter([0.2929, 0.2929], [1, -0.4142])
    pll = PLL(9600.0, 1200.0)

    #mixer delay queue
    DELAY_NUM = 4 #(9600/1200)/2 = 4
    delay_queue = []
    for i in range(DELAY_NUM):
        delay_queue.append(0) #pre-populate delay queue

    #start flag search queue
    flag_queue = bitarray()
    for i in range(32):
        flag_queue.append(1)

    stream = audio.open(format = pyaudio.paInt16,
    channels = 1,
    rate = sample_rate, #sample rate
    input = True,
    frames_per_buffer = 1,
    input_device_index = input_device)
    
    message = []
    
    while True:
        sample = int.from_bytes(stream.read(1, exception_on_overflow = False), "little", signed=True)
        sample /= (2 ** 15) #normalize

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
        
        '''
        Comparator
        '''
        bit = 1
        if (lp_output > 0):
            bit = 0

        '''
        PLL clock recovery
        '''
        pll_out = pll.loop(bit)
        
        '''
        Bitstream Decoding (only on clock edges)
        '''
        if (pll_out == 1): #sample
            #==state transitions==#
            if (dsp_state == 'FLAG_SEARCH'):
                if ba2int(flag_queue) == 0x01010101: #detect start flag (and make sure it's not a fluke)
                    dsp_state = 'START_FLAG'
                    #print("LOCKED")
            elif (dsp_state == 'START_FLAG'):
                if (bit_index == 0 and output_byte != 0x01):
                    dsp_state = 'MESSAGE'
                    #print("IN MESSAGE")
            elif (dsp_state == 'MESSAGE'):
                current_flag = ba2int(flag_queue) & 0xFF
                if (current_flag == 0x01 or current_flag == 0x7F): #detect end flags
                    if (bit_index == 0 and current_flag == 0x7F):
                        #message.append(0x00)
                        pass
                    else:
                        #message.append(0x00)
                        pass
                    
                    dsp_state = 'END_FLAG'
                    #print("END FLAG REACHED")
                elif (len(message) > 400 or (bit_index == 0 and output_byte == 0x00)):
                    dsp_state = 'RX_RESET'
                    timestring = output_string = time.strftime("%Y-%m-%d %H:%M:%S",time.gmtime())
                    print(timestring, end='|')
                    print("DEMODULATION FAILED")
            elif (dsp_state == 'END_FLAG'):
                ones_count = num_ones(ba2int(flag_queue) & 0xFF, 8)
                if (ones_count != 1 and ones_count != 7):
                    dsp_state = 'RX_DECODE'
                    #print("END FLAG COMPLETE")
            elif (dsp_state == 'RX_DECODE'):
                dsp_state = 'RX_RESET'
                #print("RX RESET")
            elif (dsp_state == 'RX_RESET'):
                dsp_state = 'FLAG_SEARCH'
                #print("FLAG SEARCH")
            
            #==state actions==#
            if (dsp_state == 'FLAG_SEARCH'):
                flag_queue.append(bit)
                flag_queue.pop(0)
            elif (dsp_state == 'START_FLAG' or dsp_state == 'MESSAGE' or dsp_state == 'END_FLAG'):
                if (bit_index == 0):
                    #print(hex(output_byte))
                    
                    if (dsp_state == 'MESSAGE'):
                        message.append(output_byte)
                    
                    output_byte = 0
            
                output_byte |= (bit << (7 - bit_index))
                
                #increment index
                if (bit_index == 7):
                    bit_index = 0
                else: 
                    bit_index += 1
                
                #do flag queue
                flag_queue.append(bit)
                flag_queue.pop(0)
                    
            elif (dsp_state == 'RX_DECODE' and len(message) > 17): #if packet is minimal length
                timestring = output_string = time.strftime("%Y-%m-%d %H:%M:%S",time.gmtime())
            
                message_bits = bitstring.BitArray(bytearray(message)) #lmao
                message_bits = NRZI_to_NRZ(message_bits)
                message_bits = unstuff_bits(message_bits)
                message_bytes = bytearray(message_bits.tobytes())
                
                #message_bytes.pop(-1) #remove end bytes
                #message_bytes.pop(-1)
                
                crc = ~crc_16(message_bytes[0:-2]) & 0xFFFF #crc of all but last 2 bytes
                
                temp_crc = crc
                crc = 0
                for i in range(16):
                    crc |= ((temp_crc >> (15-i)) & 0x01) << i
                    
                print("CRC: {}".format(hex(crc)))
                
                flip_bit_order(message_bytes)
                
                if (len(message_bytes) >= 2):
                    packet_crc = (((message_bytes[-1] << 8) & 0xFF00) | (message_bytes[-2] & 0x00FF)) & 0xFFFF
                else:
                    packet_crc = 0xFFFF
                
                print("Packet CRC: {}".format(hex(packet_crc)))
                
                message_string = ''
                
                #extract calls
                last = False
                call_index = 0
                while not last and (call_index < len(message_bytes) - 7):
                    call_bytes = message_bytes[(call_index * 7):((call_index + 1) *7)]
                    
                    if call_bytes[6] & 0x01 == 0x01:
                        last = True
                        
                    for i in range(6):
                        call_bytes[i] >>= 1
                        
                        if call_bytes[i] != 0x20:
                            message_string += str(chr(call_bytes[i]))
                        
                    ID = (call_bytes[6] >> 1) & 0x0F
                    
                    if (ID > 0):
                        message_string += '-'
                    
                        if (ID >= 10):
                            message_string += '1'
                            
                        message_string += str(chr((ID % 10) + 0x30))
                    
                    call_index += 1
                    message_string += '|'
                
                #get rest of message but avoid CRC or flags
                message_string += message_bytes[(call_index * 7) + 2: -2].decode('utf-8', errors='replace')
                
                print(timestring, end='|')
                print(message_string, end='|')
                
                if (crc == packet_crc):
                    print("CRC PASSED")
                else:
                    print("CRC FAILED")
                
            elif (dsp_state == 'RX_RESET'):
                message.clear()
                bit_index = 0
                output_byte = 0x01

def main():
    info = audio.get_host_api_info_by_index(0)
    numdevices = info.get('deviceCount')

    for i in range(0, numdevices):
        if (audio.get_device_info_by_host_api_device_index(0, i).get('maxInputChannels')) > 0:
            print("Input Device id ", i, " - ", audio.get_device_info_by_host_api_device_index(0, i).get('name'))

    input_device = int(input("Select input device ID:"))

    #thread = threading.Thread(target=rx_thread, args=[input_device])
    #thread.start()
    rx_thread(input_device)
    
    '''
    while True:
        message = str(input(">"))
        packet_bytes = bytearray()
        
        #create packet contents
        packet_bytes.extend(convert_callsign(dest_call))
        packet_bytes.extend(convert_callsign(source_call))
        
        for repeater in repeaters:
            packet_bytes.extend(convert_callsign(repeater))
            
        packet_bytes[-1] |= 0x01 #append last address bit
        
        packet_bytes.append(0x03)
        packet_bytes.append(0xF0)
        
        packet_bytes.extend(bytearray(message, 'utf-8'))
        
        #bitwise packet encoding
        flip_bit_order(packet_bytes)
        
        crc = ~crc_16(packet_bytes) & 0xFFFF
        print("CRC: {}".format(hex(crc)))
        
        packet_bytes.append((crc >> 8) & 0xFF)
        packet_bytes.append(crc & 0xFF)
        packet_bits = bitstring.BitArray(packet_bytes)
        packet_bits = stuff_bits(packet_bits)
        packet_bits = NRZ_to_NRZI(packet_bits)
        
        if (len(packet_bits) / 8 > 400):
            print("WARNING: message is too long: len = {}, max = 400".format(len(packet_bits) / 8))
        
        packet_bits = add_flags(packet_bits, 32, 32)

        #print(packet_bits)
        
        #wave file creation
        wave_output = wave.open('temp.wav','w')
        wave_output.setnchannels(1) #mono
        wave_output.setsampwidth(2) #16 bit samples
        wave_output.setframerate(26400) #same sample rate as modulator on MSP430
        
        #audio outputting
        print("Transmitting")
        
        current_angle = 0
        symbol_counter = 0
        symbol_index = 0
        current_symbol = '0'
        for i in range(len(packet_bits.bin) * 22): #22 samples per symbol
            if symbol_counter == 0: #get next symbol
                if (symbol_index < len(packet_bits.bin) - 1):
                    current_symbol = packet_bits.bin[symbol_index]
                    symbol_index += 1
        
            if current_symbol == '0': #2200 Hz
                current_angle += 30
            elif current_symbol == '1': #1200 Hz
                current_angle += 16.3636 
            
            if current_angle > 360: #prevent large values
                current_angle -= 360
            
            sample = int(math.sin(current_angle * 0.0174533) * 32767) #convert to radians and scale
            wave_output.writeframesraw(struct.pack('<h', sample))
            
            symbol_counter += 1
            
            if (symbol_counter == 22):
                symbol_counter = 0
        
        wave_output.close()

        winsound.PlaySound('temp.wav', winsound.SND_FILENAME) #play file we just created
        print("Transmission complete")
    '''
    
main()