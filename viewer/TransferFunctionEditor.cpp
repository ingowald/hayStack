// SPDX-FileCopyrightText: Copyright (c) 2018-2026 Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "TransferFunctionEditor.h"

namespace hs {
  namespace viewer {

    using namespace owl::common;
    using owl::viewer::ColorMapLibrary;

    namespace {
      uint32_t make8Bit(float value)
      {
        return min(255, max(0, int(value * 256.f)));
      }

      uint32_t makeRGBA(const vec4f &color)
      {
        return (make8Bit(color.x) << 0)
          + (make8Bit(color.y) << 8)
          + (make8Bit(color.z) << 16)
          + (make8Bit(color.w) << 24);
      }

      vec2f cvt(ImVec2 value)
      {
        return { value.x, value.y };
      }

      ImVec2 cvt(vec2f value)
      {
        return { value.x, value.y };
      }
    }

    ColorMapLibrary TransferFunctionEditor::colorMapLibrary;

    TransferFunctionEditor::TransferFunctionEditor()
    {
      colorMap = colorMapLibrary.getMap(0);
      updateTexture();
    }

    void TransferFunctionEditor::mouseDragEnd()
    {
      lastDragPos = -1.f;
    }

    void TransferFunctionEditor::mouseDrag(vec2f newPos, int button)
    {
      if (newPos == lastDragPos)
        return;
      if (lastDragPos.x == -1.f)
        lastDragPos = newPos;

      const int width = int(colorMap.size()) - 1;
      int x0 = int(lastDragPos.x * width + .5f);
      int x1 = int(newPos.x * width + .5f);
      x0 = std::min(width, std::max(0, x0));
      x1 = std::min(width, std::max(0, x1));

      if (x0 == x1) {
        colorMap[x0].w = button == 0
          ? newPos.y
          : (newPos.y > .35f ? 1.f : 0.f);
      } else {
        const int begin = std::min(x0, x1);
        const int end = std::max(x0, x1);
        for (int x = begin; x <= end; ++x) {
          const float fraction = (x - x0) / float(x1 - x0);
          float opacity
            = (1.f - fraction) * lastDragPos.y + fraction * newPos.y;
          if (button != 0)
            opacity = opacity > .35f ? 1.f : 0.f;
          colorMap[x].w = opacity;
        }
      }

      updateTexture();
      colorMapUpdated = true;
      lastDragPos = newPos;
    }

    void TransferFunctionEditor::updateTexture()
    {
      GLint previousTexture = 0;
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
      if (uiTexture == (GLuint)-1) {
        glGenTextures(1, &uiTexture);
        glBindTexture(GL_TEXTURE_2D, uiTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      }

      glBindTexture(GL_TEXTURE_2D, uiTexture);
      const int width = int(colorMap.size());
      const int height = width;
      std::vector<uint32_t> pixels(width * height);
      for (int x = 0; x < width; ++x) {
        const int opacityHeight
          = std::min(int(height * colorMap[x].w + .5f), height);
        vec4f color = colorMap[x];
        color.w = 1.f;
        const uint32_t rgba = makeRGBA(color);
        for (int y = 0; y < opacityHeight; ++y)
          pixels[x + (height - 1 - y) * width] = rgba;
        for (int y = opacityHeight; y < height; ++y)
          pixels[x + (height - 1 - y) * width] = 0xff444444;
      }

      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGB8,
                   width,
                   height,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   pixels.data());
      glBindTexture(GL_TEXTURE_2D, previousTexture);
    }

    void TransferFunctionEditor::changeColorScheme(int newID)
    {
      auto newMap = colorMapLibrary.getMap(newID);
      if (newMap.size() != colorMap.size())
        newMap = newMap.resampledTo(colorMap.size());
      for (int i = 0; i < int(colorMap.size()); ++i)
        newMap[i].w = colorMap[i].w;
      colorMap = newMap;
      colorMapID = newID;
      updateTexture();
      colorMapUpdated = true;
    }

