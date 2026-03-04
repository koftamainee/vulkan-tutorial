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

FVulkanTriangle::FVulkanTriangle(std::string &&ApplicationName, int WindowWidth, int WindowHeight) :
  mApplicationName(std::move(ApplicationName)),
  mWindowWidth(WindowWidth),
  mWindowHeight(WindowHeight) {}


FVulkanTriangle::~FVulkanTriangle() {
  Cleanup();
}


std::expected<std::unique_ptr<FVulkanTriangle>, EResult> FVulkanTriangle::New(
  std::string ApplicationName, int WindowWidth,
  int WindowHeight) {

  auto App = std::unique_ptr<FVulkanTriangle>(
    new FVulkanTriangle(std::move(ApplicationName), WindowWidth, WindowHeight));

  if (const auto Result = App->InitWindow(); Result != EResult::Success) { return std::unexpected(Result); }
  if (const auto Result = App->InitVulkan(); Result != EResult::Success) { return std::unexpected(Result); }
  return App;
}

EResult FVulkanTriangle::Run() {
  if (const auto Result = MainLoop(); Result != EResult::Success) { return Result; }

  return EResult::Success;
}

EResult FVulkanTriangle::InitWindow() {
  if (const auto Result = glfwInit(); Result != GLFW_TRUE) { return EResult::GLFWInitFailed; }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  mWindow = glfwCreateWindow(static_cast<int>(mWindowWidth), static_cast<int>(mWindowHeight),
                             mApplicationName.c_str(), nullptr, nullptr);
  if (mWindow == nullptr) {
    return EResult::WindowCreationFailed;
  }


  return EResult::Success;
}

void FVulkanTriangle::DeinitWindow() {
  if (mWindow != nullptr) { glfwDestroyWindow(mWindow); }
  mWindow = nullptr;
  glfwTerminate();
}

