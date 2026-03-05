def pitch_tempo_warp(input_path, input_key_midi, input_tempo, target_key_midi, target_tempo, winSize=4096, padFactor=4):
    """
    Adapted from DAFx 2º Edition, Sec 7.4.4 \n
    Key Changes from Source:
         * Phase Locking and Scaling: By identifying local maxima in the magnitude spectrum at each FFT, the phase deviation can lock to each of those key frequencies, preserving the phase's vertical alignment. This has the effect of preserving drum transients and removing metallic sounds from inharmonicity on sustained notes that is common in pitch shifters.
         * Overlap-Add and Normalization: With a fixed window and hop size, window modulation happens that causes an audible fast tremolo effect. Dynamically calculating hop sizes from the pitch ratio keeps a constant overlap for adding. Normalizing with an energy sum helps preserve the spectral shape of the input.
         * Brick-wall Anti-Aliasing: Anti-aliasing is necessary when pitching up to prevent frequencies from exceeding the nyquist maximum and wrapping back down to introduce artifacts. Including a normal anti-aliasing filter within the process block introduced clicks at grain transitions. Because this algorithm has to take the FFT anyway, applying a brickwall filter is the most efficient way to remove aliasing.
         * Stereo Link: Rather than computing each phase increment individually, the stereo link preserves the phase difference between channels. Without this link, differences in phase would cause low-frequency cancellation when summed to mono.
    \n
    :param input_path: Path to Song (.wav)
    :param input_key_midi: Input Song Midi Note Value of Key (C4=60)
    :param input_tempo: Input Song BPM
    :param target_key_midi: Target Midi Note Value of Key
    :param target_tempo: Target BPM
    :param winSize: Size in samples of window for FFT
    :param: padFactor: Scales nFFT by value for better FFT precision
    :return: None, writes the output audio file to "[NAME]_Key[target_key_midi]_Tempo[target_tempo].wav"
    """
    import numpy as np
    from scipy.io import wavfile
    import scipy.signal as sig
    import time

    def m_to_f(midiNote):
        """
        Helper Function to convert midi note to frequency \n
        :param midiNote: Midi Note Value (C4=60) \n
        :return: Freq (Hz) \n
        """
        return 440.0 * (2**((midiNote - 69) / 12.0))

    def get_locked_phase(mag, phase, prev_phase, synthesis_phase, h_a, h_s, keyRatio, ww):
        """
        Helper function to perform phase-locking for resynthesis
        :param mag: magnitude spectrum of FFT input
        :param phase: phase spectrum of FFT input
        :param prev_phase: previous phase spectrum of FFT input
        :param synthesis_phase: current synthesis phase step
        :return: locked phase
        """
        # Find Local Maxima
        peaks = (mag > np.roll(mag, 1)) & (mag > np.roll(mag, -1))
        peak_idx = np.where(peaks)[0]

        # Bypass if no peaks
        if len(peak_idx) == 0:
            return synthesis_phase + ((phase - prev_phase) % (2*np.pi))

        # Nearest Peak
        bin_idx = np.arange(len(mag))
        idx = np.searchsorted(peak_idx, bin_idx)
        idx = np.clip(idx, 1, len(peak_idx) - 1)
        left, right = peak_idx[idx - 1], peak_idx[idx]
        nearest_peak_idx = np.where((bin_idx - left) <= (right - bin_idx), left, right)

        # Calculate true freq and synthesis phase for PEAK bins only
        dPhi = (phase - prev_phase) - ww
        dPhi = (dPhi + np.pi) % (2 * np.pi) - np.pi # Wrap to [-π, π]
        omega = (ww + dPhi) * (h_s / h_a) * keyRatio

        # Update synthesis phase of the whole spectrum based on peak movements
        ### Lock phase of bins to their assigned peak
        new_psi = synthesis_phase[nearest_peak_idx] + omega[nearest_peak_idx] + (phase - phase[nearest_peak_idx])
        return new_psi


    ### Read In Audio File
    sr, audioIn = wavfile.read(input_path)
    # Standardize to float32 [-1, 1]
    if audioIn.dtype == np.int16:
        audioIn = audioIn.astype(np.float32) / 32768.0
    elif audioIn.dtype == np.int32:
        audioIn = audioIn.astype(np.float32) / 2147483648.0
    elif audioIn.dtype != np.float32:
        audioIn = audioIn.astype(np.float32)

    # Mono-Stereo Conversion for consistency
    if len(audioIn.shape) == 1:
        audioIn = np.column_stack((audioIn,audioIn))

    # Define Ratios for Current and Target Key/BPM
    keyRatio = m_to_f(target_key_midi) / m_to_f(input_key_midi)
    tempoRatio = target_tempo / input_tempo

    # Define Hop Sizes
    lx = int(np.floor(winSize / keyRatio))
    h_s = max(1,lx // 8)
    h_a = max(1,int(h_s * tempoRatio)) # max to ensure progress at low tempos

    # FFT Setup
    nFFT = winSize * padFactor
    win = sig.windows.hann(winSize,"periodic")
    winResamp = sig.windows.hann(lx, "periodic")

    # Initialize Audio IO
    audioIn = np.pad(audioIn, ((winSize, winSize), (0, 0)))
    output_len = int(len(audioIn) / tempoRatio) + winSize * 2
    audioOut = np.zeros((output_len, 2))
    weightOut = np.zeros(output_len)

    # Initialize Phase Increments
    ww = 2 * np.pi * h_a * np.arange(nFFT) / nFFT
    phi0L, phi0R = np.zeros(nFFT), np.zeros(nFFT)
    psiL, psiR = np.zeros(nFFT), np.zeros(nFFT)

    # Linear Interpolation of a grain of length nFFT
    ix = np.floor(np.arange(lx) * (winSize / lx)).astype(int)
    ix1 = np.minimum(ix + 1, winSize - 1)
    dx = (np.arange(lx) * (winSize / lx)) - ix

    # Process Block
    pIn, pOut = 0, 0
    pEnd = len(audioIn) - winSize
    prev_energy=0

    # Anti-Aliasing Threshold
    ### Zero out bins above nyquist threshold in lieu of a traditional anti-aliasing filter
    if keyRatio > 1:
        nyquist_bin = int(nFFT / (2 * keyRatio))
    else:
        nyquist_bin = nFFT // 2

    # Time benchmarking
    process_times = []
    
    while pIn<pEnd:
        iteration_start = time.perf_counter()

        # Extract and Window
        grainL_raw = audioIn[pIn : pIn + winSize, 0]
        grainR_raw = audioIn[pIn : pIn + winSize, 1]

        # Transient Detection
        curr_energy = np.sum((grainL_raw**2 + grainR_raw**2))
        is_transient = curr_energy > prev_energy * 5 # scaling factor for actual transients, not just up-slopes
        prev_energy = curr_energy

        # Take FFT
        fftL = np.fft.fft(grainL_raw, n=nFFT) # no double windowing
        fftR = np.fft.fft(grainR_raw, n=nFFT)
        rL, phiL = np.abs(fftL), np.angle(fftL)
        rR, phiR = np.abs(fftR), np.angle(fftR)

        # Spectral Brick-wall antialiasing filter
        # no tremolo artifact with this implementation, and we're taking FFT anyway so it's more efficient
        if keyRatio > 1.0:
            rL[nyquist_bin : nFFT - nyquist_bin] = 0
            rR[nyquist_bin : nFFT - nyquist_bin] = 0

        # Stereo-Linked Phase Alignment:
        phase_diff = phiR - phiL

        # Update Phases
        if is_transient:
            psiL, psiR = phiL, phiR
        else:
            psiL = get_locked_phase(rL, phiL, phi0L, psiL, h_a, h_s, keyRatio, ww)
            psiR = psiL + phase_diff

        phi0L, phi0R = phiL, phiR

        # Resynthesis - Inverse FFT & Window
        resL_raw = np.real(np.fft.ifft(rL * np.exp(1j * psiL), n=nFFT))[:winSize]
        resR_raw = np.real(np.fft.ifft(rR * np.exp(1j * psiR), n=nFFT))[:winSize]

        g3L = (resL_raw[ix] + dx * (resL_raw[ix1] - resL_raw[ix])) * winResamp
        g3R = (resR_raw[ix] + dx * (resR_raw[ix1] - resR_raw[ix])) * winResamp

        # Overlap-Add:
        endIdx= pOut + lx
        if endIdx <= len(audioOut):
            audioOut[pOut : endIdx, 0] += g3L
            audioOut[pOut : endIdx, 1] += g3R
            weightOut[pOut : endIdx] += winResamp**2 # Standard OLA weight

        # Increment Frame
        pIn += h_a
        pOut += h_s

        # Record iteration time
        iteration_end = time.perf_counter()
        process_times.append(iteration_end - iteration_start)
    
    # Report benchmarking statistics
    if process_times:
        avg_time = np.mean(process_times) * 1000  # Convert to milliseconds
        min_time = np.min(process_times) * 1000
        max_time = np.max(process_times) * 1000
        total_time = np.sum(process_times)
        print(f"\nProcess Block Benchmarking:")
        print(f"  Total iterations: {len(process_times)}")
        print(f"  Average time per iteration: {avg_time:.3f} ms")
        print(f"  Min time: {min_time:.3f} ms")
        print(f"  Max time: {max_time:.3f} ms")
        print(f"  Total processing time: {total_time:.3f} s\n")

    ### Normalize and Convert Output Type
    mask = weightOut > 1e-3
    for c in range(2): # numChans
        # Normalizing here reduces the tremolo-like artifact from window modulation
        audioOut[mask,c] /= (weightOut[mask])

    audioOut=audioOut.astype("float32")

    audioOut = audioOut / (np.max(np.abs(audioOut)) + 1e-9)
    ### Write Output
    wavfile.write(input_path.split(".")[0]+"_Key"+str(target_key_midi)+"_Tempo"+str(target_tempo)+".wav",sr,audioOut)
    return None

pitch_tempo_warp("Luxury.wav",60,120,58,100)
