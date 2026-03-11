#include "FVulkanApplication.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <fstream>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


template <>
struct std::hash<FVulkanApplication::FVertex> {
  size_t operator()(FVulkanApplication::FVertex const &Vertex) const noexcept {
    return ((hash<glm::vec3>()(Vertex.Position) ^
        (hash<glm::vec3>()(Vertex.Color) << 1)) >> 1) ^
      (hash<glm::vec2>()(Vertex.TexCoord) << 1);
  }
};

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

FVulkanApplication::~FVulkanApplication() {
  if (bIsInitialized) {
    Destroy();
    bIsInitialized = false;
  }
}

void FVulkanApplication::Init(std::string &&InApplicationName, int InWindowWidth, int InWindowHeight) {
  check(!bIsInitialized);

  ApplicationName = std::move(InApplicationName);
  WindowWidth = InWindowWidth;
  WindowHeight = InWindowHeight;

  InitWindow();

  InitVulkan();

  bIsInitialized = true;
}

void FVulkanApplication::Destroy() {
  if (bIsInitialized) {
    DeinitVulkan();
    DeinitWindow();
    bIsInitialized = false;
  }
}

void FVulkanApplication::Run() {
  check(bIsInitialized);
  MainLoop();
}

void FVulkanApplication::MainLoop() {
  while (!glfwWindowShouldClose(Window)) {
    glfwPollEvents();
    const EResult Result = DrawFrame();
    if (Result != EResult::Success) {
      std::cerr << "MainLoop(): " << ToString(Result) << ". Continuing...\n";
    }
  }

  vk_verify(vkDeviceWaitIdle(Device));
}

void FVulkanApplication::InitWindow() {
  fatal(glfwInit() != GLFW_TRUE, "Failed to initialize GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  Window = glfwCreateWindow(WindowWidth, WindowHeight,
                            ApplicationName.c_str(), nullptr, nullptr);
  fatal(Window == nullptr, "Failed to create GLFW window");

  glfwSetWindowUserPointer(Window, this);
  glfwSetFramebufferSizeCallback(Window, GLFWFramebufferResizeCallback);
}

void FVulkanApplication::DeinitWindow() {
  if (Window != nullptr) {
    glfwDestroyWindow(Window);
    Window = nullptr;
  }
  glfwTerminate();
}

void FVulkanApplication::InitVulkan() {
  CreateInstance();
  if (EnableValidationLayers) {
    CreateDebugMessenger();
  }
  CreateSurface();
  CreatePhysicalDevice();
  CreateLogicalDevice();
  CreateSwapChain();
  CreateImageViews();
  CreateDescriptorSetLayout();
  CreateGraphicsPipeline();
  CreateCommandPool();
  CreateDepthResources();
  CreateTextureImage();
  CreateTextureImageView();
  CreateTextureSampler();
  LoadModel();
  CreateVertexBuffer();
  CreateIndexBuffer();
  CreateUniformBuffer();
  CreateDescriptorPool();
  CreateDescriptorSets();
  CreateCommandBuffers();
  CreateSyncObjects();
}

