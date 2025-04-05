#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <string>
#include "H5Reader.hpp"
#include "EventFrame.hpp"
#include <opencv2/opencv.hpp>

// ※ std::lower_bound, std::upper_bound を利用して、numpy の searchsorted 相当の処理を行います。

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <h5_file> <width> <height> <delta_t_ms> <duration_t_ms>" << std::endl;
        return 1;
    }

    // コマンドライン引数のパース
    std::string h5FilePath = argv[1];
    int width = std::stoi(argv[2]);
    int height = std::stoi(argv[3]);
    int delta_t_ms = std::stoi(argv[4]);
    int duration_t_ms = std::stoi(argv[5]);

    // タイムスタンプの単位がマイクロ秒であれば、変換
    long long delta_t_us = static_cast<long long>(delta_t_ms) * 1000;
    long long duration_t_us = static_cast<long long>(duration_t_ms) * 1000;

    try {
        // H5Reader の生成（H5Reader.hpp / H5Reader.cpp を利用）
        H5Reader reader(h5FilePath, width, height);

        // ファイル内に指定されたサイズ（あるいは引数のサイズ）を取得
        auto hw = reader.getHeightAndWidth();
        if (hw.first.has_value() && hw.second.has_value()) {
            width = hw.first.value();
            height = hw.second.value();
        }

        // EventFrame のインスタンス生成（EventFrame.hpp / EventFrame.cpp）
        // downsample=false として初期化
        EventFrame eventFrame(height, width, false);

        // 補正済みタイムスタンプ配列の取得
        std::vector<long long> t_array = reader.getTime();
        if (t_array.empty()) {
            std::cerr << "No time data available." << std::endl;
            return 1;
        }
        long long start_time = t_array.front();
        long long end_time = t_array.back();
        long long current_time = start_time;

        // VideoWriter の設定
        double fps = 1000.0 / delta_t_ms;  // 例：delta=100msなら fps=10
        std::string output_video_filename = "event_video_cpp.mp4";
        int fourcc = cv::VideoWriter::fourcc('m','p','4','v');
        cv::Size frameSize(width, height);
        cv::VideoWriter videoWriter(output_video_filename, fourcc, fps, frameSize);
        if (!videoWriter.isOpened()) {
            std::cerr << "Failed to open video writer." << std::endl;
            return 1;
        }

        int frame_index = 0;
        while (current_time <= end_time) {
            // 現在時刻から duration 分のウィンドウ開始時刻を計算
            long long window_start_time = current_time - duration_t_us;
            if (window_start_time < start_time) {
                window_start_time = start_time;
            }

            // std::lower_bound / upper_bound を利用して、インデックスを探索（t_array は昇順と仮定）
            auto idx_start_it = std::lower_bound(t_array.begin(), t_array.end(), window_start_time);
            auto idx_end_it = std::upper_bound(t_array.begin(), t_array.end(), current_time);
            size_t idx_start = std::distance(t_array.begin(), idx_start_it);
            size_t idx_end   = std::distance(t_array.begin(), idx_end_it);

            if (idx_end <= idx_start) {
                std::cout << "Skipping frame " << frame_index 
                          << ": no events in window [" << window_start_time << ", " << current_time << "]" 
                          << std::endl;
                current_time += delta_t_us;
                frame_index++;
                continue;
            }

            // イベントスライスの取得
            H5Reader::EventData events = reader.getEventSlice(idx_start, idx_end);
            // ※ events.x, events.y, events.p, events.t にそれぞれイベントの情報が入っています

            // ここでイベント表現（例えば、EventFrame）を生成
            // EventFrame::construct は、イベントデータ（std::vector<long long> など）を受け取り、cv::Mat 形式の画像を返す実装とします
            // main.cpp 内の該当部分

            // 取得した events.x, events.y, events.p は vector<long long> なので、vector<int> に変換
            std::vector<int> x_int(events.x.begin(), events.x.end());
            std::vector<int> y_int(events.y.begin(), events.y.end());
            std::vector<int> pol_int(events.p.begin(), events.p.end());

            // t については、EventFrame の実装では vector<double> を受け取るので、double に変換
            std::vector<double> t_double(events.t.begin(), events.t.end());

            // ここで construct（または createFrame）を呼び出す
            cv::Mat frame_img = eventFrame.construct(x_int, y_int, pol_int, t_double);


            // もし生成された画像が RGB 形式であれば、OpenCV 用に BGR に変換（実装次第）
            cv::Mat frame_img_bgr;
            cv::cvtColor(frame_img, frame_img_bgr, cv::COLOR_RGB2BGR);

            // 動画フレームに書き込み
            videoWriter.write(frame_img_bgr);

            std::cout << "Frame " << frame_index 
                      << ": Window [" << window_start_time << ", " << current_time 
                      << "], event count: " << events.t.size() << " added to video." 
                      << std::endl;

            frame_index++;
            current_time += delta_t_us;
        }

        videoWriter.release();
        std::cout << "Video saved to " << output_video_filename << std::endl;
    }
    catch (std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
