import numpy
import wave
import math
import struct

import winsound
import bitstring

source_call = "W6NXP"
dest_call = "APRS"
repeaters = ["WIDE1-1", "WIDE2-1"]

#2200 Hz -> 360/12 = 30
#1200 Hz -> 360/22 = 16.3636

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
                
        '''
        crc ^= buf[i] << 8
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1 #TODO: FIX THIS
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1
        '''

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

while True:
    message = str(input(">"))
    packet_bytes = bytearray()
    
    '''
    Create packet contents
    '''
    packet_bytes.extend(convert_callsign(dest_call))
    packet_bytes.extend(convert_callsign(source_call))
    
    for repeater in repeaters:
        packet_bytes.extend(convert_callsign(repeater))
        
    packet_bytes[-1] |= 0x01 #append last address bit
    
    packet_bytes.append(0x03)
    packet_bytes.append(0xF0)
    
    packet_bytes.extend(bytearray(message, 'utf-8'))
    
    '''
    Bitwise packet encoding
    '''
    flip_bit_order(packet_bytes)
    
    crc = ~crc_16(packet_bytes) & 0xFFFF
    print("CRC: {}".format(hex(crc)))
    
    packet_bytes.append((crc >> 8) & 0xFF)
    packet_bytes.append(crc & 0xFF)
    packet_bits = bitstring.BitArray(packet_bytes)
    packet_bits = stuff_bits(packet_bits)
    packet_bits = NRZ_to_NRZI(packet_bits)
    packet_bits = add_flags(packet_bits, 32, 32)

    print(packet_bits)
    
    #wave file creation
    wave_output = wave.open('temp.wav','w')
    wave_output.setnchannels(1) #mono
    wave_output.setsampwidth(2) #16 bit samples
    wave_output.setframerate(26400) #same sample rate as modulator on MSP430
    
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