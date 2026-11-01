// ======================================================================== //
// Copyright 2018-2026 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "GLFWBaseViewer.h"
#include "ColorMaps.h"

namespace owl {
  namespace viewer {
                
    struct XFEditor {
      XFEditor();
      /* the imgui draw function for this widget */
      bool run_ui();

      /*! set a completely new color map, including both color and
          alpha (eg, when loading one from file */
      void setColorAndAlpha(const vec4f *cm, size_t cmSize);
      /*! set only the color scheme, but keep existing alpha values */
      void setColorOnly(const vec4f *cm, size_t cmSize);
      void changeColorScheme(int libraryColorMapID);
                       
      ColorMap getColorMap() 
      { cmUpdated = false; return colorMap; }
      
      // gets set to true every time the color map gets modified; gets
      // set to false every time it is retried via getColorMap();
      bool cmUpdated = true;
      
      static ColorMapLibrary colorMapLibrary;
    private:
      void mouseDragEnd();
      void mouseDrag(vec2f newPos, int button);
      
      // the position where the last drag started
      vec2f lastDragPos = -1.f;
      ColorMap colorMap;
      int colorMapID = 0;

      /*! re-render the uiTexture from current colorMap */
      void updateTexture();
      /*! a opengl texture used for drawing the the current color map */
      GLuint uiTexture = (GLuint)-1;
    };
  
  }
}
