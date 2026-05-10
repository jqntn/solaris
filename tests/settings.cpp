#include <solaris/settings.hpp>

#include <filesystem>
#include <fstream>
#include <print>
#include <string_view>
#include <system_error>

namespace {

[[nodiscard]] int
Fail(std::string_view message)
{
  std::println("{}", message);
  return 1;
}

[[nodiscard]] std::filesystem::path
ScratchPath()
{
  std::filesystem::path path = std::filesystem::temp_directory_path();
  path /= "solaris_settings_test.json";
  std::error_code error;
  std::filesystem::remove(path, error);
  return path;
}

[[nodiscard]] int
CheckMissingFileReturnsDefaults()
{
  const std::filesystem::path path = ScratchPath();
  const solaris::Settings loaded = solaris::LoadSettings(path);
  const solaris::Settings defaults;

  if (loaded.musicVolume != defaults.musicVolume ||
      loaded.musicMuted != defaults.musicMuted ||
      loaded.showFps != defaults.showFps) {
    return Fail("expected missing settings file to yield defaults");
  }

  return 0;
}

[[nodiscard]] int
CheckRoundTrip()
{
  const std::filesystem::path path = ScratchPath();
  const solaris::Settings written = { .musicVolume = 0.6f,
                                      .musicMuted = true,
                                      .showFps = true };
  solaris::SaveSettings(path, written);
  const solaris::Settings loaded = solaris::LoadSettings(path);

  std::error_code error;
  std::filesystem::remove(path, error);

  if (loaded.musicVolume != written.musicVolume ||
      loaded.musicMuted != written.musicMuted ||
      loaded.showFps != written.showFps) {
    return Fail("expected save+load to round-trip values");
  }

  return 0;
}

[[nodiscard]] int
CheckCorruptFileReturnsDefaults()
{
  const std::filesystem::path path = ScratchPath();
  {
    std::ofstream stream(path);
    stream << "{garbage";
  }

  const solaris::Settings loaded = solaris::LoadSettings(path);
  const solaris::Settings defaults;

  std::error_code error;
  std::filesystem::remove(path, error);

  if (loaded.musicVolume != defaults.musicVolume ||
      loaded.musicMuted != defaults.musicMuted ||
      loaded.showFps != defaults.showFps) {
    return Fail("expected corrupt settings file to yield defaults");
  }

  return 0;
}

}

int
main()
{
  if (const int result = CheckMissingFileReturnsDefaults(); result != 0) {
    return result;
  }
  if (const int result = CheckRoundTrip(); result != 0) {
    return result;
  }
  if (const int result = CheckCorruptFileReturnsDefaults(); result != 0) {
    return result;
  }

  return 0;
}
