#include <solaris/menu_commands.hpp>

#include <algorithm>
#include <charconv>
#include <system_error>

namespace solaris {

namespace {

[[nodiscard]] bool
ParseFloat(std::string_view value, float& parsed)
{
  const char* begin = value.data();
  const char* end = value.data() + value.size();
  const std::from_chars_result result = std::from_chars(begin, end, parsed);
  return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] bool
ParseBool(std::string_view value, bool& parsed)
{
  if (value == "true" || value == "1" || value == "on") {
    parsed = true;
    return true;
  }

  if (value == "false" || value == "0" || value == "off") {
    parsed = false;
    return true;
  }

  return false;
}

}

MainMenuCommand
ParseMainMenuCommand(std::string_view command, std::string_view payload)
{
  if (command == "new_game") {
    return MainMenuCommand{ .kind = MainMenuCommandKind::NewGame };
  }

  if (command == "quit") {
    return MainMenuCommand{ .kind = MainMenuCommandKind::Quit };
  }

  if (command == "set_music_volume") {
    float volume = 0.0f;
    if (!ParseFloat(payload, volume)) {
      return MainMenuCommand{};
    }

    return MainMenuCommand{ .kind = MainMenuCommandKind::SetMusicVolume,
                            .musicVolume = std::clamp(volume, 0.0f, 1.0f) };
  }

  if (command == "set_music_muted") {
    bool muted = false;
    if (!ParseBool(payload, muted)) {
      return MainMenuCommand{};
    }

    return MainMenuCommand{ .kind = MainMenuCommandKind::SetMusicMuted,
                            .musicMuted = muted };
  }

  return MainMenuCommand{};
}

}