void FVulkanApplication::DeinitVulkan() {
  DestroySyncObjects();
  DestroyCommandBuffers();
  DestroyDescriptorSets();
  DestroyDescriptorPool();
  DestroyUniformBuffer();
  DestroyIndexBuffer();
  DestroyVertexBuffer();
  DestroyTextureSampler();
  DestroyTextureImageView();
  DestroyTextureImage();
  DestroyDepthResources();
  DestroyCommandPool();
  DestroyGraphicsPipeline();
  DestroyDescriptorSetLayout();
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

void FVulkanApplication::CreateInstance() {
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

  vk_verify(vkCreateInstance(&CreateInfo, nullptr, &Instance));
}

std::vector<const char *> FVulkanApplication::GetRequiredInstanceExtensions() {
  uint32_t GLFWExtensionsCount = 0;
  const char **GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionsCount);
  fatal(GLFWExtensionsCount == 0, "Failed to get GLFW extensions");


  uint32_t VkExtensionsCount = 0;
  vk_verify(vkEnumerateInstanceExtensionProperties(nullptr, &VkExtensionsCount, nullptr));

  std::vector<VkExtensionProperties> VkExtensions(VkExtensionsCount);
  vk_verify(vkEnumerateInstanceExtensionProperties(nullptr, &VkExtensionsCount, VkExtensions.data()));


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

void FVulkanApplication::DestroyInstance() {
  if (Instance != VK_NULL_HANDLE) { vkDestroyInstance(Instance, nullptr); }
  Instance = VK_NULL_HANDLE;
}

void FVulkanApplication::CreateDebugMessenger() {
  check(Instance != VK_NULL_HANDLE);
  ValidateLayers();

  const auto vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
  check(vkCreateDebugUtilsMessenger != nullptr);

  const VkDebugUtilsMessengerCreateInfoEXT CreateInfo = MakeDebugUtilsMessengerCreateInfo();

  vk_verify(vkCreateDebugUtilsMessenger(Instance, &CreateInfo, nullptr, &DebugMessenger));

}

void FVulkanApplication::ValidateLayers() {
  uint32_t vkLayerPropertiesCount = 0;
  vk_verify(vkEnumerateInstanceLayerProperties(&vkLayerPropertiesCount, nullptr));

  std::vector<VkLayerProperties> VkLayers(vkLayerPropertiesCount);
  vk_verify(vkEnumerateInstanceLayerProperties(&vkLayerPropertiesCount, VkLayers.data()));

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

VkDebugUtilsMessengerCreateInfoEXT FVulkanApplication::MakeDebugUtilsMessengerCreateInfo() {
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

void FVulkanApplication::DestroyDebugMessenger() {
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

void FVulkanApplication::CreateSurface() {
  fatal(glfwCreateWindowSurface(Instance, Window, nullptr, &Surface) != VK_SUCCESS, "Failed to create surface");
}

void FVulkanApplication::DestroySurface() {
  if (Surface != VK_NULL_HANDLE) {
    check(Instance != VK_NULL_HANDLE);
    vkDestroySurfaceKHR(Instance, Surface, nullptr);
    Surface = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreatePhysicalDevice() {
  check(Instance != VK_NULL_HANDLE);
  uint32_t PhysicalDevicesCount = 0;
  vk_verify(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, nullptr));

  std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDevicesCount);
  vk_verify(vkEnumeratePhysicalDevices(Instance, &PhysicalDevicesCount, PhysicalDevices.data()));

  PhysicalDevice = PickPhysicalDevice(PhysicalDevices, Surface);
}

VkPhysicalDevice FVulkanApplication::PickPhysicalDevice(
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

    if (!Features.samplerAnisotropy) {
      continue;
    }

    uint32_t ExtensionCount = 0;
    vk_verify(vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr));
    std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
    vk_verify(vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount,
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
    vk_verify(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr));

    uint32_t PresentModeCount = 0;
    vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr));

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

void FVulkanApplication::DestroyPhysicalDevice() {
  PhysicalDevice = VK_NULL_HANDLE;
}

void FVulkanApplication::CreateLogicalDevice() {
  check(Instance != VK_NULL_HANDLE);
  check(PhysicalDevice != VK_NULL_HANDLE);

  const auto QueueIndices = FindQueueFamilies(PhysicalDevice, Surface);

  QueueFamilyIndices = QueueIndices;

  std::set<uint32_t> UniqueQueueFamilies = {
    QueueIndices.GraphicsFamily,
    QueueIndices.PresentFamily
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
    .features = {.samplerAnisotropy = true},
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

  vk_verify(vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &Device));

  vkGetDeviceQueue(Device, QueueIndices.GraphicsFamily, 0, &GraphicsQueue);
  vkGetDeviceQueue(Device, QueueIndices.PresentFamily, 0, &PresentQueue);
}

FVulkanApplication::FQueueFamilyIndices FVulkanApplication::FindQueueFamilies(VkPhysicalDevice PhysicalDevice,
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

void FVulkanApplication::DestroyLogicalDevice() {
  if (Device != VK_NULL_HANDLE) {
    vkDestroyDevice(Device, nullptr);

    PresentQueue = VK_NULL_HANDLE;
    GraphicsQueue = VK_NULL_HANDLE;
    Device = VK_NULL_HANDLE;
    QueueFamilyIndices = FQueueFamilyIndices();
  }
}

void FVulkanApplication::CreateSwapChain() {
  check(PhysicalDevice != VK_NULL_HANDLE);
  check(Device != VK_NULL_HANDLE);

  VkSurfaceCapabilitiesKHR Capabilities;
  vk_verify(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &Capabilities));

  uint32_t FormatsCount = 0;
  vk_verify(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, nullptr));

  std::vector<VkSurfaceFormatKHR> Formats(FormatsCount);
  vk_verify(vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, Formats.data()));

  uint32_t PresentModesCount = 0;
  vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModesCount, nullptr));

  std::vector<VkPresentModeKHR> PresentModes(PresentModesCount);
  vk_verify(vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModesCount,
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

  vk_verify(vkCreateSwapchainKHR(Device, &SwapChainCreateInfo, nullptr, &SwapChain));

  uint32_t SwapChainImageCount = 0;
  vk_verify(vkGetSwapchainImagesKHR(Device, SwapChain, &SwapChainImageCount, nullptr));

  SwapChainImages.resize(SwapChainImageCount);
  vk_verify(vkGetSwapchainImagesKHR(Device, SwapChain, &SwapChainImageCount, SwapChainImages.data()));
}

VkSurfaceFormatKHR FVulkanApplication::PickSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &Formats) {
  for (const auto &Format : Formats) {
    if (Format.format == VK_FORMAT_R8G8B8A8_SRGB && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return Format;
    }
  }
  return Formats[0];
}

