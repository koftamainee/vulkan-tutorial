#include <cstring>
#include <iostream>
#include "FVulkanTriangle.h"

int main() {

  FVulkanTriangle App;
  App.Init("VulkanTriangle", 800, 600);

  App.Run();


  return EXIT_SUCCESS;
}
