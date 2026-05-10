#include <AppCore/JSHelpers.h>
#include <AppCore/Platform.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/KeyCodes.h>
#include <Ultralight/KeyEvent.h>
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
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <machina/path_helpers.hpp>
#include <machina/web_overlay.hpp>
#include <memory>
#include <raylib.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace machina {

namespace {

constexpr int scrollPixelsPerWheelStep = 32;
constexpr std::string_view hitTestScriptPrefix = R"js(
(function() {
  const checkMouse = )js";
constexpr std::string_view hitTestScriptMiddle = R"js(;
  const x = )js";
constexpr std::string_view hitTestScriptBody = R"js(;
  const y = )js";
constexpr std::string_view hitTestScriptSuffix = R"js(;

  function visibleElement(node) {
    if (!node || node.nodeType !== 1) {
      return false;
    }

    const style = getComputedStyle(node);
    return style.pointerEvents !== 'none' &&
      style.visibility !== 'hidden' &&
      style.display !== 'none' &&
      Number(style.opacity || '1') > 0.01;
  }

  function capturesMouseAtPoint() {
    if (!checkMouse) {
      return false;
    }

    let node = document.elementFromPoint(x, y);
    while (node && node !== document.documentElement) {
      if (node === document.body) {
        return false;
      }

      if (visibleElement(node)) {
        const rect = node.getBoundingClientRect();
        if (rect.width > 0 && rect.height > 0) {
          return true;
        }
      }

      node = node.parentElement;
    }

    return false;
  }

  function capturesKeyboard() {
    const node = document.activeElement;
    if (!node || node === document.body || node === document.documentElement) {
      return false;
    }

    const tag = node.tagName ? node.tagName.toLowerCase() : '';
    return tag === 'input' ||
      tag === 'textarea' ||
      tag === 'select' ||
      node.isContentEditable === true;
  }

  return (capturesMouseAtPoint() ? '1' : '0') + '|' +
    (capturesKeyboard() ? '1' : '0');
})()
)js";

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

[[nodiscard]] std::string
ToStdString(const ultralight::String& value)
{
  return std::string(value.utf8().data(), value.utf8().length());
}

[[nodiscard]] bool
ShiftKeyDown()
{
  return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}

[[nodiscard]] unsigned
KeyboardModifiers()
{
  unsigned modifiers = 0u;
  if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
    modifiers |= ultralight::KeyEvent::kMod_AltKey;
  }
  if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
    modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
  }
  if (ShiftKeyDown()) {
    modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
  }
  return modifiers;
}

[[nodiscard]] int
UltralightVirtualKey(int raylibKey)
{
  if ((raylibKey >= KEY_ZERO && raylibKey <= KEY_NINE) ||
      (raylibKey >= KEY_A && raylibKey <= KEY_Z)) {
    return raylibKey;
  }

  switch (raylibKey) {
    case KEY_BACKSPACE:
      return ultralight::KeyCodes::GK_BACK;
    case KEY_TAB:
      return ultralight::KeyCodes::GK_TAB;
    case KEY_ENTER:
    case KEY_KP_ENTER:
      return ultralight::KeyCodes::GK_RETURN;
    case KEY_ESCAPE:
      return ultralight::KeyCodes::GK_ESCAPE;
    case KEY_SPACE:
      return ultralight::KeyCodes::GK_SPACE;
    case KEY_LEFT:
      return ultralight::KeyCodes::GK_LEFT;
    case KEY_UP:
      return ultralight::KeyCodes::GK_UP;
    case KEY_RIGHT:
      return ultralight::KeyCodes::GK_RIGHT;
    case KEY_DOWN:
      return ultralight::KeyCodes::GK_DOWN;
    case KEY_INSERT:
      return ultralight::KeyCodes::GK_INSERT;
    case KEY_DELETE:
      return ultralight::KeyCodes::GK_DELETE;
    case KEY_HOME:
      return ultralight::KeyCodes::GK_HOME;
    case KEY_END:
      return ultralight::KeyCodes::GK_END;
    case KEY_PAGE_UP:
      return ultralight::KeyCodes::GK_PRIOR;
    case KEY_PAGE_DOWN:
      return ultralight::KeyCodes::GK_NEXT;
    default:
      return 0;
  }
}