EResult FVulkanTriangle::InitVulkan() {
  if (const auto Result = CreateVKInstance(); Result != EResult::Success) {
    return Result;
  }

  if (mEnableValidationLayers) {
    if (const auto Result = CreateDebugMessenger(); Result != EResult::Success) {
      DeinitVulkan();
      return Result;
    }
  }

  if (const auto Result = CreateSurface(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreatePhysicalDevice(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateLogicalDevice(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateSwapChain(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateImageViews(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateGraphicsPipeline(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateCommandPool(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateCommandBuffer(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  if (const auto Result = CreateSyncObjects(); Result != EResult::Success) {
    DeinitVulkan();
    return Result;
  }

  return EResult::Success;
}

void FVulkanTriangle::DeinitVulkan() {
  DestroySyncObjects();
  DestroyCommandBuffer();
  DestroyCommandPool();
  DestroyGraphicsPipeline();
  DestroyImageViews();
  DestroySwapChain();
  DestroyLogicalDevice();
  DestroyPhysicalDevice();
  DestroySurface();
  if (mEnableValidationLayers) {
    DestroyDebugMessenger();
  }
  DestroyVKInstance();
}

EResult FVulkanTriangle::MainLoop() {
  uint64_t FrameCount = 0;
  double LastTime = glfwGetTime();

  while (!glfwWindowShouldClose(mWindow)) {
    glfwPollEvents();
    const auto Result = DrawFrame();
    if (Result != EResult::Success) {
      return Result;
    }

    ++FrameCount;
    const double Now = glfwGetTime();
    const double Delta = Now - LastTime;
    if (Delta >= 1.0) {
      const auto FPS = static_cast<uint64_t>(FrameCount / Delta);
     std::cout << "FPS | " << FPS << "\n";
      FrameCount = 0;
      LastTime = Now;
    }
  }

  const auto Result = vkDeviceWaitIdle(mDevice);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToWaitIdleDevice;
  }
  return EResult::Success;
}

void FVulkanTriangle::Cleanup() {
  DeinitVulkan();
  DeinitWindow();
}

EResult FVulkanTriangle::CreateVKInstance() {
  const VkApplicationInfo AppInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = mApplicationName.c_str(),
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_4,
  };

  auto ExtensionsResult = GetRequiredInstanceExtensions();
  if (!ExtensionsResult) { return ExtensionsResult.error(); }
  const auto &Extensions = ExtensionsResult.value();

  const VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo = MakeDebugUtilsMessengerCreateInfo();

  const VkInstanceCreateInfo CreateInfo = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = mEnableValidationLayers ? &DebugCreateInfo : nullptr,
    .pApplicationInfo = &AppInfo,
    .enabledLayerCount = mEnableValidationLayers
    ? static_cast<uint32_t>(mValidationLayers.size())
    : 0,
    .ppEnabledLayerNames = mEnableValidationLayers
    ? mValidationLayers.data()
    : nullptr,
    .enabledExtensionCount = static_cast<uint32_t>(Extensions.size()),
    .ppEnabledExtensionNames = Extensions.data(),
  };

  auto const Result = vkCreateInstance(&CreateInfo, nullptr, &mInstance);
  if (Result != VK_SUCCESS) { return EResult::FailedToCreateVkInstance; }

  return EResult::Success;
}

std::expected<std::vector<const char *>, EResult> FVulkanTriangle::GetRequiredInstanceExtensions() {
  uint32_t GLFWExtensionsCount = 0;
  const char **GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionsCount);
  if (GLFWExtensionsCount == 0) {
    return std::unexpected(EResult::FailedToGetGLFWExtensions);
  }

  uint32_t VkExtensionsCount = 0;
  auto Result = vkEnumerateInstanceExtensionProperties(nullptr, &VkExtensionsCount, nullptr);
  if (Result != VK_SUCCESS) { return std::unexpected(EResult::FailedToGetVkExtensions); }

  std::vector<VkExtensionProperties> VkExtensions(VkExtensionsCount);
  Result = vkEnumerateInstanceExtensionProperties(nullptr, &VkExtensionsCount, VkExtensions.data());
  if (Result != VK_SUCCESS) { return std::unexpected(EResult::FailedToGetVkExtensions); }

  for (uint32_t i = 0; i < GLFWExtensionsCount; ++i) {
    bool Found = false;
    for (const auto &VkExtension : VkExtensions) {
      if (strcmp(VkExtension.extensionName, GLFWExtensions[i]) == 0) {
        Found = true;
        break;
      }
    }
    if (!Found) { return std::unexpected(EResult::RequiredGLFWExtensionIsNotSupported); }
  }

  auto Extensions = std::vector<const char *>(GLFWExtensions, GLFWExtensions + GLFWExtensionsCount);
  if (mEnableValidationLayers) {
    Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return Extensions;
}

EResult FVulkanTriangle::ValidateLayers() {
  uint32_t vkLayerPropertiesCount = 0;
  auto Result = vkEnumerateInstanceLayerProperties(&vkLayerPropertiesCount, nullptr);
  if (Result != VK_SUCCESS) { return EResult::FailedToGetVkLayers; }

  std::vector<VkLayerProperties> VkLayers(vkLayerPropertiesCount);
  Result = vkEnumerateInstanceLayerProperties(&vkLayerPropertiesCount, VkLayers.data());
  if (Result != VK_SUCCESS) { return EResult::FailedToGetVkLayers; }

  for (auto const NeededLayer : mValidationLayers) {
    bool Found = false;
    for (const auto &VkLayer : VkLayers) {
      if (strcmp(NeededLayer, VkLayer.layerName) == 0) {
        Found = true;
        break;
      }
    }
    if (!Found) {
      return EResult::VkLayerNotSupported;
    }
  }

  return EResult::Success;
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

void FVulkanTriangle::DestroyVKInstance() {
  if (mInstance != VK_NULL_HANDLE) { vkDestroyInstance(mInstance, nullptr); }
  mInstance = VK_NULL_HANDLE;
}

EResult FVulkanTriangle::CreateSurface() {
  if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS) {
    return EResult::FailedToCreateSurface;
  }
  return EResult::Success;
}

void FVulkanTriangle::DestroySurface() {
  if (mSurface) {
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    mSurface = VK_NULL_HANDLE;
  }
}

EResult FVulkanTriangle::CreateDebugMessenger() {

  if (const auto Result = ValidateLayers(); Result != EResult::Success) { return Result; }

  const auto vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT"));
  if (vkCreateDebugUtilsMessenger == nullptr) {
    return EResult::FailedToCreateVkDebugMessenger;
  }

  const auto CreateInfo = MakeDebugUtilsMessengerCreateInfo();

  const auto Result = vkCreateDebugUtilsMessenger(mInstance, &CreateInfo, nullptr, &mDebugMessenger);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateVkDebugMessenger;
  }
  return EResult::Success;
}

void FVulkanTriangle::DestroyDebugMessenger() {
  if (mDebugMessenger != VK_NULL_HANDLE) {
    const auto vkDestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
    if (vkDestroyDebugUtilsMessenger != nullptr) {
      vkDestroyDebugUtilsMessenger(mInstance, mDebugMessenger, nullptr);
    }
  }
  mDebugMessenger = VK_NULL_HANDLE;
}

EResult FVulkanTriangle::CreatePhysicalDevice() {
  uint32_t PhysicalDevicesCount = 0;
  auto Result = vkEnumeratePhysicalDevices(mInstance, &PhysicalDevicesCount, nullptr);
  if (Result != VK_SUCCESS) {
    if (PhysicalDevicesCount == 0) {
      return EResult::NoVulkanGPUFound;
    }
    return EResult::FailedToEnumeratePhysicalDevices;
  }

  std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDevicesCount);
  Result = vkEnumeratePhysicalDevices(mInstance, &PhysicalDevicesCount, PhysicalDevices.data());
  if (Result != VK_SUCCESS) {
    if (PhysicalDevicesCount == 0) {
      return EResult::NoVulkanGPUFound;
    }
    return EResult::FailedToEnumeratePhysicalDevices;
  }

  auto PickedDeviceResult = PickPhysicalDevice(PhysicalDevices, mSurface);
  if (!PickedDeviceResult) {
    return PickedDeviceResult.error();
  }
  mPhysicalDevice = PickedDeviceResult.value();

  return EResult::Success;
}

std::expected<VkPhysicalDevice, EResult> FVulkanTriangle::PickPhysicalDevice(
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

    if (!FindQueueFamilies(PhysicalDevice, Surface).isComplete()) {
      continue;
    }

    uint32_t ExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);
    std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, AvailableExtensions.data());

    bool HasAllExtensions = true;
    for (const char *Required : mDeviceExtensions) {
      bool Found = false;
      for (uint32_t i = 0; i < ExtensionCount; i++) {
        if (strcmp(AvailableExtensions[i].extensionName, Required) == 0) {
          Found = true;
          break;
        }
      }
      if (!Found) {
        HasAllExtensions = false;
        break;
      }
    }
    if (!HasAllExtensions) {
      continue;
    }

    uint32_t FormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr);

    uint32_t PresentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr);

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

  if (PhysicalDevicesRanked.empty()) {
    return std::unexpected(EResult::NoVulkanGPUFound);
  }

  if (PhysicalDevicesRanked.rbegin()->first > 0) {
    return PhysicalDevicesRanked.rbegin()->second;
  }
  else {
    return std::unexpected(EResult::NoVulkanGPUFound);
  }

}

