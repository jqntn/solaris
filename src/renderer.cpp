#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <entt/entt.hpp>
#include <machina/ecs.hpp>
#include <machina/level_description.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <machina/renderer.hpp>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace machina {

Matrix
RaylibMatrixFromTransform(const std::array<float, 16>& transform)
{
  return { transform[0], transform[4], transform[8],  transform[12],
           transform[1], transform[5], transform[9],  transform[13],
           transform[2], transform[6], transform[10], transform[14],
           transform[3], transform[7], transform[11], transform[15] };
}

namespace {

constexpr int environmentRadianceMapSlot = MATERIAL_MAP_HEIGHT;
constexpr int environmentIrradianceMapSlot = MATERIAL_MAP_BRDF;
constexpr std::string_view materialShaderName = "material_shader";

struct DrawCommand
{
  std::size_t mesh = 0;
  std::size_t material = 0;
  Matrix modelMatrix = MatrixIdentity();
};

unsigned char
ColorByte(float value)
{
  return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

std::string
SanitizeIdentifier(std::string_view value)
{
  std::string result;
  result.reserve(value.size());

  for (const unsigned char character : value) {
    if (std::isalnum(character) != 0 || character == '_') {
      result.push_back(static_cast<char>(character));
    } else {
      result.push_back('_');
    }
  }

  if (result.empty()) {
    return "input";
  }

  if (std::isdigit(static_cast<unsigned char>(result.front())) != 0) {
    result.insert(result.begin(), '_');
  }

  return result;
}

std::string
ShaderCacheKey(const MaterialDescription& material)
{
  std::vector<std::pair<std::string, std::string>> inputs;
  inputs.reserve(material.inputs.size());
  for (const MaterialInput& input : material.inputs) {
    inputs.emplace_back(input.name, input.type);
  }
  std::ranges::sort(inputs);

  std::string key = material.nodeCategory + '\n' + material.nodeType;
  for (const std::pair<std::string, std::string>& input : inputs) {
    key += '\n';
    key += input.first;
    key += ':';
    key += input.second;
  }

  return key;
}

std::string
MaterialUniformName(const MaterialInput& input)
{
  return std::string(materialShaderName) + '_' + SanitizeIdentifier(input.name);
}

std::string
NormalizedInputValue(std::string value)
{
  for (char& character : value) {
    if (character == ',' || character == '(' || character == ')') {
      character = ' ';
    }
  }

  return value;
}

bool
ParseFloatValues(const std::string& value,
                 std::array<float, 4>& parsed,
                 std::size_t count)
{
  std::istringstream stream(NormalizedInputValue(value));
  for (std::size_t index = 0; index < count; ++index) {
    if (!(stream >> parsed[index])) {
      return false;
    }
  }

  std::string extra;
  return !(stream >> extra);
}

bool
ParseIntValue(const std::string& value, int& parsed)
{
  std::istringstream stream(NormalizedInputValue(value));
  if (!(stream >> parsed)) {
    return false;
  }

  std::string extra;
  return !(stream >> extra);
}

bool
ParseBoolValue(std::string value, int& parsed)
{
  for (char& character : value) {
    character =
      static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }

  if (value == "true" || value == "1") {
    parsed = 1;
    return true;
  }

  if (value == "false" || value == "0") {
    parsed = 0;
    return true;
  }

  return false;
}

bool
ConfigureUniformValue(const MaterialInput& input,
                      UploadedMaterialUniform& uniform)
{
  if (input.type == "float") {
    uniform.uniformType = SHADER_UNIFORM_FLOAT;
    return ParseFloatValues(input.value, uniform.floatValues, 1);
  }

  if (input.type == "vector2") {
    uniform.uniformType = SHADER_UNIFORM_VEC2;
    return ParseFloatValues(input.value, uniform.floatValues, 2);
  }

  if (input.type == "vector3" || input.type == "color3") {
    uniform.uniformType = SHADER_UNIFORM_VEC3;
    return ParseFloatValues(input.value, uniform.floatValues, 3);
  }

  if (input.type == "integer") {
    uniform.uniformType = SHADER_UNIFORM_INT;
    return ParseIntValue(input.value, uniform.intValues[0]);
  }

  if (input.type == "boolean") {
    uniform.uniformType = SHADER_UNIFORM_INT;
    return ParseBoolValue(input.value, uniform.intValues[0]);
  }

  return false;
}

void
ConfigureShader(Shader& shader)
{
  const int position = GetShaderLocationAttrib(shader, "i_position");
  if (position >= 0) {
    shader.locs[SHADER_LOC_VERTEX_POSITION] = position;
  }

  const int texcoord = GetShaderLocationAttrib(shader, "i_texcoord_0");
  if (texcoord >= 0) {
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = texcoord;
  }

  const int normal = GetShaderLocationAttrib(shader, "i_normal");
  if (normal >= 0) {
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = normal;
  }

  const int tangent = GetShaderLocationAttrib(shader, "i_tangent");
  if (tangent >= 0) {
    shader.locs[SHADER_LOC_VERTEX_TANGENT] = tangent;
  }

  const int mvp = GetShaderLocation(shader, "u_worldViewProjectionMatrix");
  if (mvp >= 0) {
    shader.locs[SHADER_LOC_MATRIX_MVP] = mvp;
  }

  const int model = GetShaderLocation(shader, "u_worldMatrix");
  if (model >= 0) {
    shader.locs[SHADER_LOC_MATRIX_MODEL] = model;
  }

  const int normalMatrix =
    GetShaderLocation(shader, "u_worldInverseTransposeMatrix");
  if (normalMatrix >= 0) {
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = normalMatrix;
  }
}

UploadedMaterial
MakeUploadedMaterial(Material material)
{
  UploadedMaterial uploaded = {};
  uploaded.material = material;
  uploaded.viewPositionLocation =
    GetShaderLocation(material.shader, "u_viewPosition");
  uploaded.envRadianceLocation =
    GetShaderLocation(material.shader, "u_envRadiance");
  uploaded.envIrradianceLocation =
    GetShaderLocation(material.shader, "u_envIrradiance");
  uploaded.envLightIntensityLocation =
    GetShaderLocation(material.shader, "u_envLightIntensity");
  uploaded.envRadianceMipsLocation =
    GetShaderLocation(material.shader, "u_envRadianceMips");
  uploaded.envRadianceSamplesLocation =
    GetShaderLocation(material.shader, "u_envRadianceSamples");
  uploaded.activeLightCountLocation =
    GetShaderLocation(material.shader, "u_numActiveLightSources");
  uploaded.lightTypeLocation =
    GetShaderLocation(material.shader, "u_lightData[0].type");
  uploaded.lightDirectionLocation =
    GetShaderLocation(material.shader, "u_lightData[0].direction");
  uploaded.lightColorLocation =
    GetShaderLocation(material.shader, "u_lightData[0].color");
  uploaded.lightIntensityLocation =
    GetShaderLocation(material.shader, "u_lightData[0].intensity");
  return uploaded;
}

std::vector<UploadedMaterialUniform>
MakeUploadedMaterialUniforms(const Shader& shader,
                             const MaterialDescription& material,
                             std::vector<Diagnostic>& diagnostics)
{
  std::vector<UploadedMaterialUniform> uniforms;

  for (const MaterialInput& input : material.inputs) {
    UploadedMaterialUniform uniform = {};
    uniform.location =
      GetShaderLocation(shader, MaterialUniformName(input).c_str());
    if (!ConfigureUniformValue(input, uniform)) {
      diagnostics.push_back({ "Material " + material.path +
                              " has unsupported or invalid input " +
                              input.name + " of type " + input.type });
      continue;
    }

    if (uniform.location >= 0) {
      uniforms.push_back(uniform);
    }
  }

  return uniforms;
}

void
SetIntUniform(const Shader& shader, int location, int value)
{
  if (location >= 0) {
    SetShaderValue(shader, location, &value, SHADER_UNIFORM_INT);
  }
}

void
SetFloatUniform(const Shader& shader, int location, float value)
{
  if (location >= 0) {
    SetShaderValue(shader, location, &value, SHADER_UNIFORM_FLOAT);
  }
}

void
SetVec3Uniform(const Shader& shader, int location, Vector3 value)
{
  if (location >= 0) {
    SetShaderValue(shader, location, &value, SHADER_UNIFORM_VEC3);
  }
}

void
SetMaterialParameterUniforms(const UploadedMaterial* material)
{
  const Shader& shader = material->material.shader;
  for (const UploadedMaterialUniform& uniform : material->parameterUniforms) {
    if (uniform.location < 0) {
      continue;
    }

    switch (uniform.uniformType) {
      case SHADER_UNIFORM_INT:
      case SHADER_UNIFORM_IVEC2:
      case SHADER_UNIFORM_IVEC3:
      case SHADER_UNIFORM_IVEC4:
        SetShaderValue(shader,
                       uniform.location,
                       uniform.intValues.data(),
                       uniform.uniformType);
        break;
      default:
        SetShaderValue(shader,
                       uniform.location,
                       uniform.floatValues.data(),
                       uniform.uniformType);
        break;
    }
  }
}

Vector3
FallbackTangent(const Vec3& normal)
{
  const Vector3 n = Vector3Normalize(Vector3{ normal.x, normal.y, normal.z });
  const Vector3 reference = std::abs(n.y) < 0.999f
                              ? Vector3{ 0.0f, 1.0f, 0.0f }
                              : Vector3{ 1.0f, 0.0f, 0.0f };
  return Vector3Normalize(Vector3CrossProduct(reference, n));
}

Texture2D
LoadNeutralEnvironmentTexture()
{
  Image image = GenImageColor(4, 2, Color{ 214, 218, 222, 255 });
  Texture2D texture = LoadTextureFromImage(image);
  UnloadImage(image);

  SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
  return texture;
}

void
SetMaterialSampler(Material& material, int mapSlot, int location)
{
  if (location < 0) {
    return;
  }

  material.shader.locs[SHADER_LOC_MAP_DIFFUSE + mapSlot] = location;
  material.maps[mapSlot].texture = LoadNeutralEnvironmentTexture();
}

void
ConfigureMaterialEnvironmentMaps(UploadedMaterial* material)
{
  SetMaterialSampler(material->material,
                     environmentRadianceMapSlot,
                     material->envRadianceLocation);
  SetMaterialSampler(material->material,
                     environmentIrradianceMapSlot,
                     material->envIrradianceLocation);
}

void
ConfigureStaticLightingUniforms(const UploadedMaterial* material)
{
  const Shader& shader = material->material.shader;
  const Vector3 direction =
    Vector3Normalize(Vector3{ -4.076245f, -5.903862f, 1.005454f });
  const Vector3 color = { 1.0f, 1.0f, 1.0f };
  const int envMips = 1;
  const int envSamples = 8;

  SetFloatUniform(shader, material->envLightIntensityLocation, 0.4f);
  SetIntUniform(shader, material->envRadianceMipsLocation, envMips);
  SetIntUniform(shader, material->envRadianceSamplesLocation, envSamples);
  SetIntUniform(shader, material->activeLightCountLocation, 1);
  SetIntUniform(shader, material->lightTypeLocation, 1);
  SetVec3Uniform(shader, material->lightDirectionLocation, direction);
  SetVec3Uniform(shader, material->lightColorLocation, color);
  SetFloatUniform(shader, material->lightIntensityLocation, 2.0f);
}

void
ConfigureFrameUniforms(const UploadedMaterial* material, const Camera& camera)
{
  SetVec3Uniform(
    material->material.shader, material->viewPositionLocation, camera.position);
}

bool
DrawCommandLess(const UploadedMaterial* leftMaterial,
                const UploadedMaterial* rightMaterial,
                const DrawCommand& left,
                const DrawCommand& right)
{
  const unsigned int leftShader = leftMaterial->material.shader.id;
  const unsigned int rightShader = rightMaterial->material.shader.id;

  if (leftShader != rightShader) {
    return leftShader < rightShader;
  }

  if (left.material != right.material) {
    return left.material < right.material;
  }

  return left.mesh < right.mesh;
}

Mesh
UploadMeshDescription(const MeshDescription& description)
{
  Mesh mesh = {};
  mesh.vertexCount = static_cast<int>(description.vertices.size());
  mesh.triangleCount = static_cast<int>(description.indices.size() / 3);

  mesh.vertices = static_cast<float*>(
    MemAlloc(description.vertices.size() * 3 * sizeof(float)));
  mesh.normals = static_cast<float*>(
    MemAlloc(description.vertices.size() * 3 * sizeof(float)));
  mesh.tangents = static_cast<float*>(
    MemAlloc(description.vertices.size() * 4 * sizeof(float)));
  mesh.texcoords = static_cast<float*>(
    MemAlloc(description.vertices.size() * 2 * sizeof(float)));
  mesh.indices = static_cast<unsigned short*>(
    MemAlloc(description.indices.size() * sizeof(unsigned short)));

  for (std::size_t index = 0; index < description.vertices.size(); ++index) {
    const MeshVertex& vertex = description.vertices[index];
    mesh.vertices[index * 3 + 0] = vertex.position.x;
    mesh.vertices[index * 3 + 1] = vertex.position.y;
    mesh.vertices[index * 3 + 2] = vertex.position.z;
    mesh.normals[index * 3 + 0] = vertex.normal.x;
    mesh.normals[index * 3 + 1] = vertex.normal.y;
    mesh.normals[index * 3 + 2] = vertex.normal.z;
    const Vector3 tangent = FallbackTangent(vertex.normal);
    mesh.tangents[index * 4 + 0] = tangent.x;
    mesh.tangents[index * 4 + 1] = tangent.y;
    mesh.tangents[index * 4 + 2] = tangent.z;
    mesh.tangents[index * 4 + 3] = 1.0f;
    mesh.texcoords[index * 2 + 0] = vertex.texcoord.x;
    mesh.texcoords[index * 2 + 1] = vertex.texcoord.y;
  }

  std::memcpy(mesh.indices,
              description.indices.data(),
              description.indices.size() * sizeof(unsigned short));

  UploadMesh(&mesh, false);
  return mesh;
}

}

void
Renderer::ShaderDeleter::operator()(Shader* shader) const noexcept
{
  if (shader != nullptr && shader->id != 0u) {
    UnloadShader(*shader);
  }

  delete shader;
}

void
Renderer::MeshDeleter::operator()(Mesh* mesh) const noexcept
{
  if (mesh != nullptr) {
    UnloadMesh(*mesh);
  }

  delete mesh;
}

void
Renderer::UploadedMaterialDeleter::operator()(
  UploadedMaterial* material) const noexcept
{
  if (material != nullptr) {
    material->material.shader.id = rlGetShaderIdDefault();
    material->material.shader.locs = rlGetShaderLocsDefault();
    UnloadMaterial(material->material);
  }

  delete material;
}

std::vector<Diagnostic>
Renderer::Load(const LevelDescription& level,
               const MaterialXShaderGenerator& generator)
{
  std::vector<Diagnostic> diagnostics;
  std::unordered_map<std::string, std::size_t> shaderCache;

  for (const MaterialDescription& materialDescription : level.materials) {
    const std::string cacheKey = ShaderCacheKey(materialDescription);
    std::size_t shaderIndex = 0;
    const std::unordered_map<std::string, std::size_t>::const_iterator
      cachedShader = shaderCache.find(cacheKey);
    if (cachedShader != shaderCache.end()) {
      shaderIndex = cachedShader->second;
    } else {
      ShaderGenerationResult generated =
        generator.Generate(materialDescription);
      if (!generated.Ok()) {
        diagnostics.insert(diagnostics.end(),
                           generated.diagnostics.begin(),
                           generated.diagnostics.end());
        return diagnostics;
      }

      Shader shader =
        LoadShaderFromMemory(generated.shader.vertexSource.c_str(),
                             generated.shader.fragmentSource.c_str());
      if (shader.id == 0) {
        diagnostics.push_back(
          { "raylib failed to compile generated MaterialX shader for " +
            materialDescription.path });
        return diagnostics;
      }

      ConfigureShader(shader);
      shaderIndex = shaders.size();
      shaders.push_back(ShaderHandle(new Shader(shader)));
      shaderCache.emplace(cacheKey, shaderIndex);
    }

    Material material = LoadMaterialDefault();
    const Shader* shader = shaders[shaderIndex].get();
    material.shader = *shader;
    material.maps[MATERIAL_MAP_DIFFUSE].color =
      Color{ ColorByte(materialDescription.baseColor[0]),
             ColorByte(materialDescription.baseColor[1]),
             ColorByte(materialDescription.baseColor[2]),
             255 };
    MaterialHandle uploaded(
      new UploadedMaterial(MakeUploadedMaterial(material)));
    uploaded->parameterUniforms = MakeUploadedMaterialUniforms(
      uploaded->material.shader, materialDescription, diagnostics);
    if (!diagnostics.empty()) {
      return diagnostics;
    }

    ConfigureMaterialEnvironmentMaps(uploaded.get());
    ConfigureStaticLightingUniforms(uploaded.get());
    materials.push_back(std::move(uploaded));
  }

  for (const MeshDescription& meshDescription : level.meshes) {
    Mesh mesh = UploadMeshDescription(meshDescription);
    meshes.push_back(MeshHandle(new Mesh(mesh)));
  }

  return diagnostics;
}

void
Renderer::Draw(entt::registry& registry, const Camera& camera) const
{
  std::vector<DrawCommand> drawCommands;

  for (const MaterialHandle& material : materials) {
    ConfigureFrameUniforms(material.get(), camera);
  }

  registry.view<const Renderable, const Transform>().each(
    [this, &drawCommands](const entt::entity entity,
                          const Renderable& renderable,
                          const Transform& transform) {
      (void)entity;
      if (renderable.mesh >= meshes.size() ||
          renderable.material >= materials.size()) {
        return;
      }

      drawCommands.push_back({ renderable.mesh,
                               renderable.material,
                               RaylibMatrixFromTransform(transform.world) });
    });

  std::ranges::sort(drawCommands,
                    [this](const DrawCommand& left, const DrawCommand& right) {
                      return DrawCommandLess(materials[left.material].get(),
                                             materials[right.material].get(),
                                             left,
                                             right);
                    });

  std::size_t activeMaterial = materials.size();
  for (const DrawCommand& command : drawCommands) {
    const UploadedMaterial* material = materials[command.material].get();
    if (command.material != activeMaterial) {
      SetMaterialParameterUniforms(material);
      activeMaterial = command.material;
    }

    const Mesh* mesh = meshes[command.mesh].get();
    DrawMesh(*mesh, material->material, command.modelMatrix);
  }
}

}