VkPresentModeKHR FVulkanApplication::PickSwapChainPresentMode(const std::vector<VkPresentModeKHR> &PresentModes) {

  for (const auto &PresentMode : PresentModes) {
    if (PresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return PresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D FVulkanApplication::PickSwapChainExtent(const VkSurfaceCapabilitiesKHR &Capabilities, GLFWwindow *Window) {
  check(Window != nullptr);

  if (Capabilities.currentExtent.width != UINT32_MAX) {
    return Capabilities.currentExtent;
  }

  int Width = 0;
  int Height = 0;

  glfwGetFramebufferSize(Window, &Width, &Height);


  std::cerr << "PickSwapChainExtent(): Framebuffer size: " << Width << "x" << Height << std::endl;

  return {
    .width = std::clamp<uint32_t>(Width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width),
    .height = std::clamp<uint32_t>(Height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height)};

}

void FVulkanApplication::DestroySwapChain() {
  if (SwapChain != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroySwapchainKHR(Device, SwapChain, nullptr);
    SwapChain = VK_NULL_HANDLE;
    SwapChainImages.clear();
  }
}

void FVulkanApplication::RecreateSwapChain() {
  check(Device != VK_NULL_HANDLE);
  vk_verify(vkDeviceWaitIdle(Device));
  int Width = 0, Height = 0;
  glfwGetFramebufferSize(Window, &Width, &Height);

  while (Width == 0 || Height == 0) {
    glfwGetFramebufferSize(Window, &Width, &Height);
    glfwWaitEvents();
  }

  DestroyDepthResources();
  DestroyImageViews();
  DestroySwapChain();

  CreateSwapChain();
  CreateImageViews();
  CreateDepthResources();
  bFramebufferResized = false;
}

void FVulkanApplication::CreateImageViews() {
  check(Device != VK_NULL_HANDLE);
  check(!SwapChainImages.empty());

  SwapChainImageViews.clear();


  for (const auto Image : SwapChainImages) {
    VkImageView ImageView = CreateImageView(Image, SwapChainSurfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    SwapChainImageViews.emplace_back(ImageView);
  }
}

void FVulkanApplication::DestroyImageViews() {
  for (const auto ImageView : SwapChainImageViews) {
    DestroyImageView(ImageView);
  }
  SwapChainImageViews.clear();
}

void FVulkanApplication::CreateDescriptorSetLayout() {
  check(Device != VK_NULL_HANDLE);

  static constexpr std::array Bindings = {
    VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT, nullptr},
    VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                 VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
  };

  static constexpr VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = Bindings.size(),
    .pBindings = Bindings.data(),
  };

  vk_verify(vkCreateDescriptorSetLayout(Device, &DescriptorSetLayoutCreateInfo, nullptr, &DescriptorSetLayout));

}

void FVulkanApplication::DestroyDescriptorSetLayout() {
  if (DescriptorSetLayout != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(Device, DescriptorSetLayout, nullptr);
    DescriptorSetLayout = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreateGraphicsPipeline() {
  check(Device != VK_NULL_HANDLE);
  const auto ShaderCode = ReadFile("../shaders/slang.spv");
  checkf(!ShaderCode.empty(), "Loaded empty shader");

  VkShaderModule ShaderModule = CreateShaderModule(ShaderCode);
  check(ShaderModule != VK_NULL_HANDLE);

  const VkPipelineShaderStageCreateInfo VertShaderStageCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = ShaderModule,
    .pName = "vert_main",
  };

  const VkPipelineShaderStageCreateInfo FragShaderStageCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = ShaderModule,
    .pName = "frag_main",
  };

  const std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {VertShaderStageCreateInfo,
                                                                       FragShaderStageCreateInfo};

  constexpr VkVertexInputBindingDescription BindingDescription = FVertex::GetBindingDescription();
  constexpr std::array AttributeDescriptions = FVertex::GetAttributeDescription();

  VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &BindingDescription,
    .vertexAttributeDescriptionCount = AttributeDescriptions.size(),
    .pVertexAttributeDescriptions = AttributeDescriptions.data(),
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
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasSlopeFactor = 1.0f,
    .lineWidth = 1.0f,
  };

  constexpr VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
  };

  VkPipelineDepthStencilStateCreateInfo DepthStencilStateCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
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

  const VkPipelineLayoutCreateInfo LayoutCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &DescriptorSetLayout,
    .pushConstantRangeCount = 0,
  };

  vk_verify(vkCreatePipelineLayout(Device, &LayoutCreateInfo, nullptr, &GraphicsPipelineLayout));

  VkFormat DepthFormat = FindDepthFormat();

  const VkPipelineRenderingCreateInfo RenderingCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &SwapChainSurfaceFormat.format,
    .depthAttachmentFormat = DepthFormat,
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
    .pDepthStencilState = &DepthStencilStateCreateInfo,
    .pColorBlendState = &ColorBlendStateCreateInfo,
    .pDynamicState = &DynamicStateCreateInfo,
    .layout = GraphicsPipelineLayout,
    .renderPass = nullptr,
  };

  vk_verify(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, nullptr,
    &GraphicsPipeline));

  DestroyShaderModule(ShaderModule);
}

VkShaderModule FVulkanApplication::CreateShaderModule(const std::vector<char> &ShaderCode) const {
  check(Device != VK_NULL_HANDLE);
  const VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = ShaderCode.size(),
    .pCode = reinterpret_cast<const uint32_t *>(ShaderCode.data())
  };

  VkShaderModule ShaderModule = VK_NULL_HANDLE;
  vk_verify(vkCreateShaderModule(Device, &ShaderModuleCreateInfo, nullptr, &ShaderModule));
  return ShaderModule;
}

void FVulkanApplication::DestroyShaderModule(VkShaderModule ShaderModule) const {
  if (ShaderModule != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyShaderModule(Device, ShaderModule, nullptr);
  }
}

