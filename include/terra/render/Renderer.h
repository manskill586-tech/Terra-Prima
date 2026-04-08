#pragma once

#include <string>

namespace terra::render {

class Renderer {
public:
  virtual ~Renderer() = default;
  virtual void BeginFrame() = 0;
  virtual void EndFrame() = 0;
  virtual void DrawDebugText(const std::string& text) = 0;
};

class NullRenderer final : public Renderer {
public:
  void BeginFrame() override {}
  void EndFrame() override {}
  void DrawDebugText(const std::string& text) override;
};

} // namespace terra::render
