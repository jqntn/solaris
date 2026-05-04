#include <machina/usd_level_loader.hpp>

#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <array>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

PXR_NAMESPACE_USING_DIRECTIVE

namespace machina {
namespace {

std::string
PathOf(const UsdPrim& prim)
{
  return prim ? prim.GetPath().GetString() : std::string("<invalid prim>");
}

std::string
PathOf(const UsdShadeMaterial& material)
{
  return material ? material.GetPath().GetString()
                  : std::string("<invalid material>");
}

std::string
Scalar(double value)
{
  std::ostringstream stream;
  stream << std::setprecision(9) << value;
  return stream.str();
}

template<typename Vec>
std::string
VectorValue(const Vec& value, int count)
{
  std::ostringstream stream;
  stream << std::setprecision(9);

  for (int index = 0; index < count; ++index) {
    if (index != 0) {
      stream << ", ";
    }

    stream << value[index];
  }

  return stream.str();
}

std::optional<std::string>
ValueString(const VtValue& value)
{
  if (value.IsHolding<bool>()) {
    return value.UncheckedGet<bool>() ? "true" : "false";
  }

  if (value.IsHolding<int>()) {
    return std::to_string(value.UncheckedGet<int>());
  }

  if (value.IsHolding<float>()) {
    return Scalar(value.UncheckedGet<float>());
  }

  if (value.IsHolding<double>()) {
    return Scalar(value.UncheckedGet<double>());
  }

  if (value.IsHolding<GfVec2f>()) {
    return VectorValue(value.UncheckedGet<GfVec2f>(), 2);
  }

  if (value.IsHolding<GfVec2d>()) {
    return VectorValue(value.UncheckedGet<GfVec2d>(), 2);
  }

  if (value.IsHolding<GfVec3f>()) {
    return VectorValue(value.UncheckedGet<GfVec3f>(), 3);
  }

  if (value.IsHolding<GfVec3d>()) {
    return VectorValue(value.UncheckedGet<GfVec3d>(), 3);
  }

  return std::nullopt;
}

std::optional<std::string>
MaterialXType(const SdfValueTypeName& usdType)
{
  const std::string token = usdType.GetAsToken().GetString();

  if (token == "bool") {
    return "boolean";
  }

  if (token == "int") {
    return "integer";
  }

  if (token == "float" || token == "double") {
    return "float";
  }

  if (token == "float2" || token == "double2" || token == "texCoord2f" ||
      token == "texCoord2d") {
    return "vector2";
  }

  if (token == "float3" || token == "double3" || token == "vector3f" ||
      token == "vector3d" || token == "normal3f" || token == "normal3d") {
    return "vector3";
  }

  if (token == "color3f" || token == "color3d") {
    return "color3";
  }

  return std::nullopt;
}

std::string
ObjectName(const UsdPrim& meshPrim)
{
  UsdPrim objectPrim = meshPrim.GetParent();
  std::string name;

  if (objectPrim &&
      objectPrim.GetAttribute(TfToken("userProperties:blender:object_name"))
        .Get(&name)) {
    return name;
  }

  return objectPrim ? objectPrim.GetName().GetString()
                    : meshPrim.GetName().GetString();
}

std::array<float, 16>
MatrixValue(GfMatrix4d matrix, double metersPerUnit)
{
  if (metersPerUnit != 1.0) {
    matrix[3][0] *= metersPerUnit;
    matrix[3][1] *= metersPerUnit;
    matrix[3][2] *= metersPerUnit;
  }

  return {
    static_cast<float>(matrix[0][0]), static_cast<float>(matrix[0][1]),
    static_cast<float>(matrix[0][2]), static_cast<float>(matrix[0][3]),
    static_cast<float>(matrix[1][0]), static_cast<float>(matrix[1][1]),
    static_cast<float>(matrix[1][2]), static_cast<float>(matrix[1][3]),
    static_cast<float>(matrix[2][0]), static_cast<float>(matrix[2][1]),
    static_cast<float>(matrix[2][2]), static_cast<float>(matrix[2][3]),
    static_cast<float>(matrix[3][0]), static_cast<float>(matrix[3][1]),
    static_cast<float>(matrix[3][2]), static_cast<float>(matrix[3][3]),
  };
}

std::string
NodeCategoryFromId(const TfToken& id)
{
  std::string value = id.GetString();

  if (value.starts_with("ND_")) {
    value.erase(0, 3);
  }

  const std::string suffix = "_surfaceshader";
  if (value.ends_with(suffix)) {
    value.erase(value.size() - suffix.size());
  }

  return value;
}

std::optional<MaterialDescription>
ReadMaterial(const UsdShadeMaterial& material,
             std::vector<Diagnostic>& diagnostics)
{
  UsdShadeShader shader = material.ComputeSurfaceSource(TfToken("mtlx"));

  if (!shader) {
    diagnostics.push_back({ "Material " + PathOf(material) +
                            " has no outputs:mtlx:surface source" });
    return std::nullopt;
  }

  TfToken id;
  shader.GetIdAttr().Get(&id);
  const std::string category = NodeCategoryFromId(id);

  if (category.empty()) {
    diagnostics.push_back({ "Material " + PathOf(material) +
                            " has an unsupported MaterialX shader id " +
                            id.GetString() });
    return std::nullopt;
  }

  MaterialDescription description;
  description.path = PathOf(material);
  description.name = material.GetPrim().GetName().GetString();
  description.nodeCategory = category;
  description.nodeType = "surfaceshader";

  for (const UsdShadeInput& input : shader.GetInputs()) {
    if (input.HasConnectedSource()) {
      continue;
    }

    VtValue value;
    if (!input.Get(&value)) {
      continue;
    }

    std::optional<std::string> type = MaterialXType(input.GetTypeName());
    std::optional<std::string> stringValue = ValueString(value);
    if (!type || !stringValue) {
      continue;
    }

    const std::string name = input.GetBaseName().GetString();
    description.inputs.push_back({ name, *type, *stringValue });

    if (name == "base_color" && value.IsHolding<GfVec3f>()) {
      const GfVec3f color = value.UncheckedGet<GfVec3f>();
      description.baseColor = { color[0], color[1], color[2] };
    }
  }

  if (description.inputs.empty()) {
    diagnostics.push_back({ "Material " + PathOf(material) +
                            " has no supported MaterialX inputs" });
    return std::nullopt;
  }

  return description;
}

std::size_t
InterpolatedIndex(const TfToken& interpolation,
                  std::size_t pointIndex,
                  std::size_t faceVertexIndex,
                  std::size_t faceIndex)
{
  if (interpolation == UsdGeomTokens->faceVarying) {
    return faceVertexIndex;
  }

  if (interpolation == UsdGeomTokens->vertex ||
      interpolation == UsdGeomTokens->varying) {
    return pointIndex;
  }

  if (interpolation == UsdGeomTokens->uniform) {
    return faceIndex;
  }

  return 0;
}

Vec3
VectorBetween(const GfVec3f& start, const GfVec3f& end)
{
  return { end[0] - start[0], end[1] - start[1], end[2] - start[2] };
}

Vec3
Cross(const Vec3& left, const Vec3& right)
{
  return { left.y * right.z - left.z * right.y,
           left.z * right.x - left.x * right.z,
           left.x * right.y - left.y * right.x };
}

float
Length(const Vec3& value)
{
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3
Normalized(Vec3 value)
{
  const float normalLength = Length(value);
  if (normalLength <= 0.0f) {
    return { 0.0f, 1.0f, 0.0f };
  }

  return { value.x / normalLength,
           value.y / normalLength,
           value.z / normalLength };
}

Vec3
TriangleNormal(const GfVec3f& first,
               const GfVec3f& second,
               const GfVec3f& third)
{
  return Normalized(
    Cross(VectorBetween(first, second), VectorBetween(first, third)));
}

Vec3
NormalAt(const VtArray<GfVec3f>& normals,
         const TfToken& interpolation,
         std::size_t pointIndex,
         std::size_t faceVertexIndex,
         std::size_t faceIndex)
{
  const std::size_t index =
    InterpolatedIndex(interpolation, pointIndex, faceVertexIndex, faceIndex);

  if (index >= normals.size()) {
    return { 0.0f, 1.0f, 0.0f };
  }

  const GfVec3f normal = normals[index];
  return Normalized({ normal[0], normal[1], normal[2] });
}

void
ReadMeshNormals(const UsdGeomMesh& mesh,
                VtArray<GfVec3f>& normals,
                TfToken& interpolation)
{
  normals.clear();
  interpolation = UsdGeomTokens->constant;

  const UsdGeomPrimvar primvar =
    UsdGeomPrimvarsAPI(mesh.GetPrim()).GetPrimvar(TfToken("normals"));
  if (primvar.HasValue() &&
      primvar.ComputeFlattened(&normals, UsdTimeCode::Default())) {
    interpolation = primvar.GetInterpolation();
    return;
  }

  mesh.GetNormalsAttr().Get(&normals);
  interpolation = mesh.GetNormalsInterpolation();
}

Vec2
TexcoordAt(const VtArray<GfVec2f>& texcoords,
           const TfToken& interpolation,
           std::size_t pointIndex,
           std::size_t faceVertexIndex,
           std::size_t faceIndex)
{
  const std::size_t index =
    InterpolatedIndex(interpolation, pointIndex, faceVertexIndex, faceIndex);

  if (index >= texcoords.size()) {
    return {};
  }

  const GfVec2f texcoord = texcoords[index];
  return { texcoord[0], texcoord[1] };
}

std::size_t
UsdLocalFaceVertexIndex(int faceVertexCount, int localIndex, bool isLeftHanded)
{
  if (!isLeftHanded) {
    return static_cast<std::size_t>(localIndex);
  }

  return static_cast<std::size_t>(faceVertexCount - 1 - localIndex);
}

bool
ReadMesh(const UsdGeomMesh& mesh,
         double metersPerUnit,
         MeshDescription& description,
         std::vector<Diagnostic>& diagnostics)
{
  VtArray<GfVec3f> points;
  VtArray<int> faceVertexCounts;
  VtArray<int> faceVertexIndices;

  mesh.GetPointsAttr().Get(&points);
  mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
  mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

  if (points.empty() || faceVertexCounts.empty() || faceVertexIndices.empty()) {
    diagnostics.push_back(
      { "Mesh " + PathOf(mesh.GetPrim()) + " has no polygon data" });
    return false;
  }

  TfToken subdivisionScheme;
  mesh.GetSubdivisionSchemeAttr().Get(&subdivisionScheme);
  if (!subdivisionScheme.IsEmpty() &&
      subdivisionScheme != UsdGeomTokens->none) {
    diagnostics.push_back({ "Mesh " + PathOf(mesh.GetPrim()) +
                            " uses unsupported subdivision scheme " +
                            subdivisionScheme.GetString() });
    return false;
  }

  VtArray<GfVec3f> normals;
  TfToken normalInterpolation;
  ReadMeshNormals(mesh, normals, normalInterpolation);
  const bool useComputedFlatNormals = normals.empty();

  TfToken orientation;
  mesh.GetOrientationAttr().Get(&orientation);
  const bool isLeftHanded = orientation == UsdGeomTokens->leftHanded;

  VtArray<GfVec2f> texcoords;
  TfToken texcoordInterpolation = UsdGeomTokens->constant;
  UsdGeomPrimvar st =
    UsdGeomPrimvarsAPI(mesh.GetPrim()).GetPrimvar(TfToken("st"));
  if (st) {
    st.ComputeFlattened(&texcoords, UsdTimeCode::Default());
    texcoordInterpolation = st.GetInterpolation();
  }

  std::size_t faceVertexOffset = 0;
  for (std::size_t faceIndex = 0; faceIndex < faceVertexCounts.size();
       ++faceIndex) {
    const int faceVertexCount = faceVertexCounts[faceIndex];

    if (faceVertexCount < 3) {
      diagnostics.push_back(
        { "Mesh " + PathOf(mesh.GetPrim()) +
          " contains a face with fewer than three vertices" });
      return false;
    }

    if (faceVertexOffset + static_cast<std::size_t>(faceVertexCount) >
        faceVertexIndices.size()) {
      diagnostics.push_back(
        { "Mesh " + PathOf(mesh.GetPrim()) + " has invalid face indices" });
      return false;
    }

    for (int triangle = 1; triangle < faceVertexCount - 1; ++triangle) {
      const std::array<int, 3> localIndices = { 0, triangle, triangle + 1 };
      std::array<std::size_t, 3> usdLocalIndices = {};
      std::array<int, 3> pointIndices = {};

      for (std::size_t trianglePoint = 0; trianglePoint < localIndices.size();
           ++trianglePoint) {
        usdLocalIndices[trianglePoint] = UsdLocalFaceVertexIndex(
          faceVertexCount, localIndices[trianglePoint], isLeftHanded);
        const std::size_t faceVertexIndex =
          faceVertexOffset + usdLocalIndices[trianglePoint];
        pointIndices[trianglePoint] = faceVertexIndices[faceVertexIndex];

        if (pointIndices[trianglePoint] < 0 ||
            static_cast<std::size_t>(pointIndices[trianglePoint]) >=
              points.size()) {
          diagnostics.push_back({ "Mesh " + PathOf(mesh.GetPrim()) +
                                  " references an invalid point index" });
          return false;
        }
      }

      const Vec3 flatNormal = TriangleNormal(points[pointIndices[0]],
                                             points[pointIndices[1]],
                                             points[pointIndices[2]]);

      for (std::size_t trianglePoint = 0; trianglePoint < localIndices.size();
           ++trianglePoint) {
        const std::size_t faceVertexIndex =
          faceVertexOffset + usdLocalIndices[trianglePoint];
        const int pointIndex = faceVertexIndices[faceVertexIndex];

        if (description.vertices.size() >
            std::numeric_limits<std::uint16_t>::max()) {
          diagnostics.push_back({ "Mesh " + PathOf(mesh.GetPrim()) +
                                  " exceeds raylib 16-bit index capacity" });
          return false;
        }

        const GfVec3f position = points[pointIndex];
        Vec3 vertexNormal = flatNormal;
        if (!useComputedFlatNormals) {
          vertexNormal = NormalAt(normals,
                                  normalInterpolation,
                                  static_cast<std::size_t>(pointIndex),
                                  faceVertexIndex,
                                  faceIndex);
        }

        description.vertices.push_back(
          { { static_cast<float>(position[0] * metersPerUnit),
              static_cast<float>(position[1] * metersPerUnit),
              static_cast<float>(position[2] * metersPerUnit) },
            vertexNormal,
            TexcoordAt(texcoords,
                       texcoordInterpolation,
                       static_cast<std::size_t>(pointIndex),
                       faceVertexIndex,
                       faceIndex) });
        description.indices.push_back(
          static_cast<std::uint16_t>(description.vertices.size() - 1));
      }
    }

    faceVertexOffset += static_cast<std::size_t>(faceVertexCount);
  }

  return true;
}

}

LevelDescription
UsdLevelLoader::Load(const std::filesystem::path& path) const
{
  LevelDescription level;

  if (!std::filesystem::exists(path)) {
    level.diagnostics.push_back(
      { "USD scene does not exist: " + path.string() });
    return level;
  }

  UsdStageRefPtr stage = UsdStage::Open(path.generic_string());
  if (!stage) {
    level.diagnostics.push_back(
      { "USD scene could not be opened: " + path.string() });
    return level;
  }

  const double metersPerUnit = UsdGeomGetStageMetersPerUnit(stage);
  UsdGeomXformCache xformCache(UsdTimeCode::Default());
  std::unordered_map<std::string, std::size_t> materialIndices;

  for (const UsdPrim& prim : stage->Traverse()) {
    UsdGeomMesh mesh(prim);
    if (!mesh) {
      continue;
    }

    UsdShadeMaterial material =
      UsdShadeMaterialBindingAPI(prim).ComputeBoundMaterial();
    if (!material) {
      level.diagnostics.push_back(
        { "Mesh " + PathOf(prim) + " has no material binding" });
      continue;
    }

    const std::string materialPath = PathOf(material);
    std::unordered_map<std::string, std::size_t>::iterator materialIt =
      materialIndices.find(materialPath);
    if (materialIt == materialIndices.end()) {
      std::optional<MaterialDescription> materialDescription =
        ReadMaterial(material, level.diagnostics);
      if (!materialDescription) {
        continue;
      }

      materialIt =
        materialIndices.emplace(materialPath, level.materials.size()).first;
      level.materials.push_back(std::move(*materialDescription));
    }

    MeshDescription meshDescription;
    meshDescription.path = PathOf(prim);
    meshDescription.name = prim.GetName().GetString();

    if (!ReadMesh(mesh, metersPerUnit, meshDescription, level.diagnostics)) {
      continue;
    }

    const std::size_t meshIndex = level.meshes.size();
    level.meshes.push_back(std::move(meshDescription));

    UsdPrim objectPrim = prim.GetParent();
    if (!objectPrim) {
      objectPrim = prim;
    }

    level.entities.push_back(
      { PathOf(objectPrim),
        ObjectName(prim),
        meshIndex,
        materialIt->second,
        MatrixValue(xformCache.GetLocalToWorldTransform(objectPrim),
                    metersPerUnit) });
  }

  if (level.entities.empty() && level.diagnostics.empty()) {
    level.diagnostics.push_back(
      { "USD scene contains no mesh entities: " + path.string() });
  }

  return level;
}

}
