// StackedHistogram.hpp
#ifndef STACKED_HISTOGRAM_HPP
#define STACKED_HISTOGRAM_HPP

#include "RepresentationBase.hpp"
#include <tuple>
#include <opencv2/opencv.hpp>
#include <vector>
#include <optional>

class StackedHistogram : public RepresentationBase {
public:
    int bins;
    int height;
    int width;
    int count_cutoff;
    bool fastmode;
    int channels;
    bool downsample;

    StackedHistogram(int bins, int height, int width, std::optional<int> count_cutoff = std::nullopt, bool fastmode = true, bool downsample = false);

    static int get_numpy_dtype();  // 型に関する情報（C++版では必要に応じて定義）
    std::tuple<int, int, int> getShape() const;
    cv::Mat construct(const std::vector<int>& x,
                           const std::vector<int>& y,
                           const std::vector<int>& pol,
                           const std::vector<double>& time);
    // Tensor版の実装がある場合は別途メソッドを用意（ライブラリに応じた実装）
};

#endif // STACKED_HISTOGRAM_HPP
