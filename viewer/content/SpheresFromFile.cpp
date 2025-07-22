// ======================================================================== //
// Copyright 2022-2025 Ingo Wald                                            //
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

#include "viewer/DataLoader.h"
#include "viewer/content/SpheresFromFile.h"

namespace hs {
  namespace content {
  
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
    { return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts); }



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
 
  
    void   SpheresFromFile::executeLoad(DataRank &dataGroup, bool verbose) 
    {
      SphereSet::SP spheres = SphereSet::create();
#if 1
      mini::Matte::SP mat =
        std::make_shared<mini::Matte>();;
      mat->reflectance = .5f;
#else
      mini::DisneyMaterial::SP mat =
        std::make_shared<mini::DisneyMaterial>();;
      mat->metallic = 0.f;
      mat->ior = 1.f;
#endif
      spheres->material = mat;

      spheres->radius = radius;
      FILE *file = fopen(data.where.c_str(),"rb");
      assert(file);
      const std::string format = data.get("format","<not set>");
      size_t skipBytes = data.get_size("skipBytes",0);
      size_t sizeOfSphere = 0;
      if (format == "xyz") {
        sizeOfSphere = sizeof(vec3f);
      } else if (format == "XYZ") {
        sizeOfSphere = sizeof(vec3d);
      } else if (format == "xyzf" ||
                 format == "xyzi") {
        sizeOfSphere = sizeof(vec4f);
      } else if (format == "pcr" /*! pr = position color radius :-/ */) {
        sizeOfSphere = 7*sizeof(float);
      } else if (format == "dlaf") {
        size_t numSpheresInFile;
        int rc;
        rc = fread(&numSpheresInFile,sizeof(numSpheresInFile),1,file); assert(rc);
        float radius;
        rc = fread(&radius,sizeof(radius),1,file); assert(rc);
        spheres->radius = radius;
        float maxDistance;
        rc = fread(&maxDistance,sizeof(maxDistance),1,file); assert(rc);
        size_t begin = data.get_size("begin",0);
        size_t count = std::min(data.get_size("count",0),
                                numSpheresInFile-begin);
        int64_t end = std::min(numSpheresInFile,
                               data.get_size("end",
                                             count
                                             ? begin+count
                                             : numSpheresInFile));
        int64_t numSpheresToLoad
          = end-begin;
        // std::min((int64_t)data.get_size("count",numSpheresInFile),
        //              (int64_t)(numSpheresInFile-data.get_size("begin",0)));
        size_t my_begin
          = begin
          + (numSpheresToLoad * (thisPartID+0)) / data.numParts;
        size_t my_end
          = begin
          + (numSpheresToLoad * (thisPartID+1)) / data.numParts;
        size_t my_count = my_end - my_begin;
        spheres->origins.resize(my_count);
        size_t skipBytes
          = sizeof(size_t)
          + sizeof(float)
          + sizeof(float)
          + 6*sizeof(float)
          + my_begin*sizeof(vec3f);
        fseek(file,skipBytes,SEEK_SET);
        rc = fread(spheres->origins.data(),my_count,sizeof(vec3f),file); assert(rc);

        skipBytes
          = sizeof(size_t)
          + sizeof(float)
          + sizeof(float)
          + 6*sizeof(float)
          + numSpheresInFile*sizeof(vec3f)
          + my_begin*sizeof(float);
        fseek(file,skipBytes,SEEK_SET);
        std::vector<float> distances(my_count);
        rc = fread(distances.data(),sizeof(float),my_count,file); assert(rc);

        size_t skip = data.get_size("skip",0);
        if (skip) {
          std::vector<float> skipped_distances;
          std::vector<vec3f> skipped_origins;
          for (size_t i=0;i<spheres->origins.size();i++) {
            size_t idx = my_begin + i;
            if (idx % skip != 0) continue;
            skipped_distances.push_back(distances[i]);
            skipped_origins.push_back(spheres->origins[i]);
          }
          distances = skipped_distances;
          spheres->origins = skipped_origins;
        }

        
        spheres->colors.clear();
        for (auto d : distances) {
          vec3f color = temperature_to_rgb(powf(d/maxDistance,1.f));
          spheres->colors.push_back(color);
        }

        fclose(file);
        
        dataGroup.sphereSets.push_back(spheres);
        return;
      } else
        throw std::runtime_error("unsupported format '"+format+"'");

      int64_t numSpheresInFile = (fileSize-skipBytes) / sizeOfSphere;
      int64_t numSpheresToLoad
        = std::min((int64_t)data.get_size("count",numSpheresInFile),
                   (int64_t)(numSpheresInFile-data.get_size("begin",0)));
      if (numSpheresToLoad <= 0)
        throw std::runtime_error("no spheres to load for these begin/count values!?");
  
      size_t my_begin
        = data.get_size("begin",0)
        + (numSpheresToLoad * (thisPartID+0)) / data.numParts;
      size_t my_end
        = data.get_size("end",0)
        + (numSpheresToLoad * (thisPartID+1)) / data.numParts;
      size_t my_count = my_end - my_begin;
      // spheres->origins.resize(my_count);
      fseek(file,skipBytes+my_begin*sizeOfSphere,SEEK_SET);
      range1f scalarRange;
      if (format =="xyzf") {
        vec2f colorMapRange = data.get("map",vec2f(0.f,0.f));
        vec4f v;
        for (size_t i=0;i<my_count;i++) {
          int rc = fread((char*)&v,sizeof(v),1,file);
          assert(rc);
          spheres->origins.push_back(vec3f{v.x,v.y,v.z});

          if (colorMapRange.x != colorMapRange.y) {
            float f = v.w;
            scalarRange.extend(f);
            f = saturate((f-colorMapRange.x)/(colorMapRange.y-colorMapRange.x));
            vec3f color = .6f*temperature_to_rgb(f);
            spheres->colors.push_back(color);
          }
        }
      } else if (format =="xyzi") {
        struct
        {
          vec3f pos;
          uint32_t type;
        } v;
        for (size_t i=0;i<my_count;i++) {
          int rc = fread((char*)&v,sizeof(v),1,file);
          assert(rc);
          vec3f baseColors[] = {
            { 0,0,1 },
            { 0,1,0 },
            { 1,0,0 },
            { 1,1,0 },
            { 1,0,1 },
            { 0,1,1 },
          };
          spheres->origins.push_back(vec3f{v.pos.x,v.pos.y,v.pos.z});
          vec3f color = v.type < 6
            ? baseColors[v.type]
            : randomColor(13+(int)v.type);
          spheres->colors.push_back(.7f*color);
        }
      } else if (format =="XYZ") {
        struct
        {
          vec3d pos;
        } v;
        for (size_t i=0;i<my_count;i++) {
          int rc = fread((char*)&v,sizeof(v),1,file);
          spheres->origins.push_back(vec3f(v.pos));
        }
      } else if (format == "xyz") {
        vec3f v;
        box3f bounds;
        for (size_t i=0;i<my_count;i++) {
          int rc = fread((char*)&v,sizeof(v),1,file);
          assert(rc);
          spheres->origins.push_back(v);
          bounds.extend(v);
        }
        std::cout << "read " << my_count << " spheres w/ bounds " << bounds << std::endl;
      } else if (format == "pcr") {
        vec3f pos;
        vec3f col;
        float rad;
        for (size_t i=0;i<my_count;i++) {
          {
            int rc = fread((char*)&pos,sizeof(pos),1,file);
            assert(rc);
          }
          {
            int rc = fread((char*)&col,sizeof(col),1,file);
            assert(rc);
          }
          {
            int rc = fread((char*)&rad,sizeof(rad),1,file);
            assert(rc);
          }
          spheres->origins.push_back(pos);
          spheres->colors.push_back(col);
          spheres->radii.push_back(rad);
        }
      } else
        throw std::runtime_error("un-recognized spheres format '"+format+"'");

      if (verbose) {
        std::cout << "   ... done loading " << prettyNumber(my_count)
                  << " spheres from " << data.where << std::endl << std::flush;
        if (!scalarRange.empty())
          std::cout << "  (scalar range was " << scalarRange << ")" << std::endl;
        fflush(0);
      }
      fclose(file);

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
    { return (100/12) * divRoundUp((size_t)fileSize, (size_t)data.numParts); }

    
    void   VMDSpheres::executeLoad(DataRank &dataGroup, bool verbose)
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
