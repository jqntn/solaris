#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace machina {

class Renderer;
class SceneStack;

struct SceneDrawContext
{
  Renderer& renderer;
};

class Scene
{
public:
  virtual ~Scene() = default;

  virtual void Update(SceneStack& scenes) = 0;
  virtual void Draw(SceneDrawContext& context) = 0;
  virtual void DrawUi() = 0;
};

class SceneStack
{
public:
  void Push(std::unique_ptr<Scene> scene);
  void Pop();
  void Replace(std::unique_ptr<Scene> scene);
  void Clear();

  void Update();
  void Draw(SceneDrawContext& context);
  void DrawUi();

  [[nodiscard]] bool Empty() const;
  [[nodiscard]] std::size_t Size() const;

  void RequestQuit();
  [[nodiscard]] bool ShouldQuit() const;

private:
  enum class CommandType
  {
    Push,
    Pop,
    Replace,
    Clear,
  };

  struct Command
  {
    CommandType type = CommandType::Clear;
    std::unique_ptr<Scene> scene;
  };

  void QueueOrApply(Command command);
  void Apply(Command command);
  void ApplyPendingCommands();

  std::vector<std::unique_ptr<Scene>> scenes;
  std::vector<Command> pendingCommands;
  bool updating = false;
  bool quitRequested = false;
};

}
