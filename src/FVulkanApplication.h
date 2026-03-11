#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>
#include <array>

#include "EResult.h"

class FVulkanApplication final {
public:
  FVulkanApplication() = default;
  ~FVulkanApplication();

  FVulkanApplication(const FVulkanApplication &) = delete;
  FVulkanApplication &operator=(const FVulkanApplication &) = delete;
  FVulkanApplication(FVulkanApplication &&) noexcept = delete;
  FVulkanApplication &operator=(FVulkanApplication &&) noexcept = delete;

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
    glm::vec3 Position;
    glm::vec3 Color;
    glm::vec2 TexCoord;

    constexpr inline static VkVertexInputBindingDescription GetBindingDescription() {
      return {
        .binding = 0,
        .stride = sizeof(FVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      };
    }

    constexpr inline static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescription() {
      return {{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(FVertex, Position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FVertex, Color)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(FVertex, TexCoord)},
    }};
    }

    bool operator==(const FVertex& other) const {
      return Position == other.Position && Color == other.Color && TexCoord == other.TexCoord;
    }
  };

  friend class std::hash<FVulkanApplication::FVertex>;

  struct FUniformBufferObject {
    alignas(16) glm::mat4 Model;
    alignas(16) glm::mat4 View;
    alignas(16) glm::mat4 Projection;
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
  static VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice);
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

  void CreateDepthResources();
  VkFormat FindSupportedFormat(const std::vector<VkFormat> &Formats, VkImageTiling Tiling, VkFormatFeatureFlags Features) const;
  VkFormat FindDepthFormat() const;
  static bool HasStencilComponent(VkFormat Format);
  void DestroyDepthResources();

  void CreateTextureImage();
  std::pair<VkImage, VkDeviceMemory> CreateImage(uint32_t Width, uint32_t Height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                                                 VkFormat Format, VkImageTiling Tiling, VkImageUsageFlags Usage, VkMemoryPropertyFlags Properties) const;
  void DestroyImage(VkImage Image, VkDeviceMemory Memory) const;
  void TransitionImageLayout(VkImage Image, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t mipLevels) const;
  void CopyBufferToImage(VkBuffer Buffer, VkImage Image, uint32_t Width, uint32_t Height) const;
  void DestroyTextureImage();

  void GenerateMipMaps(VkImage Image, VkFormat ImageFormat, int32_t TextureWidth, int32_t TextureHeight, uint32_t mipLevels) const;

  VkImageView CreateImageView(VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags, uint32_t mipLevels) const;
  void DestroyImageView(VkImageView ImageView) const;

  void CreateTextureImageView();
  void DestroyTextureImageView();

  void CreateTextureSampler();
  void DestroyTextureSampler();

  void CreateColorResources();
  void DestroyColorResources();

  void LoadModel();

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

  VkCommandBuffer BeginSingleTimeCommands() const;
  void EndSingleTimeCommands(VkCommandBuffer &CommandBuffer) const;

  void TransitionImageLayout(
    VkImage Image,
    VkImageLayout OldLayout,
    VkImageLayout NewLayout,
    VkAccessFlags2 SrcAccessMask,
    VkAccessFlags2 DstAccessMask,
    VkPipelineStageFlags2 SrcStageMask,
    VkPipelineStageFlags2 DstStageMask,
    VkImageAspectFlags ImageAspectFlags, uint32_t mipLevels) const;

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

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

  const std::string kModelPath = "../models/viking_room.obj";
  const std::string kTexturePath = "../textures/viking_room.png";

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


  std::vector<FVertex> Vertices{};
  std::vector<uint32_t> Indices{};

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

  uint32_t MipLevels = 1;
  VkImage TextureImage = VK_NULL_HANDLE;
  VkDeviceMemory TextureImageMemory = VK_NULL_HANDLE;
  VkImageView TextureImageView = VK_NULL_HANDLE;

  VkImage DepthImage = VK_NULL_HANDLE;
  VkDeviceMemory DepthImageMemory = VK_NULL_HANDLE;
  VkImageView DepthImageView = VK_NULL_HANDLE;

  VkImage ColorImage = VK_NULL_HANDLE;
  VkDeviceMemory ColorImageMemory = VK_NULL_HANDLE;
  VkImageView ColorImageView = VK_NULL_HANDLE;

  VkSampler TextureSampler = VK_NULL_HANDLE;
  VkSampleCountFlagBits MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  FQueueFamilyIndices QueueFamilyIndices{};

  uint32_t FrameIndex = 0;
  bool bFramebufferResized = false;
};
