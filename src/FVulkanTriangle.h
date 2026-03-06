#pragma once

#define GLFW_INCLUDE_VULKAN

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

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

  struct Vertex {
    glm::vec2 Position;
    glm::vec3 Color;

    inline static VkVertexInputBindingDescription GetBindingDescription() {
      return {
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }
  };

private:
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
  static VkExtent2D PickSwapChainExtent(const VkSurfaceCapabilitiesKHR &Capabilities, GLFWwindow *Window);
  void DestroySwapChain();

  void RecreateSwapChain();

  void CreateImageViews();
  void DestroyImageViews();

  void CreateGraphicsPipeline();
  VkShaderModule CreateShaderModule(const std::vector<char> &ShaderCode) const;
  void DestroyShaderModule(VkShaderModule ShaderModule) const;
  void DestroyGraphicsPipeline();

  void CreateCommandPool();
  void DestroyCommandPool();

  void CreateCommandBuffers();
  void RecordCommandBuffer(uint32_t ImageIndex) const;
  void DestroyCommandBuffers();

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

  static void GLFWFramebufferResizeCallback(GLFWwindow *window, int Width, int Height);

private:
  static constexpr std::array<const char *, 1> ValidationLayers{
    "VK_LAYER_KHRONOS_validation"
  };

  static constexpr std::array<const char *, 2> DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME
  };

  const std::vector<Vertex> Vertices = {
    {.Position = {0.0f, -0.5f}, .Color = {1.0f, 0.0f, 0.0f}},
    {.Position = {0.5f, 0.5f}, .Color = {0.0f, 1.0f, 0.0f}},
    {.Position = {-0.5f, 0.5f}, .Color = {0.0f, 0.0f, 1.0f}},
  };

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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
  std::vector<VkCommandBuffer> CommandBuffers{};

  std::vector<VkSemaphore> PresentCompleteSemaphores{};
  std::vector<VkSemaphore> RenderCompleteSemaphores{};
  std::vector<VkFence> InFlightFences{};

  FQueueFamilyIndices QueueFamilyIndices{};

  uint32_t FrameIndex = 0;
  bool bFramebufferResized = false;
};