void FVulkanApplication::DestroyGraphicsPipeline() {
  if (GraphicsPipelineLayout != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyPipelineLayout(Device, GraphicsPipelineLayout, nullptr);
    GraphicsPipelineLayout = VK_NULL_HANDLE;
  }
  if (GraphicsPipeline != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyPipeline(Device, GraphicsPipeline, nullptr);
    GraphicsPipeline = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreateCommandPool() {
  const VkCommandPoolCreateInfo CommandPoolCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = QueueFamilyIndices.GraphicsFamily,
  };

  vk_verify(vkCreateCommandPool(Device, &CommandPoolCreateInfo, nullptr, &CommandPool));
}

void FVulkanApplication::DestroyCommandPool() {
  if (CommandPool != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyCommandPool(Device, CommandPool, nullptr);
    CommandPool = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreateDepthResources() {
  const VkFormat DepthFormat = FindDepthFormat();

  std::tie(DepthImage, DepthImageMemory) = CreateImage(SwapChainExtent.width, SwapChainExtent.height, 1,
                                                       DepthFormat,
                                                       VK_IMAGE_TILING_OPTIMAL,
                                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  DepthImageView = CreateImageView(DepthImage, DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

}

VkFormat FVulkanApplication::FindSupportedFormat(const std::vector<VkFormat> &Formats, VkImageTiling Tiling,
                                                 VkFormatFeatureFlags Features) const {
  check(PhysicalDevice != VK_NULL_HANDLE);

  for (const auto Format : Formats) {
    VkFormatProperties FormatProperties;
    vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, &FormatProperties);
    if (Tiling == VK_IMAGE_TILING_LINEAR && (FormatProperties.linearTilingFeatures & Features) == Features) {
      return Format;
    }
    if (Tiling == VK_IMAGE_TILING_OPTIMAL && (FormatProperties.optimalTilingFeatures & Features) == Features) {
      return Format;
    }
  }

  fatal(true, "Failed to find supported format");
}

VkFormat FVulkanApplication::FindDepthFormat() const {
  return FindSupportedFormat(
    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
    VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool FVulkanApplication::HasStencilComponent(VkFormat Format) {
  return Format == VK_FORMAT_D32_SFLOAT_S8_UINT || Format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void FVulkanApplication::DestroyDepthResources() {
  if (DepthImageView != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    DestroyImageView(DepthImageView);
    DepthImageView = VK_NULL_HANDLE;
  }
  if (DepthImageMemory != VK_NULL_HANDLE || DepthImage != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    DestroyImage(DepthImage, DepthImageMemory);
    DepthImage = VK_NULL_HANDLE;
    DepthImageMemory = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreateTextureImage() {
  check(Device != VK_NULL_HANDLE);

  int TextureWidth = 0;
  int TextureHeight = 0;
  int TextureChannels = 0;

  stbi_uc *Pixels = stbi_load(kTexturePath.c_str(), &TextureWidth, &TextureHeight, &TextureChannels,
                              STBI_rgb_alpha);
  const VkDeviceSize ImageSize = TextureWidth * TextureHeight * 4;
  MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(TextureWidth, TextureHeight))));
  fatal(Pixels == nullptr, "Failed to load texture image");

  constexpr VkBufferUsageFlags StagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  constexpr VkMemoryPropertyFlags StagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  auto [StagingBuffer, StagingMemory] = CreateBuffer(ImageSize, StagingUsage, StagingProperties);

  void *Data = nullptr;
  vk_verify(vkMapMemory(Device, StagingMemory, 0, ImageSize, 0, &Data));
  memcpy(Data, Pixels, ImageSize);
  vkUnmapMemory(Device, StagingMemory);
  stbi_image_free(Pixels);

  constexpr VkFormat ImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
  constexpr VkImageTiling ImageTiling = VK_IMAGE_TILING_OPTIMAL;
  constexpr VkImageUsageFlags ImageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    VK_IMAGE_USAGE_SAMPLED_BIT;
  constexpr VkMemoryPropertyFlags ImageMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  std::tie(TextureImage, TextureImageMemory) = CreateImage(TextureWidth, TextureHeight, MipLevels, ImageFormat,
                                                           ImageTiling, ImageUsage, ImageMemoryProperties);

  TransitionImageLayout(TextureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipLevels);

  CopyBufferToImage(StagingBuffer, TextureImage, TextureWidth, TextureHeight);

  GenerateMipMaps(TextureImage, VK_FORMAT_R8G8B8A8_SRGB, TextureWidth, TextureHeight, MipLevels);

  DestroyBuffer(StagingBuffer, StagingMemory);
}

std::pair<VkImage, VkDeviceMemory> FVulkanApplication::CreateImage(uint32_t Width, uint32_t Height, uint32_t mipLevels,
                                                                   VkFormat Format, VkImageTiling Tiling,
                                                                   VkImageUsageFlags Usage,
                                                                   VkMemoryPropertyFlags Properties) const {
  check(Device != VK_NULL_HANDLE);
  const VkImageCreateInfo ImageCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = Format,
    .extent = {Width, Height, 1},
    .mipLevels = mipLevels,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = Tiling,
    .usage = Usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VkImage Image = VK_NULL_HANDLE;
  VkDeviceMemory Memory = VK_NULL_HANDLE;
  vk_verify(vkCreateImage(Device, &ImageCreateInfo, nullptr, &Image));
  VkMemoryRequirements MemoryRequirements{};
  vkGetImageMemoryRequirements(Device, Image, &MemoryRequirements);

  const VkMemoryAllocateInfo MemoryAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = MemoryRequirements.size,
    .memoryTypeIndex = FindMemoryType(MemoryRequirements.memoryTypeBits, Properties),
  };

  vk_verify(vkAllocateMemory(Device, &MemoryAllocateInfo, nullptr, &Memory));
  vk_verify(vkBindImageMemory(Device, Image, Memory, 0));

  return {Image, Memory};
}

void FVulkanApplication::DestroyImage(VkImage Image, VkDeviceMemory Memory) const {
  if (Memory != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkFreeMemory(Device, Memory, nullptr);
    Memory = VK_NULL_HANDLE;
  }
  if (Image != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyImage(Device, Image, nullptr);
    Image = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::TransitionImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout,
                                               uint32_t mipLevels) const {
  VkCommandBuffer CommandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier ImageMemoryBarrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = OldLayout,
    .newLayout = NewLayout,
    .image = Image,
    .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, MipLevels, 0, 1},
  };

  VkPipelineStageFlags SourceStage = 0;
  VkPipelineStageFlags DestinationStage = 0;
  if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED
    && NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

    ImageMemoryBarrier.srcAccessMask = {};
    ImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    && NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

    ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else {
    checkf(true, "Not supported layout transition");
  }

  vkCmdPipelineBarrier(CommandBuffer, SourceStage, DestinationStage, 0,
                       0, nullptr, 0, nullptr,
                       1, &ImageMemoryBarrier);

  EndSingleTimeCommands(CommandBuffer);
}

void FVulkanApplication::CopyBufferToImage(VkBuffer Buffer, VkImage Image, uint32_t Width, uint32_t Height) const {
  VkCommandBuffer CommandBuffer = BeginSingleTimeCommands();

  const VkBufferImageCopy Region{
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
    .imageOffset = {0, 0, 0},
    .imageExtent = {Width, Height, 1},
  };

  vkCmdCopyBufferToImage(CommandBuffer, Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

  EndSingleTimeCommands(CommandBuffer);
}

void FVulkanApplication::DestroyTextureImage() {
  if (TextureImageMemory != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkFreeMemory(Device, TextureImageMemory, nullptr);
    TextureImageMemory = VK_NULL_HANDLE;
  }
  if (TextureImage != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyImage(Device, TextureImage, nullptr);
    TextureImage = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::GenerateMipMaps(VkImage Image, VkFormat ImageFormat, int32_t TextureWidth,
                                         int32_t TextureHeight, uint32_t mipLevels) const {
  VkFormatProperties FormatProperties;
  vkGetPhysicalDeviceFormatProperties(PhysicalDevice, ImageFormat, &FormatProperties);
  fatal(!(FormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
        "Texture image does not support linear biting");


  VkCommandBuffer CommandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier ImageMemoryBarrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = Image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    }
  };

  int32_t mipWidth = TextureWidth;
  int32_t mipHeight = TextureHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    ImageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;
    ImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr,
                         1, &ImageMemoryBarrier);
    VkOffset3D offsets[2], dstOffsets[2];
    offsets[0] = {0, 0, 0};
    offsets[1] = {mipWidth, mipHeight, 1};
    dstOffsets[0] = {0, 0, 0};
    dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    VkImageBlit blit{};
    memcpy(blit.srcOffsets, offsets, sizeof(offsets));
    memcpy(blit.dstOffsets, dstOffsets, sizeof(dstOffsets));
    blit.srcSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = i - 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    };
    blit.dstSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = i,
      .baseArrayLayer = 0,
      .layerCount = 1,
    };

    vkCmdBlitImage(CommandBuffer, Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    ImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    ImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    ImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr,
                         1, &ImageMemoryBarrier);

    if (mipWidth > 1) {
      mipWidth /= 2;
    }
    if (mipHeight > 1) {
      mipHeight /= 2;
    }
  }

  ImageMemoryBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
  ImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  ImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  ImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                       nullptr, 0, nullptr,
                       1, &ImageMemoryBarrier);

  EndSingleTimeCommands(CommandBuffer);
}

VkImageView FVulkanApplication::CreateImageView(VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags,
                                                uint32_t mipLevels) const {
  check(Device != VK_NULL_HANDLE);
  const VkImageViewCreateInfo ImageViewCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = Image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = Format,
    .subresourceRange = {AspectFlags, 0, mipLevels, 0, 1},
  };

  VkImageView ImageView = VK_NULL_HANDLE;
  vk_verify(vkCreateImageView(Device, &ImageViewCreateInfo, nullptr, &ImageView));

  return ImageView;
}

