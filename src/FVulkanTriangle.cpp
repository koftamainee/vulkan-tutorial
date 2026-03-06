#include "FVulkanTriangle.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <fstream>
#include <vector>

namespace {
  VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
                                               VkDebugUtilsMessageTypeFlagsEXT Type,
                                               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                               void *pUserData) {
    constexpr auto SeverityToStr =
      [](VkDebugUtilsMessageSeverityFlagBitsEXT s) -> const char * {
      switch (s) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        return "VERBOSE";
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        return "INFO";
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        return "WARNING";
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        return "ERROR";
      default:
        return "UNKNOWN";
      }
    };

    constexpr auto TypeToStr =
      [](VkDebugUtilsMessageTypeFlagsEXT t) -> const char * {
      if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        return "VALIDATION";
      if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        return "PERFORMANCE";
      if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT)
        return "DEVICE_ADDRESS";
      if (t & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        return "GENERAL";
      return "UNKNOWN";
    };

    std::cerr << "[" << SeverityToStr(Severity) << "][" << TypeToStr(Type) << "] "
      << pCallbackData->pMessage
      << std::endl;

    (void)pUserData;
    return VK_FALSE;
  }

  std::vector<char> ReadFile(const std::string &FileName) {
    std::ifstream File(FileName, std::ios::ate | std::ios::binary);
    if (!File) {
      return {};
    }

    std::vector<char> Buffer(File.tellg());

    File.seekg(0, std::ios::beg);
    File.read(Buffer.data(), static_cast<std::streamsize>(Buffer.size()));

    File.close();
    return Buffer;
  }
}

FVulkanTriangle::~FVulkanTriangle() {
  if (bIsInitialized) {
    Destroy();
    bIsInitialized = false;
  }
}

void FVulkanTriangle::Init(std::string &&InApplicationName, int InWindowWidth, int InWindowHeight) {
  check(!bIsInitialized);

  ApplicationName = std::move(InApplicationName);
  WindowWidth = InWindowWidth;
  WindowHeight = InWindowHeight;

  InitWindow();

  InitVulkan();

  bIsInitialized = true;
}

void FVulkanTriangle::Destroy() {
  if (bIsInitialized) {
    DeinitVulkan();
    DeinitWindow();
    bIsInitialized = false;
  }
}

void FVulkanTriangle::Run() {
  check(bIsInitialized);
  MainLoop();
}

void FVulkanTriangle::MainLoop() {
  while (!glfwWindowShouldClose(Window)) {
    glfwPollEvents();
    const EResult Result = DrawFrame();
    if (Result != EResult::Success) {
      std::cerr << "Error during frame drawing occurred: " << ToString(Result) << ". Continuing...\n";
    }
  }

  vk_check(vkDeviceWaitIdle(Device));
}

void FVulkanTriangle::InitWindow() {
  fatal(glfwInit() != GLFW_TRUE, "Failed to initialize GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  Window = glfwCreateWindow(WindowWidth, WindowHeight,
                            ApplicationName.c_str(), nullptr, nullptr);
  fatal(Window == nullptr, "Failed to create GLFW window");

  glfwSetWindowUserPointer(Window, this);
  glfwSetFramebufferSizeCallback(Window, GLFWFramebufferResizeCallback);
}

void FVulkanTriangle::DeinitWindow() {
  if (Window != nullptr) {
    glfwDestroyWindow(Window);
    Window = nullptr;
  }
  glfwTerminate();
}

void FVulkanTriangle::InitVulkan() {
  CreateInstance();
  if (EnableValidationLayers) {
    CreateDebugMessenger();
  }
  CreateSurface();
  CreatePhysicalDevice();
  CreateLogicalDevice();
  CreateSwapChain();
  CreateImageViews();
  CreateGraphicsPipeline();
  CreateCommandPool();
  CreateCommandBuffers();
  CreateSyncObjects();

}

