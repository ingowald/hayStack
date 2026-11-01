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

#include "owlViewer/XFEditor.h"

namespace owl {
  namespace viewer {

  inline  uint32_t make_8bit(const float f)
  {
    return min(255,max(0,int(f*256.f)));
  }

  inline  uint32_t make_rgba(const vec3f color)
  {
    return
      (make_8bit(color.x) << 0) +
      (make_8bit(color.y) << 8) +
      (make_8bit(color.z) << 16) +
      (0xffU << 24);
  }
  inline  uint32_t make_rgba(const vec4f color)
  {
    return
      (make_8bit(color.x) << 0) +
      (make_8bit(color.y) << 8) +
      (make_8bit(color.z) << 16) +
      (make_8bit(color.w) << 24);
  }
    
    vec2f cvt(ImVec2 v)
    { return { v.x,v.y }; }
    ImVec2 cvt(vec2f v)
    { return { v.x,v.y }; }

    ColorMapLibrary XFEditor::colorMapLibrary;

    XFEditor::XFEditor()
    {
      colorMap = colorMapLibrary.getMap(0);
      updateTexture();
    }

    void XFEditor::mouseDragEnd()
    { lastDragPos = -1.f; }
    
    void XFEditor::mouseDrag(vec2f newPos, int button)
    {
      if (newPos == lastDragPos)
        return;

      if (lastDragPos.x == -1) {
        lastDragPos = newPos;
      }

      int w = colorMap.size()-1;
      int x0 = int(lastDragPos.x*(colorMap.size()-1)+.5f);
      int x1 = int(newPos.x*(colorMap.size()-1)+.5f);
      x0 = std::min(w,std::max(0,x0));
      x1 = std::min(w,std::max(0,x1));

      if (x0 == x1) {
        if (button == 0)
          colorMap[x0].w = newPos.y;
        else
          colorMap[x0].w = (newPos.y > .35f)?1.f:0.f;
      } else {
        int xx0 = std::min(x0,x1);
        int xx1 = std::max(x0,x1);
        for (int x=xx0;x<=xx1;x++) {
          float f = (x-x0)/float(x1-x0);
          float w = (1.f-f)*lastDragPos.y+f*newPos.y;
          if (button != 0)
            w = w > .35f?1.f:0.f;
          colorMap[x].w = w;
        }
      }
      updateTexture();
      cmUpdated = true;
      lastDragPos = newPos;
    }

    void XFEditor::updateTexture()
    {
      GLint prev_tex_2d = 0;
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_2d);      
      if (uiTexture == (GLuint)-1) {
        glGenTextures(1, &uiTexture);
        glBindTexture(GL_TEXTURE_2D, uiTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      }
      glBindTexture(GL_TEXTURE_2D, uiTexture);
      int width = colorMap.size();
      int height = width;
      std::vector<uint32_t> pixels(width*height);
      for (int x=0;x<width;x++) {
        int h = std::min(int(height*colorMap[x].w+.5f),height);
        vec4f color = colorMap[x]; color.w = 1.f;
        uint32_t rgba = make_rgba(color);
        for (int y=0;y<h;y++)
          pixels[x+(height-1-y)*width] = rgba;
        for (int y=h;y<height;y++)
          pixels[x+(height-1-y)*width] = 0xff444444;//0xffffffff;
      }
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGB8,
                   width,height,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   pixels.data());
      glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
    }

    void XFEditor::changeColorScheme(int newID)
    {
      auto newMap = colorMapLibrary.getMap(newID);
      if (newMap.size() != colorMap.size())
        newMap = newMap.resampledTo(colorMap.size());
      for (int i=0;i<colorMap.size();i++)
        newMap[i].w = colorMap[i].w;
      colorMap = newMap;
      colorMapID = newID;
      updateTexture();
      cmUpdated = true;
    }
    
    /*! set a completely new color map, including both color and
      alpha (eg, when loading one from file */
    void XFEditor::setColorAndAlpha(const vec4f *cm, size_t cmSize)
    {
      if (colorMap.size() != cmSize) colorMap.resize(cmSize);
      for (int i=0;i<cmSize;i++)
        colorMap[i] = cm[i];
      updateTexture();
      cmUpdated = true;
    }
    
    /*! set only the color scheme, but keep existing alpha values */
    void XFEditor::setColorOnly(const vec4f *cm, size_t cmSize)
    {
      if (cmSize != colorMap.size()) 
        colorMap = colorMap.resampledTo(cmSize);
      for (int i=0;i<cmSize;i++) 
        (vec3f&)colorMap[i] = (const vec3f&)cm[i];
      updateTexture();
      cmUpdated = true;
    }
    
