#include <cstring>
#include <iostream>
#include "FVulkanTriangle.h"

int main() {
  auto AppResult =
    FVulkanTriangle::New("VulkanTriangle", 800, 600);
  if (!AppResult) {
    std::cerr << "Error: " << to_string(AppResult.error()) << std::endl;
    return EXIT_FAILURE;
  }

  const auto App = std::move(AppResult.value());

  const auto Result = App->Run();
  if (Result != EResult::Success) {
    std::cerr << "Error: " << to_string(Result) << std::endl;
    return EXIT_FAILURE;
  }


  return EXIT_SUCCESS;
}