void FVulkanTriangle::DeinitVulkan() {
  DestroySyncObjects();
  DestroyCommandBuffers();
  DestroyCommandPool();
  DestroyGraphicsPipeline();
  DestroyImageViews();
  DestroySwapChain();
  DestroyLogicalDevice();
  DestroyPhysicalDevice();
  DestroySurface();
  if (EnableValidationLayers) {
    DestroyDebugMessenger();
  }
  DestroyInstance();
}

void FVulkanTriangle::CreateInstance() {
  const VkApplicationInfo AppInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = ApplicationName.c_str(),
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_4,
  };

  std::vector<const char *> Extensions = GetRequiredInstanceExtensions();

  const VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo = MakeDebugUtilsMessengerCreateInfo();

  VkInstanceCreateInfo CreateInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = nullptr,
    .pApplicationInfo = &AppInfo,
    .enabledLayerCount = 0,
    .ppEnabledLayerNames = nullptr,
    .enabledExtensionCount = static_cast<uint32_t>(Extensions.size()),
    .ppEnabledExtensionNames = Extensions.data(),
};

  if constexpr (EnableValidationLayers) {
    CreateInfo.pNext = &DebugCreateInfo;
    CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
    CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
  }

  vk_check(vkCreateInstance(&CreateInfo, nullptr, &Instance));
}

std::vector<const char *> FVulkanTriangle::GetRequiredInstanceExtensions() {
  uint32_t GLFWExtensionsCount = 0;
  const char **GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionsCount);
  fatal(GLFWExtensionsCount == 0, "Failed to get GLFW extensions");


  uint32_t VkExtensionsCount = 0;
  vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &VkExtensionsCount, nullptr));

  std::vector<VkExtensionProperties> VkExtensions(VkExtensionsCount);
  vk_check(vkEnumerateInstanceExtensionProperties(nullptr, &VkExtensionsCount, VkExtensions.data()));


  for (uint32_t i = 0; i < GLFWExtensionsCount; ++i) {
    bool Found = false;
    for (const auto &VkExtension : VkExtensions) {
      if (strcmp(VkExtension.extensionName, GLFWExtensions[i]) == 0) {
        Found = true;
        break;
      }
    }
    fatal(!Found, "Required GLFW extensions are not supported");
  }

  std::vector<const char *> Extensions(GLFWExtensions, GLFWExtensions + GLFWExtensionsCount);
  if (EnableValidationLayers) {
    Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return Extensions;
}

void FVulkanTriangle::DestroyInstance() {
  if (Instance != VK_NULL_HANDLE) { vkDestroyInstance(Instance, nullptr); }
  Instance = VK_NULL_HANDLE;
}

void FVulkanTriangle::CreateDebugMessenger() {
  check(Instance != VK_NULL_HANDLE);
  ValidateLayers();

  const auto vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
  check(vkCreateDebugUtilsMessenger != nullptr);

  const VkDebugUtilsMessengerCreateInfoEXT CreateInfo = MakeDebugUtilsMessengerCreateInfo();

  vk_check(vkCreateDebugUtilsMessenger(Instance, &CreateInfo, nullptr, &DebugMessenger));

}

void FVulkanTriangle::ValidateLayers() {
  uint32_t vkLayerPropertiesCount = 0;
  vk_check(vkEnumerateInstanceLayerProperties(&vkLayerPropertiesCount, nullptr));

  std::vector<VkLayerProperties> VkLayers(vkLayerPropertiesCount);
  vk_check(vkEnumerateInstanceLayerProperties(&vkLayerPropertiesCount, VkLayers.data()));

  for (auto const NeededLayer : ValidationLayers) {
    bool Found = false;
    for (const auto &VkLayer : VkLayers) {
      if (strcmp(NeededLayer, VkLayer.layerName) == 0) {
        Found = true;
        break;
      }
    }
    fatal(!Found, "Required VK_LAYERS are not supported");
  }
}

VkDebugUtilsMessengerCreateInfoEXT FVulkanTriangle::MakeDebugUtilsMessengerCreateInfo() {
  return {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = &DebugCallback,
  };
}

