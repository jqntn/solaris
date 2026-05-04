#include <entt/entt.hpp>
#include <filesystem>
#include <machina/level_description.hpp>
#include <machina/level_instantiator.hpp>
#include <machina/materialx_shader_generator.hpp>
#include <machina/renderer.hpp>
#include <machina/strategic_camera_controller.hpp>
#include <machina/usd_level_loader.hpp>
#include <machina/web_overlay.hpp>
#include <memory>
#include <print>
#include <raylib.h>
#include <string_view>
#include <vector>

extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance =
    0x00000001;
}

namespace {

std::filesystem::path
SampleScenePath()
{
  return std::filesystem::current_path() / MACHINA_ASSETS_ROOT / "scenes" /
         "suzannes.usda";
}

void
PrintDiagnostics(std::string_view label,
                 const std::vector<machina::Diagnostic>& diagnostics)
{
  for (const machina::Diagnostic& diagnostic : diagnostics) {
    std::println("{}: {}", label, diagnostic.message);
  }
}

void
DrawFps()
{
  DrawText(TextFormat("%d", GetFPS()), 8, 4, 30, GREEN);
}

}

int
main()
{
  machina::UsdLevelLoader loader;
  machina::LevelDescription level = loader.Load(SampleScenePath());

  if (!level.Ok()) {
    PrintDiagnostics("usd", level.diagnostics);
    return 1;
  }

  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);
  InitWindow(1, 1, "solaris");

  const int currentMonitor = GetCurrentMonitor();
  const int monitorWidth = GetMonitorWidth(currentMonitor);
  const int monitorHeight = GetMonitorHeight(currentMonitor);
  SetWindowSize(monitorWidth + 1, monitorHeight + 1);
  SetWindowPosition(0, 0);
  SetExitKey(KEY_NULL);

  const int webOverlayWidth = GetScreenWidth();
  const int webOverlayHeight = GetScreenHeight();
  std::unique_ptr<machina::WebOverlay> webOverlay =
    std::make_unique<machina::WebOverlay>(
      0, 0, webOverlayWidth, webOverlayHeight);

  machina::MaterialXShaderGenerator shaderGenerator(
    std::filesystem::current_path() / MACHINA_MATERIALX_LIBRARY_ROOT);
  machina::Renderer renderer;
  std::vector<machina::Diagnostic> rendererDiagnostics =
    renderer.Load(level, shaderGenerator);
  if (!rendererDiagnostics.empty()) {
    PrintDiagnostics("renderer", rendererDiagnostics);
    CloseWindow();
    return 1;
  }

  entt::registry registry;
  machina::LevelInstantiator().Instantiate(registry, level);

  bool showFps = false;
  machina::StrategicCameraController cameraController;

  while (!WindowShouldClose()) {
    showFps = showFps != IsKeyPressed(KEY_F1);
    const machina::WebOverlayInputCapture overlayCapture =
      webOverlay->Update(true);
    cameraController.Update(machina::StrategicCameraControls::Read(
      { .mouseBlockedByUi = overlayCapture.mouse,
        .keyboardBlockedByUi = overlayCapture.keyboard }));
    const Camera camera = cameraController.Camera3D();

    BeginDrawing();
    ClearBackground(Color{ 63, 63, 63, 255 });

    BeginMode3D(camera);
    renderer.Draw(registry, camera);
    DrawGrid(20, 1.0f);
    EndMode3D();

    if (showFps) {
      DrawFps();
    }

    webOverlay->Draw();

    EndDrawing();
  }

  webOverlay.reset();
  CloseWindow();
  return 0;
}
