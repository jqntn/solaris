#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <raylib.h>
#include <resources.h>
#include <shaders.h>
#include <string>

#define RAYGUI_IMPLEMENTATION
#include <raygui/raygui.h>
#include <raygui/style_cyber.h>

// ==============
// === DRIVER ===
// ==============

extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance =
    0x00000001;
}

// =============
// === TYPES ===
// =============

class IScene
{
public:
  virtual ~IScene() {}
  virtual void tick() = 0;
  virtual void draw() = 0;
};

// =================
// === CONSTANTS ===
// =================

constexpr uint16_t REF_RES = 1600;

// =============
// === UTILS ===
// =============

static float
GetScalingFactor()
{
  return (float)GetScreenHeight() / REF_RES;
}

static void
DrawFPS()
{
  DrawText(TextFormat("%d", GetFPS()), 8, 4, 30, GREEN);
}

static Model
LoadModelFromMemory(const char* fileType,
                    const unsigned char* fileData,
                    int dataSize)
{
  namespace fs = std::filesystem;

  fs::path file_path =
    fs::temp_directory_path() / ("out" + std::string(fileType));

  std::ofstream out(file_path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(fileData), dataSize);
  out.close();

  Model model = LoadModel(file_path.string().c_str());

  fs::remove(file_path);

  return model;
}

// ===============
// === GLOBALS ===
// ===============

std::unique_ptr<IScene> g_current_scene;

// ==============
// === SCENES ===
// ==============

class MainMenu final : public IScene
{
private:
  Texture m_texture;
  Font m_font;
  Music m_music;

  uint32_t m_font_size = 256;
  uint32_t m_font_spacing = 64;

public:
  ~MainMenu() { UnloadTexture(m_texture); }
  MainMenu()
  {
    GuiLoadStyleCyber();

    m_font_size *= GetScalingFactor();
    m_font_spacing *= GetScalingFactor();

    m_texture = LoadTexture("sresources/graphics/mainmenu.jpg");
    m_font = LoadFontEx("sresources/fonts/Orbitron/Orbitron-Regular.ttf",
                        m_font_size,
                        nullptr,
                        0);
    m_music = LoadMusicStream("sresources/sounds/mainmenu.ogg");

    SetMusicVolume(m_music, 0.25f);
    PlayMusicStream(m_music);
  }

  void tick() override { UpdateMusicStream(m_music); }
  void draw() override
  {
    /* STATICS */

    static float bg_scale = (float)GetScreenHeight() / m_texture.height;
    static Vector2 bg_position =
      Vector2{ (GetScreenWidth() - m_texture.width * bg_scale) / 2.0f, 0.0f };

    constexpr const char* text = "SOLARIS";
    static Vector2 text_size =
      MeasureTextEx(m_font, text, m_font_size, m_font_spacing);
    static Vector2 text_pos =
      Vector2{ (GetScreenWidth() - text_size.x) / 2.0f, 150.0f };

    /* */

    BeginDrawing();
    ClearBackground(BLACK);

    DrawTextureEx(m_texture, bg_position, 0.0f, bg_scale, WHITE);
    DrawTextEx(m_font, text, text_pos, m_font_size, m_font_spacing, WHITE);

    EndDrawing();

    // GuiButton(Rectangle{ 25, 255, 125, 30 }, "QUIT");
  }
};

class TestScene0 final : public IScene
{
private:
  Model m_model;
  Shader m_shader;
  RenderTexture2D m_target_0;
  RenderTexture2D m_target_1;

  Camera m_camera;
  Vector3 m_position;

  int m_pass_loc = 0;
  int m_pass = 0;

public:
  ~TestScene0()
  {
    UnloadModel(m_model);
    UnloadShader(m_shader);
    UnloadRenderTexture(m_target_0);
    UnloadRenderTexture(m_target_1);
  }
  TestScene0()
  {
    uint32_t screen_width = GetScreenWidth();
    uint32_t screen_height = GetScreenHeight();

    m_model = LoadModelFromMemory(".glb", helmet_glb, helmet_glb_len);
    m_shader = LoadShaderFromMemory(
      NULL,
      std::string(reinterpret_cast<char*>(blur_frag), blur_frag_len).c_str());

    m_pass_loc = GetShaderLocation(m_shader, "pass");
    m_pass = 0;

    m_target_0 = LoadRenderTexture(screen_width, screen_height);
    m_target_1 = LoadRenderTexture(screen_width, screen_height);

    m_position = Vector3{ 0.0f, 1.0f, 0.0f };
    m_camera = {
      .position = Vector3{ 2.0f, 2.0f, 2.0f },
      .target = m_position,
      .up = Vector3{ 0.0f, 1.0f, 0.0f },
      .fovy = 50.0f,
      .projection = CAMERA_PERSPECTIVE,
    };
  }

  void tick() override {}
  void draw() override
  {
    UpdateCamera(&m_camera, CAMERA_ORBITAL);

    BeginTextureMode(m_target_0);
    ClearBackground(RAYWHITE);
    BeginMode3D(m_camera);
    DrawModel(m_model, m_position, 1.0f, WHITE);
    DrawGrid(10, 1.0f);
    EndMode3D();
    EndTextureMode();

    BeginTextureMode(m_target_1);
    ClearBackground(RAYWHITE);
    m_pass = 0;
    SetShaderValue(m_shader, m_pass_loc, &m_pass, SHADER_UNIFORM_INT);
    BeginShaderMode(m_shader);
    DrawTextureRec(m_target_0.texture,
                   Rectangle{ 0.0f,
                              0.0f,
                              (float)m_target_0.texture.width,
                              (float)-m_target_0.texture.height },
                   Vector2{},
                   WHITE);
    EndShaderMode();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    m_pass = 1;
    SetShaderValue(m_shader, m_pass_loc, &m_pass, SHADER_UNIFORM_INT);
    BeginShaderMode(m_shader);
    DrawTextureRec(m_target_1.texture,
                   Rectangle{ 0.0f,
                              0.0f,
                              (float)m_target_1.texture.width,
                              (float)-m_target_1.texture.height },
                   Vector2{},
                   WHITE);
    EndShaderMode();
    DrawFPS();
    EndDrawing();
  }
};

// ==================
// === ENTRYPOINT ===
// ==================

int
main()
{
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);
  InitWindow(0, 0, "solaris");
  InitAudioDevice();

  g_current_scene = std::make_unique<MainMenu>();

  while (!WindowShouldClose()) {
    g_current_scene->tick();
    g_current_scene->draw();
  }

  g_current_scene.reset();

  CloseAudioDevice();
  CloseWindow();
}