void FVulkanTriangle::DestroyDebugMessenger() {
  if (DebugMessenger != VK_NULL_HANDLE) {
    check(Instance != VK_NULL_HANDLE);
    const auto vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (vkDestroyDebugUtilsMessenger != nullptr) {
      vkDestroyDebugUtilsMessenger(Instance, DebugMessenger, nullptr);
    }
  }
  DebugMessenger = VK_NULL_HANDLE;
}

void FVulkanTriangle::CreateSurface() {
  fatal(glfwCreateWindowSurface(Instance, Window, nullptr, &Surface) != VK_SUCCESS, "Failed to create surface");
}

void FVulkanTriangle::DestroySurface() {
  if (Surface != VK_NULL_HANDLE) {
    check(Instance != VK_NULL_HANDLE);
    vkDestroySurfaceKHR(Instance, Surface, nullptr);
    Surface = VK_NULL_HANDLE;
  }
}

void FVulkanTriangle::CreatePhysicalDevice() {
  check(Instance != VK_NULL_HANDLE);
  uint32_t PhysicalDevicesCount = 0;
  vk_check(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, nullptr));

  std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDevicesCount);
  vk_check(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, PhysicalDevices.data()));

  PhysicalDevice = PickPhysicalDevice(PhysicalDevices, Surface);
}

VkPhysicalDevice FVulkanTriangle::PickPhysicalDevice(
  const std::vector<VkPhysicalDevice> &Devices, VkSurfaceKHR Surface) {

  std::multimap<int, VkPhysicalDevice> PhysicalDevicesRanked;

  for (const auto PhysicalDevice : Devices) {
    VkPhysicalDeviceProperties Properties;
    vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);

    VkPhysicalDeviceFeatures Features;
    vkGetPhysicalDeviceFeatures(PhysicalDevice, &Features);

    if (!Features.geometryShader) {
      continue;
    }

    if (Properties.apiVersion < VK_API_VERSION_1_3) {
      continue;
    }

    uint32_t QueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilies.data());

    if (!FindQueueFamilies(PhysicalDevice, Surface).IsComplete()) {
      continue;
    }

    uint32_t ExtensionCount = 0;
    vk_check(vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr));
    std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
    vk_check(vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount,
      AvailableExtensions.data()));


    bool bHasAllExtensions = true;
    for (const char *Required : DeviceExtensions) {
      bool Found = false;
      for (uint32_t i = 0; i < ExtensionCount; i++) {
        if (strcmp(AvailableExtensions[i].extensionName, Required) == 0) {
          Found = true;
          break;
        }
      }
      if (!Found) {
        bHasAllExtensions = false;
        break;
      }
    }
    if (!bHasAllExtensions) {
      continue;
    }

    uint32_t FormatCount = 0;
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr));

    uint32_t PresentModeCount = 0;
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr));

    if (FormatCount == 0 || PresentModeCount == 0) {
      continue;
    }

    int Score = 0;

    if (Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      Score += 2000;
    }

    if (Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      Score += 1000;
    }

    Score += static_cast<int>(Properties.limits.maxImageDimension2D);

    PhysicalDevicesRanked.insert(std::make_pair(Score, PhysicalDevice));

  }

  fatal(PhysicalDevicesRanked.empty(), "Supported GPU device does not exist");
  fatal(PhysicalDevicesRanked.rbegin()->first <= 0, "Supported GPU device does not exits");

  return PhysicalDevicesRanked.rbegin()->second;
}

void FVulkanTriangle::DestroyPhysicalDevice() {
  PhysicalDevice = VK_NULL_HANDLE;
}

