#include <solaris/game_scene.hpp>

#include <filesystem>
#include <machina/level_instantiator.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <machina/usd_level_loader.hpp>
#include <raylib.h>
#include <utility>

namespace {

std::filesystem::path
SampleScenePath()
{
  return std::filesystem::current_path() / MACHINA_ASSETS_ROOT / "scenes" /
         "suzannes.usda";
}

void
DrawFps()
{
  DrawText(TextFormat("%d", GetFPS()), 8, 4, 30, GREEN);
}

}

GameScene::GameScene(machina::Renderer renderer, entt::registry registry)
  : renderer(std::move(renderer))
  , registry(std::move(registry))
  , webOverlay(std::make_unique<machina::WebOverlay>(0,
                                                     0,
                                                     GetScreenWidth(),
                                                     GetScreenHeight()))
{
}

GameScene::CreateResult
GameScene::Create()
{
  machina::LevelDescription level =
    machina::UsdLevelLoader().Load(SampleScenePath());
  if (!level.Ok()) {
    return CreateResult{ .diagnosticLabel = "usd",
                         .diagnostics = std::move(level.diagnostics) };
  }

  machina::MaterialXShaderGenerator shaderGenerator(
    std::filesystem::current_path() / MACHINA_MATERIALX_LIBRARY_ROOT);
  machina::Renderer renderer;
  std::vector<machina::Diagnostic> diagnostics =
    renderer.Load(level, shaderGenerator);
  if (!diagnostics.empty()) {
    return CreateResult{ .diagnosticLabel = "renderer",
                         .diagnostics = std::move(diagnostics) };
  }

  entt::registry registry;
  machina::LevelInstantiator().Instantiate(registry, level);

  return CreateResult{
    .scene = std::unique_ptr<machina::Scene>(
      new GameScene(std::move(renderer), std::move(registry))),
  };
}

void
GameScene::Update(machina::SceneStack&)
{
  showFps = showFps != IsKeyPressed(KEY_F1);
  const machina::WebOverlayInputCapture overlayCapture =
    webOverlay->Update(true);
  cameraController.Update(machina::StrategicCameraControls::Read(
    { .mouseBlockedByUi = overlayCapture.mouse,
      .keyboardBlockedByUi = overlayCapture.keyboard }));
  camera = cameraController.Camera3D();
}

void
GameScene::Draw()
{
  ClearBackground(Color{ 63, 63, 63, 255 });

  BeginMode3D(camera);
  renderer.Draw(registry, camera);
  DrawGrid(20, 1.0f);
  EndMode3D();

  if (showFps) {
    DrawFps();
  }

  webOverlay->Draw();
}
