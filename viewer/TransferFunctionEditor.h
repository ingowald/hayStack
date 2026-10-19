// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "owlViewer/OWLViewer.h"
#include "owlViewer/ColorMaps.h"

namespace hs {
  namespace viewer {

    struct TransferFunctionEditor {
      TransferFunctionEditor();

      bool runUI(bool showColorMapSelector = true);
      void setColorAndAlpha(const owl::common::vec4f *cm, size_t cmSize);
      void setColorOnly(const owl::common::vec4f *cm, size_t cmSize);
      void changeColorScheme(int libraryColorMapID);

      owl::viewer::ColorMap getColorMap()
      {
        colorMapUpdated = false;
        return colorMap;
      }

      bool colorMapUpdated = true;

    private:
      void mouseDragEnd();
      void mouseDrag(owl::common::vec2f newPos, int button);
      void updateTexture();

      static owl::viewer::ColorMapLibrary colorMapLibrary;
      owl::common::vec2f lastDragPos = -1.f;
      bool clickedOnItem = false;
      owl::viewer::ColorMap colorMap;
      int colorMapID = 0;
      GLuint uiTexture = (GLuint)-1;
    };

  }
}
