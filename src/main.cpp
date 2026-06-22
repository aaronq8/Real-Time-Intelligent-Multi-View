#include "AppConfig.h"
#include "AudioDecodeWorker.h"
#include "AudioDecoder.h"
#include "AudioExcitementScorer.h"
#include "AudioFrameConverter.h"
#include "AudioScoreBuffer.h"
#include "BigStreamSelector.h"
#include "Cli.h"
#include "MediaInput.h"
#include "PlaybackClock.h"
#include "SdlRenderer.h"
#include "SpscQueue.h"
#include "VideoDecodeWorker.h"
#include "VideoFrame.h"
#include "VideoFrameBuffer.h"
#include "VisualFeatureBuffer.h"
#include "VisualFeatureExtractor.h"

#include "GoalVisualHeuristic.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

extern "C" {
#include <libavutil/samplefmt.h>
}

struct SecondBucket {
  double start_sec = 0.0;

  float max_final = 0.0f;
  float max_raw = 0.0f;
  float max_smooth = 0.0f;
  float max_rms = 0.0f;
  float max_z = 0.0f;

  double sum_final = 0.0;
  double sum_raw = 0.0;
  double sum_smooth = 0.0;
  double sum_rms = 0.0;
  double sum_z = 0.0;

  int count = 0;
};

struct PendingGoalCandidate {
  int stream_id = -1;
  double candidate_time_sec = 0.0;
  float audio_score = 0.0f;
  bool evaluated = false;
};

static float clamp01_eval(float x) { return std::clamp(x, 0.0f, 1.0f); }

static float loudness_score_eval(float rms) {
  // Tune these after looking at max_rms / avg_rms table.
  constexpr float kQuietRms = 0.025f;
  constexpr float kLoudRms = 0.090f;

  return clamp01_eval((rms - kQuietRms) / (kLoudRms - kQuietRms));
}

static float relative_score_eval(const AudioScore &s) {
  // raw catches sharp spikes, smooth catches sustained excitement.
  return std::max(s.smoothed_score, 0.65f * s.raw_score);
}

static float startup_weight_eval(double t_sec) {
  // Useful because first few seconds often have unstable baseline.
  if (t_sec < 3.0) {
    return 0.25f;
  }

  if (t_sec < 6.0) {
    return 0.25f + 0.75f * static_cast<float>((t_sec - 3.0) / 3.0);
  }

  return 1.0f;
}

static float final_score_eval(const AudioScore &s) {
  const float rel = relative_score_eval(s);
  const float loud = loudness_score_eval(s.rms);

  // Formula C: requires both relative surprise and absolute loudness.
  const float combined = 0.40f * rel + 0.40f * loud + 0.20f * rel * loud;

  return clamp01_eval(startup_weight_eval(s.start_sec) * combined);
}

static float window_rank_score_eval(const SecondBucket &b) {
  if (b.count == 0) {
    return 0.0f;
  }

  const float avg_final =
      static_cast<float>(b.sum_final / static_cast<double>(b.count));

  // Rank a second by both max and sustained average.
  return 0.70f * b.max_final + 0.30f * avg_final;
}