void FVulkanTriangle::CreateLogicalDevice() {
  check(Instance != VK_NULL_HANDLE);
  check(PhysicalDevice != VK_NULL_HANDLE);

  const auto Indices = FindQueueFamilies(PhysicalDevice, Surface);

  QueueFamilyIndices = Indices;

  std::set<uint32_t> UniqueQueueFamilies = {
    Indices.GraphicsFamily,
    Indices.PresentFamily
  };

  float QueuePriority = 0.5f;
  std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
  for (uint32_t QueueFamily : UniqueQueueFamilies) {
    QueueCreateInfos.push_back({
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = QueueFamily,
      .queueCount = 1,
      .pQueuePriorities = &QueuePriority,
    });
  }

  VkPhysicalDeviceFeatures2 Features2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .pNext = nullptr,
  };

  VkPhysicalDeviceVulkan13Features Vulkan13Features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = nullptr,
    .synchronization2 = VK_TRUE,
    .dynamicRendering = VK_TRUE,
  };

  VkPhysicalDeviceExtendedDynamicStateFeaturesEXT DynamicStateFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
    .pNext = nullptr,
  };

  Features2.pNext = &Vulkan13Features;
  Vulkan13Features.pNext = &DynamicStateFeatures;

  VkDeviceCreateInfo DeviceCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &Features2,
    .queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfos.size()),
    .pQueueCreateInfos = QueueCreateInfos.data(),
    .enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size()),
    .ppEnabledExtensionNames = DeviceExtensions.data(),
  };

  vk_check(vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &Device));

  vkGetDeviceQueue(Device, Indices.GraphicsFamily, 0, &GraphicsQueue);
  vkGetDeviceQueue(Device, Indices.PresentFamily, 0, &PresentQueue);
}

FVulkanTriangle::FQueueFamilyIndices FVulkanTriangle::FindQueueFamilies(VkPhysicalDevice PhysicalDevice,
                                                                        VkSurfaceKHR Surface) {
  check(PhysicalDevice != VK_NULL_HANDLE);
  check(Surface != VK_NULL_HANDLE);

  FQueueFamilyIndices Indices;

  uint32_t QueueFamilyPropertiesCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyPropertiesCount, nullptr);
  std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyPropertiesCount);
  vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyPropertiesCount, QueueFamilies.data());

  for (uint32_t i = 0; i < QueueFamilyPropertiesCount; i++) {
    if (QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

      Indices.GraphicsFamily = i;
      break;
    }
  }
  if (Indices.GraphicsFamily != UINT32_MAX) {
    VkBool32 PresentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Indices.GraphicsFamily, Surface, &PresentSupport);
    if (PresentSupport) {
      Indices.PresentFamily = Indices.GraphicsFamily;
    }
  }

  if (Indices.PresentFamily == UINT32_MAX) {
    for (uint32_t i = 0; i < QueueFamilyPropertiesCount; i++) {
      VkBool32 PresentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &PresentSupport);
      if ((QueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && PresentSupport) {
        Indices.GraphicsFamily = i;
        Indices.PresentFamily = i;
        break;
      }
    }
  }

  if (Indices.PresentFamily == UINT32_MAX) {
    for (uint32_t i = 0; i < QueueFamilyPropertiesCount; i++) {
      VkBool32 PresentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &PresentSupport);
      if (PresentSupport) {
        Indices.PresentFamily = i;
        break;
      }
    }
  }

  return Indices;
}

void FVulkanTriangle::DestroyLogicalDevice() {
  if (Device != VK_NULL_HANDLE) {
    vkDestroyDevice(Device, nullptr);

    PresentQueue = VK_NULL_HANDLE;
    GraphicsQueue = VK_NULL_HANDLE;
    Device = VK_NULL_HANDLE;
    QueueFamilyIndices = FQueueFamilyIndices();
  }
}