    void TransferFunctionEditor::setColorAndAlpha(const vec4f *cm,
                                                   size_t cmSize)
    {
      if (colorMap.size() != cmSize)
        colorMap.resize(cmSize);
      for (size_t i = 0; i < cmSize; ++i)
        colorMap[i] = cm[i];
      updateTexture();
      colorMapUpdated = true;
    }

    void TransferFunctionEditor::setColorOnly(const vec4f *cm, size_t cmSize)
    {
      if (cmSize != colorMap.size())
        colorMap = colorMap.resampledTo(cmSize);
      for (size_t i = 0; i < cmSize; ++i)
        (vec3f &)colorMap[i] = (const vec3f &)cm[i];
      updateTexture();
      colorMapUpdated = true;
    }

    bool TransferFunctionEditor::runUI(bool showColorMapSelector)
    {
      const ImGuiIO &io = ImGui::GetIO();

      ImGui::Text("TransferFct (draw w/ left/right mouse btn");
      if (showColorMapSelector) {
        const char *colorMapName = colorMapID == -1
          ? "custom"
          : colorMapLibrary.getMapName(colorMapID).c_str();
        if (ImGui::BeginCombo("Colormap", colorMapName)) {
          const std::vector<std::string> mapNames = colorMapLibrary.getNames();
          for (size_t i = 0; i < mapNames.size(); ++i) {
            if (ImGui::Selectable(mapNames[i].c_str(), colorMapID == int(i)))
              changeColorScheme(int(i));
          }
          ImGui::EndCombo();
        }
      }

      ImVec2 savedPos = ImGui::GetCursorStartPos();
      vec2f canvasSize = cvt(ImGui::GetContentRegionAvail());
      const vec2f canvasPos = cvt(ImGui::GetCursorScreenPos());
      canvasSize.y = std::min(canvasSize.y, canvasSize.x / 2.f) - 20.f;

      ImDrawList *drawList = ImGui::GetWindowDrawList();
      const ImVec2 viewScale(canvasSize.x, -canvasSize.y);
      const ImVec2 viewOffset(canvasPos.x, canvasPos.y + canvasSize.y);
      drawList->AddImage(reinterpret_cast<void *>(uiTexture),
                         cvt(canvasPos),
                         cvt(canvasPos + canvasSize));
      drawList->AddRect(cvt(canvasPos),
                        cvt(canvasPos + canvasSize),
                        ImColor(180, 180, 180, 255));
      ImGui::InvisibleButton("tfn_canvas", cvt(canvasSize));

      if (!io.MouseDown[0] && !io.MouseDown[1])
        clickedOnItem = false;
      if (ImGui::IsItemHovered() && (io.MouseDown[0] || io.MouseDown[1]))
        clickedOnItem = true;

      const ImVec2 minimum = ImGui::GetItemRectMin();
      const ImVec2 maximum = ImGui::GetItemRectMax();
      const ImVec2 clippedMousePosition(
        std::min(std::max(io.MousePos.x, minimum.x), maximum.x),
        std::min(std::max(io.MousePos.y, minimum.y), maximum.y));

      if (clickedOnItem) {
        vec2f mousePosition
          = (cvt(clippedMousePosition) - cvt(viewOffset)) / cvt(viewScale);
        mousePosition.x = clamp(mousePosition.x, 0.f, 1.f);
        mousePosition.y = clamp(mousePosition.y, 0.f, 1.f);
        if (io.MouseDown[0])
          mouseDrag(mousePosition, 0);
        else if (ImGui::IsMouseDown(1))
          mouseDrag(mousePosition, 1);
        else
          mouseDragEnd();
      } else {
        mouseDragEnd();
      }

      savedPos.y += canvasSize.y + 30.f;
      ImGui::Dummy({ 100, 10 });
      return colorMapUpdated;
    }

  }
}
