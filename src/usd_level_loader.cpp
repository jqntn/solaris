#include <machina/usd_level_loader.hpp>

#include <machina/material_translation.hpp>
#include <machina/mesh_assembly.hpp>

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
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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

template<typename Vec>
std::vector<double>
Components(const Vec& value, int count)
{
  std::vector<double> components;
  components.reserve(static_cast<std::size_t>(count));

  for (int index = 0; index < count; ++index) {
    components.push_back(static_cast<double>(value[index]));
  }

  return components;
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
    return FormatScalar(value.UncheckedGet<float>());
  }

  if (value.IsHolding<double>()) {
    return FormatScalar(value.UncheckedGet<double>());
  }

  if (value.IsHolding<GfVec2f>()) {
    return FormatVector(Components(value.UncheckedGet<GfVec2f>(), 2));
  }

  if (value.IsHolding<GfVec2d>()) {
    return FormatVector(Components(value.UncheckedGet<GfVec2d>(), 2));
  }

  if (value.IsHolding<GfVec3f>()) {
    return FormatVector(Components(value.UncheckedGet<GfVec3f>(), 3));
  }

  if (value.IsHolding<GfVec3d>()) {
    return FormatVector(Components(value.UncheckedGet<GfVec3d>(), 3));
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

  return std::array<float, 16>{
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

std::optional<MaterialDescription>
ReadMaterial(const UsdShadeMaterial& material,
             std::vector<Diagnostic>& diagnostics)
{
  UsdShadeShader shader = material.ComputeSurfaceSource(TfToken("mtlx"));

  if (!shader) {
    diagnostics.push_back(Diagnostic{
      "Material " + PathOf(material) + " has no outputs:mtlx:surface source",
    });
    return std::nullopt;
  }

  TfToken id;
  shader.GetIdAttr().Get(&id);
  const std::string category = NodeCategoryFromShaderId(id.GetString());

  if (category.empty()) {
    diagnostics.push_back(Diagnostic{
      "Material " + PathOf(material) +
        " has an unsupported MaterialX shader id " + id.GetString(),
    });
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

    std::optional<std::string> type =
      MaterialXType(input.GetTypeName().GetAsToken().GetString());
    std::optional<std::string> stringValue = ValueString(value);
    if (!type || !stringValue) {
      continue;
    }

    const std::string name = input.GetBaseName().GetString();
    description.inputs.push_back(MaterialInput{ name, *type, *stringValue });

    if (name == "base_color" && value.IsHolding<GfVec3f>()) {
      const GfVec3f color = value.UncheckedGet<GfVec3f>();
      description.baseColor =
        std::array<float, 3>{ color[0], color[1], color[2] };
    }
  }

  if (description.inputs.empty()) {
    diagnostics.push_back(Diagnostic{
      "Material " + PathOf(material) + " has no supported MaterialX inputs",
    });
    return std::nullopt;
  }

  return description;
}

Interpolation
InterpolationOf(const TfToken& interpolation)
{
  if (interpolation == UsdGeomTokens->faceVarying) {
    return Interpolation::FaceVarying;
  }

  if (interpolation == UsdGeomTokens->vertex) {
    return Interpolation::Vertex;
  }

  if (interpolation == UsdGeomTokens->varying) {
    return Interpolation::Varying;
  }

  if (interpolation == UsdGeomTokens->uniform) {
    return Interpolation::Uniform;
  }

  return Interpolation::Constant;
}

std::vector<Vec3>
Vec3Values(const VtArray<GfVec3f>& values)
{
  std::vector<Vec3> result;
  result.reserve(values.size());

  for (const GfVec3f& value : values) {
    result.push_back(Vec3{ value[0], value[1], value[2] });
  }

  return result;
}

std::vector<Vec2>
Vec2Values(const VtArray<GfVec2f>& values)
{
  std::vector<Vec2> result;
  result.reserve(values.size());

  for (const GfVec2f& value : values) {
    result.push_back(Vec2{ value[0], value[1] });
  }

  return result;
}

std::vector<int>
IntValues(const VtArray<int>& values)
{
  return std::vector<int>(values.begin(), values.end());
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

bool
ReadMesh(const UsdGeomMesh& mesh,
         double metersPerUnit,
         MeshDescription& description,
         std::vector<Diagnostic>& diagnostics)
{
  TfToken subdivisionScheme;
  mesh.GetSubdivisionSchemeAttr().Get(&subdivisionScheme);
  if (!subdivisionScheme.IsEmpty() &&
      subdivisionScheme != UsdGeomTokens->none) {
    diagnostics.push_back(Diagnostic{
      "Mesh " + PathOf(mesh.GetPrim()) +
        " uses unsupported subdivision scheme " + subdivisionScheme.GetString(),
    });
    return false;
  }

  VtArray<GfVec3f> points;
  VtArray<int> faceVertexCounts;
  VtArray<int> faceVertexIndices;

  mesh.GetPointsAttr().Get(&points);
  mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
  mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);

  VtArray<GfVec3f> normals;
  TfToken normalInterpolation;
  ReadMeshNormals(mesh, normals, normalInterpolation);

  TfToken orientation;
  mesh.GetOrientationAttr().Get(&orientation);

  VtArray<GfVec2f> texcoords;
  TfToken texcoordInterpolation = UsdGeomTokens->constant;
  UsdGeomPrimvar st =
    UsdGeomPrimvarsAPI(mesh.GetPrim()).GetPrimvar(TfToken("st"));
  if (st) {
    st.ComputeFlattened(&texcoords, UsdTimeCode::Default());
    texcoordInterpolation = st.GetInterpolation();
  }

  MeshSource source;
  source.points = Vec3Values(points);
  source.faceVertexCounts = IntValues(faceVertexCounts);
  source.faceVertexIndices = IntValues(faceVertexIndices);
  source.normals = Vec3Values(normals);
  source.texcoords = Vec2Values(texcoords);
  source.normalInterpolation = InterpolationOf(normalInterpolation);
  source.texcoordInterpolation = InterpolationOf(texcoordInterpolation);
  source.leftHanded = orientation == UsdGeomTokens->leftHanded;
  source.metersPerUnit = metersPerUnit;

  return BuildMesh(source, description, diagnostics);
}

}

LevelDescription
UsdLevelLoader::Load(const std::filesystem::path& path) const
{
  LevelDescription level;

  if (!std::filesystem::exists(path)) {
    level.diagnostics.push_back(Diagnostic{
      "USD scene does not exist: " + path.string(),
    });
    return level;
  }

  UsdStageRefPtr stage = UsdStage::Open(path.generic_string());
  if (!stage) {
    level.diagnostics.push_back(Diagnostic{
      "USD scene could not be opened: " + path.string(),
    });
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
      level.diagnostics.push_back(Diagnostic{
        "Mesh " + PathOf(prim) + " has no material binding",
      });
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

    level.entities.push_back(EntityDescription{
      PathOf(objectPrim),
      ObjectName(prim),
      meshIndex,
      materialIt->second,
      MatrixValue(xformCache.GetLocalToWorldTransform(objectPrim),
                  metersPerUnit),
    });
  }

  if (level.entities.empty() && level.diagnostics.empty()) {
    level.diagnostics.push_back(Diagnostic{
      "USD scene contains no mesh entities: " + path.string(),
    });
  }

  return level;
}

}
