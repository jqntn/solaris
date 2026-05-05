#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace machina {

struct WebOverlayInputCapture
{
  bool mouse = false;
  bool keyboard = false;
};

using WebOverlayCommandHandler =
  std::function<void(std::string command, std::string payload)>;

class WebOverlay
{
public:
  WebOverlay(int x,
             int y,
             int width,
             int height,
             std::string pageUrl,
             WebOverlayCommandHandler commandHandler = {});
  ~WebOverlay();

  [[nodiscard]] WebOverlayInputCapture Update(bool acceptsInput);
  [[nodiscard]] bool EvaluateScript(std::string_view script) const;
  void Draw() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

}
