#pragma once

#include <machina/scene.hpp>
#include <machina/web_overlay.hpp>
#include <memory>
#include <raylib.h>
#include <string>
#include <string_view>

namespace machina {
class Renderer;
}

class MainMenuScene final : public machina::Scene
{
public:
  explicit MainMenuScene(machina::Renderer& renderer, bool& showFps);
  ~MainMenuScene() override;

  void Update(machina::SceneStack& scenes) override;
  void Draw(machina::SceneDrawContext& context) override;
  void DrawUi() override;

private:
  void HandleWebCommand(std::string command, std::string payload);
  void ApplyMusicVolume();
  void StartNewGame(machina::SceneStack& scenes);
  void SetMenuStatus(std::string_view tone, std::string_view message) const;

  machina::Renderer& renderer;
  bool& showFps;
  std::unique_ptr<machina::WebOverlay> webOverlay;
  Music music = Music{};
  float musicVolume = 0.25f;
  bool musicMuted = false;
  bool musicValid = false;
  bool newGameRequested = false;
  bool quitRequested = false;
};
