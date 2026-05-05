#include <solaris/menu_commands.hpp>

#include <print>
#include <string_view>

namespace {

[[nodiscard]] int
Fail(std::string_view message)
{
  std::println("{}", message);
  return 1;
}

[[nodiscard]] int
CheckActionCommands()
{
  const solaris::MainMenuCommand newGame =
    solaris::ParseMainMenuCommand("new_game", "");
  if (newGame.kind != solaris::MainMenuCommandKind::NewGame) {
    return Fail("expected new_game command to parse");
  }

  const solaris::MainMenuCommand quit =
    solaris::ParseMainMenuCommand("quit", "");
  if (quit.kind != solaris::MainMenuCommandKind::Quit) {
    return Fail("expected quit command to parse");
  }

  const solaris::MainMenuCommand unknown =
    solaris::ParseMainMenuCommand("load_latest", "");
  if (unknown.kind != solaris::MainMenuCommandKind::Unknown) {
    return Fail("expected unknown command to stay unknown");
  }

  return 0;
}

[[nodiscard]] int
CheckVolumeClamping()
{
  const solaris::MainMenuCommand low =
    solaris::ParseMainMenuCommand("set_music_volume", "-0.25");
  if (low.kind != solaris::MainMenuCommandKind::SetMusicVolume ||
      low.musicVolume != 0.0f) {
    return Fail("expected low volume payload to clamp to zero");
  }

  const solaris::MainMenuCommand high =
    solaris::ParseMainMenuCommand("set_music_volume", "1.5");
  if (high.kind != solaris::MainMenuCommandKind::SetMusicVolume ||
      high.musicVolume != 1.0f) {
    return Fail("expected high volume payload to clamp to one");
  }

  const solaris::MainMenuCommand invalid =
    solaris::ParseMainMenuCommand("set_music_volume", "quiet");
  if (invalid.kind != solaris::MainMenuCommandKind::Unknown) {
    return Fail("expected invalid volume payload to be ignored");
  }

  return 0;
}

[[nodiscard]] int
CheckMuteParsing()
{
  const solaris::MainMenuCommand muted =
    solaris::ParseMainMenuCommand("set_music_muted", "true");
  if (muted.kind != solaris::MainMenuCommandKind::SetMusicMuted ||
      !muted.musicMuted) {
    return Fail("expected true mute payload to parse");
  }

  const solaris::MainMenuCommand unmuted =
    solaris::ParseMainMenuCommand("set_music_muted", "0");
  if (unmuted.kind != solaris::MainMenuCommandKind::SetMusicMuted ||
      unmuted.musicMuted) {
    return Fail("expected false mute payload to parse");
  }

  const solaris::MainMenuCommand invalid =
    solaris::ParseMainMenuCommand("set_music_muted", "maybe");
  if (invalid.kind != solaris::MainMenuCommandKind::Unknown) {
    return Fail("expected invalid mute payload to be ignored");
  }

  return 0;
}

}

int
main()
{
  if (const int result = CheckActionCommands(); result != 0) {
    return result;
  }
  if (const int result = CheckVolumeClamping(); result != 0) {
    return result;
  }
  if (const int result = CheckMuteParsing(); result != 0) {
    return result;
  }

  return 0;
}