void FVulkanTriangle::CreateSwapChain() {
  check(PhysicalDevice != VK_NULL_HANDLE);
  check(Device != VK_NULL_HANDLE);

  VkSurfaceCapabilitiesKHR Capabilities;
  vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &Capabilities));

  uint32_t FormatsCount = 0;
  vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, nullptr));

  std::vector<VkSurfaceFormatKHR> Formats(FormatsCount);
  vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, Formats.data()));

  uint32_t PresentModesCount = 0;
  vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModesCount, nullptr));

  std::vector<VkPresentModeKHR> PresentModes(PresentModesCount);
  vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModesCount,
    PresentModes.data()));

  SwapChainSurfaceFormat = PickSwapChainSurfaceFormat(Formats);
  SwapChainExtent = PickSwapChainExtent(Capabilities, Window);
  const VkPresentModeKHR PresentMode = PickSwapChainPresentMode(PresentModes);

  uint32_t ImageCount = Capabilities.minImageCount + 1;
  if (Capabilities.maxImageCount > 0 && ImageCount > Capabilities.maxImageCount) {
    ImageCount = Capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR SwapChainCreateInfo{
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .flags = VkSwapchainCreateFlagsKHR(),
    .surface = Surface,
    .minImageCount = ImageCount,
    .imageFormat = SwapChainSurfaceFormat.format,
    .imageColorSpace = SwapChainSurfaceFormat.colorSpace,
    .imageExtent = SwapChainExtent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .preTransform = Capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = PresentMode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE,
  };

  const std::array<uint32_t, 2> QueueFamilyIndicesArray = {QueueFamilyIndices.GraphicsFamily,
                                                           QueueFamilyIndices.PresentFamily};

  if (QueueFamilyIndicesArray[0] != QueueFamilyIndicesArray[1]) {
    SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    SwapChainCreateInfo.queueFamilyIndexCount = 2;
    SwapChainCreateInfo.pQueueFamilyIndices = QueueFamilyIndicesArray.data();
  }

  vk_check(vkCreateSwapchainKHR(Device, &SwapChainCreateInfo, nullptr, &SwapChain));

  uint32_t SwapChainImageCount = 0;
  vk_check(vkGetSwapchainImagesKHR(Device, SwapChain, &SwapChainImageCount, nullptr));

  SwapChainImages.resize(SwapChainImageCount);
  vk_check(vkGetSwapchainImagesKHR(Device, SwapChain, &SwapChainImageCount, SwapChainImages.data()));
}

VkSurfaceFormatKHR FVulkanTriangle::PickSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &Formats) {
  for (const auto &Format : Formats) {
    if (Format.format == VK_FORMAT_R8G8B8A8_SRGB && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return Format;
    }
  }
  return Formats[0];
}