void FVulkanApplication::DestroyImageView(VkImageView ImageView) const {
  if (ImageView != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyImageView(Device, ImageView, nullptr);
  }
}

void FVulkanApplication::CreateTextureImageView() {
  TextureImageView = CreateImageView(TextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, MipLevels);
}

void FVulkanApplication::DestroyTextureImageView() {
  if (TextureImageView != VK_NULL_HANDLE) {
    DestroyImageView(TextureImageView);
    TextureImageView = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreateTextureSampler() {
  check(Device != VK_NULL_HANDLE);
  check(PhysicalDevice != VK_NULL_HANDLE);

  VkPhysicalDeviceProperties Properties;
  vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
  const VkSamplerCreateInfo TextureSamplerCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias = 0.0f,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = Properties.limits.maxSamplerAnisotropy,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .minLod = 0.0f,
    .maxLod = VK_LOD_CLAMP_NONE,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
  };

  vk_verify(vkCreateSampler(Device, &TextureSamplerCreateInfo, nullptr, &TextureSampler));
}

void FVulkanApplication::DestroyTextureSampler() {
  if (TextureSampler != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroySampler(Device, TextureSampler, nullptr);
    TextureSampler = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::LoadModel() {
  tinyobj::attrib_t Attrib;
  std::vector<tinyobj::shape_t> Shapes;
  std::vector<tinyobj::material_t> Materials;
  std::string Warn, Err;

  fatal(!tinyobj::LoadObj(&Attrib, &Shapes, &Materials, &Warn, &Err, kModelPath.c_str()), "Failed to load 3d model");

  std::unordered_map<FVertex, uint32_t> UniqueVertices{};

  for (const auto &Shape : Shapes) {
    for (const auto &Index : Shape.mesh.indices) {
      FVertex Vertex{};
      Vertex.Position = {
        Attrib.vertices[3 * Index.vertex_index + 0],
        Attrib.vertices[3 * Index.vertex_index + 1],
        Attrib.vertices[3 * Index.vertex_index + 2]
      };
      Vertex.TexCoord = {
        Attrib.texcoords[2 * Index.texcoord_index + 0],
        1.0f - Attrib.texcoords[2 * Index.texcoord_index + 1],
      };

      Vertex.Color = {1.0f, 1.0f, 1.0f};

      if (!UniqueVertices.contains(Vertex)) {
        UniqueVertices[Vertex] = static_cast<uint32_t>(UniqueVertices.size());
        Vertices.push_back(Vertex);
      }


      Indices.push_back(UniqueVertices[Vertex]);
    }
  }
}

void FVulkanApplication::CreateVertexBuffer() {
  check(Device != VK_NULL_HANDLE);
  const VkDeviceSize BufferSize = sizeof(Vertices[0]) * Vertices.size();

  constexpr VkMemoryPropertyFlags StagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  constexpr VkBufferUsageFlags StagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  constexpr VkMemoryPropertyFlags VertexProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  constexpr VkBufferUsageFlags VertexUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

  VkBuffer StagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory StagingBufferMemory = VK_NULL_HANDLE;
  std::tie(StagingBuffer, StagingBufferMemory) = CreateBuffer(BufferSize, StagingUsage,
                                                              StagingProperties);
  std::tie(VertexBuffer, VertexBufferMemory) = CreateBuffer(BufferSize, VertexUsage, VertexProperties);

  void *data;
  vk_verify(vkMapMemory(Device, StagingBufferMemory, 0, BufferSize, 0, &data));
  memcpy(data, Vertices.data(), BufferSize);
  vkUnmapMemory(Device, StagingBufferMemory);

  CopyBuffer(StagingBuffer, VertexBuffer, BufferSize);

  DestroyBuffer(StagingBuffer, StagingBufferMemory);
}

std::pair<VkBuffer, VkDeviceMemory> FVulkanApplication::CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                                                                     VkMemoryPropertyFlags Properties) const {
  check(Device != VK_NULL_HANDLE);
  const VkBufferCreateInfo BufferCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = Size,
    .usage = Usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VkBuffer Buffer = VK_NULL_HANDLE;
  VkDeviceMemory Memory = VK_NULL_HANDLE;
  vk_verify(vkCreateBuffer(Device, &BufferCreateInfo, nullptr, &Buffer));

  VkMemoryRequirements MemoryRequirements;
  vkGetBufferMemoryRequirements(Device, Buffer, &MemoryRequirements);


  const VkMemoryAllocateInfo MemoryAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = MemoryRequirements.size,
    .memoryTypeIndex = FindMemoryType(MemoryRequirements.memoryTypeBits, Properties),
  };

  vk_verify(vkAllocateMemory(Device, &MemoryAllocateInfo, nullptr, &Memory));
  vk_verify(vkBindBufferMemory(Device, Buffer, Memory, 0));

  return {Buffer, Memory};
}


uint32_t FVulkanApplication::FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties) const {
  check(PhysicalDevice != VK_NULL_HANDLE);

  VkPhysicalDeviceMemoryProperties MemoryProperties;
  vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

  for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; i++) {
    if ((TypeFilter & (1 << i)) && (MemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties) {
      return i;
    }
  }

  fatal(true, "Failed to find suitable memory type");
}

