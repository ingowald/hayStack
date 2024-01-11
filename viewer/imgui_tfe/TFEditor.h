#pragma once

#include <vector>
#include "owl/common.h"
#include "RGBAEditor.h"
#include "ColorMaps.h"

namespace hs {
  class TFEditor
  {
  public:
    TFEditor();

    void drawImmediate();

    bool cmapUpdated() const { return cmapUpdated_; }
    bool rangeUpdated() const { return rangeUpdated_; }
    bool opacityUpdated() const { return opacityUpdated_; }

    void downdate() { cmapUpdated_ = rangeUpdated_ = opacityUpdated_ = false; }

    std::vector<owl::vec4f> getColorMap();
    owl::interval<float> getRange() const { return range; }
    owl::interval<float> getRelDomain() const { return relDomain; }
    float getOpacityScale() const {return opacityScale;}
    void setRange(owl::interval<float> r) { range = r; }

    void loadFromFile(const char* fname);
    void saveToFile(const char* fname);

    std::string saveFilename = "owlDVR.xf";
  private:
    LookupTable vktLUT;
    RGBAEditor vktTFE;

    bool firstFrame;
    bool cmapUpdated_;
    bool rangeUpdated_;
    bool opacityUpdated_;

    owl::interval<float> range;
    owl::interval<float> relDomain;
    float opacityScale;

    ColorMapLibrary cmapLib;
    std::vector<std::string> cmapNames;

    int selectedCMapID;

    void selectCMap(int cmapID, bool keepAlpha);
  };

} // namespace hs