void FVulkanTriangle::DestroyPhysicalDevice() {
  mPhysicalDevice = VK_NULL_HANDLE;
}

FVulkanTriangle::FQueueFamilyIndices FVulkanTriangle::FindQueueFamilies(VkPhysicalDevice PhysicalDevice,
                                                                        VkSurfaceKHR Surface) {
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

EResult FVulkanTriangle::CreateLogicalDevice() {
  const auto Indices = FindQueueFamilies(mPhysicalDevice, mSurface);
  if (!Indices.isComplete()) {
    return EResult::FailedToCreateVkDevice;
  }

  mQueueFamilyIndices = Indices;

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
    .enabledExtensionCount = static_cast<uint32_t>(mDeviceExtensions.size()),
    .ppEnabledExtensionNames = mDeviceExtensions.data(),
  };

  auto Result = vkCreateDevice(mPhysicalDevice, &DeviceCreateInfo, nullptr, &mDevice);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateVkDevice;
  }

  vkGetDeviceQueue(mDevice, Indices.GraphicsFamily, 0, &mGraphicsQueue);
  vkGetDeviceQueue(mDevice, Indices.PresentFamily, 0, &mPresentQueue);

  return EResult::Success;
}

void FVulkanTriangle::DestroyLogicalDevice() {
  if (mDevice) {
    vkDestroyDevice(mDevice, nullptr);
  }
  mPresentQueue = VK_NULL_HANDLE;
  mGraphicsQueue = VK_NULL_HANDLE;
  mDevice = VK_NULL_HANDLE;
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
  if (Capabilities.currentExtent.width != UINT32_MAX) {
    return Capabilities.currentExtent;
  }

  // int Width = 0;
  // int Height = 0;
  //
  // glfwGetFramebufferSize(Window, &Width, &Height);
  //

  // TODO: fixme
  int Width = mWindowWidth;
  int Height = mWindowHeight;

    std::cout << "Framebuffer size: " << Width << "x" << Height << std::endl;

  return {
    .width = std::clamp<uint32_t>(Width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width),
    .height = std::clamp<uint32_t>(Height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height)};

}

