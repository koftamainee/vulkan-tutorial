#ifndef VULKAN_FVULKANTRIANGLE_H
#define VULKAN_FVULKANTRIANGLE_H
#define GLFW_INCLUDE_VULKAN

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "EResult.h"

class FVulkanTriangle final {
public:
  FVulkanTriangle() = default;
  ~FVulkanTriangle();

  FVulkanTriangle(const FVulkanTriangle &) = delete;
  FVulkanTriangle &operator=(const FVulkanTriangle &) = delete;
  FVulkanTriangle(FVulkanTriangle &&) noexcept = delete;
  FVulkanTriangle &operator=(FVulkanTriangle &&) noexcept = delete;

  void Init(std::string &&InApplicationName, int InWindowWidth, int InWindowHeight);
  void Destroy();
  void Run();

private:
  struct FQueueFamilyIndices {
    uint32_t GraphicsFamily = UINT32_MAX;
    uint32_t PresentFamily = UINT32_MAX;

    inline bool IsComplete() const {
      return GraphicsFamily != UINT32_MAX && PresentFamily != UINT32_MAX;
    }
  };

  void MainLoop();

  void InitWindow();
  void DeinitWindow();

  void InitVulkan();
  void DeinitVulkan();

  void CreateInstance();
  static std::vector<const char *> GetRequiredInstanceExtensions();
  void DestroyInstance();

  void CreateDebugMessenger();
  static void ValidateLayers();
  static VkDebugUtilsMessengerCreateInfoEXT MakeDebugUtilsMessengerCreateInfo();
  void DestroyDebugMessenger();

  void CreateSurface();
  void DestroySurface();

  void CreatePhysicalDevice();
  static VkPhysicalDevice PickPhysicalDevice(const std::vector<VkPhysicalDevice> &Devices, VkSurfaceKHR Surface);
  void DestroyPhysicalDevice();

  void CreateLogicalDevice();
  static FQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);
  void DestroyLogicalDevice();

  void CreateSwapChain();
  static VkSurfaceFormatKHR PickSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &Formats);
  static VkPresentModeKHR PickSwapChainPresentMode(const std::vector<VkPresentModeKHR> &PresentModes);
  VkExtent2D PickSwapChainExtent(const VkSurfaceCapabilitiesKHR &Capabilities, GLFWwindow *Window);
  void DestroySwapChain();

  void CreateImageViews();
  void DestroyImageViews();

  void CreateGraphicsPipeline();
  VkShaderModule CreateShaderModule(const std::vector<char> &ShaderCode) const;
  void DestroyShaderModule(VkShaderModule ShaderModule) const;
  void DestroyGraphicsPipeline();

  void CreateCommandPool();
  void DestroyCommandPool();

  void CreateCommandBuffer();
  void RecordCommandBuffer(uint32_t ImageIndex) const;
  void DestroyCommandBuffer();

  void TransitionImageLayout(
    uint32_t ImageIndex,
    VkImageLayout OldLayout,
    VkImageLayout NewLayout,
    VkAccessFlags2 SrcAccessMask,
    VkAccessFlags2 DstAccessMask,
    VkPipelineStageFlags2 SrcStageMask,
    VkPipelineStageFlags2 DstStageMask) const;

  void CreateSyncObjects();
  void DestroySyncObjects();

  EResult DrawFrame();

private:
  static constexpr std::array<const char *, 1> ValidationLayers{
    "VK_LAYER_KHRONOS_validation"
  };

  static constexpr std::array<const char *, 2> DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME
  };

#ifdef NDEBUG
  static constexpr bool EnableValidationLayers = false;
#else
  static constexpr bool EnableValidationLayers = true;
#endif

  bool bIsInitialized = false;
  std::string ApplicationName{};
  int WindowWidth = 0;
  int WindowHeight = 0;

  GLFWwindow *Window = nullptr;

  VkInstance Instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT DebugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR Surface = VK_NULL_HANDLE;

  VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
  VkDevice Device = VK_NULL_HANDLE;
  VkQueue GraphicsQueue = VK_NULL_HANDLE;
  VkQueue PresentQueue = VK_NULL_HANDLE;

  VkSwapchainKHR SwapChain = VK_NULL_HANDLE;
  VkSurfaceFormatKHR SwapChainSurfaceFormat{};
  VkExtent2D SwapChainExtent{};

  std::vector<VkImage> SwapChainImages{};
  std::vector<VkImageView> SwapChainImageViews{};

  VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
  VkPipeline GraphicsPipeline = VK_NULL_HANDLE;

  VkCommandPool CommandPool = VK_NULL_HANDLE;
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;

  VkSemaphore PresentCompleteSemaphore = VK_NULL_HANDLE;
  std::vector<VkSemaphore> RenderCompleteSemaphores{};
  VkFence DrawFence = VK_NULL_HANDLE;

  FQueueFamilyIndices QueueFamilyIndices{};
};

#endif //VULKAN_FVULKANTRIANGLE_H