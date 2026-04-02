#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <memory>
#include "song_analyzer.hpp"
#include "warp.hpp"

#include "dr_wav.h"

class MixtapeEngine {
public:
    MixtapeEngine(std::unique_ptr<SongData> songA, std::unique_ptr<SongData> songB)
        : songA(std::move(songA)), songB(std::move(songB)), rng(std::random_device{}())
    {
        compute_inter_song_similarity();
    }

    [[nodiscard]] std::vector<float> generate_mixtape() {
        std::cout << "Generating mixtape with dynamic, musically-aware jump logic...\n";

        unsigned int target_beats = songA->get_count() + songB->get_count();
        std::vector<RemixNode> path;
        path.reserve(target_beats);

        std::vector<RemixNode> history(HISTORY_SIZE, {0, 0});
        int history_idx = 0;

        unsigned int current_song_id = 0;
        unsigned int current_beat = 0;
        int beats_since_last_jump = 0;

        for (unsigned int i = 0; i < target_beats; i++) {
            RemixNode current_node = {current_song_id, current_beat};
            path.push_back(current_node);

            history[history_idx] = current_node;
            history_idx = (history_idx + 1) % HISTORY_SIZE;

            const SongData& current_song_data = (current_song_id == 0) ? *songA : *songB;

            unsigned int target_song_id = current_song_id;
            std::vector<unsigned int> candidates(MAX_CANDIDATES);
            std::vector<float> scores(MAX_CANDIDATES);

            if (dist100(rng) < 50) {
                target_song_id = 1 - current_song_id;
                if (target_song_id == 1) {
                    std::vector<float> row(csm.matrix.begin() + current_beat * csm.cols, csm.matrix.begin() + (current_beat + 1) * csm.cols);
                    find_top_candidates(row, candidates, scores);
                } else {
                    std::vector<float> temp_row(csm.rows);
                    for(unsigned int r=0; r < csm.rows; r++) temp_row[r] = csm.matrix[r * csm.cols + current_beat];
                    find_top_candidates(temp_row, candidates, scores);
                }
            } else {
                const SongData& s_data = (current_song_id == 0) ? *songA : *songB;
                std::vector<float> row(s_data.get_ssm().begin() + current_beat * s_data.get_count(), s_data.get_ssm().begin() + (current_beat + 1) * s_data.get_count());
                find_top_candidates(row, candidates, scores);
            }

            float max_available_sim = scores[0];
            float current_novelty = current_song_data.get_novelty()[current_beat];
            float jump_prob = calculate_jump_probability(current_beat, beats_since_last_jump, max_available_sim, current_novelty);

            if (dist01(rng) < jump_prob) {
                int selected_candidate = -1;
                for (int j = 0; j < MAX_CANDIDATES; j++) {
                    RemixNode candidate_node = {target_song_id, candidates[j]};
                    bool is_same_beat = (target_song_id == current_song_id && candidates[j] == current_beat);

                    if (scores[j] > 0.6f && !is_in_history(candidate_node, history) && !is_same_beat) {
                        selected_candidate = j;
                        break;
                    }
                }

                if (selected_candidate != -1) {
                    std::uniform_int_distribution<int> choice_dist(0, selected_candidate);
                    int choice = choice_dist(rng);

                    std::cout << ">> JUMP: Song " << current_song_id << " (Beat " << current_beat
                              << ") -> Song " << target_song_id << " (Beat " << candidates[choice]
                              << ") [Prob: " << jump_prob << " | Sim: " << scores[choice]
                              << " | Nov: " << current_novelty << "]\n";

                    current_song_id = target_song_id;
                    current_beat = candidates[choice];
                    beats_since_last_jump = 0;
                } else {
                    current_beat = (current_beat + 1) % current_song_data.get_count();
                    beats_since_last_jump++;
                }
            } else {
                current_beat = (current_beat + 1) % current_song_data.get_count();
                beats_since_last_jump++;
            }
        }

        return render_path_to_audio(path);
    }

    // Add public getters to access song metadata needed for saving output
    [[nodiscard]] unsigned int get_channels() const { return songA ? songA->get_channels() : 0; }
    [[nodiscard]] unsigned int get_sample_rate() const { return songA ? songA->get_sample_rate() : 0; }

private:
    static constexpr int MAX_CANDIDATES = 5;
    static constexpr int HISTORY_SIZE = 10;

    struct RemixNode {
        unsigned int song_id;
        unsigned int beat_index;
    };

    struct CrossSongMatrix {
        std::vector<float> matrix;
        unsigned int rows = 0;
        unsigned int cols = 0;
    };

    std::unique_ptr<SongData> songA;
    std::unique_ptr<SongData> songB;
    CrossSongMatrix csm;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist01{0.0f, 1.0f};
    std::uniform_int_distribution<int> dist100{0, 99};