EResult FVulkanTriangle::CreateSwapChain() {
  VkSurfaceCapabilitiesKHR Capabilities;
  auto Result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &Capabilities);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSurfaceCapabilities;
  }

  uint32_t FormatsCount = 0;
  Result = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &FormatsCount, nullptr);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSurfaceFormats;
  }
  std::vector<VkSurfaceFormatKHR> Formats(FormatsCount);
  Result = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &FormatsCount, Formats.data());
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSurfaceFormats;
  }

  uint32_t PresentModesCount = 0;
  Result = vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &PresentModesCount, nullptr);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSurfacePresentModes;
  }
  std::vector<VkPresentModeKHR> PresentModes(PresentModesCount);
  Result = vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &PresentModesCount,
                                                     PresentModes.data());
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSurfacePresentModes;
  }

  mSwapChainSurfaceFormat = PickSwapChainSurfaceFormat(Formats);
  mSwapChainExtent = PickSwapChainExtent(Capabilities, mWindow);
  const auto PresentMode = PickSwapChainPresentMode(PresentModes);

  auto ImageCount = Capabilities.minImageCount + 1;
  if (Capabilities.maxImageCount > 0 && ImageCount > Capabilities.maxImageCount) {
    ImageCount = Capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR SwapChainCreateInfo{
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .flags = VkSwapchainCreateFlagsKHR(),
    .surface = mSurface,
    .minImageCount = ImageCount,
    .imageFormat = mSwapChainSurfaceFormat.format,
    .imageColorSpace = mSwapChainSurfaceFormat.colorSpace,
    .imageExtent = mSwapChainExtent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .preTransform = Capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = PresentMode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE,
  };

  const uint32_t QueueFamilyIndices[] = {mQueueFamilyIndices.GraphicsFamily, mQueueFamilyIndices.PresentFamily};

  if (QueueFamilyIndices[0] != QueueFamilyIndices[1]) {
    SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    SwapChainCreateInfo.queueFamilyIndexCount = 2;
    SwapChainCreateInfo.pQueueFamilyIndices = QueueFamilyIndices;
  }


  Result = vkCreateSwapchainKHR(mDevice, &SwapChainCreateInfo, nullptr, &mSwapChain);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateSwapChain;
  }

  uint32_t SwapChainImageCount = 0;
  Result = vkGetSwapchainImagesKHR(mDevice, mSwapChain, &SwapChainImageCount, nullptr);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSwapChainImages;
  }
  mSwapChainImages.resize(SwapChainImageCount);
  Result = vkGetSwapchainImagesKHR(mDevice, mSwapChain, &SwapChainImageCount, mSwapChainImages.data());
  if (Result != VK_SUCCESS) {
    return EResult::FailedToGetSwapChainImages;
  }

  return EResult::Success;
}

void FVulkanTriangle::DestroySwapChain() {
  if (mSwapChain != VK_NULL_HANDLE) {
    mSwapChainImages.clear();
    vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    mSwapChain = VK_NULL_HANDLE;
    mSwapChainImages.clear();
  }
}

EResult FVulkanTriangle::CreateImageViews() {
  mSwapChainImageViews.clear();


  for (const auto Image : mSwapChainImages) {
    const VkImageViewCreateInfo ImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = Image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = mSwapChainSurfaceFormat.format,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    VkImageView ImageView = VK_NULL_HANDLE;
    const auto Result = vkCreateImageView(mDevice, &ImageViewCreateInfo, nullptr, &ImageView);
    if (Result != VK_SUCCESS) {
      return EResult::FailedToCreateImageView;
    }
    mSwapChainImageViews.emplace_back(ImageView);

  }


  return EResult::Success;
}

