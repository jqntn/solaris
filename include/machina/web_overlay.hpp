#pragma once

#include <memory>

namespace machina {

struct WebOverlayInputCapture
{
  bool mouse = false;
  bool keyboard = false;
};

class WebOverlay
{
public:
  WebOverlay(int x, int y, int width, int height);
  ~WebOverlay();

  [[nodiscard]] WebOverlayInputCapture Update(bool acceptsInput);
  void Draw() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

}
