def pitch_tempo_warp(input_path, input_key_midi, input_tempo, target_key_midi, target_tempo):
    import numpy as np
    from scipy.io import wavfile
    import scipy.fft as fft
    import scipy.signal as sig
    import librosa
    from matplotlib import pyplot as plt

    def m_to_f(midiNote):
        return 440.0 * (2**((midiNote - 69) / 12.0))

    sr,audioIn=wavfile.read(input_path)
    audioIn=audioIn/max(max(audioIn[:,0]),max(audioIn[:,1]))

    #print(type(audioIn[0,0]))
    #print(np.shape(audioIn))

    nFFT=4096
    winSize=2048
    hopSize=512

    target_length = int(np.ceil(len(audioIn[:,0]) / hopSize) * hopSize)


    keyRatio=m_to_f(target_key_midi) / m_to_f(input_key_midi)
    tempoRatio=target_tempo/input_tempo

    #print(keyRatio)
    #print(tempoRatio)

    audioOut=np.zeros((int(np.ceil(len(audioIn[:,0]) / hopSize) * hopSize + winSize),2),dtype="complex128")
    print(np.shape(audioOut))

    for frameIdx in range(0,int(len(audioIn[:,0])/hopSize)):
        frameAudioL=audioIn[frameIdx*hopSize:(frameIdx*hopSize)+winSize,0]
        frameAudioR=audioIn[frameIdx*hopSize:(frameIdx*hopSize)+winSize,1]
        #print(len(frameAudioL))


        frameFftL=fft.fft(frameAudioL*sig.windows.hann(len(frameAudioL)),nFFT)
        frameFftR=fft.fft(frameAudioR*sig.windows.hann(len(frameAudioR)),nFFT)
        #xf=fft.fftfreq(nFFT,1/sr)*keyRatio



        frameAudioL=fft.ifft(frameFftL,nFFT)
        frameAudioR=fft.ifft(frameFftR,nFFT)
        #print(len(frameAudioL[:nFFT//2]))


        audioOut[frameIdx*hopSize:(frameIdx*hopSize)+winSize,0]+=frameAudioL[:nFFT//2]
        audioOut[frameIdx*hopSize:(frameIdx*hopSize)+winSize,1]+=frameAudioR[:nFFT//2]
        #print(".")


    audioOut=audioOut/max(max(audioOut[:,0]),max(audioOut[:,1]))
    audioOut=audioOut.astype("float32")


    plt.subplot(2,1,1)
    plt.plot(audioIn[:,0],"b")
    plt.subplot(2,1,2)
    plt.plot(audioOut[:,0],"b",alpha=0.5)
    plt.show()


        #plt.plot(xf[0:nFFT//2], 2.0/nFFT * np.abs(frameFftL[0:nFFT//2]))
        #plt.show()

        #print(len(frameAudio[:,0]))



    wavfile.write(input_path.split(".")[0]+"_"+str(target_key_midi)+".wav",sr,audioOut)

pitch_tempo_warp("Luxury.wav",60,120,72,100)

