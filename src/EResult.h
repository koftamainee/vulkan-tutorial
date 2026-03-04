#ifndef VULKAN_ERESULT_H
#define VULKAN_ERESULT_H

#include <string>

enum class EResult {
  Success,

  InvalidWindowDimensions,
  GLFWInitFailed,
  WindowCreationFailed,
  FailedToGetGLFWExtensions,
  FailedToGetVkExtensions,
  RequiredGLFWExtensionIsNotSupported,
  FailedToGetVkLayers,
  VkLayerNotSupported,
  FailedToCreateVkInstance,
  FailedToCreateVkDebugMessenger,
  FailedToCreateSurface,
  FailedToEnumeratePhysicalDevices,
  NoVulkanGPUFound,
  FailedToCreateVkDevice,
  FailedToGetSurfaceCapabilities,
  FailedToGetSurfaceFormats,
  FailedToGetSurfacePresentModes,
  FailedToCreateSwapChain,
  FailedToGetSwapChainImages,
  FailedToCreateImageView,
  FailedToReadShadersFromFile,
  FailedToCreateShaderModule,
  FailedToCreatePipelineLayout,
  FailedToCreateGraphicsPipeline,
  FailedToCreateCommandPool,
  FailedToAllocateCommandBuffers,
  FailedToBeginCommandBuffer,
  FailedToEndCommandBuffer,
  FailedToCreateSemaphore,
  FailedToCreateFence,
  FailedToWaitForFence,
  FailedToAcquireNextImage,
  FailedToResetFence,
  FailedToSubmitToQueue,
  FailedToPresentQueue,
  FailedToWaitIdleDevice,
};

constexpr std::string_view to_string(EResult Result) {
  switch (Result) {
  // clang-format off
  case EResult::Success:                             return "Success";
  case EResult::InvalidWindowDimensions:             return "Invalid window dimensions";
  case EResult::GLFWInitFailed:                      return "GLFW initialization failed";
  case EResult::WindowCreationFailed:                return "Window creation failed";
  case EResult::FailedToGetGLFWExtensions:           return "Failed to get GLFW extensions";
  case EResult::FailedToGetVkExtensions:             return "Failed to get Vulkan extensions";
  case EResult::FailedToGetVkLayers:                 return "Failed to get Vulkan layers";
  case EResult::VkLayerNotSupported:                 return "VkLayer not supported";
  case EResult::RequiredGLFWExtensionIsNotSupported: return "Required GLFW extension is not supported";
  case EResult::FailedToCreateVkInstance:            return "Failed to create Vulkan instance";
  case EResult::FailedToCreateSurface:               return "Failed to create Vulkan surface";
  case EResult::FailedToCreateVkDebugMessenger:      return "Failed to create Vulkan debug messenger";
  case EResult::FailedToEnumeratePhysicalDevices:    return "Failed to enumerate physical devices";
  case EResult::NoVulkanGPUFound:                    return "No Vulkan GPU found";
  case EResult::FailedToCreateVkDevice:              return "Failed to create Vulkan device";
  case EResult::FailedToGetSurfaceFormats:           return "Failed to get Vulkan surface formats";
  case EResult::FailedToGetSurfacePresentModes:      return "Failed to get Vulkan surface present modes";
  case EResult::FailedToCreateSwapChain:             return "Failed to create Vulkan swap chain";
  case EResult::FailedToGetSwapChainImages:          return "Failed to get Vulkan swap chain images";
  case EResult::FailedToGetSurfaceCapabilities:      return "Failed to get Vulkan surface capabilities";
  case EResult::FailedToCreateImageView:             return "Failed to create Vulkan image views";
  case EResult::FailedToReadShadersFromFile:         return "Failed to read shaders from file";
  case EResult::FailedToCreateShaderModule:          return "Failed to create shader module";
  case EResult::FailedToCreatePipelineLayout:        return "Failed to create pipeline layout";
  case EResult::FailedToCreateGraphicsPipeline:      return "Failed to create graphics pipeline";
  case EResult::FailedToCreateCommandPool:           return "Failed to create command pool";
  case EResult::FailedToAllocateCommandBuffers:      return "Failed to allocate command buffers";
  case EResult::FailedToBeginCommandBuffer:          return "Failed to begin command buffer";
  case EResult::FailedToEndCommandBuffer:            return "Failed to end command buffer";
  case EResult::FailedToCreateSemaphore:             return "Failed to create semaphore";
  case EResult::FailedToCreateFence:                 return "Failed to create fence";
  case EResult::FailedToWaitForFence:                return "Failed to wait for fence";
  case EResult::FailedToAcquireNextImage:            return "Failed to acquire next image";
  case EResult::FailedToResetFence:                  return "Failed to reset fence";
  case EResult::FailedToSubmitToQueue:               return "Failed to submit to queue";
  case EResult::FailedToPresentQueue:                return "Failed to present queue";
  case EResult::FailedToWaitIdleDevice:              return "Failed to wait idle device";
  default:                                           return "Unknown error";
    //clang-format on
  }
}

#endif //VULKAN_ERESULT_H