    void compute_inter_song_similarity() {
        csm.rows = songA->get_count();
        csm.cols = songB->get_count();
        csm.matrix.resize(csm.rows * csm.cols, 0.0f);

        std::cout << "Computing cross-song similarity matrix...\n";
        float max_dist = 0.0f;
        for (unsigned int i = 0; i < csm.rows; i++) {
            for (unsigned int j = 0; j < csm.cols; j++) {
                float dist = 0.0f;
                if (!songA->get_features().empty() && !songB->get_features().empty() && songA->get_n_coeffs() == songB->get_n_coeffs()) {
                    for(unsigned int k = 0; k < songA->get_n_coeffs(); k++){
                         float diff = songA->get_features()[i * songA->get_n_coeffs() + k] - songB->get_features()[j * songB->get_n_coeffs() + k];
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
    }

    static void find_top_candidates(const std::vector<float>& sim_row, std::vector<unsigned int>& candidates, std::vector<float>& scores) {
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

    static bool is_in_history(const RemixNode& beat, const std::vector<RemixNode>& history) {
        return std::any_of(history.begin(), history.end(), [&beat](const RemixNode& h) {
            return h.song_id == beat.song_id && h.beat_index == beat.beat_index;
        });
    }

    static float calculate_jump_probability(unsigned int current_beat, int beats_since_last_jump, float max_sim, float novelty_score) {
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

    [[nodiscard]] std::vector<float> render_path_to_audio(const std::vector<RemixNode>& path) const {
        unsigned int channels = songA->get_channels();

        unsigned int total_f = 0;
        for (const auto& node : path) {
            const SongData& s_data = (node.song_id == 0) ? *songA : *songB;
            unsigned int start = s_data.get_beat_samples()[node.beat_index];
            unsigned int end = (node.beat_index < s_data.get_count() - 1) ? s_data.get_beat_samples()[node.beat_index + 1] : start + (s_data.get_sample_rate() / 2);
            total_f += (end - start);
        }

        std::vector<float> buffer(total_f * channels, 0.0f);
        unsigned int cursor = 0;

        for (const auto& node : path) {
            const SongData& s_data = (node.song_id == 0) ? *songA : *songB;
            unsigned int start = s_data.get_beat_samples()[node.beat_index];
            unsigned int end = (node.beat_index < s_data.get_count() - 1) ? s_data.get_beat_samples()[node.beat_index + 1] : start + (s_data.get_sample_rate() / 2);

            const auto& audio_data = s_data.get_audio_data();
            std::copy(audio_data.begin() + start * channels,
                      audio_data.begin() + end * channels,
                      buffer.begin() + cursor * channels);
            cursor += (end - start);
        }

        return buffer;
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <songA.wav> <songB.wav> [master_bpm] [master_midi_key]\n";
        return 1;
    }

    std::string song_a_path = argv[1];
    std::string song_b_path = argv[2];
    bool run_warp = (argc == 5);

    auto songA = std::make_unique<SongData>();
    std::cout << "Analyzing Song A: " << song_a_path << "\n";
    if (!songA->analyze_song(song_a_path)) {
        std::cout << "Error analyzing Song A. Check if the file exists and is a valid WAV.\n";
        return 1;
    }
    std::cout << "Song A: " << songA->get_bpm() << " BPM | " << songA->get_count() << " Beats | Key: " << songA->get_key() << "\n";

    auto songB = std::make_unique<SongData>();
    std::cout << "Analyzing Song B: " << song_b_path << "\n";
    if (!songB->analyze_song(song_b_path)) {
        std::cout << "Error analyzing Song B. Check if the file exists and is a valid WAV.\n";
        return 1;
    }
    std::cout << "Song B: " << songB->get_bpm() << " BPM | " << songB->get_count() << " Beats | Key: " << songB->get_key() << "\n";

    if (songA->get_channels() != songB->get_channels()) {
        std::cout << "Error: Songs must have the same number of channels.\n";
        return 1;
    }

    if (run_warp) {
        float master_bpm = std::stof(argv[3]);
        int master_key = std::stoi(argv[4]);
        std::cout << "Using Master Clock: " << master_bpm << " BPM, Key " << master_key << "\n";

        songA->set_audio_data(warp_audio(songA->get_audio_data(), songA->get_channels(), songA->get_sample_rate(), songA->get_bpm(), songA->get_key(), master_bpm, master_key));
        songB->set_audio_data(warp_audio(songB->get_audio_data(), songB->get_channels(), songB->get_sample_rate(), songB->get_bpm(), songB->get_key(), master_bpm, master_key));
    }

    MixtapeEngine engine(std::move(songA), std::move(songB));
    std::vector<float> mixtape_buffer = engine.generate_mixtape();

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = engine.get_channels();
    format.sampleRate = engine.get_sample_rate();
    format.bitsPerSample = 32;

    drwav out;
    if (drwav_init_file_write(&out, "remix_output.wav", &format, nullptr)) {
        drwav_write_pcm_frames(&out, mixtape_buffer.size() / format.channels, mixtape_buffer.data());
        drwav_uninit(&out);
        std::cout << "\nDone! Saved to: remix_output.wav\n";
    } else {
        std::cout << "Error: Could not open output file for writing.\n";
    }

    return 0;
}