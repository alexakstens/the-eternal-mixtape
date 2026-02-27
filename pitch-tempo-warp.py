def pitch_tempo_warp(input_path, input_key_midi, input_tempo, target_key_midi, target_tempo, winSize=2048, hopSize=256):
    """
    Adapted from DAFx 2º Edition, Sec 7.4.4 \n
    Adjust time and pitch of input audio file using an FFT-IFFT approach \n
    Writes the output audiofile to "[NAME]_Key[target_key_midi]_Tempo[target_tempo].wav"
    :param input_path: Path to Song (.wav)
    :param input_key_midi: Input Song Midi Note Value of Key (C4=60)
    :param input_tempo: Input Song BPM
    :param target_key_midi: Target Midi Note Value of Key
    :param target_tempo: Target BPM
    :param winSize: Size in samples of window for FFT
    :return: None
    """
    import numpy as np
    from scipy.io import wavfile
    import scipy.signal as sig
    import time


    """
    Parameter Notes:
    * Typical of FFTs, nFFT tradeoff b/w freq and time precision is VERY noticeable without hop/overlap
    * Too big a difference between winSize and hopSize creates an audible delay (>8ms for 4096 nFFT)
    * We should implement zero-padding to get more precision out of the FFT, (at least) double the length of each frame
    """

    def m_to_f(midiNote):
        """
        Helper Function to convert midi note to frequency \n
        :param midiNote: Midi Note Value (C4=60) \n
        :return: Freq (Hz) \n
        """
        return 440.0 * (2**((midiNote - 69) / 12.0))


    ### Read In Audio File
    sr,audioIn=wavfile.read(input_path)
    audioIn = audioIn / np.max(np.abs(audioIn))

    ### Define Ratios for Current and Target Key/BPM
    keyRatio=m_to_f(target_key_midi) / m_to_f(input_key_midi)
    tempoRatio=target_tempo/input_tempo

    ### Define Step Sizes
    n2 = winSize # synthesis
    n1 = int(n2/keyRatio) # analysis


    ### Initialize Audio I/O
    audioIn=np.concatenate((np.zeros([winSize,2]),audioIn,np.zeros([winSize-np.mod(len(audioIn),n1),2])))
    output_len = int(np.ceil(len(audioIn) * n2 / n1)) + winSize
    audioOut=np.zeros((output_len, 2))

    ### Initialize Phase Increments
    ww = 2*np.pi*n1*np.arange(winSize)/winSize  # shape (2048,)
    phi0L=np.zeros(winSize)
    phi0R=np.zeros(winSize)
    psiL=np.zeros(winSize)
    psiR=np.zeros(winSize)

    ### Linear Interpolation of a grain of length nFFT
    lx=int(np.floor(winSize*n1/n2))
    x=np.arange(lx)*(winSize/lx)
    ix=np.floor(x).astype(int)
    ix1=ix+1
    dx=x-ix
    dx1=1.0-dx

    ### Process Block
    pIn=0
    pOut=0
    pEnd=len(audioIn)-winSize
    
    win1=sig.windows.hamming(winSize,"periodic")
    win2=win1
    
    # Time benchmarking
    process_times = []
    
    while pIn<pEnd:
        iteration_start = time.perf_counter()

        grainL=audioIn[pIn:pIn+winSize,0] # Need zero-padding here
        grainR=audioIn[pIn:pIn+winSize,1] # ^^^

        ### Take FFT
        frameFftL=np.fft.fft(grainL*win1)
        frameFftR=np.fft.fft(grainR*win1)
        rL=np.abs(frameFftL)
        rR=np.abs(frameFftR)
        phiL=np.angle(frameFftL)
        phiR=np.angle(frameFftR)

        ### Compute Phase Increment
        deltaPhiL=ww+((((phiL-phi0L-ww) + np.pi) % (2 * np.pi) - np.pi))
        deltaPhiR=ww+((((phiR-phi0R-ww) + np.pi) % (2 * np.pi) - np.pi))
        phi0L=phiL
        phi0R=phiR

        psiL=((psiL+deltaPhiL*tempoRatio) + np.pi) % (2 * np.pi) - np.pi
        psiR=((psiR+deltaPhiR*tempoRatio) + np.pi) % (2 * np.pi) - np.pi

        ### Synthesize time-scaled grain
        ftL=(rL*np.exp(1j*psiL))
        ftR=(rR*np.exp(1j*psiR))
        grainL=np.fft.fftshift(np.real(np.fft.ifft(ftL)))*win2
        grainR=np.fft.fftshift(np.real(np.fft.ifft(ftR)))*win2

        ### Interpolate Grain
        grain2L=np.append(grainL,0)
        grain2R=np.append(grainR,0)
        grain3L=grain2L[ix]*dx1 + grain2L[ix1]*dx
        grain3R=grain2R[ix]*dx1 + grain2R[ix1]*dx

        ### Overlap-Add:
        try:
            audioOut[pOut:pOut+lx,0]+=grain3L
            audioOut[pOut:pOut+lx,1]+=grain3R
        except:
            a=1

        ### Increment Frame
        pIn+=int(n1*(hopSize/winSize))
        pOut+=int(n2*(hopSize/winSize))

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
    audioOut=audioOut.astype("float32")
    """
    Apply anti-aliasing filter here
    """
    audioOut = audioOut / np.max(np.abs(audioOut))
    ### Write Output
    wavfile.write(input_path.split(".")[0]+"_Key"+str(target_key_midi)+"_Tempo"+str(target_tempo)+".wav",sr,audioOut)
    return None

pitch_tempo_warp("Luxury.wav",60,120,67,120)