void FVulkanTriangle::DestroyImageViews() {
  for (const auto ImageView : mSwapChainImageViews) {
    vkDestroyImageView(mDevice, ImageView, nullptr);
  }
  mSwapChainImageViews.clear();
}

EResult FVulkanTriangle::CreateGraphicsPipeline() {
  const auto ShaderCode = ReadFile("../shaders/slang.spv");
  if (ShaderCode.empty()) {
    return EResult::FailedToReadShadersFromFile;
  }

  const auto ShaderModuleResult = CreateShaderModule(ShaderCode);
  if (!ShaderModuleResult) {
    return ShaderModuleResult.error();
  }
  const auto ShaderModule = ShaderModuleResult.value();

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

  constexpr VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {
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

  auto Result = vkCreatePipelineLayout(mDevice, &LayoutCreateInfo, nullptr, &mPipelineLayout);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreatePipelineLayout;
  }

  const VkPipelineRenderingCreateInfo RenderingCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &mSwapChainSurfaceFormat.format,
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
    .layout = mPipelineLayout,
    .renderPass = nullptr,
  };

  Result = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, nullptr,
                                     &mGraphicsPipeline);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateGraphicsPipeline;
  }

  DestroyShaderModule(ShaderModule);
  return EResult::Success;
}

void FVulkanTriangle::DestroyGraphicsPipeline() {
  if (mPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    mPipelineLayout = VK_NULL_HANDLE;
  }
  if (mGraphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
    mGraphicsPipeline = VK_NULL_HANDLE;
  }
}

std::expected<VkShaderModule, EResult> FVulkanTriangle::CreateShaderModule(const std::vector<char> &ShaderCode) const {
  const VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = ShaderCode.size(),
    .pCode = reinterpret_cast<const uint32_t *>(ShaderCode.data())
  };

  VkShaderModule ShaderModule = VK_NULL_HANDLE;
  const auto Result = vkCreateShaderModule(mDevice, &ShaderModuleCreateInfo, nullptr, &ShaderModule);
  if (Result != VK_SUCCESS) {
    return std::unexpected(EResult::FailedToCreateShaderModule);
  }

  return ShaderModule;
}

void FVulkanTriangle::DestroyShaderModule(VkShaderModule ShaderModule) const {
  vkDestroyShaderModule(mDevice, ShaderModule, nullptr);
}

EResult FVulkanTriangle::CreateCommandPool() {
  const VkCommandPoolCreateInfo CommandPoolCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = mQueueFamilyIndices.GraphicsFamily,
  };

  const auto Result = vkCreateCommandPool(mDevice, &CommandPoolCreateInfo, nullptr, &mCommandPool);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateCommandPool;
  }

  return EResult::Success;
}

void FVulkanTriangle::DestroyCommandPool() {
  if (mCommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    mCommandPool = VK_NULL_HANDLE;
  }
}

EResult FVulkanTriangle::CreateCommandBuffer() {

  const VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = mCommandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  const auto Result = vkAllocateCommandBuffers(mDevice, &CommandBufferAllocateInfo, &mCommandBuffer);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToAllocateCommandBuffers;
  }

  return EResult::Success;
}

void FVulkanTriangle::DestroyCommandBuffer() {
  if (mCommandBuffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(mDevice, mCommandPool, 1, &mCommandBuffer);
    mCommandBuffer = VK_NULL_HANDLE;
  }
}

