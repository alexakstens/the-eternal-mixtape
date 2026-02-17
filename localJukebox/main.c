#include "beat_tracker.h"
#include "dr_wav.h"
#include <time.h>

void generate_remix(BeatData* data, const char* out_filename) {
    printf("\n--- Generating Infinite Remix ---\n");
    srand(time(NULL));

    unsigned int current_beat = 0;
    unsigned int target_beats = data->count * 2; // Create a remix 2x length of original
    unsigned int* path = malloc(sizeof(unsigned int) * target_beats);

    // 1. Pathfinding Logic
    for (unsigned int i = 0; i < target_beats; i++) {
        path[i] = current_beat;

        unsigned int jump_targets[512];
        int jump_count = 0;

        for (unsigned int j = 0; j < data->count; j++) {
            float similarity = data->ssm[current_beat * data->count + j];
            // If very similar and not just the next beat
            if (similarity > 0.91f && abs((int)current_beat - (int)j) > 16) {
                jump_targets[jump_count++] = j;
                if (jump_count >= 512) break;
            }
        }

        if (jump_count > 0 && (rand() % 100 < 15)) { // 15% chance to jump
            unsigned int next_b = jump_targets[rand() % jump_count];
            printf("Jump! Beat %u -> %u (Similarity: %.3f)\n", current_beat, next_b, data->ssm[current_beat * data->count + next_b]);
            current_beat = next_b;
        } else {
            current_beat++;
        }

        if (current_beat >= data->count) current_beat = 0;
    }

    // 2. Audio Rendering
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = 3;
    format.channels = data->channels;
    format.sampleRate = data->sample_rate;
    format.bitsPerSample = 32;

    drwav out;
    if (!drwav_init_file_write(&out, out_filename, &format, NULL)) {
        printf("Failed to open output file.\n");
        return;
    }

    for (unsigned int i = 0; i < target_beats; i++) {
        unsigned int b = path[i];
        unsigned int start = data->beat_samples[b];
        unsigned int end = (b < data->count - 1) ? data->beat_samples[b+1] : start + (data->sample_rate / 2);

        drwav_write_pcm_frames(&out, end - start, &data->audio_data[start * data->channels]);
    }

    drwav_uninit(&out);
    free(path);
    printf("Successfully saved remix to %s\n", out_filename);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <input.wav>\n", argv[0]);
        return 1;
    }

    BeatData my_beats = track_beats(argv[1]);

    if (my_beats.count > 0) {
        printf("BPM: %.2f | Beats: %u\n", my_beats.bpm, my_beats.count);

        // Print SSM Preview
        int size = (my_beats.count < 40) ? my_beats.count : 40;
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                float val = my_beats.ssm[i * my_beats.count + j];
                if (val > 0.98) printf("█");
                else if (val > 0.90) printf("▓");
                else if (val > 0.80) printf("▒");
                else printf(" ");
            }
            printf("\n");
        }

        generate_remix(&my_beats, "remix_output.wav");
    }

    free_beat_data(&my_beats);
    return 0;
}