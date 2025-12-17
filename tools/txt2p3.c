#include <iostream>
#include <fstream>
#include <owl/common/math/vec.h>
#include <owl/common/math/box.h>
#include <vector>

using namespace owl::common;

int main(int ac, char **av)
{
  std::ifstream in(av[1]);
  std::ofstream out(av[2],std::ios::binary);
  box3f bounds;
  std::vector<vec3f> points;
  while (in) {
    vec3f p;
    in >> p.x >> p.y >> p.z;
    p.z /= 10000.f;
    if (!in.good()) break;
    bounds.extend(p);
    points.push_back(p);
  }
  std::cout << bounds.lower << " " << bounds.upper << std::endl;
  for (auto &p : points)
    p -= bounds.lower;
  out.write((char*)points.data(),points.size()*sizeof(vec3f));
}
