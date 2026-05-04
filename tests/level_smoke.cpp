#include <cmath>
#include <filesystem>
#include <fstream>
#include <machina/level_description.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <machina/renderer.hpp>
#include <machina/usd_level_loader.hpp>
#include <print>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

int
Fail(std::string_view message)
{
  std::println("{}", message);
  return 1;
}

bool
Near(float left, float right)
{
  return std::fabs(left - right) <= 0.0001f;
}

float
Dot(const machina::Vec3& left, const machina::Vec3& right)
{
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

float
Length(const machina::Vec3& value)
{
  return std::sqrt(Dot(value, value));
}

bool
Same(const machina::Vec3& left, const machina::Vec3& right)
{
  return Near(left.x, right.x) && Near(left.y, right.y) &&
         Near(left.z, right.z);
}

bool
AllTriangleCornerNormalsMatch(const machina::MeshDescription& mesh,
                              float minimumDot)
{
  for (std::size_t index = 0; index < mesh.indices.size(); index += 3) {
    const machina::MeshVertex& first = mesh.vertices[mesh.indices[index]];
    const machina::MeshVertex& second = mesh.vertices[mesh.indices[index + 1]];
    const machina::MeshVertex& third = mesh.vertices[mesh.indices[index + 2]];

    if (Dot(first.normal, second.normal) < minimumDot ||
        Dot(first.normal, third.normal) < minimumDot) {
      return false;
    }
  }

  return true;
}

bool
HasSplitTriangleNormals(const machina::MeshDescription& mesh, float maximumDot)
{
  for (std::size_t index = 0; index < mesh.indices.size(); index += 3) {
    const machina::MeshVertex& first = mesh.vertices[mesh.indices[index]];
    const machina::MeshVertex& second = mesh.vertices[mesh.indices[index + 1]];
    const machina::MeshVertex& third = mesh.vertices[mesh.indices[index + 2]];

    if (Dot(first.normal, second.normal) < maximumDot ||
        Dot(first.normal, third.normal) < maximumDot) {
      return true;
    }
  }

  return false;
}

std::filesystem::path
SampleScenePath()
{
  return std::filesystem::current_path() / MACHINA_ASSETS_ROOT / "scenes" /
         "suzannes.usda";
}

std::filesystem::path
NormalInterpolationScenePath()
{
  const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     "machina_normal_interpolation.usda";
  std::ofstream scene(path);
  scene << R"usda(#usda 1.0
(
    defaultPrim = "root"
    metersPerUnit = 1
    upAxis = "Y"
)

def Xform "root"
{
    def Scope "_materials"
    {
        def Material "Material_001"
        {
            token outputs:mtlx:surface.connect = </root/_materials/Material_001/Shader.outputs:surface>

            def Shader "Shader"
            {
                uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                color3f inputs:base_color = (1, 1, 1)
                token outputs:surface
            }
        }
    }

    def Xform "FlatObject"
    {
        def Mesh "FlatMesh" (
            prepend apiSchemas = ["MaterialBindingAPI"]
        )
        {
            int[] faceVertexCounts = [3]
            int[] faceVertexIndices = [0, 1, 2]
            rel material:binding = </root/_materials/Material_001>
            normal3f[] normals = [(1, 0, 0), (1, 0, 0), (1, 0, 0)] (
                interpolation = "faceVarying"
            )
            point3f[] points = [(0, 0, 0), (0, 0, 1), (1, 0, 0)]
            uniform token subdivisionScheme = "none"
        }
    }

    def Xform "SmoothObject"
    {
        def Mesh "SmoothMesh" (
            prepend apiSchemas = ["MaterialBindingAPI"]
        )
        {
            int[] faceVertexCounts = [3]
            int[] faceVertexIndices = [0, 1, 2]
            rel material:binding = </root/_materials/Material_001>
            normal3f[] normals = [(0, 1, 0), (0, 0, 1), (1, 0, 0)] (
                interpolation = "faceVarying"
            )
            point3f[] points = [(2, 0, 0), (2, 0, 1), (3, 0, 0)]
            uniform token subdivisionScheme = "none"
        }
    }

    def Xform "PrimvarObject"
    {
        def Mesh "PrimvarNormalMesh" (
            prepend apiSchemas = ["MaterialBindingAPI"]
        )
        {
            int[] faceVertexCounts = [3]
            int[] faceVertexIndices = [0, 1, 2]
            rel material:binding = </root/_materials/Material_001>
            normal3f[] normals = [(0, 1, 0), (0, 1, 0), (0, 1, 0)] (
                interpolation = "faceVarying"
            )
            normal3f[] primvars:normals = [(1, 0, 0), (0, 1, 0), (0, 0, 1)] (
                interpolation = "faceVarying"
            )
            int[] primvars:normals:indices = [2, 1, 0]
            point3f[] points = [(4, 0, 0), (4, 0, 1), (5, 0, 0)]
            uniform token subdivisionScheme = "none"
        }
    }
}
)usda";
  return path;
}

}

