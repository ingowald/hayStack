// SPDX-FileCopyrightText: Copyright (c) 2023++ Ingo Wald
// SPDX-License-Identifier: Apache-2.0

#include "hayStack/loader/SpheresFromFile.h"
#include "hayStack/loader/ColorMapper.h"

namespace hs {
  namespace loader {
    extern bool verbose;
  
    SpheresFromFile::SpheresFromFile(const ResourceSpecifier &data,
                                     int thisPartID,
                                     float defaultRadius)
      : data(data),
        fileSize(getFileSize(data.where)),
        thisPartID(thisPartID),
        radius(data.get_float("radius",defaultRadius))
    {}

    std::string SpheresFromFile::toString() 
    {
      return "Spheres{fileName="+data.where
        +", part "+std::to_string(thisPartID)+" of "
        + std::to_string(data.numParts)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }

  
    void SpheresFromFile::create(DataLoader *loader,
                                 const ResourceSpecifier &dataURL)
    {
      for (int i=0;i<dataURL.numParts;i++)
        loader->addContent(new SpheresFromFile(dataURL,i,
                                               loader->defaultRadius));
    }
    
    size_t SpheresFromFile::projectedSize() 
    {

      /* number of scalar values for each sphere, first three are
         always x,y,z position */
      int stride = data.get_int("stride",3);
      
      /* index of scalar to be used for color mapping; '-1' means 'no
         color mapping' */
      int sidx   = data.get_int("sidx",-1);
      assert(sidx < stride);

      /* index of scalar to be used for storing radius; '-1' means 'no
         radius' */
      int ridx   = data.get_int("ridx",-1);

      /* index of rgb 'c'olor to be used for color mapping; '-1' means 'no
         color in particle' */
      int cidx   = data.get_int("cidx",-1);

      /*! old-time format description */
      std::string format = data.get("format","");
      if (format == "xyz")    { stride=3; ridx=-1; sidx=-1; cidx=-1; }
      if (format == "xyzr")   { stride=4; ridx= 3; sidx=-1; cidx=-1; }
      if (format == "xyzf")   { stride=4; ridx=-1; sidx= 3; cidx=-1; }
      if (format == "xyzrgb") { stride=4; ridx= 3; sidx=-1; cidx= 3; }

      ColorMapper colorMapper(data);
      if (sidx >= 0 && !colorMapper.valid())
        throw std::runtime_error
          ("spheres: requested scalar mapping, but no 'map' range specified");

      size_t sizeOfSphere = stride * sizeof(float);
      size_t skipBytes = data.get_size("skip",0);
      size_t numSpheresInFile = (fileSize-skipBytes) / sizeOfSphere;
      // ------------------------------------------------------------------
      // compute *full* [begin,end] range of all particles in the file
      // that *all* parts of this loader together are supposed to read
      // ------------------------------------------------------------------
      size_t begin = data.get_size("begin",0);
      size_t count = std::min(data.get_size("count",0),
                              numSpheresInFile-begin);
      int64_t end = std::min(numSpheresInFile,
                             data.get_size("end",
                                           count
                                           ? begin+count
                                           : numSpheresInFile));
      PING;
      return (end-begin)*/* guesstimate for mem cost of sphere...*/50;
      // return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts);
    }



    inline float saturate(float f) { return min(1.f,max(0.f,f)); }
  
    inline vec3f hue_to_rgb(float hue)
    {
      float s = saturate( hue ) * 6.0f;
      float r = saturate( fabsf(s - 3.f) - 1.0f );
      float g = saturate( 2.0f - fabsf(s - 2.0f) );
      float b = saturate( 2.0f - fabsf(s - 4.0f) );
      return vec3f(r, g, b); 
    }
    
    inline vec3f temperature_to_rgb(float t)
    {
      float K = 4.0f / 6.0f;
      float h = K - K * t;
      float v = .5f + 0.5f * t;    return v * hue_to_rgb(h);
    }

    inline vec3f lerp3f(float f, const std::vector<vec4f> &xf)
    {
      int N = xf.size();
      assert(!xf.empty());
      f = saturate(f);
      if (N == 1) return (const vec3f &)xf[0];

      float Nf = f * (N-1);
      int i0 = std::min(N-2,int(Nf));
      float ff = Nf - i0;
      vec4f v = (1.f-ff)*xf[i0] + f*xf[i0+1];
      return (const vec3f&)v;
    }

