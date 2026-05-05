#pragma once

#include <array>
#include <entt/entity/fwd.hpp>
#include <machina/level_description.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <memory>
#include <raylib.h>
#include <raymath.h>
#include <vector>

namespace machina {

[[nodiscard]] Matrix
RaylibMatrixFromTransform(const std::array<float, 16>& transform);

struct UploadedMaterialUniform
{
  int location = -1;
  int uniformType = 0;
  std::array<float, 4> floatValues = std::array<float, 4>{};
  std::array<int, 4> intValues = std::array<int, 4>{};
};

struct UploadedMaterial
{
  Material material = Material{};
  std::vector<UploadedMaterialUniform> parameterUniforms;
  int viewPositionLocation = -1;
  int envRadianceLocation = -1;
  int envIrradianceLocation = -1;
  int envLightIntensityLocation = -1;
  int envRadianceMipsLocation = -1;
  int envRadianceSamplesLocation = -1;
  int activeLightCountLocation = -1;
  int lightTypeLocation = -1;
  int lightDirectionLocation = -1;
  int lightColorLocation = -1;
  int lightIntensityLocation = -1;
};

class Renderer
{
public:
  [[nodiscard]] std::vector<Diagnostic> Load(
    const LevelDescription& level,
    const MaterialXShaderGenerator& generator);
  void BeginFrame();
  void Submit(entt::registry& registry, const Camera& camera);
  void Flush();
  void Draw(entt::registry& registry, const Camera& camera);

private:
  struct DrawCommand
  {
    Camera camera = Camera{};
    const Mesh* mesh = nullptr;
    const UploadedMaterial* material = nullptr;
    Matrix modelMatrix = MatrixIdentity();
  };

  struct ShaderDeleter
  {
    void operator()(Shader* shader) const noexcept;
  };

  struct MeshDeleter
  {
    void operator()(Mesh* mesh) const noexcept;
  };

  struct UploadedMaterialDeleter
  {
    void operator()(UploadedMaterial* material) const noexcept;
  };

  using ShaderHandle = std::unique_ptr<Shader, ShaderDeleter>;
  using MeshHandle = std::unique_ptr<Mesh, MeshDeleter>;
  using MaterialHandle =
    std::unique_ptr<UploadedMaterial, UploadedMaterialDeleter>;

  std::vector<ShaderHandle> shaders;
  std::vector<MeshHandle> meshes;
  std::vector<MaterialHandle> materials;
  std::vector<DrawCommand> drawCommands;
};

}
