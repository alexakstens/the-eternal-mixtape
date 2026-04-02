#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include "song_analyzer.hpp"
#include "warp.hpp"

#include "dr_wav.h"

constexpr int MAX_CANDIDATES = 5;
constexpr int HISTORY_SIZE = 10;

struct CrossSongMatrix {
    std::vector<float> matrix;
    unsigned int rows = 0;
    unsigned int cols = 0;
};

CrossSongMatrix compute_inter_song_similarity(const SongData& songA, const SongData& songB) {
    CrossSongMatrix csm;
    csm.rows = songA.count;
    csm.cols = songB.count;
    csm.matrix.resize(csm.rows * csm.cols, 0.0f);

    std::cout << "Computing cross-song similarity matrix...\n";
    float max_dist = 0.0f;
    for (unsigned int i = 0; i < csm.rows; i++) {
        for (unsigned int j = 0; j < csm.cols; j++) {
            float dist = 0.0f;
            if (!songA.features.empty() && !songB.features.empty() && songA.n_coeffs == songB.n_coeffs) {
                for(unsigned int k = 0; k < songA.n_coeffs; k++){
                     float diff = songA.features[i * songA.n_coeffs + k] - songB.features[j * songB.n_coeffs + k];
                     dist += diff * diff;
                }
                dist = std::sqrt(dist);
            } else {
                dist = static_cast<float>(std::abs(static_cast<int>(i) - static_cast<int>(j)));
            }
            csm.matrix[i * csm.cols + j] = dist;
            if (dist > max_dist) max_dist = dist;
        }
    }

    if (max_dist > 0.0f) {
        for (float& val : csm.matrix) {
            val = 1.0f - (val / max_dist);
        }
    }
    return csm;
}

struct RemixNode {
    unsigned int song_id;
    unsigned int beat_index;
};