VkPresentModeKHR FVulkanTriangle::PickSwapChainPresentMode(const std::vector<VkPresentModeKHR> &PresentModes) {

  for (const auto &PresentMode : PresentModes) {
    if (PresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return PresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D FVulkanTriangle::PickSwapChainExtent(const VkSurfaceCapabilitiesKHR &Capabilities, GLFWwindow *Window) {
  check(Window != nullptr);

  if (Capabilities.currentExtent.width != UINT32_MAX) {
    return Capabilities.currentExtent;
  }

  int Width = 0;
  int Height = 0;

  glfwGetFramebufferSize(Window, &Width, &Height);


  std::cout << "Framebuffer size: " << Width << "x" << Height << std::endl;

  return {
    .width = std::clamp<uint32_t>(Width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width),
    .height = std::clamp<uint32_t>(Height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height)};

}

void FVulkanTriangle::DestroySwapChain() {
  if (SwapChain != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroySwapchainKHR(Device, SwapChain, nullptr);
    SwapChain = VK_NULL_HANDLE;
    SwapChainImages.clear();
  }
}

void FVulkanTriangle::RecreateSwapChain() {
  check(Device != VK_NULL_HANDLE);
  vk_check(vkDeviceWaitIdle(Device));
  int Width = 0, Height = 0;
  glfwGetFramebufferSize(Window, &Width, &Height);

  while (Width == 0 || Height == 0) {
    glfwGetFramebufferSize(Window, &Width, &Height);
    glfwWaitEvents();
  }

  DestroyImageViews();
  DestroySwapChain();

  CreateSwapChain();
  CreateImageViews();
  bFramebufferResized = false;
}

void FVulkanTriangle::CreateImageViews() {
  check(Device != VK_NULL_HANDLE);
  check(!SwapChainImages.empty());

  SwapChainImageViews.clear();


  for (const auto Image : SwapChainImages) {
    const VkImageViewCreateInfo ImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = Image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = SwapChainSurfaceFormat.format,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    VkImageView ImageView = VK_NULL_HANDLE;
    vk_check(vkCreateImageView(Device, &ImageViewCreateInfo, nullptr, &ImageView));
    SwapChainImageViews.emplace_back(ImageView);
  }
}

void FVulkanTriangle::DestroyImageViews() {
  for (const auto ImageView : SwapChainImageViews) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyImageView(Device, ImageView, nullptr);
  }
  SwapChainImageViews.clear();
}

void FVulkanTriangle::CreateGraphicsPipeline() {
  check(Device != VK_NULL_HANDLE);
  const auto ShaderCode = ReadFile("../shaders/slang.spv");
  checkf(!ShaderCode.empty(), "Loaded empty shader");

  VkShaderModule ShaderModule = CreateShaderModule(ShaderCode);
  check(ShaderModule != VK_NULL_HANDLE);

  const VkPipelineShaderStageCreateInfo VertShaderStageCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = ShaderModule,
    .pName = "VertMain",
  };

  const VkPipelineShaderStageCreateInfo FragShaderStageCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = ShaderModule,
    .pName = "FragMain",
  };

  const std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {VertShaderStageCreateInfo,
                                                                       FragShaderStageCreateInfo};

  VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };

  constexpr std::array<VkDynamicState, 2> DynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  const VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = static_cast<uint32_t>(DynamicStates.size()),
    .pDynamicStates = DynamicStates.data(),
  };

  const VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };


  constexpr VkPipelineRasterizationStateCreateInfo RasterizerStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasSlopeFactor = 1.0f,
    .lineWidth = 1.0f,
  };

  constexpr VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
  };

  constexpr VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
    VK_COLOR_COMPONENT_A_BIT,
  };

  const VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments = &ColorBlendAttachmentState,
  };

  constexpr VkPipelineLayoutCreateInfo LayoutCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 0,
    .pushConstantRangeCount = 0,
  };

  vk_check(vkCreatePipelineLayout(Device, &LayoutCreateInfo, nullptr, &PipelineLayout));

  const VkPipelineRenderingCreateInfo RenderingCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &SwapChainSurfaceFormat.format,
  };

  const VkGraphicsPipelineCreateInfo GraphicsPipelineCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &RenderingCreateInfo,
    .stageCount = 2,
    .pStages = ShaderStages.data(),
    .pVertexInputState = &VertexInputStateCreateInfo,
    .pInputAssemblyState = &InputAssemblyStateCreateInfo,
    .pViewportState = &ViewportStateCreateInfo,
    .pRasterizationState = &RasterizerStateCreateInfo,
    .pMultisampleState = &MultisampleStateCreateInfo,
    .pColorBlendState = &ColorBlendStateCreateInfo,
    .pDynamicState = &DynamicStateCreateInfo,
    .layout = PipelineLayout,
    .renderPass = nullptr,
  };

  vk_check(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, nullptr,
    &GraphicsPipeline));

  DestroyShaderModule(ShaderModule);
}

VkShaderModule FVulkanTriangle::CreateShaderModule(const std::vector<char> &ShaderCode) const {
  check(Device != VK_NULL_HANDLE);
  const VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = ShaderCode.size(),
    .pCode = reinterpret_cast<const uint32_t *>(ShaderCode.data())
  };

  VkShaderModule ShaderModule = VK_NULL_HANDLE;
  vk_check(vkCreateShaderModule(Device, &ShaderModuleCreateInfo, nullptr, &ShaderModule));
  return ShaderModule;
}

void FVulkanTriangle::DestroyShaderModule(VkShaderModule ShaderModule) const {
  if (ShaderModule != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyShaderModule(Device, ShaderModule, nullptr);
  }
}