void FVulkanApplication::CopyBuffer(VkBuffer Src, VkBuffer Dst, VkDeviceSize Size) const {
  check(Device != VK_NULL_HANDLE);
  check(CommandPool != VK_NULL_HANDLE);
  check(GraphicsQueue != VK_NULL_HANDLE);

  VkCommandBuffer CopyCommandBuffer = BeginSingleTimeCommands();

  const VkBufferCopy CopyRegion = {
    .srcOffset = 0,
    .dstOffset = 0,
    .size = Size,
  };

  vkCmdCopyBuffer(CopyCommandBuffer, Src, Dst, 1, &CopyRegion);

  EndSingleTimeCommands(CopyCommandBuffer);
}


void FVulkanApplication::DestroyBuffer(VkBuffer Buffer, VkDeviceMemory Memory) const {
  if (Buffer != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    if (Memory != VK_NULL_HANDLE) {
      vkFreeMemory(Device, Memory, nullptr);
      Memory = VK_NULL_HANDLE;
    }
    vkDestroyBuffer(Device, Buffer, nullptr);
    Buffer = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::DestroyVertexBuffer() const {
  DestroyBuffer(VertexBuffer, VertexBufferMemory);
}

void FVulkanApplication::CreateIndexBuffer() {
  check(Device != VK_NULL_HANDLE);
  const VkDeviceSize BufferSize = sizeof(Indices[0]) * Indices.size();

  constexpr VkMemoryPropertyFlags StagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  constexpr VkBufferUsageFlags StagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  constexpr VkMemoryPropertyFlags IndexProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  constexpr VkBufferUsageFlags IndexUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

  VkBuffer StagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory StagingBufferMemory = VK_NULL_HANDLE;
  std::tie(StagingBuffer, StagingBufferMemory) = CreateBuffer(BufferSize, StagingUsage,
                                                              StagingProperties);
  std::tie(IndexBuffer, IndexBufferMemory) = CreateBuffer(BufferSize, IndexUsage, IndexProperties);

  void *data;
  vk_verify(vkMapMemory(Device, StagingBufferMemory, 0, BufferSize, 0, &data));
  memcpy(data, Indices.data(), BufferSize);
  vkUnmapMemory(Device, StagingBufferMemory);

  CopyBuffer(StagingBuffer, IndexBuffer, BufferSize);

  DestroyBuffer(StagingBuffer, StagingBufferMemory);
}

void FVulkanApplication::DestroyIndexBuffer() const {
  DestroyBuffer(IndexBuffer, IndexBufferMemory);
}

void FVulkanApplication::CreateUniformBuffer() {
  check(Device != VK_NULL_HANDLE);
  UniformBuffers.clear();
  UniformBuffersMemory.clear();
  UniformBuffersMapped.clear();

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    constexpr VkDeviceSize BufferSize = sizeof(FUniformBufferObject);
    constexpr VkBufferUsageFlags Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    constexpr VkMemoryPropertyFlags Properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto [Buffer, Memory] = CreateBuffer(BufferSize, Usage, Properties);
    void *Mapped;
    vk_verify(vkMapMemory(Device, Memory, 0, BufferSize, 0, &Mapped));

    UniformBuffers.emplace_back(Buffer);
    UniformBuffersMemory.emplace_back(Memory);
    UniformBuffersMapped.emplace_back(Mapped);
  }
}

void FVulkanApplication::UpdateUniformBuffer(uint32_t CurrentImage) const {

  FUniformBufferObject UBO{};
  UBO.Model = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  UBO.View = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                         glm::vec3(0.0f, 0.0f, 1.0f));
  UBO.Projection = glm::perspective(glm::radians(45.0f), static_cast<float>(SwapChainExtent.width) /
                                    static_cast<float>(SwapChainExtent.height), 0.1f, 10.0f);

  UBO.Projection[1][1] *= -1;

  memcpy(UniformBuffersMapped[CurrentImage], &UBO, sizeof(UBO));
}

