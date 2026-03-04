#ifndef VULKAN_FVULKANTRIANGLE_H
#define VULKAN_FVULKANTRIANGLE_H
#define GLFW_INCLUDE_VULKAN

#include <memory>
#include <expected>
#include <string>
#include <vector>
#include <array>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "EResult.h"

class FVulkanTriangle final {
public:
  static std::expected<std::unique_ptr<FVulkanTriangle>, EResult> New(
    std::string ApplicationName, int WindowWidth,
    int WindowHeight);

  ~FVulkanTriangle();
  FVulkanTriangle(const FVulkanTriangle &) = delete;
  FVulkanTriangle &operator=(const FVulkanTriangle &) = delete;
  FVulkanTriangle(FVulkanTriangle &&) noexcept = delete;
  FVulkanTriangle &operator=(FVulkanTriangle &&) noexcept = delete;

  EResult Run();


private:
  struct FQueueFamilyIndices {
    uint32_t GraphicsFamily = UINT32_MAX;
    uint32_t PresentFamily = UINT32_MAX;

    inline bool isComplete() const {
      return GraphicsFamily != UINT32_MAX && PresentFamily != UINT32_MAX;
    }
  };


private:
  FVulkanTriangle(std::string &&ApplicationName, int WindowWidth,
            int WindowHeight);


  EResult InitWindow();
  void DeinitWindow();

  EResult InitVulkan();
  void DeinitVulkan();

  EResult CreateVKInstance();
  static std::expected<std::vector<const char *>, EResult> GetRequiredInstanceExtensions();
  void DestroyVKInstance();

  EResult CreateDebugMessenger();
  static EResult ValidateLayers();
  static VkDebugUtilsMessengerCreateInfoEXT MakeDebugUtilsMessengerCreateInfo();
  void DestroyDebugMessenger();

  EResult CreateSurface();
  void DestroySurface();

  EResult CreatePhysicalDevice();
  static std::expected<VkPhysicalDevice, EResult> PickPhysicalDevice(const std::vector<VkPhysicalDevice> &Devices,
                                                                     VkSurfaceKHR Surface);
  void DestroyPhysicalDevice();

  EResult CreateLogicalDevice();
  static FQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);
  void DestroyLogicalDevice();

  static VkSurfaceFormatKHR PickSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &Formats);
  static VkPresentModeKHR PickSwapChainPresentMode(const std::vector<VkPresentModeKHR> &PresentModes);
  VkExtent2D PickSwapChainExtent(const VkSurfaceCapabilitiesKHR &Capabilities, GLFWwindow *Window);

  EResult CreateSwapChain();
  void DestroySwapChain();

  EResult CreateImageViews();
  void DestroyImageViews();

  EResult CreateGraphicsPipeline();
  void DestroyGraphicsPipeline();

  std::expected<VkShaderModule, EResult> CreateShaderModule(const std::vector<char> &ShaderCode) const;
  void DestroyShaderModule(VkShaderModule ShaderModule) const;

  EResult CreateCommandPool();
  void DestroyCommandPool();

  EResult CreateCommandBuffer();
  void DestroyCommandBuffer();

  EResult RecordCommandBuffer(uint32_t ImageIndex) const;

  void TransitionImageLayout(
    uint32_t ImageIndex,
    VkImageLayout OldLayout,
    VkImageLayout NewLayout,
    VkAccessFlags2 SrcAccessMask,
    VkAccessFlags2 DstAccessMask,
    VkPipelineStageFlags2 SrcStageMask,
    VkPipelineStageFlags2 DstStageMask) const;

  EResult DrawFrame();

  EResult CreateSyncObjects();
  void DestroySyncObjects();

  EResult MainLoop();
  void Cleanup();

private:
  std::string mApplicationName{};
  int mWindowWidth{};
  int mWindowHeight{};
  GLFWwindow *mWindow{};
  VkInstance mInstance{};
  VkSurfaceKHR mSurface{};
  VkDebugUtilsMessengerEXT mDebugMessenger{};
  VkPhysicalDevice mPhysicalDevice{};
  VkDevice mDevice{};
  VkQueue mGraphicsQueue{};
  VkQueue mPresentQueue{};
  FQueueFamilyIndices mQueueFamilyIndices{};
  VkSwapchainKHR mSwapChain{};
  VkSurfaceFormatKHR mSwapChainSurfaceFormat{};
  VkExtent2D mSwapChainExtent{};
  std::vector<VkImage> mSwapChainImages{};
  std::vector<VkImageView> mSwapChainImageViews{};
  VkPipelineLayout mPipelineLayout{};
  VkPipeline mGraphicsPipeline{};
  VkCommandPool mCommandPool{};
  VkCommandBuffer mCommandBuffer{};
  VkSemaphore mPresentCompleteSemaphore{};
  std::vector<VkSemaphore> mRenderCompleteSemaphores{};
  VkFence mDrawFence{};


private:
  static constexpr std::array<const char *, 1> mValidationLayers{
    "VK_LAYER_KHRONOS_validation"
  };

  static constexpr std::array<const char *, 2> mDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME};

#ifdef NDEBUG
  static constexpr bool mEnableValidationLayers = false;
#else
  static constexpr bool mEnableValidationLayers = true;
#endif
};

#endif //VULKAN_FVULKANTRIANGLE_H