static void run_audio_scoring_evaluation(MediaInput &input, int input_id,
                                         int max_audio_frames) {
  if (input.audio_stream_index() < 0) {
    std::cout << "Input " << input_id << " has no audio stream.\n";
    return;
  }

  std::cout << "\n=== Audio scoring evaluation for input " << input_id
            << " ===\n";

  AudioDecoder audio_decoder(input, input_id);
  audio_decoder.open();

  AudioExcitementScorer scorer(input_id);

  constexpr int kMaxSeconds = 300;
  std::vector<SecondBucket> buckets(kMaxSeconds);

  int decoded_audio_frames = 0;
  int scored_chunks = 0;

  for (int frame_id = 0; frame_id < max_audio_frames; ++frame_id) {
    std::optional<AudioFrame> af = audio_decoder.decode_next_frame();

    if (!af) {
      std::cout << "EOF reached after " << frame_id << " audio frames.\n";
      break;
    }

    ++decoded_audio_frames;

    std::optional<AudioChunk> chunk = convert_audio_frame_to_mono_float(*af);

    if (!chunk) {
      continue;
    }

    AudioScore score = scorer.process_chunk(*chunk);
    ++scored_chunks;

    const float final = final_score_eval(score);

    const int sec = static_cast<int>(score.start_sec);

    if (sec < 0 || sec >= static_cast<int>(buckets.size())) {
      continue;
    }

    SecondBucket &b = buckets[sec];
    b.start_sec = static_cast<double>(sec);

    b.max_final = std::max(b.max_final, final);
    b.max_raw = std::max(b.max_raw, score.raw_score);
    b.max_smooth = std::max(b.max_smooth, score.smoothed_score);
    b.max_rms = std::max(b.max_rms, score.rms);
    b.max_z = std::max(b.max_z, score.z_score);

    b.sum_final += final;
    b.sum_raw += score.raw_score;
    b.sum_smooth += score.smoothed_score;
    b.sum_rms += score.rms;
    b.sum_z += score.z_score;

    ++b.count;
  }

  std::cout << "decoded_audio_frames=" << decoded_audio_frames
            << " scored_chunks=" << scored_chunks << "\n";

  std::cout << "\nsec" << ",max_final" << ",avg_final" << ",rank_score"
            << ",max_rms" << ",avg_rms" << ",max_raw" << ",avg_raw"
            << ",max_smooth" << ",avg_smooth" << ",max_z" << ",avg_z" << "\n";

  for (const SecondBucket &b : buckets) {
    if (b.count == 0) {
      continue;
    }

    const float avg_final =
        static_cast<float>(b.sum_final / static_cast<double>(b.count));
    const float avg_rms =
        static_cast<float>(b.sum_rms / static_cast<double>(b.count));
    const float avg_raw =
        static_cast<float>(b.sum_raw / static_cast<double>(b.count));
    const float avg_smooth =
        static_cast<float>(b.sum_smooth / static_cast<double>(b.count));
    const float avg_z =
        static_cast<float>(b.sum_z / static_cast<double>(b.count));

    std::cout << static_cast<int>(b.start_sec) << "," << b.max_final << ","
              << avg_final << "," << window_rank_score_eval(b) << ","
              << b.max_rms << "," << avg_rms << "," << b.max_raw << ","
              << avg_raw << "," << b.max_smooth << "," << avg_smooth << ","
              << b.max_z << "," << avg_z << "\n";
  }

  std::vector<std::pair<float, int>> ranked;

  for (int sec = 0; sec < static_cast<int>(buckets.size()); ++sec) {
    if (buckets[sec].count == 0) {
      continue;
    }

    ranked.push_back({window_rank_score_eval(buckets[sec]), sec});
  }

  std::sort(ranked.begin(), ranked.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  std::cout << "\nTop scored seconds:\n";

  const int top_k = std::min<int>(15, ranked.size());

  for (int k = 0; k < top_k; ++k) {
    const int sec = ranked[k].second;
    const SecondBucket &b = buckets[sec];

    const float avg_final =
        static_cast<float>(b.sum_final / static_cast<double>(b.count));
    const float avg_rms =
        static_cast<float>(b.sum_rms / static_cast<double>(b.count));
    const float avg_raw =
        static_cast<float>(b.sum_raw / static_cast<double>(b.count));
    const float avg_smooth =
        static_cast<float>(b.sum_smooth / static_cast<double>(b.count));

    std::cout << "# " << std::setw(2) << (k + 1) << " sec=" << sec
              << " rank_score=" << ranked[k].first
              << " max_final=" << b.max_final << " avg_final=" << avg_final
              << " max_rms=" << b.max_rms << " avg_rms=" << avg_rms
              << " max_raw=" << b.max_raw << " avg_raw=" << avg_raw
              << " max_smooth=" << b.max_smooth << " avg_smooth=" << avg_smooth
              << " max_z=" << b.max_z << "\n";
  }

  std::cout << "\nTarget check around 18-19 sec:\n";

  for (int sec = 16; sec <= 21; ++sec) {
    if (sec < 0 || sec >= static_cast<int>(buckets.size())) {
      continue;
    }

    const SecondBucket &b = buckets[sec];

    if (b.count == 0) {
      continue;
    }

    const float avg_final =
        static_cast<float>(b.sum_final / static_cast<double>(b.count));
    const float avg_rms =
        static_cast<float>(b.sum_rms / static_cast<double>(b.count));

    std::cout << "sec=" << sec << " rank_score=" << window_rank_score_eval(b)
              << " max_final=" << b.max_final << " avg_final=" << avg_final
              << " max_rms=" << b.max_rms << " avg_rms=" << avg_rms
              << " max_raw=" << b.max_raw << " max_smooth=" << b.max_smooth
              << " max_z=" << b.max_z << "\n";
  }
}

int main(int argc, char **argv) {
  AppConfig config;

  const ParseResult parse_result = parse_args(argc, argv, config);

  if (parse_result == ParseResult::Help) {
    return 0;
  }

  if (parse_result == ParseResult::Error) {
    return 1;
  }

  try {
    std::array<MediaInput, 3> inputs;

    for (size_t i = 0; i < config.input_paths.size(); ++i) {
      inputs[i] = MediaInput(config.input_paths[i]);
      inputs[i].open();
      inputs[i].print_info(static_cast<int>(i));
    }

    std::cout << "\nAll inputs opened successfully.\n";
    // TEMP
    constexpr bool kRunAudioScoringEvaluation = false;

    if (kRunAudioScoringEvaluation) {
      constexpr int kEvalInputId = 2;
      constexpr int kMaxAudioFrames = 5000;

      run_audio_scoring_evaluation(inputs[kEvalInputId], kEvalInputId,
                                   kMaxAudioFrames);

      return 0;
    }
    // end TEMP
    constexpr size_t queue_capacity = 64;

    std::array<std::unique_ptr<SpscQueue<std::shared_ptr<VideoFrame>>>, 3>
        video_queues;
    std::array<std::unique_ptr<VideoDecodeWorker>, 3> workers;

    std::array<std::unique_ptr<SpscQueue<AudioScore>>, 3> audio_queues;
    std::array<std::unique_ptr<AudioDecodeWorker>, 3> audio_workers;

    for (size_t i = 0; i < 3; ++i) {
      video_queues[i] =
          std::make_unique<SpscQueue<std::shared_ptr<VideoFrame>>>(
              queue_capacity);

      audio_queues[i] = std::make_unique<SpscQueue<AudioScore>>(512);

      workers[i] = std::make_unique<VideoDecodeWorker>(
          inputs[i], static_cast<int>(i), *video_queues[i]);
      audio_workers[i] = std::make_unique<AudioDecodeWorker>(
          config.input_paths[i], static_cast<int>(i), *audio_queues[i]);
    }
    // start audio + video decoder threads
    for (auto &worker : workers) {
      worker->start();
    }
    for (int i = 0; i < 3; i++) {
      audio_workers[i]->start();
    }

    SdlRenderer renderer(config.window_width, config.window_height);

    PlaybackClock clock;
    clock.start(0.0);

    std::array<VideoFrameBuffer, 3> frame_buffers;
    std::array<VisualFeatureExtractor, 3> visual_extractors;
    std::array<VisualFeatureBuffer, 3> visual_buffers;
    std::array<double, 3> last_visual_sample_time = {-1e9, -1e9, -1e9};

    constexpr double kVisualSampleIntervalSec = 0.25;

    // Frames currently selected for display by PTS.
    std::array<std::shared_ptr<VideoFrame>, 3> display_frames;
    std::array<std::shared_ptr<VideoFrame>, 3> uploaded_frames;

    std::array<int, 3> received_count = {0, 0, 0};

    std::array<AudioScoreBuffer, 3> audio_score_buffers;
    std::array<bool, 3> audio_done = {false, false, false};
    GoalVisualHeuristic goal_visual_heuristic;
    std::vector<PendingGoalCandidate> pending_goal_candidates;
    BigStreamSelector big_stream_selector(0);

    auto last_stats_time = std::chrono::steady_clock::now();
    uint64_t render_count = 0;

    int big_stream_id = 0;

    while (true) {
      if (renderer.poll_quit()) {
        break;
      }

      constexpr double max_buffer_lead_sec = 0.5;
      constexpr double max_audio_score_lead_sec = 1.0;

      constexpr float kAudioGoalCandidateThreshold = 0.55f;
      constexpr double kMinCandidateGapSec = 5.0;
      constexpr double kVisualConfirmationDelaySec = 3.75;

      const double t_now = clock.now_sec();

      // --------------------------------------------------------------------------
      // 1. Pull audio scores from audio workers.
      // --------------------------------------------------------------------------
      for (size_t i = 0; i < 3; ++i) {
        while (audio_score_buffers[i].needs_more_scores(
            t_now, max_audio_score_lead_sec)) {
          std::optional<AudioScore> score = audio_queues[i]->try_pop();

          if (!score) {
            break;
          }

          audio_score_buffers[i].push(*score);
        }

        if (audio_queues[i]->closed() && audio_queues[i]->consumer_empty()) {
          audio_done[i] = true;
        }
      }

      // --------------------------------------------------------------------------
      // 2. Pull video frames from video workers.
      // --------------------------------------------------------------------------
      for (size_t i = 0; i < 3; ++i) {
        while (frame_buffers[i].needs_more_frames(t_now, max_buffer_lead_sec)) {
          std::optional<std::shared_ptr<VideoFrame>> frame =
              video_queues[i]->try_pop();

          if (!frame) {
            break;
          }

          if (*frame) {
            frame_buffers[i].push(*frame);
            ++received_count[i];
          }
        }
      }

      // --------------------------------------------------------------------------
      // 3. Select display frames and sample visual features.
      // --------------------------------------------------------------------------
      for (size_t i = 0; i < 3; ++i) {
        std::shared_ptr<VideoFrame> selected =
            frame_buffers[i].frame_for_time(t_now);

        if (selected && selected != display_frames[i]) {
          display_frames[i] = selected;
        }

        // Sample visual features at low FPS.
        if (display_frames[i] &&
            t_now - last_visual_sample_time[i] >= kVisualSampleIntervalSec) {
          std::optional<VisualFrameFeature> vf =
              visual_extractors[i].extract(*display_frames[i]);

          if (vf) {
            visual_buffers[i].push(*vf);
            last_visual_sample_time[i] = t_now;
          }
        }

        if (display_frames[i] && display_frames[i] != uploaded_frames[i]) {
          renderer.update_texture(*display_frames[i]);
          uploaded_frames[i] = display_frames[i];
        }
      }

      // --------------------------------------------------------------------------
      // 4. Compute current audio scores.
      // --------------------------------------------------------------------------
      std::array<float, 3> current_audio_scores = {
          audio_score_buffers[0].score_for_time(t_now),
          audio_score_buffers[1].score_for_time(t_now),
          audio_score_buffers[2].score_for_time(t_now),
      };

      // --------------------------------------------------------------------------
      // 5. Add pending goal candidates from high audio score.
      // --------------------------------------------------------------------------
      for (int stream_id = 0; stream_id < 3; ++stream_id) {
        const float score = current_audio_scores[stream_id];

        if (score < kAudioGoalCandidateThreshold) {
          continue;
        }

        bool recently_added = false;

        for (const PendingGoalCandidate &c : pending_goal_candidates) {
          if (c.stream_id == stream_id &&
              std::abs(c.candidate_time_sec - t_now) < kMinCandidateGapSec) {
            recently_added = true;
            break;
          }
        }

        if (recently_added) {
          continue;
        }

        PendingGoalCandidate candidate;
        candidate.stream_id = stream_id;
        candidate.candidate_time_sec = t_now;
        candidate.audio_score = score;

        pending_goal_candidates.push_back(candidate);

        std::cout << "[goal-candidate]" << " stream=" << stream_id
                  << " t=" << t_now << " audio_score=" << score << "\n";
      }

      // --------------------------------------------------------------------------
      // 6. Evaluate pending goal candidates once enough future visual samples
      // exist.
      // --------------------------------------------------------------------------
      for (PendingGoalCandidate &candidate : pending_goal_candidates) {
        if (candidate.evaluated) {
          continue;
        }

        if (t_now <
            candidate.candidate_time_sec + kVisualConfirmationDelaySec) {
          continue;
        }

        GoalVisualEvidence evidence = goal_visual_heuristic.evaluate(
            visual_buffers[candidate.stream_id], candidate.candidate_time_sec);

        candidate.evaluated = true;

        std::cout << "[goal-visual]" << " stream=" << candidate.stream_id
                  << " t=" << candidate.candidate_time_sec
                  << " audio=" << candidate.audio_score
                  << " field_before=" << evidence.green_before
                  << " early_avg=" << evidence.green_early_avg
                  << " replay_avg=" << evidence.green_replay_avg
                  << " replay_min=" << evidence.green_replay_min
                  << " early_drop=" << evidence.early_green_drop
                  << " replay_drop=" << evidence.replay_green_drop
                  << " low_field_dur=" << evidence.low_green_duration_sec
                  << " visual_score=" << evidence.visual_score
                  << " confirmed=" << evidence.confirmed << "\n";

        if (evidence.confirmed) {
          std::cout << "[goal-confirmed]" << " stream=" << candidate.stream_id
                    << " t=" << candidate.candidate_time_sec << "\n";

          big_stream_selector.notify_confirmed_goal(candidate.stream_id, t_now);
        } else {
          std::cout << "[goal-rejected]" << " stream=" << candidate.stream_id
                    << " t=" << candidate.candidate_time_sec << "\n";
        }
      }

      // --------------------------------------------------------------------------
      // 7. Remove old evaluated candidates.
      // --------------------------------------------------------------------------
      pending_goal_candidates.erase(
          std::remove_if(
              pending_goal_candidates.begin(), pending_goal_candidates.end(),
              [t_now](const PendingGoalCandidate &c) {
                return c.evaluated && t_now > c.candidate_time_sec + 10.0;
              }),
          pending_goal_candidates.end());

      // --------------------------------------------------------------------------
      // 8. Select big stream based on current audio scores.
      // --------------------------------------------------------------------------
      big_stream_id = big_stream_selector.update(
          t_now); // --------------------------------------------------------------------------
      // 9. Debug stats.
      // --------------------------------------------------------------------------
      // const auto now = std::chrono::steady_clock::now();

      // if (now - last_stats_time >= std::chrono::seconds(1)) {
      //   last_stats_time = now;

      //   std::cout << "[audio-scores]" << " t=" << t_now
      //             << " big=" << big_stream_id
      //             << " s0=" << current_audio_scores[0]
      //             << " s1=" << current_audio_scores[1]
      //             << " s2=" << current_audio_scores[2] << "\n";

      //   for (size_t i = 0; i < 3; ++i) {
      //     const VisualFrameFeature *vf = visual_buffers[i].latest();

      //     if (!vf) {
      //       std::cout << "  visual stream=" << i << " none\n";
      //       continue;
      //     }

      //     std::cout << "  visual stream=" << i << " t=" << vf->pts_sec
      //               << " green=" << vf->green_ratio
      //               << " center_green=" << vf->center_green_ratio
      //               << " tile_green=" << vf->green_tile_ratio
      //               << " center_non_green=" << vf->center_non_green_ratio
      //               << " field=" << vf->field_visibility_score
      //               << " diff=" << vf->scene_diff
      //               << " edge=" << vf->edge_density
      //               << " n=" << visual_buffers[i].size() << "\n";
      //   }

      //   for (size_t i = 0; i < 3; ++i) {
      //     std::cout << "  audio stream=" << i
      //               << " queue=" << audio_queues[i]->size()
      //               << " buffer=" << audio_score_buffers[i].size()
      //               << " done=" << audio_done[i] << " buffered_until="
      //               << audio_score_buffers[i].buffered_until_sec() << "\n";
      //   }

      //   std::cout << "  pending_goal_candidates="
      //             << pending_goal_candidates.size() << "\n";
      // }

      // --------------------------------------------------------------------------
      // 10. Render current selected frames.
      // --------------------------------------------------------------------------
      renderer.render(display_frames, big_stream_id);
      ++render_count;
    }

    // Stop producers blocked on push_blocking().
    for (auto &queue : video_queues) {
      if (queue) {
        queue->close();
      }
    }

    for (auto &queue : audio_queues) {
      if (queue) {
        queue->close();
      }
    }

    // Now joins can complete.
    for (auto &worker : workers) {
      if (worker) {
        worker->join();
      }
    }

    for (auto &worker : audio_workers) {
      if (worker) {
        worker->join();
      }
    }

    for (size_t i = 0; i < 3; ++i) {
      if (workers[i]->exception()) {
        std::rethrow_exception(workers[i]->exception());
      }
    }

    std::cout << "\nReceived frame counts:\n";
    for (size_t i = 0; i < 3; ++i) {
      std::cout << "  stream " << i << ": " << received_count[i] << " frames\n";
    }
  }

  catch (const std::exception &e) {
    std::cerr << "\nFatal error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}