#pragma once

// std
#include <vector>
// ours
#include "hayStack/HayMaker.h" // for math types
#include "ColorFormat.h"

namespace hs {

    class LookupTable
    {
    public:
        LookupTable() = default;

        LookupTable(int w, int h, int d, ColorFormat format)
            : rgba_(w*size_t(h)*d)
            , dims_({w,h,d})
            , format_(format)
        {}

        void setData(const vec4f *data)
        { memcpy(rgba_.data(), data, sizeof(vec4f)*rgba_.size()); }

        const vec4f *getData() const
        { return rgba_.data(); }

        vec3i getDims() const
        { return dims_; }

        bool empty() const
        { return rgba_.empty(); }

        ColorFormat getColorFormat() const
        { return format_; }

    private:
        std::vector<vec4f> rgba_;
        vec3i dims_;
        ColorFormat format_;
    };

} // namespace hs
