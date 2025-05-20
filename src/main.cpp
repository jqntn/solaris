#include <cstdint>
#include <filesystem>
#include <fstream>
#include <raylib.h>
#include <resources.h>
#include <shaders.h>
#include <string>

extern "C"
{
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
  __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance =
    0x00000001;
}

namespace fs = std::filesystem;

static Model
LoadModelFromMemory(const char* fileType,
                    const unsigned char* fileData,
                    int dataSize)
{
  fs::path file_path =
    fs::temp_directory_path() / ("out" + std::string(fileType));

  std::ofstream out(file_path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(fileData), dataSize);
  out.close();

  Model model = LoadModel(file_path.string().c_str());

  fs::remove(file_path);

  return model;
}

int
main()
{
  SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);
  InitWindow(0, 0, "solaris");

  uint32_t screen_width = GetScreenWidth();
  uint32_t screen_height = GetScreenHeight();

  Model model = LoadModelFromMemory(".glb", helmet_glb, helmet_glb_len);

  Shader shader = LoadShaderFromMemory(
    NULL,
    std::string(reinterpret_cast<char*>(blur_frag), blur_frag_len).c_str());

  int pass_loc = GetShaderLocation(shader, "pass");
  int pass = 0;

  RenderTexture2D target_0 = LoadRenderTexture(screen_width, screen_height);
  RenderTexture2D target_1 = LoadRenderTexture(screen_width, screen_height);

  Vector3 position = Vector3{ 0.0f, 1.0f, 0.0f };
  Camera camera = {
    .position = Vector3{ 2.0f, 2.0f, 2.0f },
    .target = position,
    .up = Vector3{ 0.0f, 1.0f, 0.0f },
    .fovy = 50.0f,
    .projection = CAMERA_PERSPECTIVE,
  };

  while (!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_ORBITAL);

    BeginTextureMode(target_0);
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);
    DrawModel(model, position, 1.0f, WHITE);
    DrawGrid(10, 1.0f);
    EndMode3D();
    EndTextureMode();

    BeginTextureMode(target_1);
    ClearBackground(RAYWHITE);
    pass = 0;
    SetShaderValue(shader, pass_loc, &pass, SHADER_UNIFORM_INT);
    BeginShaderMode(shader);
    DrawTextureRec(target_0.texture,
                   Rectangle{ 0.0f,
                              0.0f,
                              (float)target_0.texture.width,
                              (float)-target_0.texture.height },
                   Vector2{},
                   WHITE);
    EndShaderMode();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(RAYWHITE);
    pass = 1;
    SetShaderValue(shader, pass_loc, &pass, SHADER_UNIFORM_INT);
    BeginShaderMode(shader);
    DrawTextureRec(target_1.texture,
                   Rectangle{ 0.0f,
                              0.0f,
                              (float)target_1.texture.width,
                              (float)-target_1.texture.height },
                   Vector2{},
                   WHITE);
    EndShaderMode();
    DrawText(TextFormat("%d", GetFPS()), 8, 4, 30, GREEN);
    EndDrawing();
  }

  UnloadModel(model);
  UnloadShader(shader);
  UnloadRenderTexture(target_0);
  UnloadRenderTexture(target_1);

  CloseWindow();
}