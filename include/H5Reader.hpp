#ifndef H5_READER_HPP
#define H5_READER_HPP

#include <H5Cpp.h>
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <optional>

// H5Reader クラスは、HDF5 ファイルからイベントデータを読み込むクラスです。
class H5Reader {
public:
    // コンストラクタ
    // h5FilePath: HDF5 ファイルのパス
    // width, height: （オプション）明示的に指定された場合の画像サイズ
    H5Reader(const std::string& h5FilePath, std::optional<int> width = std::nullopt, std::optional<int> height = std::nullopt);
    ~H5Reader();

    // 明示的にファイルを閉じる
    void close();
    bool isOpen() const;

    // 高さと幅を返す（指定されていなければ nullopt）
    std::pair<std::optional<int>, std::optional<int>> getHeightAndWidth() const;

    // タイムスタンプデータ（t）の配列を取得（内部で補正済み）
    std::vector<long long> getTime();

    // 元のデータ型情報を取得（キー: "t", "x", "y", "p"）
    std::map<std::string, std::string> getOriginalDtypes();

    // イベントスライスの結果を格納する構造体
    struct EventData {
        std::vector<long long> x;
        std::vector<long long> y;
        std::vector<long long> p;
        std::vector<long long> t;
        std::optional<int> height;
        std::optional<int> width;
    };

    // 指定したインデックス範囲 [idxStart, idxEnd) のイベントデータを取得
    EventData getEventSlice(size_t idxStart, size_t idxEnd);

    // イベント全体の統計情報を取得
    std::map<std::string, long long> getEventSummary();

private:
    H5::H5File* h5File;           // HDF5 ファイルへのポインタ
    bool open;                   // ファイルがオープンしているか
    std::optional<int> width;    // 画像の幅（オプション）
    std::optional<int> height;   // 画像の高さ（オプション）
    std::vector<long long> allTimes;  // タイムスタンプを保持する配列
    std::vector<std::string> eventPath; // イベントデータのパス（例: {"CD", "events"} または {"events"}）

    // イベントデータが格納されているグループを返す
    H5::Group getEventGroup();

    // タイムスタンプ配列が降順になっている場合に補正（非減少にする）
    void correctTime(std::vector<long long>& timeArray);
};

#endif // H5_READER_HPP
