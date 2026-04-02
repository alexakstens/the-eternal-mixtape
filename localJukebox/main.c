#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "song_analyzer.h"
#include "warp.h"

#include "dr_wav.h"

#define MAX_CANDIDATES 5
#define HISTORY_SIZE 10

typedef struct {
    float* matrix;
    unsigned int rows;
    unsigned int cols;
} CrossSongMatrix;

CrossSongMatrix compute_inter_song_similarity(SongData* songA, SongData* songB) {
    CrossSongMatrix csm;
    csm.rows = songA->count;
    csm.cols = songB->count;
    csm.matrix = malloc(sizeof(float) * csm.rows * csm.cols);

    printf("Computing cross-song similarity matrix...\n");
    float max_dist = 0.0f;
    for (unsigned int i = 0; i < csm.rows; i++) {
        for (unsigned int j = 0; j < csm.cols; j++) {
            float dist = 0.0f;
            if (songA->features && songB->features && songA->n_coeffs == songB->n_coeffs) {
                for(unsigned int k = 0; k < songA->n_coeffs; k++){
                     float diff = songA->features[i * songA->n_coeffs + k] - songB->features[j * songB->n_coeffs + k];
                     dist += diff * diff;
                }
                dist = sqrtf(dist);
            } else {
                dist = (float)(abs((int)i - (int)j));
            }
            csm.matrix[i * csm.cols + j] = dist;
            if (dist > max_dist) max_dist = dist;
        }
    }

    if (max_dist > 0) {
        for (unsigned int i = 0; i < csm.rows * csm.cols; i++) {
            csm.matrix[i] = 1.0f - (csm.matrix[i] / max_dist);
        }
    }
    return csm;
}

void free_csm(CrossSongMatrix* csm) {
    if (csm->matrix) free(csm->matrix);
}

typedef struct {
    unsigned int song_id;
    unsigned int beat_index;
} RemixNode;

// Helper to find top N candidates from a similarity row
void find_top_candidates(const float* sim_row, unsigned int num_beats, unsigned int* candidates, float* scores) {
    for (int i = 0; i < MAX_CANDIDATES; i++) {
        scores[i] = -1.0f;
        candidates[i] = 0;
    }

    for (unsigned int i = 0; i < num_beats; i++) {
        float s = sim_row[i];
        for (int j = 0; j < MAX_CANDIDATES; j++) {
            if (s > scores[j]) {
                for (int k = MAX_CANDIDATES - 1; k > j; k--) {
                    scores[k] = scores[k - 1];
                    candidates[k] = candidates[k - 1];
                }
                scores[j] = s;
                candidates[j] = i;
                break;
            }
        }
    }
}

// Check if a beat is in the recent history
int is_in_history(RemixNode beat, const RemixNode* history, int history_len) {
    for (int i = 0; i < history_len; i++) {
        if (history[i].song_id == beat.song_id && history[i].beat_index == beat.beat_index) {
            return 1;
        }
    }
    return 0;
}

// Calculate dynamic jump probability based on musical context
float calculate_jump_probability(unsigned int current_beat, int beats_since_last_jump, float max_sim, float novelty_score) {
    // 1. Momentum: Cooldown phase (No jumps for the first 8 beats after a jump)
    if (beats_since_last_jump < 8) {
        return 0.0f;
    }

    // Base probability grows as time goes on (peaks around 32 beats / 8 bars)
    float base_prob = (float)(beats_since_last_jump - 8) / 24.0f;
    if (base_prob > 1.0f) base_prob = 1.0f;

    // 2. Metrical Grid: Heavily favor downbeats
    float metric_multiplier = 0.1f; // low chance on offbeats
    if (current_beat % 16 == 0) {
        metric_multiplier = 1.5f; // Huge boost at the start of 4-bar phrases
    } else if (current_beat % 4 == 0) {
        metric_multiplier = 1.0f; // Normal boost on downbeats
    }

    // 3. Similarity: Only jump if it sounds good
    float similarity_multiplier = max_sim;

    // 4. Structural Novelty: Jump at major structural boundaries
    // The novelty score is 0.0 to 1.0. We give a massive boost if it's a peak.
    float novelty_multiplier = 1.0f + (novelty_score * 2.0f); // Up to 3x boost on huge boundaries

    // Combine them
    float final_prob = base_prob * metric_multiplier * similarity_multiplier * novelty_multiplier;

    return (final_prob > 1.0f) ? 1.0f : final_prob;
}


