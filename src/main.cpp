#include <solaris/main_menu_scene.hpp>

#include <machina/renderer.hpp>
#include <machina/scene.hpp>
#include <memory>
#include <raylib.h>

extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance =
    0x00000001;
}

namespace {

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
  InitAudioDevice();

  machina::Renderer renderer;
  machina::SceneStack scenes;
  bool showFps = false;
  scenes.Push(std::make_unique<MainMenuScene>(renderer, showFps));
  machina::SceneDrawContext drawContext =
    machina::SceneDrawContext{ .renderer = renderer };

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
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
