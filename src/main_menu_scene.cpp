#include <solaris/main_menu_scene.hpp>

#include <algorithm>
#include <filesystem>
#include <machina/path_helpers.hpp>
#include <machina/renderer.hpp>
#include <machina/screen_helpers.hpp>
#include <solaris/game_scene.hpp>
#include <solaris/menu_commands.hpp>
#include <solaris/settings.hpp>
#include <string>
#include <utility>

namespace {

[[nodiscard]] std::filesystem::path
MainMenuMusicPath()
{
  return machina::RuntimeAssetPath() / "sounds" / "mainmenu.ogg";
}

[[nodiscard]] std::string
BoolLiteral(bool value)
{
  return value ? "true" : "false";
}

}

MainMenuScene::MainMenuScene(machina::Renderer& renderer,
                             bool& showFps,
                             solaris::Settings& settings)
  : renderer(renderer)
  , showFps(showFps)
  , settings(settings)
  , webOverlay(std::make_unique<machina::WebOverlay>(
      0,
      0,
      machina::GetVisibleScreenWidth(),
      machina::GetVisibleScreenHeight(),
      "file:///" MACHINA_ASSETS_ROOT "/web/main-menu.html",
      [this](std::string command, std::string payload) {
        HandleWebCommand(std::move(command), std::move(payload));
      },
      [this] {
        webReady = true;
        lastSyncedShowFps = this->showFps;
        PushSettingsToWeb();
      }))
  , musicVolume(settings.musicVolume)
  , musicMuted(settings.musicMuted)
  , lastSyncedShowFps(showFps)
{
  if (!IsAudioDeviceReady()) {
    return;
  }

  const std::string musicPath = MainMenuMusicPath().string();
  music = LoadMusicStream(musicPath.c_str());
  musicValid = IsMusicValid(music);
  if (!musicValid) {
    return;
  }

  music.looping = true;
  ApplyMusicVolume();
  PlayMusicStream(music);
}

MainMenuScene::~MainMenuScene()
{
  if (!musicValid) {
    return;
  }

  StopMusicStream(music);
  UnloadMusicStream(music);
}

void
MainMenuScene::Update(machina::SceneStack& scenes)
{
  if (musicValid) {
    UpdateMusicStream(music);
  }

  (void)webOverlay->Update(true);

  if (webReady && lastSyncedShowFps != showFps) {
    PushSettingsToWeb();
    lastSyncedShowFps = showFps;
  }

  if (quitRequested) {
    scenes.RequestQuit();
    quitRequested = false;
    return;
  }

  if (newGameRequested) {
    StartNewGame(scenes);
  }
}

void
MainMenuScene::Draw(machina::SceneDrawContext& context)
{
  (void)context;
}

void
MainMenuScene::DrawUi()
{
  webOverlay->Draw();
}

void
MainMenuScene::HandleWebCommand(std::string command, std::string payload)
{
  const solaris::MainMenuCommand parsed =
    solaris::ParseMainMenuCommand(command, payload);

  switch (parsed.kind) {
    case solaris::MainMenuCommandKind::NewGame:
      newGameRequested = true;
      break;
    case solaris::MainMenuCommandKind::Quit:
      quitRequested = true;
      break;
    case solaris::MainMenuCommandKind::SetMusicVolume:
      musicVolume = parsed.musicVolume;
      settings.musicVolume = musicVolume;
      ApplyMusicVolume();
      PersistSettings();
      break;
    case solaris::MainMenuCommandKind::SetMusicMuted:
      musicMuted = parsed.musicMuted;
      settings.musicMuted = musicMuted;
      ApplyMusicVolume();
      PersistSettings();
      break;
    case solaris::MainMenuCommandKind::SetShowFps:
      showFps = parsed.showFps;
      settings.showFps = showFps;
      lastSyncedShowFps = showFps;
      PersistSettings();
      break;
    case solaris::MainMenuCommandKind::Unknown:
      break;
  }
}

void
MainMenuScene::ApplyMusicVolume()
{
  if (!musicValid) {
    return;
  }

  const float effectiveVolume =
    musicMuted ? 0.0f : std::clamp(musicVolume, 0.0f, 1.0f);
  SetMusicVolume(music, effectiveVolume);
}

void
MainMenuScene::StartNewGame(machina::SceneStack& scenes)
{
  newGameRequested = false;

  GameScene::CreateResult gameScene =
    GameScene::Create(renderer, showFps, settings);
  if (gameScene.scene == nullptr || !gameScene.diagnostics.empty()) {
    return;
  }

  if (musicValid) {
    StopMusicStream(music);
  }
  scenes.Replace(std::move(gameScene.scene));
}

void
MainMenuScene::PushSettingsToWeb()
{
  const std::string script =
    "window.solarisMenuApplySettings && window.solarisMenuApplySettings(" +
    std::to_string(musicVolume) + ", " + BoolLiteral(musicMuted) + ", " +
    BoolLiteral(showFps) + ");";
  (void)webOverlay->EvaluateScript(script);
}

void
MainMenuScene::PersistSettings() const
{
  solaris::SaveSettings(solaris::DefaultSettingsPath(), settings);
}
