#pragma once

#include <entt/entt.hpp>
#include <machina/level_description.hpp>
#include <machina/renderer.hpp>
#include <machina/scene.hpp>
#include <machina/strategic_camera_controller.hpp>
#include <machina/web_overlay.hpp>
#include <memory>
#include <string_view>
#include <vector>

class GameScene final : public machina::Scene
{
public:
  struct CreateResult
  {
    std::unique_ptr<machina::Scene> scene;
    std::string_view diagnosticLabel = "game";
    std::vector<machina::Diagnostic> diagnostics;
  };

  [[nodiscard]] static CreateResult Create();

  void Update(machina::SceneStack& scenes) override;
  void Draw() override;

private:
  GameScene(machina::Renderer renderer, entt::registry registry);

  machina::Renderer renderer;
  entt::registry registry;
  std::unique_ptr<machina::WebOverlay> webOverlay;
  machina::StrategicCameraController cameraController;
  Camera camera = {};
  bool showFps = false;
};
