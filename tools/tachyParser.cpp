#include <miniScene/Scene.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

using namespace mini::common;

typedef std::pair<std::string,int> Token;

struct {
  std::vector<vec3f> vertices;
  std::vector<float> radii;
  std::vector<vec3f> colors;
} cylinders;

struct {
  std::vector<vec3f> vertices;
  std::vector<float> radii;
  std::vector<vec3f> colors;
} spheres;

struct {
  std::vector<vec3f> vertices;
  std::vector<vec3f> normals;
  std::vector<vec3f> colors;
  std::vector<vec3i> indices;
} triangles;

template<typename T>
void write(std::ofstream &out,
           const std::vector<T> &v)
{
  size_t count = v.size();
  out.write((const char *)&count,sizeof(count));
  out.write((const char *)v.data(),count*sizeof(T));
}

struct Tokens {
  Tokens(const std::string &fileName)
    : in(fileName.c_str())
  {}

  void eat(const std::string &s)
  {
    if (peeks(0) != s)
      throw std::runtime_error("in line #"+std::to_string(peek(0).second)
                               +": expected '" + s +
                               "' but found '" + peeks(0) + "'");
    else
      eat(1);
  }
  Token get() { Token next = peek(0); eat(1); return next; }
  std::string gets() { return get().first; }
  std::string peeks(int ID=0) {
    return peek(ID).first;
  }
  float getf()
  {
    return std::stof(gets());
  }
  vec3f get3f()
  {
    vec3f v;
    v.x = getf();
    v.y = getf();
    v.z = getf();
    return v;
  }
  
  float geti()
  {
    return std::stof(gets());
  }
  vec3i get3i()
  {
    vec3i v;
    v.x = geti();
    v.y = geti();
    v.z = geti();
    return v;
  }
  
  Token peek(int ID=0) {
    prefetch(ID+1);
    return peeked[ID];
  }
  void prefetch(int howMany)
  {
    while (peeked.size() < howMany)
      peeked.push_back(nextFromFile());
  }
  void eat(int num) {
    peek(num);
    peeked.erase(peeked.begin(),peeked.begin()+num);
  }
private:
  std::vector<Token> peeked;
  Token nextFromFile()
  {
    if (!in)

      return {"",-1};
    
    std::string s;
    while (true) {
      ss >> s;
      if (!s.empty())
        return {s,lineNo};
      
      if (!in) return {"",-1};
      getNextLine();
    }
  }
  void getNextLine()
  {
    std::string line;
    std::getline(in,line);
    ++lineNo;
    
    if (line[0] == '#') line = "";
    ss = std::stringstream(line);
  }
  int lineNo = 1;
  std::ifstream in;
  std::stringstream ss;
};


vec3f readTexture(Tokens &tokens)
{
// Texture
//   Ambient 0 Diffuse 0.65 Specular 0 Opacity 1
//   Phong Plastic 0.5 Phong_size 40 Color 1 0 0 TexFunc 0
  tokens.eat("Texture");
  tokens.eat("Ambient");
  tokens.eat(1);
  tokens.eat("Diffuse");
  float diff = tokens.getf();
  tokens.eat("Specular");
  tokens.eat(1);
  tokens.eat("Opacity");
  tokens.eat(1);
  tokens.eat("Phong");
  tokens.eat("Plastic");
  tokens.eat(1);
  tokens.eat("Phong_size");
  tokens.eat(1);
  tokens.eat("Color");
  vec3f color = diff * tokens.get3f();
  tokens.eat("TexFunc");
  tokens.eat(1);
  return color;
}

void readCylinder(Tokens &tokens)
{
  tokens.eat("FCylinder");
  tokens.eat("Base");
  vec3f base = tokens.get3f();// -0.95 -0.95 0
  tokens.eat("Apex");
  vec3f apex = tokens.get3f();// -0.7375 -0.95 0
  tokens.eat("Rad");
  float rad = tokens.getf();//0.03

  vec3f color = readTexture(tokens);
  
  cylinders.vertices.push_back(base);
  cylinders.vertices.push_back(apex);
  cylinders.radii.push_back(rad);
  cylinders.colors.push_back(color);
}
      
void readSTri(Tokens &tokens)
{
  tokens.eat("STri");
  tokens.eat("V0");
  vec3f v0 = tokens.get3f();
  tokens.eat("V1");
  vec3f v1 = tokens.get3f();
  tokens.eat("V2");
  vec3f v2 = tokens.get3f();
  tokens.eat("N0");
  vec3f n0 = tokens.get3f();
  tokens.eat("N1");
  vec3f n1 = tokens.get3f();
  tokens.eat("N2");
  vec3f n2 = tokens.get3f();

  
  vec3f color = readTexture(tokens);

  int begin = triangles.vertices.size();
  triangles.vertices.push_back(v0);
  triangles.vertices.push_back(v1);
  triangles.vertices.push_back(v2);
  triangles.normals.push_back(n0);
  triangles.normals.push_back(n1);
  triangles.normals.push_back(n2);
  triangles.colors.push_back(color);
  triangles.colors.push_back(color);
  triangles.colors.push_back(color);
  triangles.indices.push_back(vec3i(begin)+vec3i(0,1,2));
}
      