    bool XFEditor::run_ui()
    {
      const ImGuiIO &io = ImGui::GetIO();
    
      ImGui::Text("TransferFct (draw w/ left/right mouse btn");
      // ImGui::TextWrapped("Lft btn to draw opacity");
      // ImGui::TextWrapped("Rgt btn to set 0/1 opacity");

      const char *colorMapName
        = (colorMapID == -1)
        ? "custom"
        : colorMapLibrary.getMapName(colorMapID).c_str();
      if (ImGui::BeginCombo("Colormap", colorMapName)) {
        std::vector<std::string> cmNames = colorMapLibrary.getNames();
        for (size_t i = 0; i < cmNames.size(); ++i) {
          if (ImGui::Selectable(cmNames[i].c_str(), colorMapID == i)) {
            changeColorScheme(i);
          }
        }
        ImGui::EndCombo();
      }
      ImVec2 savedPos = ImGui::GetCursorStartPos();

      vec2f canvas_size = cvt(ImGui::GetContentRegionAvail());
      vec2f canvas_pos = cvt(ImGui::GetCursorScreenPos());
#if 1
      // canvas_size.y = canvas_size.x * 2 / 3;
      canvas_size.y = std::min(canvas_size.y,
                               canvas_size.x / 2);
#endif
      canvas_size.y -= 20;

      const float point_radius = 10.f;

      ImDrawList *draw_list = ImGui::GetWindowDrawList();
#if 0
      draw_list->PushClipRect(cvt(canvas_pos),
                              cvt(canvas_pos + canvas_size));
#endif

      const ImVec2 view_scale(canvas_size.x, -canvas_size.y);
      const ImVec2 view_offset(canvas_pos.x, canvas_pos.y + canvas_size.y);

#if 1
      draw_list->AddImage(reinterpret_cast<void *>(uiTexture),
                          cvt(canvas_pos),
                          cvt(canvas_pos + canvas_size));
#endif
      draw_list->AddRect(cvt(canvas_pos),
                         cvt(canvas_pos + canvas_size),
                         ImColor(180, 180, 180, 255));
      ImGui::InvisibleButton("tfn_canvas", cvt(canvas_size));
    
      static bool clicked_on_item = false;
      if (!io.MouseDown[0] && !io.MouseDown[1]) {
        clicked_on_item = false;
      }
      if (ImGui::IsItemHovered() && (io.MouseDown[0] || io.MouseDown[1])) {
        clicked_on_item = true;
      }

      ImVec2 bbmin = ImGui::GetItemRectMin();
      ImVec2 bbmax = ImGui::GetItemRectMax();
      ImVec2 clipped_mouse_pos
        = ImVec2(std::min(std::max(io.MousePos.x, bbmin.x), bbmax.x),
                 std::min(std::max(io.MousePos.y, bbmin.y), bbmax.y));

      int selected_point;
      if (clicked_on_item) {
        vec2f mouse_pos = (cvt(clipped_mouse_pos) - cvt(view_offset))
          / cvt(view_scale);
        mouse_pos.x = clamp(mouse_pos.x, 0.f, 1.f);
        mouse_pos.y = clamp(mouse_pos.y, 0.f, 1.f);

        if (io.MouseDown[0]) {
          mouseDrag(mouse_pos,0);
        } else if (ImGui::IsMouseDown(1)) {
          mouseDrag(mouse_pos,1);
        } else if (ImGui::IsMouseDown(2)) {
          mouseDrag(mouse_pos,2);
        } else {
          selected_point = -1;
          mouseDragEnd();
        }
      } else {
        //      selected_point = -1;
        mouseDragEnd();
      }

      // printf("saved %lf %lf\n",savedPos.x,savedPos.y);
      // printf("canvassize %lf %lf\n",canvas_size.x,canvas_size.y);
      savedPos.y += (canvas_size.y+30);
      // canvas_pos.y += canvas_size.y;//100;
      // printf("saved %lf %lf\n",savedPos.x,savedPos.y);
      // ImGui::SetCursorPos(savedPos);
      // ImGui::SetCursorScreenPos(cvt(canvas_pos));
      // PRINT(canvas_pos);
      ImGui::Dummy({100,10});
      // ImGui::Text("foo");
      return cmUpdated;
    }
    
  }
}


