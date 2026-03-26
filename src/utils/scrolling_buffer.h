/**
 * @file scrolling_buffer.h
 * @brief Ring-buffer optimised for real-time plotting with ImPlot.
 *
 * Stores (x, y) pairs in a fixed-capacity circular buffer.
 * ImPlot's offset parameter lets it render directly from the ring
 * without copying into a temporary ordered array.
 *
 * Usage with ImPlot:
 *   ImPlot::PlotLine("label", buf.DataX.data(), buf.DataY.data(),
 *                    buf.Size(), 0, 0, buf.Offset, sizeof(float));
 */

#pragma once

#include <vector>
#include <cstddef>

struct ScrollingBuffer {
    int                MaxSize;
    int                Offset;
    std::vector<float> DataX;
    std::vector<float> DataY;

    explicit ScrollingBuffer(int max_size = 3600)
        : MaxSize(max_size), Offset(0) {
        DataX.reserve(max_size);
        DataY.reserve(max_size);
    }

    void AddPoint(float x, float y) {
        if (static_cast<int>(DataX.size()) < MaxSize) {
            DataX.push_back(x);
            DataY.push_back(y);
        } else {
            DataX[Offset] = x;
            DataY[Offset] = y;
            Offset = (Offset + 1) % MaxSize;
        }
    }

    int Size() const { return static_cast<int>(DataX.size()); }

    bool Empty() const { return DataX.empty(); }

    void Erase() {
        DataX.clear();
        DataY.clear();
        Offset = 0;
    }

    /// Return the latest Y value (or 0 if empty).
    float Back() const {
        if (DataY.empty()) return 0.0f;
        int idx = (Offset == 0) ? static_cast<int>(DataY.size()) - 1
                                : Offset - 1;
        return DataY[idx];
    }

    /// Return the max Y value for points with X >= xMin.
    float MaxYInWindow(float xMin) const {
        float mx = 0.0f;
        for (int i = 0; i < Size(); ++i) {
            if (DataX[i] >= xMin && DataY[i] > mx)
                mx = DataY[i];
        }
        return mx;
    }
};
