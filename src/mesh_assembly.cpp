#include <machina/mesh_assembly.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace machina {
namespace {

Vec3
VectorBetween(const Vec3& start, const Vec3& end)
{
  return Vec3{ end.x - start.x, end.y - start.y, end.z - start.z };
}

Vec3
Cross(const Vec3& left, const Vec3& right)
{
  return Vec3{ left.y * right.z - left.z * right.y,
               left.z * right.x - left.x * right.z,
               left.x * right.y - left.y * right.x };
}

float
VectorLength(const Vec3& value)
{
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3
Normalized(Vec3 value)
{
  const float normalLength = VectorLength(value);
  if (normalLength <= 0.0f) {
    return Vec3{ 0.0f, 1.0f, 0.0f };
  }

  return Vec3{ value.x / normalLength,
               value.y / normalLength,
               value.z / normalLength };
}

Vec3
TriangleNormal(const Vec3& first, const Vec3& second, const Vec3& third)
{
  return Normalized(
    Cross(VectorBetween(first, second), VectorBetween(first, third)));
}

std::size_t
InterpolatedIndex(Interpolation interpolation,
                  std::size_t pointIndex,
                  std::size_t faceVertexIndex,
                  std::size_t faceIndex)
{
  if (interpolation == Interpolation::FaceVarying) {
    return faceVertexIndex;
  }

  if (interpolation == Interpolation::Vertex ||
      interpolation == Interpolation::Varying) {
    return pointIndex;
  }

  if (interpolation == Interpolation::Uniform) {
    return faceIndex;
  }

  return 0;
}

Vec3
NormalAt(const std::vector<Vec3>& normals,
         Interpolation interpolation,
         std::size_t pointIndex,
         std::size_t faceVertexIndex,
         std::size_t faceIndex)
{
  const std::size_t index =
    InterpolatedIndex(interpolation, pointIndex, faceVertexIndex, faceIndex);

  if (index >= normals.size()) {
    return Vec3{ 0.0f, 1.0f, 0.0f };
  }

  return Normalized(normals[index]);
}

Vec2
TexcoordAt(const std::vector<Vec2>& texcoords,
           Interpolation interpolation,
           std::size_t pointIndex,
           std::size_t faceVertexIndex,
           std::size_t faceIndex)
{
  const std::size_t index =
    InterpolatedIndex(interpolation, pointIndex, faceVertexIndex, faceIndex);

  if (index >= texcoords.size()) {
    return Vec2{};
  }

  return texcoords[index];
}

std::size_t
LocalFaceVertexIndex(int faceVertexCount, int localIndex, bool isLeftHanded)
{
  if (!isLeftHanded) {
    return static_cast<std::size_t>(localIndex);
  }

  return static_cast<std::size_t>(faceVertexCount - 1 - localIndex);
}

}

bool
BuildMesh(const MeshSource& source,
          MeshDescription& description,
          std::vector<Diagnostic>& diagnostics)
{
  if (source.points.empty() || source.faceVertexCounts.empty() ||
      source.faceVertexIndices.empty()) {
    diagnostics.push_back(Diagnostic{
      "Mesh " + description.path + " has no polygon data",
    });
    return false;
  }

  const bool useComputedFlatNormals = source.normals.empty();

  std::size_t faceVertexOffset = 0;
  for (std::size_t faceIndex = 0; faceIndex < source.faceVertexCounts.size();
       ++faceIndex) {
    const int faceVertexCount = source.faceVertexCounts[faceIndex];

    if (faceVertexCount < 3) {
      diagnostics.push_back(Diagnostic{
        "Mesh " + description.path +
          " contains a face with fewer than three vertices",
      });
      return false;
    }

    if (faceVertexOffset + static_cast<std::size_t>(faceVertexCount) >
        source.faceVertexIndices.size()) {
      diagnostics.push_back(Diagnostic{
        "Mesh " + description.path + " has invalid face indices",
      });
      return false;
    }

    for (int triangle = 1; triangle < faceVertexCount - 1; ++triangle) {
      const std::array<int, 3> localIndices =
        std::array<int, 3>{ 0, triangle, triangle + 1 };
      std::array<std::size_t, 3> usdLocalIndices = std::array<std::size_t, 3>{};
      std::array<int, 3> pointIndices = std::array<int, 3>{};

      for (std::size_t trianglePoint = 0; trianglePoint < localIndices.size();
           ++trianglePoint) {
        usdLocalIndices[trianglePoint] = LocalFaceVertexIndex(
          faceVertexCount, localIndices[trianglePoint], source.leftHanded);
        const std::size_t faceVertexIndex =
          faceVertexOffset + usdLocalIndices[trianglePoint];
        pointIndices[trianglePoint] = source.faceVertexIndices[faceVertexIndex];

        if (pointIndices[trianglePoint] < 0 ||
            static_cast<std::size_t>(pointIndices[trianglePoint]) >=
              source.points.size()) {
          diagnostics.push_back(Diagnostic{
            "Mesh " + description.path + " references an invalid point index",
          });
          return false;
        }
      }

      const Vec3 flatNormal = TriangleNormal(source.points[pointIndices[0]],
                                             source.points[pointIndices[1]],
                                             source.points[pointIndices[2]]);

      for (std::size_t trianglePoint = 0; trianglePoint < localIndices.size();
           ++trianglePoint) {
        const std::size_t faceVertexIndex =
          faceVertexOffset + usdLocalIndices[trianglePoint];
        const int pointIndex = source.faceVertexIndices[faceVertexIndex];

        if (description.vertices.size() >
            std::numeric_limits<std::uint16_t>::max()) {
          diagnostics.push_back(Diagnostic{
            "Mesh " + description.path +
              " exceeds raylib 16-bit index capacity",
          });
          return false;
        }

        const Vec3 position = source.points[pointIndex];
        Vec3 vertexNormal = flatNormal;
        if (!useComputedFlatNormals) {
          vertexNormal = NormalAt(source.normals,
                                  source.normalInterpolation,
                                  static_cast<std::size_t>(pointIndex),
                                  faceVertexIndex,
                                  faceIndex);
        }

        description.vertices.push_back(MeshVertex{
          Vec3{ static_cast<float>(position.x * source.metersPerUnit),
                static_cast<float>(position.y * source.metersPerUnit),
                static_cast<float>(position.z * source.metersPerUnit) },
          vertexNormal,
          TexcoordAt(source.texcoords,
                     source.texcoordInterpolation,
                     static_cast<std::size_t>(pointIndex),
                     faceVertexIndex,
                     faceIndex),
        });
        description.indices.push_back(
          static_cast<std::uint16_t>(description.vertices.size() - 1));
      }
    }

    faceVertexOffset += static_cast<std::size_t>(faceVertexCount);
  }

  return true;
}

}
