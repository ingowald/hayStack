// ======================================================================== //
// Copyright 2022-2023 Ingo Wald                                            //
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
#include "viewer/content/Capsules.h"
#include <fstream>

namespace hs {
  namespace content {
    
    struct FatCapsule {
      struct {
        vec3f position;
        float radius;
        vec3f color;
      } vertex[2];
    };
    inline bool operator<(const FatCapsule &a, const FatCapsule &b)
    {
      return memcmp(&a,&b,sizeof(a)) < 0;
    }
  
    Capsules::Capsules(const ResourceSpecifier &data,
                       // size_t fileSize,
                       int thisPartID)
      : data(data),
        fileSize(getFileSize(data.where)),
        thisPartID(thisPartID)
    {}
  
    std::string Capsules::toString()
    {
      return "capsules://{fileName="+data.where
        +", part "+std::to_string(thisPartID)+" of "
        + std::to_string(data.numParts)+", proj size "
        +prettyNumber(projectedSize())+"B}";
    }
    void Capsules::create(DataLoader *loader,
                          const ResourceSpecifier &data)
    {
      // const std::string fileName = data.where;
      // const size_t fileSize
      //   = (fileName == "<test>"
      //      ? 100000
      //      : getFileSize(data.where));
      for (int i=0;i<data.numParts;i++)
        loader->addContent(new Capsules(data,i));
      // data.where,
      //                                   fileSize,i,
      //                                   data.numParts));
    }
  
    size_t Capsules::projectedSize() 
    { return fileSize*40; }
  