void FVulkanTriangle::DestroyGraphicsPipeline() {
  if (PipelineLayout != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
    PipelineLayout = VK_NULL_HANDLE;
  }
  if (GraphicsPipeline != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyPipeline(Device, GraphicsPipeline, nullptr);
    GraphicsPipeline = VK_NULL_HANDLE;
  }
}

void FVulkanTriangle::CreateCommandPool() {
  const VkCommandPoolCreateInfo CommandPoolCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = QueueFamilyIndices.GraphicsFamily,
  };

  vk_check(vkCreateCommandPool(Device, &CommandPoolCreateInfo, nullptr, &CommandPool));
}

void FVulkanTriangle::DestroyCommandPool() {
  if (CommandPool != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyCommandPool(Device, CommandPool, nullptr);
    CommandPool = VK_NULL_HANDLE;
  }
}

void FVulkanTriangle::CreateCommandBuffers() {
  check(Device != VK_NULL_HANDLE);

  CommandBuffers.clear();
  CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = CommandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  vk_check(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, CommandBuffers.data()));
}

void FVulkanTriangle::RecordCommandBuffer(uint32_t ImageIndex) const {
  check(!CommandBuffers.empty());
  check(CommandBuffers[FrameIndex] != VK_NULL_HANDLE);
  check(GraphicsPipeline != VK_NULL_HANDLE);

  constexpr VkCommandBufferBeginInfo BeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  vk_check(vkBeginCommandBuffer(CommandBuffers[FrameIndex], &BeginInfo));

  TransitionImageLayout(
    ImageIndex,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    {},
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  constexpr VkClearColorValue ClearColor{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};

  const VkRenderingAttachmentInfo RenderingAttachmentInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = SwapChainImageViews[ImageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {ClearColor},
  };

  const VkRenderingInfo RenderingInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = {.offset = {0, 0}, .extent = SwapChainExtent},
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &RenderingAttachmentInfo,
  };

  vkCmdBeginRendering(CommandBuffers[FrameIndex], &RenderingInfo);

  vkCmdBindPipeline(CommandBuffers[FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

  const VkViewport Viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = static_cast<float>(SwapChainExtent.width),
    .height = static_cast<float>(SwapChainExtent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  const VkRect2D Scissor = {
    .offset = {0, 0},
    .extent = SwapChainExtent,
  };

  vkCmdSetViewport(CommandBuffers[FrameIndex], 0, 1, &Viewport);
  vkCmdSetScissor(CommandBuffers[FrameIndex], 0, 1, &Scissor);

  vkCmdDraw(CommandBuffers[FrameIndex], 3, 1, 0, 0);

  vkCmdEndRendering(CommandBuffers[FrameIndex]);

  TransitionImageLayout(
    ImageIndex,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    {},
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
    );

  vk_check(vkEndCommandBuffer(CommandBuffers[FrameIndex]));
}

void FVulkanTriangle::DestroyCommandBuffers() {
  check(CommandBuffers.size() == MAX_FRAMES_IN_FLIGHT);
  check(Device != VK_NULL_HANDLE);
  vkFreeCommandBuffers(Device, CommandPool, MAX_FRAMES_IN_FLIGHT, CommandBuffers.data());
  CommandBuffers.clear();
}


void FVulkanTriangle::TransitionImageLayout(uint32_t ImageIndex, VkImageLayout OldLayout, VkImageLayout NewLayout,
                                            VkAccessFlags2 SrcAccessMask, VkAccessFlags2 DstAccessMask,
                                            VkPipelineStageFlags2 SrcStageMask,
                                            VkPipelineStageFlags2 DstStageMask) const {
  const VkImageMemoryBarrier2 Barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = SrcStageMask,
    .srcAccessMask = SrcAccessMask,
    .dstStageMask = DstStageMask,
    .dstAccessMask = DstAccessMask,
    .oldLayout = OldLayout,
    .newLayout = NewLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = SwapChainImages[ImageIndex],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  const VkDependencyInfo DependencyInfo = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .dependencyFlags = 0,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &Barrier,
  };

  vkCmdPipelineBarrier2(CommandBuffers[FrameIndex], &DependencyInfo);
}

void FVulkanTriangle::CreateSyncObjects() {
  check(!SwapChainImages.empty());
  check(PresentCompleteSemaphores.empty() && RenderCompleteSemaphores.empty() && InFlightFences.empty());
  check(Device != VK_NULL_HANDLE);

  constexpr VkSemaphoreCreateInfo SemaphoreCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  constexpr VkFenceCreateInfo FenceCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };


  RenderCompleteSemaphores.resize(SwapChainImages.size());
  for (auto &Semaphore : RenderCompleteSemaphores) {
    vk_check(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphore));
  }
  PresentCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  for (auto &Semaphore : PresentCompleteSemaphores) {
    vk_check(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphore));
  }

  InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  for (auto &Fence : InFlightFences) {
    vk_check(vkCreateFence(Device, &FenceCreateInfo, nullptr, &Fence));
  }
}

