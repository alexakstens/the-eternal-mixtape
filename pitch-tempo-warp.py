def pitch_tempo_warp(input_path, input_key_midi, input_tempo, target_key_midi, target_tempo, nFFT=4096, winSize=2048, hopSize=512):
    """
    Adjust Time and Pitch of input audio file \n
    Rewrites audio file with new tempo and key through FFT/IFFT Resynthesis \n
    Hann window used for FFT analysis, overlap-add method used for resynthesis \n
    :param input_path: Path to Song (.wav)
    :param input_key_midi: Input Song Midi Note Value of Key (C4=60)
    :param input_tempo: Input Song BPM
    :param target_key_midi: Target Midi Note Value of Key
    :param target_tempo: Target BPM
    :param nFFT: number of samples for FFT/IFFT (0-padded or truncated if nFFT!=len(frame)
    :param winSize: Size in samples of window for FFT
    :param hopSize: Size in samples of frame-shift
    :return: None
    """
    import numpy as np
    from scipy.io import wavfile
    import scipy.fft as fft
    import scipy.signal as sig

    def m_to_f(midiNote):
        """
        Helper Function to convert midi note to frequency \n
        :param midiNote: Midi Note Value (C4=60) \n
        :return: Freq (Hz) \n

        """
        return 440.0 * (2**((midiNote - 69) / 12.0))


    ### Read In Audio File
    sr,audioIn=wavfile.read(input_path)
    audioIn=audioIn/max(max(audioIn[:,0]),max(audioIn[:,1]))

    ### Define Ratios for Current and Target Key/BPM

    keyRatio=m_to_f(target_key_midi) / m_to_f(input_key_midi)
    tempoRatio=target_tempo/input_tempo

    ### Init Audio File Output
    audioOut=np.zeros((int(np.ceil(len(audioIn[:,0]) / hopSize) * hopSize + winSize),2),dtype="complex128")


    ### Process Block
    for frameIdx in range(0,int(len(audioIn[:,0])/hopSize)):
        frameAudioL=audioIn[frameIdx*hopSize:(frameIdx*hopSize)+winSize,0]
        frameAudioR=audioIn[frameIdx*hopSize:(frameIdx*hopSize)+winSize,1]

        frameFftL=fft.fft(frameAudioL*sig.windows.hann(len(frameAudioL)),nFFT)
        frameFftR=fft.fft(frameAudioR*sig.windows.hann(len(frameAudioR)),nFFT)
        #xf=fft.fftfreq(nFFT,1/sr)*keyRatio

        """ ============================================================================================================
        PITCH AND TiME SHIFTING HERE
        ============================================================================================================ """

        frameAudioL=fft.ifft(frameFftL,nFFT)
        frameAudioR=fft.ifft(frameFftR,nFFT)

        ### Overlap-Add:
        audioOut[frameIdx*hopSize:(frameIdx*hopSize)+winSize,0]+=frameAudioL[:nFFT//2]
        audioOut[frameIdx*hopSize:(frameIdx*hopSize)+winSize,1]+=frameAudioR[:nFFT//2]

    ### Normalize and Convert Output Type
    audioOut=audioOut.astype("float32")
    audioOut=audioOut/max(max(audioOut[:,0]),max(audioOut[:,1]))
    


    ### Write Output
    wavfile.write(input_path.split(".")[0]+"_"+str(target_key_midi)+".wav",sr,audioOut)
    return None

pitch_tempo_warp("Luxury.wav",60,120,72,100)

