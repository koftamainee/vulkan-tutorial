#include <cstring>
#include <iostream>
#include "FVulkanApplication.h"

int main() {
  FVulkanApplication App;
  App.Init("VulkanTriangle", 800, 600);

  App.Run();

  return EXIT_SUCCESS;
}
