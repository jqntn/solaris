#include <solaris/game_scene.hpp>

#include <filesystem>
#include <machina/level_instantiator.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <machina/renderer.hpp>
#include <machina/runtime_paths.hpp>
#include <machina/usd_level_loader.hpp>
#include <raylib.h>
#include <solaris/main_menu_scene.hpp>
#include <string>
#include <utility>

namespace {

std::filesystem::path
SampleScenePath()
{
  return machina::RuntimeAssetPath() / "scenes" / "suzannes.usda";
}

}

GameScene::GameScene(ConstructorTag,
                     machina::Renderer& renderer,
                     bool& showFps,
                     entt::registry registry)
  : renderer(renderer)
  , showFps(showFps)
  , registry(std::move(registry))
  , webOverlay(std::make_unique<machina::WebOverlay>(
      0,
      0,
      GetScreenWidth(),
      GetScreenHeight(),
      "file:///web/game-hud.html",
      [this](std::string command, std::string payload) {
        HandleWebCommand(std::move(command), std::move(payload));
      }))
{
}

GameScene::CreateResult
GameScene::Create(machina::Renderer& renderer, bool& showFps)
{
  machina::LevelDescription level =
    machina::UsdLevelLoader().Load(SampleScenePath());
  if (!level.Ok()) {
    return CreateResult{ .diagnosticLabel = "usd",
                         .diagnostics = std::move(level.diagnostics) };
  }

  machina::MaterialXShaderGenerator shaderGenerator(
    machina::RuntimeMaterialXPath());
  std::vector<machina::Diagnostic> diagnostics =
    renderer.Load(level, shaderGenerator);
  if (!diagnostics.empty()) {
    return CreateResult{ .diagnosticLabel = "renderer",
                         .diagnostics = std::move(diagnostics) };
  }

  entt::registry registry;
  machina::LevelInstantiator().Instantiate(registry, level);

  return CreateResult{
    .scene = std::make_unique<GameScene>(
      ConstructorTag{}, renderer, showFps, std::move(registry)),
  };
}

void
GameScene::Update(machina::SceneStack& scenes)
{
  const machina::WebOverlayInputCapture overlayCapture =
    webOverlay->Update(true);
  cameraController.Update(machina::StrategicCameraControls::Read(
    machina::StrategicCameraControlCapture{
      .mouseBlockedByUi = overlayCapture.mouse,
      .keyboardBlockedByUi = overlayCapture.keyboard,
    }));
  camera = cameraController.Camera3D();

  if (quitRequested) {
    scenes.RequestQuit();
    quitRequested = false;
    return;
  }

  if (mainMenuRequested) {
    mainMenuRequested = false;
    scenes.Replace(std::make_unique<MainMenuScene>(renderer, showFps));
  }
}

void
GameScene::Draw(machina::SceneDrawContext& context)
{
  context.renderer.Submit(registry, camera);

  BeginMode3D(camera);
  DrawGrid(20, 1.0f);
  EndMode3D();
}

void
GameScene::DrawUi()
{
  webOverlay->Draw();
}

void
GameScene::HandleWebCommand(std::string command, std::string payload)
{
  (void)payload;
  if (command == "main_menu") {
    mainMenuRequested = true;
    return;
  }

  if (command == "quit") {
    quitRequested = true;
  }
}