void FVulkanApplication::DestroyUniformBuffer() {
  if (!UniformBuffers.empty()) {
    check(UniformBuffers.size() == UniformBuffersMemory.size());
    check(UniformBuffersMemory.size() == UniformBuffersMapped.size());
    check(Device != VK_NULL_HANDLE);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkUnmapMemory(Device, UniformBuffersMemory[i]);
      vkFreeMemory(Device, UniformBuffersMemory[i], nullptr);
      vkDestroyBuffer(Device, UniformBuffers[i], nullptr);
    }
    UniformBuffers.clear();
    UniformBuffersMemory.clear();
    UniformBuffersMapped.clear();
  }
}

void FVulkanApplication::CreateDescriptorPool() {
  check(Device != VK_NULL_HANDLE);

  static constexpr std::array PoolSize{
    VkDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT),
    VkDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT)
  };


  static constexpr VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets = MAX_FRAMES_IN_FLIGHT,
    .poolSizeCount = PoolSize.size(),
    .pPoolSizes = PoolSize.data(),
  };

  vk_verify(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));
}

void FVulkanApplication::DestroyDescriptorPool() {
  if (DescriptorPool != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
    DescriptorPool = VK_NULL_HANDLE;
  }
}

void FVulkanApplication::CreateDescriptorSets() {
  check(Device != VK_NULL_HANDLE);

  const std::vector<VkDescriptorSetLayout> DescriptorSetLayouts(MAX_FRAMES_IN_FLIGHT, DescriptorSetLayout);
  const VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = DescriptorPool,
    .descriptorSetCount = static_cast<uint32_t>(DescriptorSetLayouts.size()),
    .pSetLayouts = DescriptorSetLayouts.data(),
  };

  DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
  vk_verify(vkAllocateDescriptorSets(Device, &DescriptorSetAllocateInfo, DescriptorSets.data()));

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    const VkDescriptorBufferInfo BufferInfo{
      .buffer = UniformBuffers[i],
      .offset = 0,
      .range = sizeof(FUniformBufferObject),
    };

    const VkDescriptorImageInfo ImageInfo{
      .sampler = TextureSampler,
      .imageView = TextureImageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    std::array DescriptorWrites = {
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = DescriptorSets[i],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &BufferInfo,
      },
      VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = DescriptorSets[i],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &ImageInfo,
      },
    };

    vkUpdateDescriptorSets(Device, DescriptorWrites.size(), DescriptorWrites.data(), 0, nullptr);
  }
}

void FVulkanApplication::DestroyDescriptorSets() {
  if (!DescriptorSets.empty()) {
    check(Device != VK_NULL_HANDLE);
    vkFreeDescriptorSets(Device, DescriptorPool, static_cast<uint32_t>(DescriptorSets.size()), DescriptorSets.data());
    DescriptorSets.clear();
  }
}

void FVulkanApplication::CreateCommandBuffers() {
  check(Device != VK_NULL_HANDLE);

  CommandBuffers.clear();
  CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = CommandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
  };

  vk_verify(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, CommandBuffers.data()));
}