[[nodiscard]] ultralight::KeyEvent
MakeKeyEvent(ultralight::KeyEvent::Type type, int raylibKey, int virtualKey)
{
  ultralight::KeyEvent event = ultralight::KeyEvent{};
  event.type = type;
  event.modifiers = KeyboardModifiers();
  event.virtual_key_code = virtualKey;
  event.native_key_code = raylibKey;
  event.is_keypad = raylibKey == KEY_KP_ENTER ||
                    (raylibKey >= KEY_KP_0 && raylibKey <= KEY_KP_EQUAL);
  ultralight::GetKeyIdentifierFromVirtualKeyCode(virtualKey,
                                                 event.key_identifier);
  ultralight::GetKeyFromVirtualKeyCode(virtualKey, ShiftKeyDown(), event.text);
  ultralight::GetKeyFromVirtualKeyCode(
    virtualKey, false, event.unmodified_text);
  return event;
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

[[nodiscard]] std::size_t
MouseButtonIndex(int raylibButton)
{
  switch (raylibButton) {
    case MOUSE_BUTTON_LEFT:
      return 0;
    case MOUSE_BUTTON_MIDDLE:
      return 1;
    case MOUSE_BUTTON_RIGHT:
      return 2;
    default:
      return 0;
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
  std::unique_ptr<Texture2D> loadedTexture =
    std::make_unique<Texture2D>(texture);
  return TextureHandle(loadedTexture.release());
}

void
DrainKeyboardInput()
{
  while (GetKeyPressed() != 0) {
  }
}

void
InitializeUltralightPlatform()
{
  static const bool initialized = [] {
    const std::filesystem::path ultralightDir =
      ApplicationDirectory() / MACHINA_ULTRALIGHT_ROOT;
    const std::string ultralightCachePath = ultralightDir.string();
    const std::string ultralightLogPath =
      (ultralightDir / "ultralight.log").string();
    const std::string vfsRoot = ApplicationDirectory().string();

    ultralight::Config config;
    config.cache_path = ultralightCachePath.c_str();
    config.resource_path_prefix = MACHINA_ULTRALIGHT_ROOT "/resources/";
    ultralight::Platform::instance().set_config(config);
    ultralight::Platform::instance().set_font_loader(
      ultralight::GetPlatformFontLoader());
    ultralight::Platform::instance().set_file_system(
      ultralight::GetPlatformFileSystem(vfsRoot.c_str()));
    ultralight::Platform::instance().set_logger(
      ultralight::GetDefaultLogger(ultralightLogPath.c_str()));
    return true;
  }();
  (void)initialized;
}

[[nodiscard]] ultralight::RefPtr<ultralight::Renderer>
SharedUltralightRenderer()
{
  InitializeUltralightPlatform();
  static ultralight::RefPtr<ultralight::Renderer> renderer =
    ultralight::Renderer::Create();
  return renderer;
}

}

class WebOverlay::Impl final
  : public ultralight::LoadListener
  , public ultralight::ViewListener
{
public:
  Impl(int overlayX,
       int overlayY,
       int overlayWidth,
       int overlayHeight,
       std::string overlayPageUrl,
       WebOverlayCommandHandler overlayCommandHandler,
       WebOverlayReadyHandler overlayReadyHandler)
    : x(overlayX)
    , y(overlayY)
    , width(overlayWidth)
    , height(overlayHeight)
    , pageUrl(std::move(overlayPageUrl))
    , commandHandler(std::move(overlayCommandHandler))
    , readyHandler(std::move(overlayReadyHandler))
    , texture(LoadBlankTexture(overlayWidth, overlayHeight))
    , uploadPixels(static_cast<std::size_t>(overlayWidth * overlayHeight * 4))
  {
    renderer = SharedUltralightRenderer();

    ultralight::ViewConfig viewConfig;
    viewConfig.is_accelerated = false;
    viewConfig.is_transparent = true;
    viewConfig.initial_focus = true;

    view = renderer->CreateView(static_cast<std::uint32_t>(width),
                                static_cast<std::uint32_t>(height),
                                viewConfig,
                                nullptr);
    view->set_load_listener(this);
    view->set_view_listener(this);
    view->Focus();
    view->LoadURL(ultralight::String(pageUrl.data(), pageUrl.size()));
  }

  ~Impl() override
  {
    if (view) {
      view->set_load_listener(nullptr);
      view->set_view_listener(nullptr);
      view = nullptr;
    }

    renderer = nullptr;

    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }

  WebOverlayInputCapture Update(bool acceptsInput)
  {
    mouseInside =
      acceptsInput && PointInRect(GetMousePosition(), x, y, width, height);
    WebOverlayInputCapture inputCapture =
      acceptsInput ? QueryInputCapture() : WebOverlayInputCapture{};

    if (mouseInside) {
      if (AnyCapturedMouseButton()) {
        inputCapture.mouse = true;
      }

      ForwardMouseInput(inputCapture.mouse);
      if (!inputCapture.mouse && !AnyCapturedMouseButton()) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
      }
    } else if (wasMouseInside) {
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    if (acceptsInput) {
      if (IsWindowFocused()) {
        ForwardKeyboardInput();
      } else {
        DrainKeyboardInput();
      }
    }

    wasMouseInside = mouseInside;

    renderer->Update();
    renderer->RefreshDisplay(0);
    renderer->Render();
    UploadIfDirty();

    if (readyPending && !readyDelivered) {
      readyPending = false;
      readyDelivered = true;
      if (readyHandler) {
        readyHandler();
      }
    }

    return inputCapture;
  }

  void Draw() const
  {
    const Texture2D* overlayTexture = texture.get();
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    DrawTexture(*overlayTexture, x, y, WHITE);
    EndBlendMode();
  }

  [[nodiscard]] bool EvaluateScript(std::string_view script) const
  {
    ultralight::String exception;
    view->EvaluateScript(ultralight::String(script.data(), script.size()),
                         &exception);
    return exception.empty();
  }

  void OnChangeCursor(ultralight::View* caller,
                      ultralight::Cursor cursor) override
  {
    (void)caller;
    if (mouseInside) {
      SetMouseCursor(RaylibCursor(cursor));
    }
  }

  void OnDOMReady(ultralight::View* caller,
                  std::uint64_t frameId,
                  bool isMainFrame,
                  const ultralight::String& url) override
  {
    (void)frameId;
    (void)url;
    if (!isMainFrame) {
      return;
    }

    view->Focus();

    if (commandHandler) {
      ultralight::RefPtr<ultralight::JSContext> context =
        caller->LockJSContext();
      ultralight::SetJSContext(context->ctx());
      ultralight::JSObject global = ultralight::JSGlobalObject();
      global["solarisNativeCommand"] =
        ultralight::JSCallback(std::bind(&WebOverlay::Impl::OnNativeCommand,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    }

    if (!readyDelivered) {
      readyPending = true;
    }
  }

private:
  void OnNativeCommand(const ultralight::JSObject& thisObject,
                       const ultralight::JSArgs& args)
  {
    (void)thisObject;
    if (!commandHandler || args.empty()) {
      return;
    }

    const ultralight::String commandString = args[0].ToString();
    std::string payload;
    if (args.size() >= 2) {
      const ultralight::String payloadString = args[1].ToString();
      payload = ToStdString(payloadString);
    }

    commandHandler(ToStdString(commandString), std::move(payload));
  }

  [[nodiscard]] WebOverlayInputCapture QueryInputCapture() const
  {
    const Vector2 mousePosition = GetMousePosition();
    const int localX =
      std::clamp(static_cast<int>(std::floor(mousePosition.x)) - x, 0, width);
    const int localY =
      std::clamp(static_cast<int>(std::floor(mousePosition.y)) - y, 0, height);
    const std::string result =
      EvaluateString(HitTestScript(mouseInside, localX, localY));

    if (result.size() < 3) {
      return WebOverlayInputCapture{};
    }

    return WebOverlayInputCapture{ .mouse = result[0] == '1',
                                   .keyboard = result[2] == '1' };
  }

  [[nodiscard]] std::string HitTestScript(bool checkMouse,
                                          int localX,
                                          int localY) const
  {
    std::string script;
    script.reserve(hitTestScriptPrefix.size() + hitTestScriptMiddle.size() +
                   hitTestScriptBody.size() + hitTestScriptSuffix.size() + 32);
    script += hitTestScriptPrefix;
    script += checkMouse ? "true" : "false";
    script += hitTestScriptMiddle;
    script += std::to_string(localX);
    script += hitTestScriptBody;
    script += std::to_string(localY);
    script += hitTestScriptSuffix;
    return script;
  }

  [[nodiscard]] std::string EvaluateString(std::string_view script) const
  {
    ultralight::String exception;
    const ultralight::String result = view->EvaluateScript(
      ultralight::String(script.data(), script.size()), &exception);
    if (!exception.empty() || result.empty()) {
      return std::string{};
    }

    return std::string(result.utf8().data(), result.utf8().length());
  }

  void ForwardMouseInput(bool mouseCaptured)
  {
    const Vector2 mousePosition = GetMousePosition();
    const int localX =
      std::clamp(static_cast<int>(std::floor(mousePosition.x)) - x, 0, width);
    const int localY =
      std::clamp(static_cast<int>(std::floor(mousePosition.y)) - y, 0, height);

    ultralight::MouseEvent moveEvent = ultralight::MouseEvent{};
    moveEvent.type = ultralight::MouseEvent::kType_MouseMoved;
    moveEvent.x = localX;
    moveEvent.y = localY;
    moveEvent.button = CurrentCapturedMouseButton();
    view->FireMouseEvent(moveEvent);

    ForwardMouseButton(localX, localY, MOUSE_BUTTON_LEFT, mouseCaptured);
    ForwardMouseButton(localX, localY, MOUSE_BUTTON_MIDDLE, mouseCaptured);
    ForwardMouseButton(localX, localY, MOUSE_BUTTON_RIGHT, mouseCaptured);
    if (mouseCaptured) {
      ForwardScrollInput();
    }
  }

  void ForwardMouseButton(int localX,
                          int localY,
                          int raylibButton,
                          bool mouseCaptured)
  {
    const bool isPressed = IsMouseButtonPressed(raylibButton);
    const bool isReleased = IsMouseButtonReleased(raylibButton);
    if (!isPressed && !isReleased) {
      return;
    }

    const std::size_t buttonIndex = MouseButtonIndex(raylibButton);
    if (isPressed && mouseCaptured) {
      capturedMouseButtons[buttonIndex] = true;
    }

    if (!mouseCaptured && !capturedMouseButtons[buttonIndex]) {
      return;
    }

    if (isPressed) {
      view->Focus();
    }

    ultralight::MouseEvent buttonEvent = ultralight::MouseEvent{};
    buttonEvent.type = isPressed ? ultralight::MouseEvent::kType_MouseDown
                                 : ultralight::MouseEvent::kType_MouseUp;
    buttonEvent.x = localX;
    buttonEvent.y = localY;
    buttonEvent.button = UltralightMouseButton(raylibButton);
    view->FireMouseEvent(buttonEvent);

    if (isReleased) {
      capturedMouseButtons[buttonIndex] = false;
    }
  }

  void ForwardScrollInput()
  {
    const Vector2 wheel = GetMouseWheelMoveV();
    if (wheel.x == 0.0f && wheel.y == 0.0f) {
      return;
    }

    ultralight::ScrollEvent scrollEvent = ultralight::ScrollEvent{};
    scrollEvent.type = ultralight::ScrollEvent::kType_ScrollByPixel;
    scrollEvent.delta_x = static_cast<int>(wheel.x * scrollPixelsPerWheelStep);
    scrollEvent.delta_y = static_cast<int>(wheel.y * scrollPixelsPerWheelStep);
    view->FireScrollEvent(scrollEvent);
  }

  void ForwardKeyboardInput()
  {
    int raylibKey = GetKeyPressed();
    while (raylibKey != 0) {
      const int virtualKey = UltralightVirtualKey(raylibKey);
      if (virtualKey != 0) {
        view->FireKeyEvent(MakeKeyEvent(
          ultralight::KeyEvent::kType_RawKeyDown, raylibKey, virtualKey));
      }
      raylibKey = GetKeyPressed();
    }
  }

  [[nodiscard]] ultralight::MouseEvent::Button CurrentCapturedMouseButton()
    const
  {
    if (capturedMouseButtons[MouseButtonIndex(MOUSE_BUTTON_LEFT)] &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      return ultralight::MouseEvent::kButton_Left;
    }
    if (capturedMouseButtons[MouseButtonIndex(MOUSE_BUTTON_MIDDLE)] &&
        IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
      return ultralight::MouseEvent::kButton_Middle;
    }
    if (capturedMouseButtons[MouseButtonIndex(MOUSE_BUTTON_RIGHT)] &&
        IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      return ultralight::MouseEvent::kButton_Right;
    }
    return ultralight::MouseEvent::kButton_None;
  }

  [[nodiscard]] bool AnyCapturedMouseButton() const
  {
    return capturedMouseButtons[0] || capturedMouseButtons[1] ||
           capturedMouseButtons[2];
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
  std::string pageUrl;
  WebOverlayCommandHandler commandHandler;
  WebOverlayReadyHandler readyHandler;
  TextureHandle texture;
  std::vector<std::uint8_t> uploadPixels;
  ultralight::RefPtr<ultralight::Renderer> renderer;
  ultralight::RefPtr<ultralight::View> view;
  std::array<bool, 3> capturedMouseButtons = std::array<bool, 3>{};
  bool mouseInside = false;
  bool wasMouseInside = false;
  bool readyPending = false;
  bool readyDelivered = false;
};

WebOverlay::WebOverlay(int x,
                       int y,
                       int width,
                       int height,
                       std::string pageUrl,
                       WebOverlayCommandHandler commandHandler,
                       WebOverlayReadyHandler readyHandler)
  : impl(std::make_unique<Impl>(x,
                                y,
                                width,
                                height,
                                std::move(pageUrl),
                                std::move(commandHandler),
                                std::move(readyHandler)))
{
}

WebOverlay::~WebOverlay() = default;

WebOverlayInputCapture
WebOverlay::Update(bool acceptsInput)
{
  return impl->Update(acceptsInput);
}

bool
WebOverlay::EvaluateScript(std::string_view script) const
{
  return impl->EvaluateScript(script);
}

void
WebOverlay::Draw() const
{
  impl->Draw();
}

}
