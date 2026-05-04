#pragma once

#include <memory>

namespace machina {

class WebOverlay
{
public:
  WebOverlay(int x, int y, int width, int height);
  ~WebOverlay();

  void Update(bool acceptsInput);
  void Draw() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl;
};

}
