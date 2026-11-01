// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ui/GLFWBaseViewer.h"
#include "hayStack/ColorMap.h"

namespace hs {
  namespace viewer {

    struct TransferFunctionEditor {
      TransferFunctionEditor();

      bool runUI(bool showColorMapSelector = true);
      void setColorAndAlpha(const vec4f *cm, size_t cmSize);
      void setColorOnly(const vec4f *cm, size_t cmSize);
      void changeColorScheme(int libraryColorMapID);
      
      ColorMap getColorMap()
      {
        colorMapUpdated = false;
        return colorMap;
      }

      bool colorMapUpdated = true;

    private:
      void mouseDragEnd();
      void mouseDrag(vec2f newPos, int button);
      void updateTexture();

      // static owl::viewer::ColorMapLibrary colorMapLibrary;
      vec2f lastDragPos = -1.f;
      bool clickedOnItem = false;
      ColorMap colorMap;
      int colorMapID = 0;
      GLuint uiTexture = (GLuint)-1;
    };

  }
}