void find_top_candidates(const std::vector<float>& sim_row, std::vector<unsigned int>& candidates, std::vector<float>& scores) {
    scores.assign(MAX_CANDIDATES, -1.0f);
    candidates.assign(MAX_CANDIDATES, 0);

    for (unsigned int i = 0; i < sim_row.size(); i++) {
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

bool is_in_history(const RemixNode& beat, const std::vector<RemixNode>& history) {
    for (const auto& h : history) {
        if (h.song_id == beat.song_id && h.beat_index == beat.beat_index) {
            return true;
        }
    }
    return false;
}

float calculate_jump_probability(unsigned int current_beat, int beats_since_last_jump, float max_sim, float novelty_score) {
    if (beats_since_last_jump < 8) return 0.0f;

    float base_prob = static_cast<float>(beats_since_last_jump - 8) / 24.0f;
    if (base_prob > 1.0f) base_prob = 1.0f;

    float metric_multiplier = 0.1f;
    if (current_beat % 16 == 0) {
        metric_multiplier = 1.5f;
    } else if (current_beat % 4 == 0) {
        metric_multiplier = 1.0f;
    }

    float similarity_multiplier = max_sim;
    float novelty_multiplier = 1.0f + (novelty_score * 2.0f);

    float final_prob = base_prob * metric_multiplier * similarity_multiplier * novelty_multiplier;
    return (final_prob > 1.0f) ? 1.0f : final_prob;
}

std::vector<float> generate_mixtape_buffer(const SongData& songA, const SongData& songB, const CrossSongMatrix& csm) {
    std::cout << "Generating mixtape with dynamic, musically-aware jump logic...\n";

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_int_distribution<int> dist100(0, 99);

    unsigned int target_beats = songA.count + songB.count;
    std::vector<RemixNode> path;
    path.reserve(target_beats);

    std::vector<RemixNode> history(HISTORY_SIZE, {0, 0});
    int history_idx = 0;

    unsigned int current_song = 0;
    unsigned int current_beat = 0;
    int beats_since_last_jump = 0;

    for (unsigned int i = 0; i < target_beats; i++) {
        RemixNode current_node = {current_song, current_beat};
        path.push_back(current_node);

        history[history_idx] = current_node;
        history_idx = (history_idx + 1) % HISTORY_SIZE;

        const SongData& current_song_data = (current_song == 0) ? songA : songB;

        unsigned int target_song = current_song;
        std::vector<unsigned int> candidates(MAX_CANDIDATES);
        std::vector<float> scores(MAX_CANDIDATES);

        bool try_inter_song = (dist100(rng) < 50);

        if (try_inter_song) {
            target_song = 1 - current_song;
            if (target_song == 1) {
                std::vector<float> row(csm.matrix.begin() + current_beat * csm.cols, csm.matrix.begin() + (current_beat + 1) * csm.cols);
                find_top_candidates(row, candidates, scores);
            } else {
                std::vector<float> temp_row(csm.rows);
                for(unsigned int r=0; r < csm.rows; r++) temp_row[r] = csm.matrix[r * csm.cols + current_beat];
                find_top_candidates(temp_row, candidates, scores);
            }
        } else {
            const SongData& s_data = (current_song == 0) ? songA : songB;
            std::vector<float> row(s_data.ssm.begin() + current_beat * s_data.count, s_data.ssm.begin() + (current_beat + 1) * s_data.count);
            find_top_candidates(row, candidates, scores);
        }

        float max_available_sim = scores[0];
        float current_novelty = current_song_data.novelty[current_beat];
        float jump_prob = calculate_jump_probability(current_beat, beats_since_last_jump, max_available_sim, current_novelty);

        if (dist01(rng) < jump_prob) {
            int selected_candidate = -1;
            for (int j = 0; j < MAX_CANDIDATES; j++) {
                RemixNode candidate_node = {target_song, candidates[j]};
                bool is_same_beat = (target_song == current_song && candidates[j] == current_beat);

                if (scores[j] > 0.6f && !is_in_history(candidate_node, history) && !is_same_beat) {
                    selected_candidate = j;
                    break;
                }
            }

            if (selected_candidate != -1) {
                std::uniform_int_distribution<int> choice_dist(0, selected_candidate);
                int choice = choice_dist(rng);

                std::cout << ">> JUMP: Song " << current_song << " (Beat " << current_beat
                          << ") -> Song " << target_song << " (Beat " << candidates[choice]
                          << ") [Prob: " << jump_prob << " | Sim: " << scores[choice]
                          << " | Nov: " << current_novelty << "]\n";

                current_song = target_song;
                current_beat = candidates[choice];
                beats_since_last_jump = 0;
            } else {
                current_beat = (current_beat + 1) % current_song_data.count;
                beats_since_last_jump++;
            }
        } else {
            current_beat = (current_beat + 1) % current_song_data.count;
            beats_since_last_jump++;
        }
    }

    unsigned int channels = songA.channels;

    // Calculate total frames required
    unsigned int total_f = 0;
    for (const auto& node : path) {
        const SongData& s_data = (node.song_id == 0) ? songA : songB;
        unsigned int start = s_data.beat_samples[node.beat_index];
        unsigned int end = (node.beat_index < s_data.count - 1) ? s_data.beat_samples[node.beat_index + 1] : start + (s_data.sample_rate / 2);
        total_f += (end - start);
    }

    std::vector<float> buffer(total_f * channels, 0.0f);
    unsigned int cursor = 0;

    for (const auto& node : path) {
        const SongData& s_data = (node.song_id == 0) ? songA : songB;
        unsigned int start = s_data.beat_samples[node.beat_index];
        unsigned int end = (node.beat_index < s_data.count - 1) ? s_data.beat_samples[node.beat_index + 1] : start + (s_data.sample_rate / 2);
        unsigned int frames_to_copy = end - start;

        std::copy(s_data.audio_data.begin() + start * channels,
                  s_data.audio_data.begin() + end * channels,
                  buffer.begin() + cursor * channels);
        cursor += frames_to_copy;
    }

    return buffer;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <songA.wav> <songB.wav> [master_bpm] [master_midi_key]\n";
        return 1;
    }

    std::string song_a_path = argv[1];
    std::string song_b_path = argv[2];
    bool run_warp = (argc == 5);

    std::cout << "Analyzing Song A: " << song_a_path << "\n";
    SongData songA;
    if (!songA.analyze_song(song_a_path)) {
        std::cout << "Error analyzing Song A. Check if the file exists and is a valid WAV.\n";
        return 1;
    }
    std::cout << "Song A: " << songA.bpm << " BPM | " << songA.count << " Beats | Key: " << songA.key << "\n";

    std::cout << "Analyzing Song B: " << song_b_path << "\n";
    SongData songB;
    if (!songB.analyze_song(song_b_path)) {
        std::cout << "Error analyzing Song B. Check if the file exists and is a valid WAV.\n";
        return 1;
    }
    std::cout << "Song B: " << songB.bpm << " BPM | " << songB.count << " Beats | Key: " << songB.key << "\n";

    if (songA.channels != songB.channels) {
        std::cout << "Error: Songs must have the same number of channels.\n";
        return 1;
    }

    CrossSongMatrix csm = compute_inter_song_similarity(songA, songB);

    if (run_warp) {
        float master_bpm = std::stof(argv[3]);
        int master_key = std::stoi(argv[4]);
        std::cout << "Using Master Clock: " << master_bpm << " BPM, Key " << master_key << "\n";
        std::cout << "Warning: Warping is experimental and may desync beat markers.\n";

        songA.audio_data = warp_audio(songA.audio_data, songA.channels, songA.sample_rate, songA.bpm, songA.key, master_bpm, master_key);
        songB.audio_data = warp_audio(songB.audio_data, songB.channels, songB.sample_rate, songB.bpm, songB.key, master_bpm, master_key);
    }

    std::vector<float> mixtape_buffer = generate_mixtape_buffer(songA, songB, csm);

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = songA.channels;
    format.sampleRate = songA.sample_rate;
    format.bitsPerSample = 32;

    drwav out;
    if (drwav_init_file_write(&out, "remix_output.wav", &format, nullptr)) {
        drwav_write_pcm_frames(&out, mixtape_buffer.size() / songA.channels, mixtape_buffer.data());
        drwav_uninit(&out);
        std::cout << "\nDone! Saved to: remix_output.wav\n";
    } else {
        std::cout << "Error: Could not open output file for writing.\n";
    }

    return 0;
}