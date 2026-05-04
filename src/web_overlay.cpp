#include <AppCore/Platform.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/Listener.h>
#include <Ultralight/MouseEvent.h>
#include <Ultralight/RefPtr.h>
#include <Ultralight/Renderer.h>
#include <Ultralight/ScrollEvent.h>
#include <Ultralight/String.h>
#include <Ultralight/View.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Surface.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <machina/web_overlay.hpp>
#include <memory>
#include <raylib.h>
#include <string_view>
#include <vector>

namespace machina {

namespace {

constexpr std::string_view overlayUrl = "file:///web/poc.html";
constexpr int scrollPixelsPerWheelStep = 32;

struct RaylibTextureDeleter
{
  void operator()(Texture2D* texture) const noexcept
  {
    if (texture != nullptr && texture->id != 0u) {
      UnloadTexture(*texture);
    }

    delete texture;
  }
};

using TextureHandle = std::unique_ptr<Texture2D, RaylibTextureDeleter>;

[[nodiscard]] bool
PointInRect(Vector2 point, int x, int y, int width, int height)
{
  return point.x >= static_cast<float>(x) &&
         point.x < static_cast<float>(x + width) &&
         point.y >= static_cast<float>(y) &&
         point.y < static_cast<float>(y + height);
}

[[nodiscard]] ultralight::MouseEvent::Button
UltralightMouseButton(int raylibButton)
{
  switch (raylibButton) {
    case MOUSE_BUTTON_LEFT:
      return ultralight::MouseEvent::kButton_Left;
    case MOUSE_BUTTON_MIDDLE:
      return ultralight::MouseEvent::kButton_Middle;
    case MOUSE_BUTTON_RIGHT:
      return ultralight::MouseEvent::kButton_Right;
    default:
      return ultralight::MouseEvent::kButton_None;
  }
}

[[nodiscard]] int
RaylibCursor(ultralight::Cursor cursor)
{
  switch (cursor) {
    case ultralight::kCursor_Cross:
    case ultralight::kCursor_Cell:
      return MOUSE_CURSOR_CROSSHAIR;
    case ultralight::kCursor_Hand:
    case ultralight::kCursor_Grab:
    case ultralight::kCursor_Grabbing:
      return MOUSE_CURSOR_POINTING_HAND;
    case ultralight::kCursor_IBeam:
    case ultralight::kCursor_VerticalText:
      return MOUSE_CURSOR_IBEAM;
    case ultralight::kCursor_EastResize:
    case ultralight::kCursor_WestResize:
    case ultralight::kCursor_EastWestResize:
    case ultralight::kCursor_ColumnResize:
      return MOUSE_CURSOR_RESIZE_EW;
    case ultralight::kCursor_NorthResize:
    case ultralight::kCursor_SouthResize:
    case ultralight::kCursor_NorthSouthResize:
    case ultralight::kCursor_RowResize:
      return MOUSE_CURSOR_RESIZE_NS;
    case ultralight::kCursor_NorthEastResize:
    case ultralight::kCursor_SouthWestResize:
    case ultralight::kCursor_NorthEastSouthWestResize:
      return MOUSE_CURSOR_RESIZE_NESW;
    case ultralight::kCursor_NorthWestResize:
    case ultralight::kCursor_SouthEastResize:
    case ultralight::kCursor_NorthWestSouthEastResize:
      return MOUSE_CURSOR_RESIZE_NWSE;
    case ultralight::kCursor_Move:
      return MOUSE_CURSOR_RESIZE_ALL;
    case ultralight::kCursor_NoDrop:
    case ultralight::kCursor_NotAllowed:
      return MOUSE_CURSOR_NOT_ALLOWED;
    default:
      return MOUSE_CURSOR_DEFAULT;
  }
}

[[nodiscard]] TextureHandle
LoadBlankTexture(int width, int height)
{
  Image image = GenImageColor(width, height, BLANK);
  Texture2D texture = LoadTextureFromImage(image);
  UnloadImage(image);

  SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
  return TextureHandle(new Texture2D(texture));
}

}

class WebOverlay::Impl final : public ultralight::ViewListener
{
public:
  Impl(int overlayX, int overlayY, int overlayWidth, int overlayHeight)
    : x(overlayX)
    , y(overlayY)
    , width(overlayWidth)
    , height(overlayHeight)
    , texture(LoadBlankTexture(overlayWidth, overlayHeight))
    , uploadPixels(static_cast<std::size_t>(overlayWidth * overlayHeight * 4))
  {
    ultralight::Config config;
    ultralight::Platform::instance().set_config(config);
    ultralight::Platform::instance().set_font_loader(
      ultralight::GetPlatformFontLoader());
    ultralight::Platform::instance().set_file_system(
      ultralight::GetPlatformFileSystem("./assets"));
    ultralight::Platform::instance().set_logger(
      ultralight::GetDefaultLogger("ultralight.log"));

    renderer = ultralight::Renderer::Create();

    ultralight::ViewConfig viewConfig;
    viewConfig.is_accelerated = false;
    viewConfig.is_transparent = true;
    viewConfig.initial_focus = true;

    view = renderer->CreateView(static_cast<std::uint32_t>(width),
                                static_cast<std::uint32_t>(height),
                                viewConfig,
                                nullptr);
    view->set_view_listener(this);
    view->Focus();
    view->LoadURL(ultralight::String(overlayUrl.data(), overlayUrl.size()));
  }

  ~Impl() override
  {
    if (view) {
      view->set_view_listener(nullptr);
      view = nullptr;
    }

    renderer = nullptr;

    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }

  void Update(bool acceptsInput)
  {
    mouseInside =
      acceptsInput && PointInRect(GetMousePosition(), x, y, width, height);

    if (mouseInside) {
      ForwardMouseInput();
    } else if (wasMouseInside) {
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    wasMouseInside = mouseInside;

    renderer->Update();
    renderer->RefreshDisplay(0);
    renderer->Render();
    UploadIfDirty();
  }

  void Draw() const
  {
    const Texture2D* overlayTexture = texture.get();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexture(*overlayTexture, x, y, WHITE);
    EndBlendMode();
  }

  void OnChangeCursor(ultralight::View* caller,
                      ultralight::Cursor cursor) override
  {
    (void)caller;
    if (mouseInside) {
      SetMouseCursor(RaylibCursor(cursor));
    }
  }

private:
  void ForwardMouseInput()
  {
    const Vector2 mousePosition = GetMousePosition();
    const int localX =
      std::clamp(static_cast<int>(std::floor(mousePosition.x)) - x, 0, width);
    const int localY =
      std::clamp(static_cast<int>(std::floor(mousePosition.y)) - y, 0, height);

    ultralight::MouseEvent moveEvent = {};
    moveEvent.type = ultralight::MouseEvent::kType_MouseMoved;
    moveEvent.x = localX;
    moveEvent.y = localY;
    moveEvent.button = CurrentMouseButton();
    view->FireMouseEvent(moveEvent);

    ForwardMouseButton(localX, localY, MOUSE_BUTTON_LEFT);
    ForwardMouseButton(localX, localY, MOUSE_BUTTON_MIDDLE);
    ForwardMouseButton(localX, localY, MOUSE_BUTTON_RIGHT);
    ForwardScrollInput();
  }

  void ForwardMouseButton(int localX, int localY, int raylibButton)
  {
    const bool isPressed = IsMouseButtonPressed(raylibButton);
    const bool isReleased = IsMouseButtonReleased(raylibButton);
    if (!isPressed && !isReleased) {
      return;
    }

    if (isPressed) {
      view->Focus();
    }

    ultralight::MouseEvent buttonEvent = {};
    buttonEvent.type = isPressed ? ultralight::MouseEvent::kType_MouseDown
                                 : ultralight::MouseEvent::kType_MouseUp;
    buttonEvent.x = localX;
    buttonEvent.y = localY;
    buttonEvent.button = UltralightMouseButton(raylibButton);
    view->FireMouseEvent(buttonEvent);
  }

  void ForwardScrollInput()
  {
    const Vector2 wheel = GetMouseWheelMoveV();
    if (wheel.x == 0.0f && wheel.y == 0.0f) {
      return;
    }

    ultralight::ScrollEvent scrollEvent = {};
    scrollEvent.type = ultralight::ScrollEvent::kType_ScrollByPixel;
    scrollEvent.delta_x = static_cast<int>(wheel.x * scrollPixelsPerWheelStep);
    scrollEvent.delta_y = static_cast<int>(wheel.y * scrollPixelsPerWheelStep);
    view->FireScrollEvent(scrollEvent);
  }

  [[nodiscard]] ultralight::MouseEvent::Button CurrentMouseButton() const
  {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      return ultralight::MouseEvent::kButton_Left;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
      return ultralight::MouseEvent::kButton_Middle;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      return ultralight::MouseEvent::kButton_Right;
    }
    return ultralight::MouseEvent::kButton_None;
  }

  void UploadIfDirty()
  {
    ultralight::BitmapSurface* surface =
      static_cast<ultralight::BitmapSurface*>(view->surface());
    if (surface == nullptr || surface->dirty_bounds().IsEmpty()) {
      return;
    }

    ultralight::RefPtr<ultralight::Bitmap> bitmap = surface->bitmap();
    const std::uint8_t* pixels =
      static_cast<const std::uint8_t*>(bitmap->LockPixels());
    if (pixels == nullptr) {
      return;
    }

    const std::uint32_t bitmapWidth = bitmap->width();
    const std::uint32_t bitmapHeight = bitmap->height();
    const std::uint32_t rowBytes = bitmap->row_bytes();
    const std::size_t requiredSize =
      static_cast<std::size_t>(bitmapWidth * bitmapHeight * 4u);
    uploadPixels.resize(requiredSize);

    for (std::uint32_t row = 0; row < bitmapHeight; ++row) {
      const std::uint8_t* sourceRow =
        pixels + static_cast<std::size_t>(row * rowBytes);
      std::uint8_t* targetRow =
        uploadPixels.data() + static_cast<std::size_t>(row * bitmapWidth * 4u);

      for (std::uint32_t column = 0; column < bitmapWidth; ++column) {
        const std::uint8_t* source = sourceRow + column * 4u;
        std::uint8_t* target = targetRow + column * 4u;
        target[0] = source[2];
        target[1] = source[1];
        target[2] = source[0];
        target[3] = source[3];
      }
    }

    bitmap->UnlockPixels();
    Texture2D* overlayTexture = texture.get();
    UpdateTexture(*overlayTexture, uploadPixels.data());
    surface->ClearDirtyBounds();
  }

  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  TextureHandle texture;
  std::vector<std::uint8_t> uploadPixels;
  ultralight::RefPtr<ultralight::Renderer> renderer;
  ultralight::RefPtr<ultralight::View> view;
  bool mouseInside = false;
  bool wasMouseInside = false;
};

WebOverlay::WebOverlay(int x, int y, int width, int height)
  : impl(std::make_unique<Impl>(x, y, width, height))
{
}

WebOverlay::~WebOverlay() = default;

void
WebOverlay::Update(bool acceptsInput)
{
  impl->Update(acceptsInput);
}

void
WebOverlay::Draw() const
{
  impl->Draw();
}

}
