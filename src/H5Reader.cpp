#include "H5Reader.hpp"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <stdexcept>

H5Reader::H5Reader(const std::string& h5FilePath, std::optional<int> width, std::optional<int> height)
    : width(width), height(height), open(false)
{
    // C++17 の std::filesystem を利用してファイルの存在チェック
    if (!std::filesystem::exists(h5FilePath)) {
        throw std::runtime_error(h5FilePath + " does not exist.");
    }
    // 拡張子のチェック
    std::string ext = std::filesystem::path(h5FilePath).extension().string();
    if (ext != ".h5" && ext != ".hdf5") {
        throw std::runtime_error("File must be HDF5 format.");
    }

    try {
        // 読み取り専用モードでファイルをオープン
        h5File = new H5::H5File(h5FilePath, H5F_ACC_RDONLY);
        open = true;
    } catch (H5::Exception& e) {
        throw std::runtime_error("Failed to open H5 file: " + std::string(e.getDetailMsg()));
    }

    // ファイル構造の判定
    // "CD" グループが存在し、その中に "events" があればそちらを使用
    // なければルート直下の "events" を使用
    try {
        H5::Group groupCD = h5File->openGroup("CD");
        H5::Group eventsGroup = groupCD.openGroup("events");
        eventPath.push_back("CD");
        eventPath.push_back("events");
    } catch (H5::Exception& e) {
        try {
            H5::Group eventsGroup = h5File->openGroup("events");
            eventPath.push_back("events");
        } catch (H5::Exception& ex) {
            throw std::runtime_error("Unsupported H5 file structure. Cannot find events data.");
        }
    }
}

H5Reader::~H5Reader() {
    if (open) {
        h5File->close();
        delete h5File;
        open = false;
    }
}

void H5Reader::close() {
    if (open) {
        h5File->close();
        delete h5File;
        open = false;
    }
}

bool H5Reader::isOpen() const {
    return open;
}

std::pair<std::optional<int>, std::optional<int>> H5Reader::getHeightAndWidth() const {
    return {height, width};
}

H5::Group H5Reader::getEventGroup() {
    if (!open) {
        throw std::runtime_error("File is closed.");
    }
    // eventPath の内容に沿ってグループを辿る
    H5::Group group = h5File->openGroup(eventPath[0]);
    for (size_t i = 1; i < eventPath.size(); i++) {
        group = group.openGroup(eventPath[i]);
    }
    return group;
}

void H5Reader::correctTime(std::vector<long long>& timeArray) {
    if (timeArray.empty()) return;
    long long timeLast = timeArray[0];
    for (size_t i = 0; i < timeArray.size(); i++) {
        if (timeArray[i] < timeLast) {
            timeArray[i] = timeLast;
        } else {
            timeLast = timeArray[i];
        }
    }
}

std::vector<long long> H5Reader::getTime() {
    if (!open) {
        throw std::runtime_error("File is closed.");
    }
    if (allTimes.empty()) {
        H5::Group eventGroup = getEventGroup();
        // "t" データセットを開く（タイムスタンプ）
        H5::DataSet tDataset = eventGroup.openDataSet("t");
        H5::DataSpace dataspace = tDataset.getSpace();
        hsize_t numElements;
        dataspace.getSimpleExtentDims(&numElements, nullptr);
        allTimes.resize(numElements);
        // ここでは HDF5 側で int64_t (long long) 型として保存されていると仮定
        tDataset.read(allTimes.data(), H5::PredType::NATIVE_LLONG);
        correctTime(allTimes);
    }
    return allTimes;
}

std::map<std::string, std::string> H5Reader::getOriginalDtypes() {
    if (!open) {
        throw std::runtime_error("File is closed.");
    }
    H5::Group eventGroup = getEventGroup();
    std::map<std::string, std::string> dtypes;
    std::vector<std::string> keys = {"t", "x", "y", "p"};
    for (const auto& key : keys) {
        try {
            H5::DataSet dataset = eventGroup.openDataSet(key);
            H5::DataType dtype = dataset.getDataType();
            // 型クラスを簡易的に文字列として返す例
            H5T_class_t type_class = dtype.getClass();
            switch (type_class) {
                case H5T_INTEGER:
                    dtypes[key] = "INTEGER";
                    break;
                case H5T_FLOAT:
                    dtypes[key] = "FLOAT";
                    break;
                default:
                    dtypes[key] = "OTHER";
                    break;
            }
        } catch (H5::Exception& e) {
            dtypes[key] = "NOT FOUND";
        }
    }
    return dtypes;
}

