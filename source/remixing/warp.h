#ifndef WARP_H
#define WARP_H

// The main warping function
// Returns a pointer to the warped audio buffer
/*  Adapted from DAFx 2º Edition, Sec 7.4.4 \n
    Key Changes from Source:
         * Phase Locking and Scaling: By identifying local maxima in the magnitude spectrum at each FFT, the phase deviation can lock to each of those key frequencies, preserving the phase's vertical alignment. This has the effect of preserving drum transients and removing metallic sounds from inharmonicity on sustained notes that is common in pitch shifters.
         * Overlap-Add and Normalization: With a fixed window and hop size, window modulation happens that causes an audible fast tremolo effect. Dynamically calculating hop sizes from the pitch ratio keeps a constant overlap for adding. Normalizing with an energy sum helps preserve the spectral shape of the input.
         * Brick-wall Anti-Aliasing: Anti-aliasing is necessary when pitching up to prevent frequencies from exceeding the nyquist maximum and wrapping back down to introduce artifacts. Including a normal anti-aliasing filter within the process block introduced clicks at grain transitions. Because this algorithm has to take the FFT anyway, applying a brickwall filter is the most efficient way to remove aliasing.
         * Stereo Link: Rather than computing each phase increment individually, the stereo link preserves the phase difference between channels. Without this link, differences in phase would cause low-frequency cancellation when summed to mono.
    \n
 */
float* warp_audio(
    float* input_data, 
    unsigned int input_frames, 
    unsigned int channels,
    unsigned int sample_rate,
    float source_bpm,
    int source_key, 
    float target_bpm, 
    int target_key,
    unsigned int* out_frames_count
);

#endif