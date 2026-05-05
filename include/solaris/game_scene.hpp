#pragma once

#include <entt/entt.hpp>
#include <machina/level_description.hpp>
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

  [[nodiscard]] static CreateResult Create(machina::Renderer& renderer,
                                           bool& showFps);

private:
  struct ConstructorTag
  {};

public:
  GameScene(ConstructorTag,
            machina::Renderer& renderer,
            bool& showFps,
            entt::registry registry);

  void Update(machina::SceneStack& scenes) override;
  void Draw(machina::SceneDrawContext& context) override;
  void DrawUi() override;

private:
  void HandleWebCommand(std::string command, std::string payload);

  machina::Renderer& renderer;
  bool& showFps;
  entt::registry registry;
  std::unique_ptr<machina::WebOverlay> webOverlay;
  machina::StrategicCameraController cameraController;
  Camera camera = Camera{};
  bool mainMenuRequested = false;
  bool quitRequested = false;
};
