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

  struct FVertex {
    glm::vec2 Position;
    glm::vec3 Color;

    constexpr inline static VkVertexInputBindingDescription GetBindingDescription() {
      return {
        .binding = 0,
        .stride = sizeof(FVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }

    constexpr inline static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescription() {
      return {
        {{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(FVertex, Position)},
         {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FVertex, Color)}},
      };
    }
  };

  struct FUniformBufferObject {
    glm::mat4 Model;
    glm::mat4 View;
    glm::mat4 Projection;
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

  void CreateDescriptorSetLayout();
  void DestroyDescriptorSetLayout();

  void CreateGraphicsPipeline();
  VkShaderModule CreateShaderModule(const std::vector<char> &ShaderCode) const;
  void DestroyShaderModule(VkShaderModule ShaderModule) const;
  void DestroyGraphicsPipeline();

  void CreateCommandPool();
  void DestroyCommandPool();

  void CreateVertexBuffer();
  std::pair<VkBuffer, VkDeviceMemory> CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
                                                   VkMemoryPropertyFlags Properties) const;
  uint32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties) const;
  void CopyBuffer(VkBuffer Src, VkBuffer Dst, VkDeviceSize Size) const;
  void DestroyBuffer(VkBuffer Buffer, VkDeviceMemory Memory) const;
  void DestroyVertexBuffer() const;

  void CreateIndexBuffer();
  void DestroyIndexBuffer() const;

  void CreateUniformBuffer();
  void UpdateUniformBuffer(uint32_t CurrentImage) const;
  void DestroyUniformBuffer();

  void CreateDescriptorPool();
  void DestroyDescriptorPool();

  void CreateDescriptorSets();
  void DestroyDescriptorSets();

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

  const std::vector<FVertex> Vertices = {
    {{ 0.0000f,  0.0000f}, {1.00f, 0.85f, 0.0f}},  // 0 center — hot yellow core

    {{ 0.0000f, -0.5500f}, {1.00f, 0.08f, 0.00f}},  // 1 top
    {{ 0.5226f, -0.1699f}, {1.00f, 0.08f, 0.00f}},  // 2 upper-right
    {{ 0.3230f,  0.4455f}, {0.60f, 0.00f, 0.00f}},  // 3 lower-right
    {{-0.3230f,  0.4455f}, {0.60f, 0.00f, 0.00f}},  // 4 lower-left
    {{-0.5226f, -0.1699f}, {1.00f, 0.08f, 0.00f}},  // 5 upper-left

    {{ 0.2129f, -0.2938f}, {1.00f, 0.30f, 0.00f}},  // 6
    {{ 0.3441f,  0.1119f}, {0.80f, 0.04f, 0.00f}},  // 7
    {{ 0.0000f,  0.3618f}, {0.50f, 0.00f, 0.00f}},  // 8
    {{-0.3441f,  0.1119f}, {0.80f, 0.04f, 0.00f}},  // 9
    {{-0.2129f, -0.2938f}, {1.00f, 0.30f, 0.00f}},  // 10
};

  const std::vector<uint16_t> Indices = {
    0,  1,  6,
    0,  6,  2,
    0,  2,  7,
    0,  7,  3,
    0,  3,  8,
    0,  8,  4,
    0,  4,  9,
    0,  9,  5,
    0,  5,  10,
    0,  10, 1,
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

  VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;

  VkPipelineLayout GraphicsPipelineLayout = VK_NULL_HANDLE;
  VkPipeline GraphicsPipeline = VK_NULL_HANDLE;

  VkCommandPool CommandPool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> CommandBuffers{};

  VkBuffer VertexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory VertexBufferMemory = VK_NULL_HANDLE;

  VkBuffer IndexBuffer = VK_NULL_HANDLE;
  VkDeviceMemory IndexBufferMemory = VK_NULL_HANDLE;

  std::vector<VkBuffer> UniformBuffers{};
  std::vector<VkDeviceMemory> UniformBuffersMemory{};
  std::vector<void *> UniformBuffersMapped{};

  VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> DescriptorSets{};

  std::vector<VkSemaphore> PresentCompleteSemaphores{};
  std::vector<VkSemaphore> RenderCompleteSemaphores{};
  std::vector<VkFence> InFlightFences{};

  FQueueFamilyIndices QueueFamilyIndices{};

  uint32_t FrameIndex = 0;
  bool bFramebufferResized = false;
};