void FVulkanApplication::RecordCommandBuffer(uint32_t ImageIndex) const {
  check(!CommandBuffers.empty());
  check(CommandBuffers[FrameIndex] != VK_NULL_HANDLE);
  check(GraphicsPipeline != VK_NULL_HANDLE);

  constexpr VkCommandBufferBeginInfo BeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  vk_verify(vkBeginCommandBuffer(CommandBuffers[FrameIndex], &BeginInfo));

  TransitionImageLayout(
    SwapChainImages[ImageIndex],
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    {},
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT, 1);

  TransitionImageLayout(
    DepthImage,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
    VK_IMAGE_ASPECT_DEPTH_BIT, 1);

  constexpr auto ClearColor = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};
  constexpr auto ClearDepth = VkClearDepthStencilValue{1.0f, 0};

  const VkRenderingAttachmentInfo RenderingAttachmentInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = SwapChainImageViews[ImageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {ClearColor},
  };

  const VkRenderingAttachmentInfo DepthAttachmentInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = DepthImageView,
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = {.depthStencil = ClearDepth},
  };

  const VkRenderingInfo RenderingInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = {.offset = {0, 0}, .extent = SwapChainExtent},
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &RenderingAttachmentInfo,
    .pDepthAttachment = &DepthAttachmentInfo,
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

  constexpr VkDeviceSize Offsets[] = {0};
  vkCmdBindVertexBuffers(CommandBuffers[FrameIndex], 0, 1, &VertexBuffer, Offsets);
  vkCmdBindIndexBuffer(CommandBuffers[FrameIndex], IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(CommandBuffers[FrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipelineLayout, 0,
                          1, &DescriptorSets[FrameIndex], 0, nullptr);
  vkCmdDrawIndexed(CommandBuffers[FrameIndex], Indices.size(), 1, 0, 0, 0);

  vkCmdEndRendering(CommandBuffers[FrameIndex]);

  TransitionImageLayout(
    SwapChainImages[ImageIndex],
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    {},
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT, 1
    );

  vk_verify(vkEndCommandBuffer(CommandBuffers[FrameIndex]));
}

void FVulkanApplication::DestroyCommandBuffers() {
  check(CommandBuffers.size() == MAX_FRAMES_IN_FLIGHT);
  check(Device != VK_NULL_HANDLE);
  vkFreeCommandBuffers(Device, CommandPool, MAX_FRAMES_IN_FLIGHT, CommandBuffers.data());
  CommandBuffers.clear();
}

VkCommandBuffer FVulkanApplication::BeginSingleTimeCommands() const {
  check(CommandPool != VK_NULL_HANDLE);
  check(Device != VK_NULL_HANDLE);

  const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = CommandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  VkCommandBuffer Buffer;
  vk_verify(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &Buffer));
  constexpr VkCommandBufferBeginInfo CommandBufferBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vk_verify(vkBeginCommandBuffer(Buffer, &CommandBufferBeginInfo));

  return Buffer;
}

void FVulkanApplication::EndSingleTimeCommands(VkCommandBuffer &CommandBuffer) const {
  if (CommandBuffer != VK_NULL_HANDLE) {
    check(Device != VK_NULL_HANDLE);
    check(GraphicsQueue != VK_NULL_HANDLE);
    check(CommandPool != VK_NULL_HANDLE);

    vk_verify(vkEndCommandBuffer(CommandBuffer));

    const VkSubmitInfo SubmitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &CommandBuffer,
    };
    vk_verify(vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, nullptr));
    vk_verify(vkQueueWaitIdle(GraphicsQueue));

    vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
    CommandBuffer = VK_NULL_HANDLE;
  }
}


void FVulkanApplication::TransitionImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout,
                                               VkAccessFlags2 SrcAccessMask, VkAccessFlags2 DstAccessMask,
                                               VkPipelineStageFlags2 SrcStageMask,
                                               VkPipelineStageFlags2 DstStageMask,
                                               VkImageAspectFlags ImageAspectFlags, uint32_t mipLevels) const {
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
    .image = Image,
    .subresourceRange = {
      .aspectMask = ImageAspectFlags,
      .baseMipLevel = 0,
      .levelCount = mipLevels,
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

void FVulkanApplication::CreateSyncObjects() {
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
    vk_verify(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphore));
  }
  PresentCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  for (auto &Semaphore : PresentCompleteSemaphores) {
    vk_verify(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphore));
  }

  InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
  for (auto &Fence : InFlightFences) {
    vk_verify(vkCreateFence(Device, &FenceCreateInfo, nullptr, &Fence));
  }
}

void FVulkanApplication::DestroySyncObjects() {
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

EResult FVulkanApplication::DrawFrame() {
  check(Device != VK_NULL_HANDLE);
  check(!InFlightFences.empty());
  check(!PresentCompleteSemaphores.empty());
  check(!RenderCompleteSemaphores.empty());
  check(SwapChain != VK_NULL_HANDLE);

  vk_verify(vkWaitForFences(Device, 1, &InFlightFences[FrameIndex], VK_TRUE, UINT64_MAX));

  uint32_t ImageIndex = 0;
  const VkResult AcquireResult = vkAcquireNextImageKHR(Device, SwapChain, UINT64_MAX,
                                                       PresentCompleteSemaphores[FrameIndex],
                                                       VK_NULL_HANDLE, &ImageIndex);
  if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain();
    return EResult::SwapChainOutOfDate;
  }
  if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR) {
    vk_verify(AcquireResult);
  }

  vk_verify(vkResetFences(Device, 1, &InFlightFences[FrameIndex]));

  RecordCommandBuffer(ImageIndex);
  UpdateUniformBuffer(FrameIndex);

  constexpr VkPipelineStageFlags WaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  const VkSubmitInfo SubmitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &PresentCompleteSemaphores[FrameIndex],
    .pWaitDstStageMask = &WaitDstStageMask,
    .commandBufferCount = 1,
    .pCommandBuffers = &CommandBuffers[FrameIndex],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &RenderCompleteSemaphores[ImageIndex],
  };
  vk_verify(vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, InFlightFences[FrameIndex]));

  const VkPresentInfoKHR PresentInfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &RenderCompleteSemaphores[ImageIndex],
    .swapchainCount = 1,
    .pSwapchains = &SwapChain,
    .pImageIndices = &ImageIndex,
  };
  const VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &PresentInfo);
  if (PresentResult == VK_SUBOPTIMAL_KHR || PresentResult == VK_ERROR_OUT_OF_DATE_KHR || bFramebufferResized) {
    bFramebufferResized = false;
    RecreateSwapChain();
    return EResult::SwapChainSuboptimal;
  }
  vk_verify(PresentResult);

  FrameIndex = (FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  return EResult::Success;
}

void FVulkanApplication::GLFWFramebufferResizeCallback(GLFWwindow *window, int Width, int Height) {
  auto *App = static_cast<FVulkanApplication *>(glfwGetWindowUserPointer(window));
  App->bFramebufferResized = true;
}
