#include "owl/common/owl-common.h"
#include "owl/common/math/vec.h"
#include <array>
#include <vector>
#include <fstream>

using namespace owl::common;

struct DLAFScene
{
  std::vector<float> points; // (x, y, z) * numParticles
  std::vector<float> distances;
  float maxDistance{0.f};
  float radius{1.5f};
  std::array<float, 6> bounds = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
};

void importDLAFFile(const char *filename, DLAFScene &s)
{
  uint64_t numParticles = 0;

  auto *fp = std::fopen(filename, "rb");

  auto r = std::fread(&numParticles, sizeof(numParticles), 1, fp);
  r = std::fread(&s.radius, sizeof(s.radius), 1, fp);
  r = std::fread(&s.maxDistance, sizeof(s.maxDistance), 1, fp);
  r = std::fread(s.bounds.data(), sizeof(s.bounds[0]), s.bounds.size(), fp);

  s.points.resize(numParticles * 3);
  r = std::fread(s.points.data(), sizeof(s.points[0]), numParticles * 3, fp);

  s.distances.resize(numParticles);
  r = std::fread(s.distances.data(), sizeof(s.distances[0]), numParticles, fp);

  std::fclose(fp);
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
    float v = .5f + 0.5f * t;
    return v * hue_to_rgb(h);
  }


vec3f colorMap(float f)
{
  return temperature_to_rgb(f);
}

int hash(const std::string s)
{
  int sum = 0;
  for (int i=0;i<s.size();i++)
    sum = 13 * sum + int(s[i]);
  return sum;
}


int main(int ac, char **av)
{
  DLAFScene dl;
  importDLAFFile(av[1],dl);
  std::ofstream out(av[2],std::ios::binary);
  float maxDist = 1e-6f;
  for (auto d : dl.distances)
    maxDist = std::max(maxDist,d);
  int numPoints = dl.points.size()/3;
  float radius = dl.radius;
  const vec3f *points = (const vec3f*)dl.points.data();
  for (int i=0;i<numPoints;i++) {
    out.write((const char *)&points[i],sizeof(points[i]));
    vec3f color = colorMap(powf(dl.distances[i]/maxDist,1.3f));
    // quick-hack for per-rank mapping:
    // color = randomColor(hash(av[1])+14);
    out.write((const char *)&color,sizeof(color));
    out.write((const char *)&radius,sizeof(radius));
  }
}