int
main()
{
  const machina::LevelDescription level =
    machina::UsdLevelLoader().Load(SampleScenePath());

  if (!level.diagnostics.empty()) {
    for (const machina::Diagnostic& diagnostic : level.diagnostics) {
      std::println("{}", diagnostic.message);
    }

    return 1;
  }

  if (level.entities.size() != 2) {
    return Fail("expected two renderable entities");
  }

  if (level.materials.size() != 2) {
    return Fail("expected two MaterialX materials");
  }

  if (level.meshes.size() != 2) {
    return Fail("expected two mesh resources");
  }

  for (const machina::MeshDescription& mesh : level.meshes) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
      return Fail("expected populated mesh data");
    }

    for (std::size_t index = 0; index < mesh.indices.size(); index += 3) {
      const machina::MeshVertex& first = mesh.vertices[mesh.indices[index]];
      const machina::MeshVertex& second =
        mesh.vertices[mesh.indices[index + 1]];
      const machina::MeshVertex& third = mesh.vertices[mesh.indices[index + 2]];

      if (Length(first.normal) < 0.999f || Length(first.normal) > 1.001f ||
          Length(second.normal) < 0.999f || Length(second.normal) > 1.001f ||
          Length(third.normal) < 0.999f || Length(third.normal) > 1.001f) {
        return Fail("expected unit-length triangle normals");
      }
    }
  }

  const machina::MeshDescription* sampleFlatMesh = nullptr;
  const machina::MeshDescription* sampleSmoothMesh = nullptr;
  for (const machina::MeshDescription& mesh : level.meshes) {
    if (mesh.name == "Suzanne_001") {
      sampleFlatMesh = &mesh;
    } else if (mesh.name == "Suzanne") {
      sampleSmoothMesh = &mesh;
    }
  }

  if (sampleFlatMesh == nullptr || sampleSmoothMesh == nullptr) {
    return Fail("expected sample flat and smooth meshes");
  }

  if (!AllTriangleCornerNormalsMatch(*sampleFlatMesh, 0.9999f)) {
    return Fail("expected blue sample mesh normals to stay flat per triangle");
  }

  if (!HasSplitTriangleNormals(*sampleSmoothMesh, 0.99f)) {
    return Fail("expected red sample mesh normals to stay smooth per triangle");
  }

  for (const machina::EntityDescription& entity : level.entities) {
    if (entity.mesh >= level.meshes.size() ||
        entity.material >= level.materials.size()) {
      return Fail("expected resolved mesh and material bindings");
    }
  }

  const machina::LevelDescription normalLevel =
    machina::UsdLevelLoader().Load(NormalInterpolationScenePath());

  if (!normalLevel.diagnostics.empty()) {
    for (const machina::Diagnostic& diagnostic : normalLevel.diagnostics) {
      std::println("{}", diagnostic.message);
    }

    return 1;
  }

  const machina::MeshDescription* flatMesh = nullptr;
  const machina::MeshDescription* smoothMesh = nullptr;
  const machina::MeshDescription* primvarNormalMesh = nullptr;
  for (const machina::MeshDescription& mesh : normalLevel.meshes) {
    if (mesh.name == "FlatMesh") {
      flatMesh = &mesh;
    } else if (mesh.name == "SmoothMesh") {
      smoothMesh = &mesh;
    } else if (mesh.name == "PrimvarNormalMesh") {
      primvarNormalMesh = &mesh;
    }
  }

  if (flatMesh == nullptr || smoothMesh == nullptr ||
      primvarNormalMesh == nullptr) {
    return Fail("expected normal interpolation test meshes");
  }

  if (flatMesh->indices.size() != 3 || smoothMesh->indices.size() != 3 ||
      primvarNormalMesh->indices.size() != 3) {
    return Fail("expected single-triangle normal test meshes");
  }

  const machina::MeshVertex& flatFirst =
    flatMesh->vertices[flatMesh->indices[0]];
  const machina::MeshVertex& flatSecond =
    flatMesh->vertices[flatMesh->indices[1]];
  const machina::MeshVertex& flatThird =
    flatMesh->vertices[flatMesh->indices[2]];
  if (!Same(flatFirst.normal, flatSecond.normal) ||
      !Same(flatFirst.normal, flatThird.normal)) {
    return Fail("expected duplicated face-varying normals to stay flat");
  }

  if (!Same(flatFirst.normal, { 1.0f, 0.0f, 0.0f }) ||
      !Same(flatSecond.normal, { 1.0f, 0.0f, 0.0f }) ||
      !Same(flatThird.normal, { 1.0f, 0.0f, 0.0f })) {
    return Fail("expected flat face-varying normals to be preserved");
  }

  const machina::MeshVertex& smoothFirst =
    smoothMesh->vertices[smoothMesh->indices[0]];
  const machina::MeshVertex& smoothSecond =
    smoothMesh->vertices[smoothMesh->indices[1]];
  const machina::MeshVertex& smoothThird =
    smoothMesh->vertices[smoothMesh->indices[2]];
  if (Same(smoothFirst.normal, smoothSecond.normal) ||
      Same(smoothFirst.normal, smoothThird.normal)) {
    return Fail("expected distinct face-varying normals to stay smooth");
  }

  if (!Same(smoothFirst.normal, { 0.0f, 1.0f, 0.0f }) ||
      !Same(smoothSecond.normal, { 0.0f, 0.0f, 1.0f }) ||
      !Same(smoothThird.normal, { 1.0f, 0.0f, 0.0f })) {
    return Fail("expected smooth face-varying normals to be preserved");
  }

  const machina::MeshVertex& primvarFirst =
    primvarNormalMesh->vertices[primvarNormalMesh->indices[0]];
  const machina::MeshVertex& primvarSecond =
    primvarNormalMesh->vertices[primvarNormalMesh->indices[1]];
  const machina::MeshVertex& primvarThird =
    primvarNormalMesh->vertices[primvarNormalMesh->indices[2]];
  if (!Same(primvarFirst.normal, { 0.0f, 0.0f, 1.0f }) ||
      !Same(primvarSecond.normal, { 0.0f, 1.0f, 0.0f }) ||
      !Same(primvarThird.normal, { 1.0f, 0.0f, 0.0f })) {
    return Fail("expected indexed primvars:normals to override mesh normals");
  }

  const Matrix translated = machina::RaylibMatrixFromTransform({ 1.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 1.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 0.0f,
                                                                 1.0f,
                                                                 0.0f,
                                                                 2.0f,
                                                                 3.0f,
                                                                 4.0f,
                                                                 1.0f });
  const Vector3 transformed =
    Vector3Transform(Vector3{ 1.0f, 1.0f, 1.0f }, translated);
  if (!Near(transformed.x, 3.0f) || !Near(transformed.y, 4.0f) ||
      !Near(transformed.z, 5.0f)) {
    return Fail("expected row-major transforms to convert to raylib matrices");
  }

  machina::MaterialXShaderGenerator generator(std::filesystem::current_path() /
                                              MACHINA_MATERIALX_LIBRARY_ROOT);
  std::vector<machina::GeneratedShader> generatedShaders;
  for (const machina::MaterialDescription& material : level.materials) {
    const machina::ShaderGenerationResult shader = generator.Generate(material);
    if (!shader.Ok()) {
      for (const machina::Diagnostic& diagnostic : shader.diagnostics) {
        std::println("{}", diagnostic.message);
      }

      return 1;
    }

    if (shader.shader.vertexSource.empty() ||
        shader.shader.fragmentSource.empty()) {
      return Fail("expected generated GLSL sources");
    }

    if (shader.shader.vertexSource.find("u_worldViewProjectionMatrix") ==
        std::string::npos) {
      return Fail("expected generated vertex shader to expose raylib MVP");
    }

    if (shader.shader.vertexSource.find(
          "layout(location = 0) in vec3 i_position;") == std::string::npos ||
        shader.shader.vertexSource.find(
          "layout(location = 2) in vec3 i_normal;") == std::string::npos ||
        shader.shader.vertexSource.find(
          "layout(location = 4) in vec3 i_tangent;") == std::string::npos) {
      return Fail("expected generated vertex shader to bind raylib VAO slots");
    }

    if (shader.shader.vertexSource.find("flat vec3 normalWorld") !=
          std::string::npos ||
        shader.shader.fragmentSource.find("flat vec3 normalWorld") !=
          std::string::npos) {
      return Fail("expected generated shaders to interpolate normals");
    }

    if (shader.shader.vertexSource.find(
          "u_viewProjectionMatrix * hPositionWorld") != std::string::npos) {
      return Fail("expected generated vertex shader to use raylib MVP");
    }

    if (shader.shader.fragmentSource.find("u_viewPosition") ==
        std::string::npos) {
      return Fail("expected generated fragment shader to expose view position");
    }

    if (shader.shader.fragmentSource.find("u_envIrradiance") ==
        std::string::npos) {
      return Fail(
        "expected generated fragment shader to expose environment lighting");
    }

    if (shader.shader.fragmentSource.find("u_lightData[") ==
        std::string::npos) {
      return Fail("expected generated fragment shader to expose light data");
    }

    if (shader.shader.fragmentSource.find("material_shader_base_color") ==
        std::string::npos) {
      return Fail(
        "expected generated shader to use canonical material uniforms");
    }

    if (shader.shader.fragmentSource.find(material.name + "_shader") !=
        std::string::npos) {
      return Fail("expected generated shader to avoid material-specific names");
    }

    generatedShaders.push_back(shader.shader);
  }

  if (generatedShaders.size() == 2 &&
      generatedShaders[0].vertexSource != generatedShaders[1].vertexSource) {
    return Fail(
      "expected matching material graphs to share vertex shader source");
  }

  return 0;
}
