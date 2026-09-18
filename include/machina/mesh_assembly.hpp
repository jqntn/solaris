#pragma once

#include <machina/level_description.hpp>
#include <vector>

namespace machina {

enum class Interpolation
{
  Constant,
  Uniform,
  Varying,
  Vertex,
  FaceVarying
};

struct MeshSource
{
  std::vector<Vec3> points;
  std::vector<int> faceVertexCounts;
  std::vector<int> faceVertexIndices;
  std::vector<Vec3> normals;
  std::vector<Vec2> texcoords;
  Interpolation normalInterpolation = Interpolation::Constant;
  Interpolation texcoordInterpolation = Interpolation::Constant;
  bool leftHanded = false;
  double metersPerUnit = 1.0;
};

[[nodiscard]] bool
BuildMesh(const MeshSource& source,
          MeshDescription& description,
          std::vector<Diagnostic>& diagnostics);

}
