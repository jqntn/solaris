#include <solaris/game_scene.hpp>

#include <machina/level_description.hpp>
#include <machina/renderer.hpp>
#include <machina/scene.hpp>
#include <print>
#include <raylib.h>
#include <string_view>
#include <utility>
#include <vector>

extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance =
    0x00000001;
}

namespace {

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
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);
  InitWindow(1, 1, "solaris");

  const int currentMonitor = GetCurrentMonitor();
  const int monitorWidth = GetMonitorWidth(currentMonitor);
  const int monitorHeight = GetMonitorHeight(currentMonitor);
  SetWindowSize(monitorWidth + 1, monitorHeight + 1);
  SetWindowPosition(0, 0);
  SetExitKey(KEY_NULL);

  machina::Renderer renderer;
  GameScene::CreateResult gameScene = GameScene::Create(renderer);
  if (!gameScene.diagnostics.empty()) {
    PrintDiagnostics(gameScene.diagnosticLabel, gameScene.diagnostics);
    CloseWindow();
    return 1;
  }

  machina::SceneStack scenes;
  scenes.Push(std::move(gameScene.scene));
  machina::SceneDrawContext drawContext =
    machina::SceneDrawContext{ .renderer = renderer };
  bool showFps = false;

  while (!WindowShouldClose() && !scenes.ShouldQuit()) {
    showFps = showFps != IsKeyPressed(KEY_F1);
    scenes.Update();
    renderer.BeginFrame();

    BeginDrawing();
    ClearBackground(Color{ 63, 63, 63, 255 });
    scenes.Draw(drawContext);
    renderer.Flush();
    scenes.DrawUi();
    if (showFps) {
      DrawFps();
    }
    EndDrawing();
  }

  scenes.Clear();
  CloseWindow();
  return 0;
}
