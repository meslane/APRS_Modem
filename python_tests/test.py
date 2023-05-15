import numpy as np

# Define parameters
baudrate = 100  # symbol rate (baud)
oversampling = 10  # number of samples per symbol
fc = 10  # frequency of clock signal
noise_power = 0.1  # power of additive white Gaussian noise

# Generate random bits
bits = np.random.randint(0, 2, 1000)

# Convert bits to rectangular pulses
pulses = np.tile(bits, oversampling)

# Add Gaussian noise
noise = np.sqrt(noise_power) * np.random.randn(len(pulses))
noisy_signal = pulses + noise

# Initialize clock recovery variables
prev_edge = 0
prev_clock = 1
prev_phase = 0
clock = np.zeros_like(bits)

# Perform clock recovery on a sample-by-sample basis
for i in range(len(noisy_signal)):
    # Detect rising and falling edges
    edge_detected = False
    if noisy_signal[i] > 0 and noisy_signal[i-1] <= 0:
        edge_detected = True
        edge_polarity = 1  # detected rising edge
    elif noisy_signal[i] < 0 and noisy_signal[i-1] >= 0:
        edge_detected = True
        edge_polarity = -1  # detected falling edge

    if edge_detected:
        # Calculate phase shift between current edge and previous edge
        curr_edge = i // oversampling
        phase_shift = 2 * np.pi * fc * (curr_edge / baudrate - prev_edge / baudrate)

        # Update clock signal
        phase_error = phase_shift - prev_phase
        clock[i // oversampling] = prev_clock * np.cos(phase_error)
        prev_clock = edge_polarity
        prev_edge = curr_edge
        prev_phase = phase_shift

# Print results
print("Bits:\n", bits)
print("Clock:\n", clock)