EResult FVulkanTriangle::RecordCommandBuffer(uint32_t ImageIndex) const {

  constexpr VkCommandBufferBeginInfo BeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  auto Result = vkBeginCommandBuffer(mCommandBuffer, &BeginInfo);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToBeginCommandBuffer;
  }

  TransitionImageLayout(
    ImageIndex,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    {},
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  constexpr auto ClearColor = VkClearColorValue{0.0f, 0.0f, 0.0f, 1.0f};

  const VkRenderingAttachmentInfo RenderingAttachmentInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = mSwapChainImageViews[ImageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {ClearColor},
  };

  const VkRenderingInfo RenderingInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = {.offset = {0, 0}, .extent = mSwapChainExtent},
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &RenderingAttachmentInfo,
  };

  vkCmdBeginRendering(mCommandBuffer, &RenderingInfo);

  vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

  const VkViewport Viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = static_cast<float>(mSwapChainExtent.width),
    .height = static_cast<float>(mSwapChainExtent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  const VkRect2D Scissor = {
    .offset = {0, 0},
    .extent = mSwapChainExtent,
  };

  vkCmdSetViewport(mCommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(mCommandBuffer, 0, 1, &Scissor);

  vkCmdDraw(mCommandBuffer, 3, 1, 0, 0);

  vkCmdEndRendering(mCommandBuffer);

  TransitionImageLayout(
    ImageIndex,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    {},
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
    );

  Result = vkEndCommandBuffer(mCommandBuffer);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToEndCommandBuffer;
  }

  return EResult::Success;
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
    .image = mSwapChainImages[ImageIndex],
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

  vkCmdPipelineBarrier2(mCommandBuffer, &DependencyInfo);
}

EResult FVulkanTriangle::DrawFrame() {
  auto VKResult = vkWaitForFences(mDevice, 1, &mDrawFence,VK_TRUE, UINT64_MAX);
  if (VKResult != VK_SUCCESS) {
    return EResult::FailedToWaitForFence;
  }

  uint32_t ImageIndex = 0;
  VKResult = vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, mPresentCompleteSemaphore, VK_NULL_HANDLE,
                                   &ImageIndex);
  if (VKResult != VK_SUCCESS) {
    return EResult::FailedToAcquireNextImage;
  }

  const auto Result = RecordCommandBuffer(ImageIndex);
  if (Result != EResult::Success) {
    return Result;
  }

  VKResult = vkResetFences(mDevice, 1, &mDrawFence);
  if (VKResult != VK_SUCCESS) {
    return EResult::FailedToResetFence;
  }


  constexpr VkPipelineStageFlags WaitDstStorageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;


  const VkSubmitInfo SubmitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &mPresentCompleteSemaphore,
    .pWaitDstStageMask = &WaitDstStorageMask,
    .commandBufferCount = 1,
    .pCommandBuffers = &mCommandBuffer,
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = &mRenderCompleteSemaphores[ImageIndex],
  };


  VKResult = vkQueueSubmit(mGraphicsQueue, 1, &SubmitInfo, mDrawFence);
  if (VKResult != VK_SUCCESS) {
    return EResult::FailedToSubmitToQueue;
  }

  const VkPresentInfoKHR PresentInfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &mRenderCompleteSemaphores[ImageIndex],
    .swapchainCount = 1,
    .pSwapchains = &mSwapChain,
    .pImageIndices = &ImageIndex,
  };

  VKResult = vkQueuePresentKHR(mGraphicsQueue, &PresentInfo);
  if (VKResult != VK_SUCCESS) {
    return EResult::FailedToPresentQueue;
  }

  return EResult::Success;
}

EResult FVulkanTriangle::CreateSyncObjects() {
  constexpr VkSemaphoreCreateInfo SemaphoreCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  constexpr VkFenceCreateInfo FenceCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };


  mRenderCompleteSemaphores.resize(mSwapChainImages.size());
  for (auto &Semaphore : mRenderCompleteSemaphores) {
    const auto Result = vkCreateSemaphore(mDevice, &SemaphoreCreateInfo, nullptr, &Semaphore);
    if (Result != VK_SUCCESS) {
      return EResult::FailedToCreateSemaphore;
    }
  }

  auto Result = vkCreateSemaphore(mDevice, &SemaphoreCreateInfo, nullptr, &mPresentCompleteSemaphore);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateSemaphore;
  }


  Result = vkCreateFence(mDevice, &FenceCreateInfo, nullptr, &mDrawFence);
  if (Result != VK_SUCCESS) {
    return EResult::FailedToCreateFence;
  }

  return EResult::Success;
}

void FVulkanTriangle::DestroySyncObjects() {

  for (const auto Semaphore : mRenderCompleteSemaphores) {
    vkDestroySemaphore(mDevice, Semaphore, nullptr);
  }

  vkDestroySemaphore(mDevice, mPresentCompleteSemaphore, nullptr);


  if (mDrawFence != VK_NULL_HANDLE) {
    vkDestroyFence(mDevice, mDrawFence, nullptr);
    mDrawFence = VK_NULL_HANDLE;
  }
}
