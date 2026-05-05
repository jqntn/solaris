#include <solaris/game_scene.hpp>

#include <machina/level_description.hpp>
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

  GameScene::CreateResult gameScene = GameScene::Create();
  if (!gameScene.diagnostics.empty()) {
    PrintDiagnostics(gameScene.diagnosticLabel, gameScene.diagnostics);
    CloseWindow();
    return 1;
  }

  machina::SceneStack scenes;
  scenes.Push(std::move(gameScene.scene));

  while (!WindowShouldClose() && !scenes.ShouldQuit()) {
    scenes.Update();

    BeginDrawing();
    scenes.Draw();
    EndDrawing();
  }

  scenes.Clear();
  CloseWindow();
  return 0;
}
