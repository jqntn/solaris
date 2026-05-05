#pragma once

#include <string_view>

namespace solaris {

enum class MainMenuCommandKind
{
  Unknown,
  NewGame,
  Quit,
  SetMusicVolume,
  SetMusicMuted,
  SetShowFps,
};

struct MainMenuCommand
{
  MainMenuCommandKind kind = MainMenuCommandKind::Unknown;
  float musicVolume = 0.0f;
  bool musicMuted = false;
  bool showFps = false;
};

[[nodiscard]] MainMenuCommand
ParseMainMenuCommand(std::string_view command, std::string_view payload);

}
