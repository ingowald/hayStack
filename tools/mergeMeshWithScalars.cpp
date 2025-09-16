#include "viewer/DataLoader.h"

using namespace hs;

int main(int ac, char **av)
{
  std::string inFilePrefix = av[1];

  TriangleMesh mesh;
  std::ifstream verticesStream(inFilePrefix+".vertices",std::ios::binary);
  std::ifstream indicesStream(inFilePrefix+".indices",std::ios::binary);
  std::ifstream scalarsStream(inFilePrefix+".scalars",std::ios::binary);

  mesh.vertices
    = noHeader::loadVectorOf<vec3f>
    (verticesStream);
  mesh.indices
    = noHeader::loadVectorOf<vec3i>
    (indicesStream);
  mesh.scalars.perVertex
    = noHeader::loadVectorOf<float>
    (scalarsStream);

  mesh.write(inFilePrefix+".hsmesh");
}
