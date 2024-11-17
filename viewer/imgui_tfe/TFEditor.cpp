#include "TFEditor.h"

#include <fstream>
#include <imgui.h>

#include "hayStack/HayMaker.h" // for math types
#include "ColorMaps.h"


namespace hs {
  // using namespace owl;
  // using owl::common::interval;
  using mini::common::interval;
  using mini::common::vec4f;

  TFEditor::TFEditor(){
    cmapNames = cmapLib.getNames();
    selectedCMapID = -1;
    selectCMap(0, false);

    range = {0.f, 1.f};
    relDomain = {0.f, 100.f};
    opacityScale = 100.f;

    firstFrame = true;
  }

  void TFEditor::drawImmediate() {
    cmapUpdated_ = firstFrame;
    opacityUpdated_ = firstFrame;
    rangeUpdated_ = firstFrame;

    if (firstFrame)
      firstFrame = false;

    vktTFE.drawImmediate();

    if (vktTFE.updated())
      cmapUpdated_ = true;

    mini::common::interval<float> curRange = range;
    ImGui::DragFloatRange2("Range", &curRange.lo, &curRange.hi,
                           max(curRange.diagonal()/100.f, 0.0001f));

    if (curRange != range) {
      rangeUpdated_ = true;
      range.lo = std::min(curRange.lo, curRange.hi);
      range.hi = std::max(curRange.lo, curRange.hi);
    }

    interval<float> curRelDomain = relDomain;
    ImGui::DragFloatRange2("Rel Domain", &curRelDomain.lo, &curRelDomain.hi, 1.f);

    if (curRelDomain != relDomain) {
      rangeUpdated_ = true;
      relDomain.lo = std::min(curRelDomain.lo, curRelDomain.hi);
      relDomain.hi = std::max(curRelDomain.lo, curRelDomain.hi);
    }

    float curOpacityScale = opacityScale;
    ImGui::DragFloat("Opacity", &curOpacityScale, 1.f);

    if (curOpacityScale != opacityScale) {
      opacityUpdated_ = true;
      opacityScale = curOpacityScale;
    }

    int curCMapID = selectedCMapID;

    if (ImGui::BeginCombo("CMap", cmapNames[curCMapID].c_str()))
    {
      for (int i = 0; i < cmapNames.size(); ++i)
      {
        bool isSelected = curCMapID == i;
        if (ImGui::Selectable(cmapNames[i].c_str(), isSelected))
            curCMapID = i;
        if (isSelected)
            ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();

      selectCMap(curCMapID, true);
    }

    if (ImGui::Button("Save"))
      saveToFile(saveFilename.c_str());
  }

  std::vector<vec4f> TFEditor::getColorMap(){
    const LookupTable &lut = vktTFE.getUpdatedLookupTable();
    if (!lut.empty()) {
      auto dims = lut.getDims();
      auto lutData = (float *)lut.getData();

      std::vector<vec4f> cmap;
      cmap.resize(dims.x);
      std::copy(&lutData[0], &lutData[dims.x*4], (float*)&cmap[0]);
      return cmap;
    }

    return {};
  }

  void TFEditor::selectCMap(int cmapID, bool keepAlpha){
    if (cmapID == selectedCMapID)
      return;

    selectedCMapID = cmapID;

    auto map = cmapLib.getMap(cmapID);//.resampledTo(numColors);

    vktLUT = LookupTable(map.size(),1,1,ColorFormat::RGBA32F);
    vktLUT.setData((vec4f*)map.data());
    vktTFE.setLookupTable(vktLUT);

    LookupTable &updatedLUT = vktTFE.getUpdatedLookupTable();
    if (!updatedLUT.empty()){
      auto rmap = map.resampledTo(updatedLUT.getDims().x);

      if (keepAlpha){
        auto currentCMap = getColorMap();

        if (rmap.size() != currentCMap.size())
            throw std::runtime_error("TFE error");

        for (int i=0; i<rmap.size(); ++i)
          rmap[i].w = currentCMap[i].w;
      }

      updatedLUT.setData((vec4f*)&rmap[0]);
    }

    cmapUpdated_ = true;
  }

  void TFEditor::loadFromFile(const char *fname) {
    std::ifstream xfFile(fname, std::ios::binary);

    if (!xfFile.good())
      throw std::runtime_error("Could not open TF");

    static const size_t xfFileFormatMagic = 0x1235abc000;
    size_t magic;
    xfFile.read((char*)&magic,sizeof(xfFileFormatMagic));
    if (magic != xfFileFormatMagic) {
      throw std::runtime_error("Not a valid TF file");
    }

    xfFile.read((char*)&opacityScale,sizeof(opacityScale));

    xfFile.read((char*)&range.lower,sizeof(range.lower));
    xfFile.read((char*)&range.upper,sizeof(range.upper));

    xfFile.read((char*)&relDomain,sizeof(relDomain));

    ColorMap colorMap;
    int numColorMapValues;

    xfFile.read((char*)&numColorMapValues,sizeof(numColorMapValues));
    colorMap.resize(numColorMapValues);
    xfFile.read((char*)colorMap.data(),colorMap.size()*sizeof(colorMap[0]));

    vktLUT = LookupTable(numColorMapValues,1,1,ColorFormat::RGBA32F);
    vktLUT.setData((vec4f*)colorMap.data());
    vktTFE.setLookupTable(vktLUT);

    auto updatedLUT = vktTFE.getUpdatedLookupTable();
    if (!updatedLUT.empty()) {
      auto rmap = colorMap.resampledTo(updatedLUT.getDims().x);
      updatedLUT.setData((vec4f*)&rmap[0]);
    }

    firstFrame = true;
  }

  void TFEditor::saveToFile(const char *fname) {
    std::ofstream ofs(fname, std::ios::binary);

    if (!ofs.good())
      std::cerr << "Cannot save tf at " << fname << "\n";

    static const size_t xfFileFormatMagic = 0x1235abc000;
    ofs.write((char*)&xfFileFormatMagic, sizeof(xfFileFormatMagic));

    ofs.write((char*)&opacityScale,sizeof(opacityScale));
    ofs.write((char*)&range.lower,sizeof(range.lower));
    ofs.write((char*)&range.upper,sizeof(range.upper));
    ofs.write((char*)&relDomain,sizeof(relDomain));

    const auto cmap = getColorMap();
    const int numColorMapValues = cmap.size();

    ofs.write((char*)&numColorMapValues, sizeof(numColorMapValues));
    ofs.write((char*)&cmap[0], numColorMapValues * sizeof(vec4f));

    ofs.close();

    std::cout << "TFE saved to " << fname << "\n";
  }
}