float* generate_mixtape_buffer(SongData* songA, SongData* songB, CrossSongMatrix* csm, unsigned int* out_total_frames) {
    printf("Generating mixtape with dynamic, musically-aware jump logic...\n");
    // Only call srand once in main, but since it's here, we'll leave it or move it to main.
    // srand(time(NULL));

    unsigned int target_beats = songA->count + songB->count;
    RemixNode* path = malloc(sizeof(RemixNode) * target_beats);
    RemixNode history[HISTORY_SIZE] = {0};
    int history_idx = 0;

    unsigned int current_song = 0;
    unsigned int current_beat = 0;
    unsigned int total_f = 0;

    int beats_since_last_jump = 0; // Track momentum

    for (unsigned int i = 0; i < target_beats; i++) {
        path[i].song_id = current_song;
        path[i].beat_index = current_beat;
        history[history_idx] = path[i];
        history_idx = (history_idx + 1) % HISTORY_SIZE;

        SongData* current_song_data = (current_song == 0) ? songA : songB;
        unsigned int start = current_song_data->beat_samples[current_beat];
        unsigned int end = (current_beat < current_song_data->count - 1) ? current_song_data->beat_samples[current_beat+1] : start + (current_song_data->sample_rate / 2);
        total_f += (end - start);

        // -- CALCULATE DYNAMIC JUMP PROBABILITY --

        // 1. Get the max similarity score to use as a factor
        unsigned int target_song = current_song;
        float max_available_sim = 0.0f;
        unsigned int candidates[MAX_CANDIDATES];
        float scores[MAX_CANDIDATES];

        int try_inter_song = (rand() % 100 < 50); // 50/50 chance to consider inter vs intra song jump

        if (try_inter_song) {
            target_song = 1 - current_song;
            if (target_song == 1) { // A -> B
                find_top_candidates(&csm->matrix[current_beat * csm->cols], csm->cols, candidates, scores);
            } else { // B -> A
                float* temp_row = malloc(sizeof(float) * csm->rows);
                for(unsigned int r=0; r < csm->rows; r++) temp_row[r] = csm->matrix[r * csm->cols + current_beat];
                find_top_candidates(temp_row, csm->rows, candidates, scores);
                free(temp_row);
            }
        } else {
            // Intra-song
            SongData* s_data = (current_song == 0) ? songA : songB;
            find_top_candidates(&s_data->ssm[current_beat * s_data->count], s_data->count, candidates, scores);
        }
        max_available_sim = scores[0]; // Best score in the candidate pool

        // 2. Get the novelty score for the current beat
        float current_novelty = current_song_data->novelty[current_beat];

        // 3. Calculate final jump probability
        float jump_prob = calculate_jump_probability(current_beat, beats_since_last_jump, max_available_sim, current_novelty);
        float random_roll = (float)rand() / (float)RAND_MAX; // 0.0 to 1.0

        if (random_roll < jump_prob) {
            // We decided to jump! Let's pick a candidate.
            int selected_candidate = -1;
            for (int j = 0; j < MAX_CANDIDATES; j++) {
                RemixNode candidate_node = {target_song, candidates[j]};
                // Only accept candidates with decent similarity that aren't in history
                // We also want to make sure we don't jump to the *exact same beat* we are currently on if doing an intra-song jump
                int is_same_beat = (target_song == current_song && candidates[j] == current_beat);

                if (scores[j] > 0.6f && !is_in_history(candidate_node, history, HISTORY_SIZE) && !is_same_beat) {
                    selected_candidate = j;
                    break;
                }
            }

            if (selected_candidate != -1) {
                // Probabilistically choose from the valid candidates
                int choice = rand() % (selected_candidate + 1);

                printf(">> JUMP: Song %d (Beat %d) -> Song %d (Beat %d) [Prob: %.2f | Sim: %.2f | Nov: %.2f]\n",
                       current_song, current_beat, target_song, candidates[choice], jump_prob, scores[choice], current_novelty);

                current_song = target_song;
                current_beat = candidates[choice];
                beats_since_last_jump = 0; // Reset momentum!
            } else {
                // Wanted to jump, but no valid candidates were found
                current_beat = (current_beat + 1) % current_song_data->count;
                beats_since_last_jump++;
            }
        } else {
            // Linear playback
            current_beat = (current_beat + 1) % current_song_data->count;
            beats_since_last_jump++;
        }
    }

    unsigned int channels = songA->channels;
    float* buffer = calloc(total_f * channels, sizeof(float));
    unsigned int cursor = 0;

    for (unsigned int i = 0; i < target_beats; i++) {
        unsigned int sid = path[i].song_id;
        unsigned int b = path[i].beat_index;
        SongData* s_data = (sid == 0) ? songA : songB;
        unsigned int start = s_data->beat_samples[b];
        unsigned int end = (b < s_data->count - 1) ? s_data->beat_samples[b+1] : start + (s_data->sample_rate / 2);
        unsigned int frames_to_copy = end - start;

        memcpy(&buffer[cursor * channels], &s_data->audio_data[start * channels], frames_to_copy * channels * sizeof(float));
        cursor += frames_to_copy;
    }

    free(path);
    *out_total_frames = total_f;
    return buffer;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <songA.wav> <songB.wav> [master_bpm] [master_midi_key]\n", argv[0]);
        return 1;
    }

    srand((unsigned int)time(NULL)); // Seed random generator once

    char* song_a_path = argv[1];
    char* song_b_path = argv[2];
    int run_warp = (argc == 5);

    printf("Analyzing Song A: %s\n", song_a_path);
    SongData songA = analyze_song(song_a_path);
    if (songA.count == 0) { printf("Error analyzing Song A.\n"); return 1; }
    printf("Song A: %.2f BPM | %u Beats | Key: %d\n", songA.bpm, songA.count, songA.key);

    printf("Analyzing Song B: %s\n", song_b_path);
    SongData songB = analyze_song(song_b_path);
    if (songB.count == 0) { printf("Error analyzing Song B.\n"); free_song_data(&songA); return 1; }
    printf("Song B: %.2f BPM | %u Beats | Key: %d\n", songB.bpm, songB.count, songB.key);

    if (songA.channels != songB.channels) {
        printf("Error: Songs must have the same number of channels.\n");
        free_song_data(&songA); free_song_data(&songB); return 1;
    }

    CrossSongMatrix csm = compute_inter_song_similarity(&songA, &songB);

    if (run_warp) {
        float master_bpm = (float)strtod(argv[3], NULL);
        int master_key = (int)strtol(argv[4], NULL, 10);
        printf("Using Master Clock: %.2f BPM, Key %d\n", master_bpm, master_key);
        printf("Warning: Warping is experimental and may desync beat markers.\n");

        unsigned int final_count_A;
        float* warped_A = warp_audio(songA.audio_data, songA.beat_samples[songA.count-1] + (songA.sample_rate/2), songA.channels, songA.sample_rate, songA.bpm, songA.key, master_bpm, master_key, &final_count_A);
        free(songA.audio_data);
        songA.audio_data = warped_A;

        unsigned int final_count_B;
        float* warped_B = warp_audio(songB.audio_data, songB.beat_samples[songB.count-1] + (songB.sample_rate/2), songB.channels, songB.sample_rate, songB.bpm, songB.key, master_bpm, master_key, &final_count_B);
        free(songB.audio_data);
        songB.audio_data = warped_B;
    }

    unsigned int mixtape_frames_count;
    float* mixtape_buffer = generate_mixtape_buffer(&songA, &songB, &csm, &mixtape_frames_count);

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = songA.channels;
    format.sampleRate = songA.sample_rate;
    format.bitsPerSample = 32;

    drwav out;
    if (drwav_init_file_write(&out, "remix_output.wav", &format, NULL)) {
        drwav_write_pcm_frames(&out, mixtape_frames_count, mixtape_buffer);
        drwav_uninit(&out);
        printf("\nDone! Saved to: remix_output.wav\n");
    } else {
        printf("Error: Could not open output file for writing.\n");
    }

    free(mixtape_buffer);
    free_csm(&csm);
    free_song_data(&songA);
    free_song_data(&songB);

    return 0;
}