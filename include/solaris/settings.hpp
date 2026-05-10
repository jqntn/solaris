#pragma once

#include <filesystem>

namespace solaris {

struct Settings
{
  float musicVolume = 0.25f;
  bool musicMuted = false;
  bool showFps = false;
};

[[nodiscard]] Settings
LoadSettings(const std::filesystem::path& file);
void
SaveSettings(const std::filesystem::path& file, const Settings& settings);

[[nodiscard]] std::filesystem::path
DefaultSettingsPath();

}