H5Reader::EventData H5Reader::getEventSlice(size_t idxStart, size_t idxEnd) {
    if (!open) {
        throw std::runtime_error("File is closed.");
    }
    if (idxEnd < idxStart) {
        throw std::runtime_error("idxEnd must be >= idxStart");
    }
    H5::Group eventGroup = getEventGroup();
    size_t sliceSize = idxEnd - idxStart;
    EventData evData;
    evData.x.resize(sliceSize);
    evData.y.resize(sliceSize);
    evData.p.resize(sliceSize);
    evData.t.resize(sliceSize);

    // ヘルパーラムダ: 指定したデータセットの部分データを読み込む
    auto readSlice = [&](const std::string& name, void* buffer, H5::PredType type) {
        H5::DataSet dataset = eventGroup.openDataSet(name);
        H5::DataSpace filespace = dataset.getSpace();
        hsize_t offset[1] = { idxStart };
        hsize_t count[1] = { sliceSize };
        filespace.selectHyperslab(H5S_SELECT_SET, count, offset);
        H5::DataSpace memspace(1, count);
        dataset.read(buffer, type, memspace, filespace);
    };

    // x, y, p, t を読み込む（ここでは int64_t 型で保存されていると仮定）
    readSlice("x", evData.x.data(), H5::PredType::NATIVE_LLONG);
    readSlice("y", evData.y.data(), H5::PredType::NATIVE_LLONG);
    readSlice("p", evData.p.data(), H5::PredType::NATIVE_LLONG);
    readSlice("t", evData.t.data(), H5::PredType::NATIVE_LLONG);

    // タイムスタンプが昇順になっているか補正（簡易実装）
    for (size_t i = 1; i < evData.t.size(); i++) {
        if (evData.t[i] < evData.t[i - 1]) {
            evData.t[i] = evData.t[i - 1];
        }
    }

    evData.height = height;
    evData.width = width;
    return evData;
}

std::map<std::string, long long> H5Reader::getEventSummary() {
    if (!open) {
        throw std::runtime_error("File is closed.");
    }
    std::map<std::string, long long> summary;
    H5::Group eventGroup = getEventGroup();

    // x, y, p の各データセット全体を読み込む（簡易実装）
    std::vector<long long> xArray, yArray, pArray, tArray;
    {
        H5::DataSet ds = eventGroup.openDataSet("x");
        H5::DataSpace ds_space = ds.getSpace();
        hsize_t numElements;
        ds_space.getSimpleExtentDims(&numElements, nullptr);
        xArray.resize(numElements);
        ds.read(xArray.data(), H5::PredType::NATIVE_LLONG);
    }
    {
        H5::DataSet ds = eventGroup.openDataSet("y");
        H5::DataSpace ds_space = ds.getSpace();
        hsize_t numElements;
        ds_space.getSimpleExtentDims(&numElements, nullptr);
        yArray.resize(numElements);
        ds.read(yArray.data(), H5::PredType::NATIVE_LLONG);
    }
    {
        H5::DataSet ds = eventGroup.openDataSet("p");
        H5::DataSpace ds_space = ds.getSpace();
        hsize_t numElements;
        ds_space.getSimpleExtentDims(&numElements, nullptr);
        pArray.resize(numElements);
        ds.read(pArray.data(), H5::PredType::NATIVE_LLONG);
    }
    // t は getTime() を利用（補正済み）
    tArray = getTime();

    if (!tArray.empty()) {
        summary["t_min"] = *std::min_element(tArray.begin(), tArray.end());
        summary["t_max"] = *std::max_element(tArray.begin(), tArray.end());
    }
    if (!xArray.empty()) {
        summary["x_min"] = *std::min_element(xArray.begin(), xArray.end());
        summary["x_max"] = *std::max_element(xArray.begin(), xArray.end());
    }
    if (!yArray.empty()) {
        summary["y_min"] = *std::min_element(yArray.begin(), yArray.end());
        summary["y_max"] = *std::max_element(yArray.begin(), yArray.end());
    }

    long long p_on_count = 0, p_off_count = 0;
    for (const auto& val : pArray) {
        if (val == 1) p_on_count++;
        else if (val == 0) p_off_count++;
    }
    summary["p_on_count"] = p_on_count;
    summary["p_off_count"] = p_off_count;
    summary["total_count"] = static_cast<long long>(pArray.size());

    return summary;
}
