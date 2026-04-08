#include "terra/render/Renderer.h"

#include <iostream>

namespace terra::render {

void NullRenderer::DrawDebugText(const std::string& text) {
  std::cout << text << '\n';
}

} // namespace terra::render