    void   Capsules::executeLoad(DataRank &dataGroup, bool verbose) 
    {
      if (data.where == "<test>") {
#if 1
        float rr = .01f;
        
        for (int a=0;a<1000;a++) {
          std::vector<vec4f> vertices;
          std::vector<vec4f> colors;
          std::vector<vec2i> indices;

          auto rng = [&](){return (float)drand48(); };
          auto sqr = [&](float f){return f*f; };
          auto randomSphere = [&](float r){
            vec3f v;
            while (true) {
              v.x = 1.f-2.f*rng();
              v.y = 1.f-2.f*rng();
              v.z = 1.f-2.f*rng();
              if (length(v) <= 1.f) return r * normalize(v);
            }
          };
          auto lerp_l = [&](float f, vec4f a, vec4f b) { return (1.f-f)*a+f*b; };
          auto addCurve = [&](vec4f v00,
                              vec4f v01,
                              vec4f v02,
                              vec4f v03,
                              vec4f c0,
                              vec4f c1) {
            int numSegs = 16;
            for (int i=0;i<=numSegs;i++) {
              float t = i/float(numSegs);
              vec4f c = lerp_l(t,c0,c1);
              colors.push_back(c);
              vec4f v10 = lerp_l(t,v00,v01);
              vec4f v11 = lerp_l(t,v01,v02);
              vec4f v12 = lerp_l(t,v02,v03);
              vec4f v20 = lerp_l(t,v10,v11);
              vec4f v21 = lerp_l(t,v11,v12);
              vec4f v30 = lerp_l(t,v20,v21);
              vertices.push_back(v30);
              if (i) indices.push_back(int(vertices.size())-vec2i(1,2));
            }
          };
                              
          mini::DisneyMaterial::SP mat = mini::DisneyMaterial::create();
          mat->ior = 1.45f;
          mat->metallic = 1.f;
          mat->roughness = .5f*rng()*rng();
          mat->baseColor.x = sqr(rng());
          mat->baseColor.y = sqr(rng());
          mat->baseColor.z = sqr(rng());

          vec4f c0 = { sqr(rng()),sqr(rng()),sqr(rng()),1.f };
          vec4f c1 = { sqr(rng()),sqr(rng()),sqr(rng()),1.f };
          vec3f d0 = randomSphere(.1f);
          vec3f d1 = randomSphere(.1f);
          vec3f d2 = randomSphere(.1f);
          vec3f p0 = { rng(), rng(), rng() };
          vec3f p1 = p0 + d0;
          vec3f p2 = p1 + d0+d1;
          vec3f p3 = p2 + d0+d1+d2;
          float r0 = rr/4.f + rr*rng();
          float r1 = rr/4.f + rr*rng();
          float r2 = rr/4.f + rr*rng();
          float r3 = rr/4.f + rr*rng();
          addCurve(vec4f(p0,r0),
                   vec4f(p1,r1),
                   vec4f(p2,r2),
                   vec4f(p3,r3),
                   c0,c1);

          
          hs::Capsules::SP cs = hs::Capsules::create();
          cs->vertices = vertices;
          cs->indices = indices;
          cs->colors = colors;
          cs->material = mini::Metal::create();
          dataGroup.capsuleSets.push_back(cs);
        }
#else
        std::vector<vec4f> vertices = {
          {0.837575,0.654938,0.28438,0.0112988},
          {0.849629,0.640277,0.26121,0.0126267},
          {0.849629,0.640277,0.26121,0.0126267},
          {0.862121,0.620837,0.241392,0.013653},
          {0.862121,0.620837,0.241392,0.013653},
          {0.874889,0.596945,0.225178,0.0147788},
          {0.874889,0.596945,0.225178,0.0147788},
          {0.887774,0.568923,0.212819,0.016405},
          {0.887774,0.568923,0.212819,0.016405},
          {0.900616,0.537096,0.204566,0.0189326},
          {0.900616,0.537096,0.204566,0.0189326},
          {0.913255,0.501787,0.200672,0.0227626},
          {0.913255,0.501787,0.200672,0.0227626},
          {0.925531,0.463322,0.201387,0.028296},
          {0.925531,0.463322,0.201387,0.028296},
          {0.937283,0.422024,0.206964,0.0359339},
          {0.937283,0.422024,0.206964,0.0359339},
          {0.948351,0.378218,0.217652,0.0460772},
          {0.948351,0.378218,0.217652,0.0460772},
          {0.958576,0.332226,0.233705,0.0591268},
          {0.0879826,0.52152,0.213491,0.0356525},
          {0.091773,0.548679,0.229258,0.031989},
          {0.091773,0.548679,0.229258,0.031989},
          {0.0969947,0.580221,0.243401,0.0285979},
          {0.0969947,0.580221,0.243401,0.0285979},
          {0.10313,0.615321,0.256549,0.0257821},
          {0.10313,0.615321,0.256549,0.0257821},
          {0.109662,0.653156,0.269333,0.0238449},
          {0.109662,0.653156,0.269333,0.0238449},
          {0.116072,0.692901,0.282382,0.0230893},
          {0.116072,0.692901,0.282382,0.0230893},
          {0.121843,0.733732,0.296327,0.0238185},
          {0.121843,0.733732,0.296327,0.0238185},
          {0.126458,0.774823,0.311798,0.0263356},
          {0.126458,0.774823,0.311798,0.0263356},
          {0.129399,0.815352,0.329425,0.0309436},
          {0.129399,0.815352,0.329425,0.0309436},
          {0.130149,0.854493,0.349838,0.0379458},
          {0.130149,0.854493,0.349838,0.0379458},
          {0.12819,0.891421,0.373666,0.0476451},
          {0.652045,0.342175,0.764639,0.0418106},
          {0.678523,0.3425,0.783949,0.0454318},
          {0.678523,0.3425,0.783949,0.0454318},
          {0.708751,0.342343,0.806416,0.0468166},
          {0.708751,0.342343,0.806416,0.0468166},
          {0.74218,0.342008,0.831084,0.0465847},
          {0.74218,0.342008,0.831084,0.0465847},
          {0.778261,0.341796,0.856998,0.0453558},
          {0.778261,0.341796,0.856998,0.0453558},
          {0.816445,0.342007,0.883203,0.0437498},
          {0.816445,0.342007,0.883203,0.0437498},
          {0.856182,0.342946,0.908745,0.0423864},
          {0.856182,0.342946,0.908745,0.0423864},
          {0.896924,0.344912,0.932667,0.0418853},
          {0.896924,0.344912,0.932667,0.0418853},
          {0.938122,0.348207,0.954015,0.0428664},
          {0.938122,0.348207,0.954015,0.0428664},
          {0.979226,0.353134,0.971835,0.0459495},
          {0.979226,0.353134,0.971835,0.0459495},
          {1.01969,0.359995,0.98517,0.0517542},
          {0.934133,0.974679,0.313752,0.0471083},
          {0.941986,0.958752,0.336168,0.0430022},
          {0.941986,0.958752,0.336168,0.0430022},
          {0.944493,0.942307,0.357434,0.0385367},
          {0.944493,0.942307,0.357434,0.0385367},
          {0.942096,0.924824,0.377977,0.0339477},
          {0.942096,0.924824,0.377977,0.0339477},
          {0.935238,0.905787,0.398227,0.0294715},
          {0.935238,0.905787,0.398227,0.0294715},
          {0.924362,0.884678,0.418611,0.0253441},
          {0.924362,0.884678,0.418611,0.0253441},
          {0.90991,0.860978,0.439559,0.0218017},
          {0.90991,0.860978,0.439559,0.0218017},
          {0.892325,0.83417,0.461497,0.0190803},
          {0.892325,0.83417,0.461497,0.0190803},
          {0.872049,0.803736,0.484855,0.0174161},
          {0.872049,0.803736,0.484855,0.0174161},
          {0.849525,0.769158,0.51006,0.0170452},
          {0.849525,0.769158,0.51006,0.0170452},
          {0.825196,0.729919,0.537542,0.0182037},
        };
        std::vector<vec2i> index = {
          {0,1},
          {2,3},
          {4,5},
          {6,7},
          {8,9},
          {10,11},
          {12,13},
          {14,15},
          {16,17},
          {18,19},
          {20,21},
          {22,23},
          {24,25},
          {26,27},
          {28,29},
          {30,31},
          {32,33},
          {34,35},
          {36,37},
          {38,39},
          {40,41},
          {42,43},
          {44,45},
          {46,47},
          {48,49},
          {50,51},
          {52,53},
          {54,55},
          {56,57},
          {58,59},
          {60,61},
          {62,63},
          {64,65},
          {66,67},
          {68,69},
          {70,71},
          {72,73},
          {74,75},
          {76,77},
          {78,79},
        };
        
        hs::Capsules::SP cs = hs::Capsules::create();
        cs->vertices = vertices;
        cs->indices = index;
        cs->colors = vertices;
        cs->material = mini::Metal::create();
        // cs->material = mini::Matte::create();
        dataGroup.capsuleSets.push_back(cs);
#endif
        return;
      }

      hs::Capsules::SP cs = hs::Capsules::create();
      cs->material = mini::Matte::create();
      
      size_t numCapsulesInFile = fileSize / sizeof(FatCapsule);
      
      int64_t numCapsulesToLoad
        = std::min((int64_t)data.get_size("count",numCapsulesInFile),
                   (int64_t)(numCapsulesInFile-data.get_size("begin",0)));
      size_t begin
        = data.get_size("begin",0)
        + (numCapsulesToLoad * (thisPartID+0)) / data.numParts;
      size_t end
        = data.get_size("end",0)
        + (numCapsulesToLoad * (thisPartID+1)) / data.numParts;
      size_t count = end - begin;
      std::cout << "#caps part " <<thisPartID
                << " loads range " << begin << ".." << end << std::endl;

      
      // size_t begin = (thisPartID * numCapsulesTotal) / numPartsToSplitInto;
      // size_t end = ((thisPartID+1) * numCapsulesTotal) / numPartsToSplitInto;
      // size_t count = end - begin;

      std::vector<FatCapsule> fatCapsules(count);

      std::ifstream in(data.where.c_str(),std::ios::binary);
      in.seekg(begin*sizeof(FatCapsule),in.beg);
      in.read((char *)fatCapsules.data(),count*sizeof(FatCapsule));

      std::map<std::pair<vec4f,vec3f>,int> knownVertices;
      bool hadNanColors = false;
      for (auto fc : fatCapsules) {
        int segmentIndices[2];
        for (int i=0;i<2;i++) {
          const auto &fcv = fc.vertex[i];
          vec4f vertex;
          (vec3f&)vertex = fcv.position;
          vertex.w       = fcv.radius;
          vec3f color    = fcv.color;
          if (isnan(color.x)) {
            color = vec3f(-1.f);
            hadNanColors = true;
          }
          std::pair<vec4f,vec3f> key = { vertex,color };
          if (knownVertices.find(key) == knownVertices.end()) {
            knownVertices[key] = cs->vertices.size();
            cs->vertices.push_back(vertex);
            cs->colors.push_back(vec4f(color.x,color.y,color.z,0.f));
          }
          segmentIndices[i] = knownVertices[key];
        }
        cs->indices.push_back(vec2i(segmentIndices[0],segmentIndices[1]));
      }
      if (hadNanColors)
        cs->colors.clear();
      
      dataGroup.capsuleSets.push_back(cs);
    }
  
  }
}
