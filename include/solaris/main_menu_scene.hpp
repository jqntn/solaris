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
  explicit MainMenuScene(machina::Renderer& renderer);
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
  std::unique_ptr<machina::WebOverlay> webOverlay;
  Music music = Music{};
  float musicVolume = 0.72f;
  bool musicMuted = false;
  bool musicValid = false;
  bool newGameRequested = false;
  bool quitRequested = false;
};
