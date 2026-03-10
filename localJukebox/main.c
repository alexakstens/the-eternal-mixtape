#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h> // Required for seeding rand()
#include "beat_tracker.h"
#include "warp.h"

//#ifndef DR_WAV_IMPLEMENTATION
//#define DR_WAV_IMPLEMENTATION
//#endif
#include "dr_wav.h"

// Returns a shuffled buffer in memory
float* generate_remix_buffer(BeatData* data, unsigned int* out_total_frames) {
    printf("Shuffling beats for remix...\n");
    srand(time(NULL)); // Seed random for a unique remix every time

    unsigned int target_beats = data->count * 2;
    unsigned int* path = malloc(sizeof(unsigned int) * target_beats);
    unsigned int current_beat = 0;

    // 1. Pathfinding
    unsigned int total_f = 0;
    for (unsigned int i = 0; i < target_beats; i++) {
        path[i] = current_beat;
        unsigned int start = data->beat_samples[current_beat];
        unsigned int end = (current_beat < data->count - 1) ? data->beat_samples[current_beat+1] : start + (data->sample_rate / 2);
        total_f += (end - start);

        // 15% chance to jump to a random beat
        if (rand() % 100 < 15) {
            current_beat = rand() % data->count;
        } else {
            current_beat = (current_beat + 1) % data->count;
        }
    }

    // 2. Rendering to buffer
    float* buffer = malloc(total_f * data->channels * sizeof(float));
    unsigned int cursor = 0;
    for (unsigned int i = 0; i < target_beats; i++) {
        unsigned int b = path[i];
        unsigned int start = data->beat_samples[b];
        unsigned int end = (b < data->count - 1) ? data->beat_samples[b+1] : start + (data->sample_rate / 2);
        unsigned int frames_to_copy = end - start;

        memcpy(&buffer[cursor * data->channels], &data->audio_data[start * data->channels], frames_to_copy * data->channels * sizeof(float));
        cursor += frames_to_copy;
    }

    free(path);
    *out_total_frames = total_f;
    return buffer;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <input.wav> [target_bpm] [target_midi_key]\n", argv[0]);
        printf("Note: If target_bpm and key are omitted, pitch/tempo warp is bypassed.\n");
        return 1;
    }

    char* input_path = argv[1];
    int run_warp = (argc == 4);

    // 1. Analyze Original
    BeatData my_beats = track_beats(input_path);
    if (my_beats.count == 0) {
        printf("Error: Could not analyze audio file.\n");
        return 1;
    }
    printf("Original Track: %.2f BPM | %u Beats Detected\n", my_beats.bpm, my_beats.count);

    // 2. Generate Shuffled Remix
    unsigned int remix_frames_count;
    float* remix_buffer = generate_remix_buffer(&my_beats, &remix_frames_count);

    // 3. Conditional Warping
    float* final_buffer;
    unsigned int final_count;

    if (run_warp) {
        float target_bpm = (float)atof(argv[2]);
        int target_key = atoi(argv[3]);

        // Corrected variables inside the call
        final_buffer = warp_audio(
            remix_buffer,
            remix_frames_count,
            my_beats.channels,
            my_beats.sample_rate,
            my_beats.bpm,
            60, // Default source key (C)
            target_bpm,
            target_key,
            &final_count
        );
    } else {
        printf("Bypassing Warp Engine. Maintaining original speed and pitch.\n");
        final_buffer = remix_buffer;
        final_count = remix_frames_count;
    }

    // 4. Save Final Output
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = my_beats.channels;
    format.sampleRate = my_beats.sample_rate;
    format.bitsPerSample = 32;

    drwav out;
    if (drwav_init_file_write(&out, "remix_output.wav", &format, NULL)) {
        drwav_write_pcm_frames(&out, final_count, final_buffer);
        drwav_uninit(&out);
        printf("\nDone! Saved to: remix_output.wav\n");
    } else {
        printf("Error: Could not open output file for writing.\n");
    }

    // 5. Cleanup
    if (run_warp) {
        free(final_buffer); // Free the warped buffer
    }
    free(remix_buffer);     // Always free the shuffled buffer
    free_beat_data(&my_beats);

    return 0;
}