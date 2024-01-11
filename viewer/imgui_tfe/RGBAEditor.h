// This file is distributed under the MIT license.
// See the LICENSE file for details.

#pragma once

// GL
#include <GL/gl.h>
// ours
#include "hayStack/HayMaker.h" // for math types
#include "ColorFormat.h"
#include "LookupTable.h"
#include "Histogram.h"

namespace hs
{
    class RGBAEditor
    {
    public:
       ~RGBAEditor();

        //! Set a user-provided LUT
        void setLookupTable(const LookupTable &lut);

        //! Get an updated LUT that is a copied of the user-provided one
        LookupTable &getUpdatedLookupTable();

        //! Optionally set a histogram that can be displayed instead of the LUT
        void setHistogram(const Histogram &histo);

        //! Set a zoom range to visually zoom into the LUT
        void setZoom(float min, float max);

        //! Indicates that the internal copy of the LUT has changed
        bool updated() const;

        //! Render with ImGui, open in new ImgGui window
        void show();

        //! Render with ImGui but w/o window
        void drawImmediate();

    private:
        // Local LUT copy
        LookupTable rgbaLookupTable_;

        // User-provided LUT
        LookupTable userLookupTable_;

        // Local histogram with normalized data (optional)
        Histogram normalizedHistogram_;

        // User-provided histrogram
        Histogram userHistogram_;

        // Zoom min set by user
        float zoomMin_ = 0.f;

        // Zoom max set by user
        float zoomMax_ = 1.f;

        // Flag indicating that texture needs to be regenerated
        bool lutChanged_ = false;

        // Flag indicating that texture needs to be regenerated
        bool histogramChanged_ = false;

        // RGB texture
        GLuint texture_ = GLuint(-1);

        // Drawing canvas size
        vec2i canvasSize_ = { 300, 150 };

        // Mouse state for drawing
        struct MouseEvent
        {
            enum Type { PassiveMotion, Motion, Press, Release };
            enum Button { Left, Middle, Right, None };

            vec2i pos = { 0, 0 };
            int button = None;
            Type type = Motion;
        };

        // The last mouse event
        MouseEvent lastEvent_;

        // Drawing in progress
        bool drawing_ = false;

        // Raster LUT to image and upload with OpenGL
        void rasterTexture();

        // Generate normalized from user-provided histogram
        void normalizeHistogram();

        // Generate mouse event when mouse hovered over rect
        MouseEvent generateMouseEvent();

        // Handle mouse event
        void handleMouseEvent(MouseEvent const& event);
    };

} // namespace hs
