// EventFrame.cpp
#include "EventFrame.hpp"
#include <algorithm>

// コンストラクタやメソッドの実装
EventFrame::EventFrame(int height, int width, bool downsample)
    : height(height), width(width), downsample(downsample) {}

std::tuple<int, int, int> EventFrame::getShape() const {
    if (downsample)
        return std::make_tuple(3, height / 2, width / 2);
    return std::make_tuple(3, height, width);
}

cv::Mat EventFrame::construct(const std::vector<int>& x,
                                const std::vector<int>& y,
                                const std::vector<int>& pol,
                                const std::vector<double>& time) {
    std::vector<int> count_on(height * width, 0);
    std::vector<int> count_off(height * width, 0);
    size_t nEvents = x.size();
    for (size_t i = 0; i < nEvents; i++) {
        int xi = std::max(0, std::min(x[i], width - 1));
        int yi = std::max(0, std::min(y[i], height - 1));
        int idx = yi * width + xi;
        if (pol[i] == 1)
            count_on[idx]++;
        else
            count_off[idx]++;
    }

    cv::Mat frame(height, width, CV_8UC1, cv::Scalar(127));
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int idx = i * width + j;
            int diff = count_on[idx] - count_off[idx];
            if (diff > 0)
                frame.at<uchar>(i, j) = 255;
            else if (diff < 0)
                frame.at<uchar>(i, j) = 0;
        }
    }
    cv::Mat colorFrame;
    cv::cvtColor(frame, colorFrame, cv::COLOR_GRAY2BGR);
    if (downsample) {
        cv::Mat resized;
        cv::resize(colorFrame, resized, cv::Size(width / 2, height / 2), 0, 0, cv::INTER_LINEAR);
        return resized;
    }
    return colorFrame;
}