void FVulkanTriangle::DestroySyncObjects() {
  for (const auto Semaphore : RenderCompleteSemaphores) {
    check(Device != VK_NULL_HANDLE);
    vkDestroySemaphore(Device, Semaphore, nullptr);
  }
  RenderCompleteSemaphores.clear();

  for (const auto Semaphore : PresentCompleteSemaphores) {
    check(Device != VK_NULL_HANDLE);
    vkDestroySemaphore(Device, Semaphore, nullptr);
  }
  PresentCompleteSemaphores.clear();

  for (const auto Fence : InFlightFences) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyFence(Device, Fence, nullptr);
  }
}

EResult FVulkanTriangle::DrawFrame() {
  check(Device != VK_NULL_HANDLE);
  check(!InFlightFences.empty());
  check(!PresentCompleteSemaphores.empty());
  check(!RenderCompleteSemaphores.empty());
  check(SwapChain != VK_NULL_HANDLE);

  vk_check(vkWaitForFences(Device, 1, &InFlightFences[FrameIndex],VK_TRUE, UINT64_MAX));

  if (bFramebufferResized) {
    RecreateSwapChain();
  }

  uint32_t ImageIndex = 0;
  const VkResult AcquireResult = vkAcquireNextImageKHR(Device, SwapChain, UINT64_MAX,
                                                       PresentCompleteSemaphores[FrameIndex],
                                                       VK_NULL_HANDLE, &ImageIndex);
  if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain();
    return EResult::SwapChainOutOfDate;
  }
  if (AcquireResult == VK_SUBOPTIMAL_KHR) {
    RecreateSwapChain();
    return EResult::SwapChainSuboptimal;
  }
  vk_check(AcquireResult);

  RecordCommandBuffer(ImageIndex);

  vk_check(vkResetFences(Device, 1, &InFlightFences[FrameIndex]));

  constexpr VkPipelineStageFlags WaitDstStorageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;

  const VkSubmitInfo SubmitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &PresentCompleteSemaphores[FrameIndex],
    .pWaitDstStageMask = &WaitDstStorageMask,
    .commandBufferCount = 1,
    .pCommandBuffers = &CommandBuffers[FrameIndex],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &RenderCompleteSemaphores[ImageIndex],
  };


  vk_check(vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, InFlightFences[FrameIndex]));

  const VkPresentInfoKHR PresentInfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &RenderCompleteSemaphores[ImageIndex],
    .swapchainCount = 1,
    .pSwapchains = &SwapChain,
    .pImageIndices = &ImageIndex,
  };

  const VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &PresentInfo);
  if (PresentResult == VK_SUBOPTIMAL_KHR || PresentResult == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain();
    return EResult::SwapChainSuboptimal;
  }
  vk_check(PresentResult);

  FrameIndex = (FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

  return EResult::Success;
}

void FVulkanTriangle::GLFWFramebufferResizeCallback(GLFWwindow *window, int Width, int Height) {
  auto *App = static_cast<FVulkanTriangle *>(glfwGetWindowUserPointer(window));
  App->bFramebufferResized = true;
}