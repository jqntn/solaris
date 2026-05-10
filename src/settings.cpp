#include <solaris/settings.hpp>

#include <fstream>
#include <machina/path_helpers.hpp>
#include <nlohmann/json.hpp>
#include <raylib.h>

namespace solaris {

namespace {

constexpr int settingsVersion = 1;

}

Settings
LoadSettings(const std::filesystem::path& file)
{
  Settings defaults;

  std::ifstream stream(file);
  if (!stream) {
    return defaults;
  }

  nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
  if (json.is_discarded() || !json.is_object()) {
    TraceLog(LOG_WARNING,
             "settings: '%s' is not valid JSON; using defaults",
             file.string().c_str());
    return defaults;
  }

  const int version = json.value("version", 0);
  if (version != settingsVersion) {
    TraceLog(LOG_WARNING,
             "settings: unexpected version %d in '%s'; using defaults",
             version,
             file.string().c_str());
    return defaults;
  }

  Settings settings;
  settings.musicVolume = json.value("musicVolume", defaults.musicVolume);
  settings.musicMuted = json.value("musicMuted", defaults.musicMuted);
  settings.showFps = json.value("showFps", defaults.showFps);
  return settings;
}

void
SaveSettings(const std::filesystem::path& file, const Settings& settings)
{
  const nlohmann::json json = {
    { "version", settingsVersion },
    { "musicVolume", settings.musicVolume },
    { "musicMuted", settings.musicMuted },
    { "showFps", settings.showFps },
  };

  std::ofstream stream(file);
  if (!stream) {
    TraceLog(LOG_WARNING,
             "settings: failed to open '%s' for writing",
             file.string().c_str());
    return;
  }

  stream << json.dump(2);
}

std::filesystem::path
DefaultSettingsPath()
{
  return machina::ApplicationDirectory() / "settings.json";
}

}
