#pragma once

namespace hs {

    class Histogram
    {
    public:
        Histogram() = default;

        Histogram(size_t numBins) : bins_(numBins)
        {}

        bool empty() const
        { return bins_.empty(); }

        size_t getNumBins() const
        { return bins_.size(); }

        const size_t *getBinCounts() const
        { return bins_.data(); }

        size_t *getBinCounts()
        { return bins_.data(); }

    private:
        std::vector<size_t> bins_;
    };

} // namespace hs
