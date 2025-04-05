// EventFrame.hpp
#ifndef EVENT_FRAME_HPP
#define EVENT_FRAME_HPP

#include "RepresentationBase.hpp"
#include <tuple>
#include <opencv2/opencv.hpp>
#include <vector>

class EventFrame : public RepresentationBase {
public:
    int height;
    int width;
    bool downsample;

    EventFrame(int height, int width, bool downsample = false);
    std::tuple<int, int, int> getShape() const;
    cv::Mat construct(const std::vector<int>& x,
                        const std::vector<int>& y,
                        const std::vector<int>& pol,
                        const std::vector<double>& time);
};

#endif // EVENT_FRAME_HPP