void readSphere(Tokens &tokens)
{
  tokens.eat("Sphere");
  tokens.eat("Center");
  vec3f center = tokens.get3f();
  tokens.eat("Rad");
  float rad = tokens.getf();

  vec3f color = readTexture(tokens);

  
  spheres.vertices.push_back(center);
  spheres.radii.push_back(rad);
  spheres.colors.push_back(color);
}

void readVertexArray(Tokens &tokens)
{
  tokens.eat("VertexArray");
  tokens.eat("Numverts");
  int numVerts = tokens.geti();
  tokens.eat("Coords");
  std::vector<vec3f> positions;
  for (int i=0;i<numVerts;i++)
    positions.push_back(tokens.get3f());

  tokens.eat("Normals");
  std::vector<vec3f> normals;
  for (int i=0;i<numVerts;i++)
    normals.push_back(tokens.get3f());

  tokens.eat("Colors");
  std::vector<vec3f> colors;
  for (int i=0;i<numVerts;i++)
    colors.push_back(tokens.get3f());

  vec3f color = readTexture(tokens);

  tokens.eat("TriMesh");
  int numTris = tokens.geti();
  std::vector<vec3i> indices;
  for (int i=0;i<numTris;i++)
    indices.push_back(tokens.get3i());
  
  tokens.eat("End_VertexArray");

  int begin = triangles.vertices.size();
  for (auto v : positions) triangles.vertices.push_back(v);
  for (auto v : normals) triangles.normals.push_back(v);
  for (auto v : colors) triangles.colors.push_back(v);
  for (auto v : indices) triangles.indices.push_back(vec3i(begin)+v);
}
      

void readTachy(const std::string &fileName)
{
  Tokens tokens(fileName);

  tokens.eat("Begin_Scene");

  while (true) {
    std::string next = tokens.peeks();
    if (next == "Resolution") {
      tokens.eat(3);
    } else if (next == "Shader_Mode") {
      while (true ) {
        Token skip = tokens.get();
        if (skip.first == "End_Shader_Mode") break;
      }
    } else if (next == "Camera") {
      while (true ) {
        Token skip = tokens.get();
        if (skip.first == "End_Camera") break;
      }
    } else if (next == "Directional_Light") {
      tokens.eat("Directional_Light");
      tokens.eat("Direction");
      tokens.eat(3);
      tokens.eat("Color");
      tokens.eat(3);
    } else if (next == "Background") {
      tokens.eat("Background");
      tokens.eat(3);
    } else if (next == "Fog") {
      tokens.eat("Fog");
      tokens.eat("Exp2");
      tokens.eat("Start");
      tokens.eat(1);
      tokens.eat("End");
      tokens.eat(1);
      tokens.eat("Density");
      tokens.eat(1);
      tokens.eat("Color");
      tokens.eat(3);
    } else if (next == "STri") {
      readSTri(tokens);
    } else if (next == "FCylinder") {
      readCylinder(tokens);
    } else if (next == "Sphere") {
      readSphere(tokens);
    } else if (next == "VertexArray") {
      readVertexArray(tokens);
    } else if (next == "End_Scene") {
      break;
    } else {
      Token err = tokens.peek(0);
      throw std::runtime_error("un-recognized token '"+err.first
                               +"' in line #"+std::to_string(err.second));
    }
  }
}

int main(int ac, char **av)
{
  std::string inFileName, outFileBase;
  for (int i=1;i<ac;i++) {
    std::string arg = av[i];
    if (arg[0] != '-')
      inFileName = arg;
    else if (arg == "-o")
      outFileBase = av[++i];
    else throw std::runtime_error("unrecognized cmdline arg "+arg);
  }
  if (inFileName.empty() || outFileBase.empty())
    throw std::runtime_error("tachyParser inFile.tachy -o outFileBase");
    readTachy(inFileName);

  std::ofstream out_mesh(outFileBase+".vmdmesh",std::ios::binary);
  write(out_mesh,triangles.vertices);
  write(out_mesh,triangles.normals);
  write(out_mesh,triangles.colors);
  write(out_mesh,triangles.indices);
  // mini::Mesh::SP mesh = mini::Mesh::create(mini::Matte::create());
  // mesh->vertices = triangles.vertices;
  // mesh->normals = triangles.normals;
  // mesh->colors = triangles.colors;
  // mesh->indices = triangles.indices;
  // mini::Scene::SP scene = mini::Scene::create();
  // scene->instances.push_back(mini::Instance({mesh}));
  // scene->save(outFileBase+".mini");

  std::ofstream out_spheres(outFileBase+".vmdspheres",std::ios::binary);
  write(out_spheres,spheres.vertices);
  write(out_spheres,spheres.radii);
  write(out_spheres,spheres.colors);

  std::ofstream out_cyls(outFileBase+".vmdcyls",std::ios::binary);
  write(out_cyls,cylinders.vertices);
  write(out_cyls,cylinders.radii);
  write(out_cyls,cylinders.colors);
}