    void   SpheresFromFile::executeLoad(OnePartition &dataGroup) 
    {
      SphereSet::SP spheres = SphereSet::create();
      // in dataloader.cpp, in case we need to add more material params:
      spheres->material = parseAnariMaterial(data);

      /* number of scalar values for each sphere, first three are
         always x,y,z position */
      int stride = data.get_int("stride",3);
      
      /* index of scalar to be used for color mapping; '-1' means 'no
         color mapping' */
      int sidx   = data.get_int("sidx",-1);
      assert(sidx < stride);

      /* index of scalar to be used for storing radius; '-1' means 'no
         radius' */
      int ridx   = data.get_int("ridx",-1);

      /* index of rgb 'c'olor to be used for color mapping; '-1' means 'no
         color in particle' */
      int cidx   = data.get_int("cidx",-1);

      /*! old-time format description */
      std::string format = data.get("format","");
      if (format == "xyz")    { stride=3; ridx=-1; sidx=-1; cidx=-1; }
      if (format == "xyzr")   { stride=4; ridx= 3; sidx=-1; cidx=-1; }
      if (format == "xyzf")   { stride=4; ridx=-1; sidx= 3; cidx=-1; }
      if (format == "xyzrgb") { stride=4; ridx= 3; sidx=-1; cidx= 3; }

      ColorMapper colorMapper(data);
      if (sidx >= 0 && !colorMapper.valid())
        throw std::runtime_error
          ("spheres: requested scalar mapping, but no 'map' range specified");

      size_t sizeOfSphere = stride * sizeof(float);
      size_t skipBytes = data.get_size("skip",0);
      size_t numSpheresInFile = (fileSize-skipBytes) / sizeOfSphere;
      // ------------------------------------------------------------------
      // compute *full* [begin,end] range of all particles in the file
      // that *all* parts of this loader together are supposed to read
      // ------------------------------------------------------------------
      size_t begin = data.get_size("begin",0);
      size_t count = std::min(data.get_size("count",0),
                              numSpheresInFile-begin);
      int64_t end = std::min(numSpheresInFile,
                             data.get_size("end",
                                           count
                                           ? begin+count
                                           : numSpheresInFile));

      // ------------------------------------------------------------------
      // now compute *this* part's range of particles to read
      // ------------------------------------------------------------------
      size_t numSpheresToLoad = end-begin;
      size_t my_begin
        = begin//data.get_size("begin",0)
        + (numSpheresToLoad * (thisPartID+0)) / data.numParts;
      size_t my_end
        = begin//data.get_size("end",0)
        + (numSpheresToLoad * (thisPartID+1)) / data.numParts;

      PING; PRINT(numSpheresToLoad);
      
      // ------------------------------------------------------------------
      // now: read ...
      // ------------------------------------------------------------------
      FILE *file = fopen(data.where.c_str(),"rb");
      fseek(file,skipBytes+my_begin*sizeOfSphere,SEEK_SET);

      PRINT(stride);
      std::vector<float> scalars(stride);
      range1f inputRange;
      for (size_t particleID=my_begin;particleID<my_end;particleID++) {
        int rc = fread((char*)scalars.data(),scalars.size()*sizeof(float),1,file);
        assert(rc);
        vec3f pos(scalars[0],scalars[1],scalars[2]);
        spheres->origins.push_back(pos);
        
        if (sidx >= 0) {
          inputRange.extend(scalars[sidx]);
          // just for informational purposes:
          spheres->colors.push_back
            (colorMapper.map3f(scalars[sidx]));
        } else if (cidx >= 0)
          spheres->colors.push_back
            (vec3f(scalars[cidx],scalars[cidx+1],scalars[cidx+2]));
        else
          /* no color mapping */
          ;
        
        if (ridx >= 0) {
          spheres->radii.push_back(scalars[ridx]);
          PING;
        } else
          /* no radius in particle */
          ;
      }
      spheres->radius = radius;
      dataGroup.sphereSets.push_back(spheres);
    }
    
    VMDSpheres::VMDSpheres(const ResourceSpecifier &data,
                           int thisPartID)
      : data(data),
        fileSize(getFileSize(data.where)),
        thisPartID(thisPartID)
    {}
    
    void VMDSpheres::create(DataLoader *loader,
                            const ResourceSpecifier &dataURL)
    {
      for (int i=0;i<dataURL.numParts;i++)
        loader->addContent(new VMDSpheres(dataURL,i));
    }
    
    size_t VMDSpheres::projectedSize()
    {
      return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts);
    }

    
    void   VMDSpheres::executeLoad(OnePartition &dataGroup)
    {
      std::ifstream in(data.where.c_str(),std::ios::binary);
      SphereSet::SP spheres = std::make_shared<SphereSet>();
      spheres->origins = withHeader::loadVectorOf<vec3f>(in,thisPartID,data.numParts);
      spheres->radii = withHeader::loadVectorOf<float>(in,thisPartID,data.numParts);
      spheres->colors = withHeader::loadVectorOf<vec3f>(in,thisPartID,data.numParts);
      spheres->material = mini::Matte::create();
      dataGroup.sphereSets.push_back(spheres);
    }
    
    std::string VMDSpheres::toString()
    {
      return "VMDSpheres{fileName="+data.where
        +", part "+std::to_string(thisPartID)+" of "
        + std::to_string(data.numParts)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    

  }
}
