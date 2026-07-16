#pragma once

#include "dxvk_include.h"

#include "../util/util_version.h"

namespace dxvk {

  /**
   * \brief Device info
   * 
   * Stores core properties and a bunch of extension-specific
   * properties, if the respective extensions are available.
   * Structures for unsupported extensions will be undefined,
   * so before using them, check whether they are supported.
   */
  struct DxvkDeviceInfo {
    Version                                                   driverVersion;
    VkPhysicalDeviceProperties2                               core;
    VkPhysicalDeviceVulkan11Properties                        vk11;
    VkPhysicalDeviceVulkan12Properties                        vk12;
    VkPhysicalDeviceVulkan13Properties                        vk13;
    VkPhysicalDeviceConservativeRasterizationPropertiesEXT    extConservativeRasterization;
    VkPhysicalDeviceCustomBorderColorPropertiesEXT            extCustomBorderColor;
    VkPhysicalDeviceExtendedDynamicState3PropertiesEXT        extExtendedDynamicState3;
    VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT      extGraphicsPipelineLibrary;
    VkPhysicalDeviceLineRasterizationPropertiesEXT            extLineRasterization;
    VkPhysicalDeviceMultiDrawPropertiesEXT                    extMultiDraw;
    VkPhysicalDeviceSampleLocationsPropertiesEXT              extSampleLocations;
    VkPhysicalDeviceRobustness2PropertiesEXT                  extRobustness2;
    VkPhysicalDeviceTransformFeedbackPropertiesEXT            extTransformFeedback;
    VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT       extVertexAttributeDivisor;
    VkPhysicalDeviceMaintenance5PropertiesKHR                 khrMaintenance5;
    VkPhysicalDeviceMaintenance7PropertiesKHR                 khrMaintenance7;
  };


  /**
   * \brief Device features
   * 
   * Stores core features and extension-specific features.
   * If the respective extensions are not available, the
   * extended features will be marked as unsupported.
   */
  struct DxvkDeviceFeatures {
    VkPhysicalDeviceFeatures2                                 core;
    VkPhysicalDeviceVulkan11Features                          vk11;
    VkPhysicalDeviceVulkan12Features                          vk12;
    VkPhysicalDeviceVulkan13Features                          vk13;
    VkBool32                                                  amdShaderFragmentMask;
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT   extAttachmentFeedbackLoopLayout;
    VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT   extDynamicRenderingUnusedAttachments;
    VkPhysicalDeviceBorderColorSwizzleFeaturesEXT             extBorderColorSwizzle;
    VkBool32                                                  extConservativeRasterization;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT              extCustomBorderColor;
    VkPhysicalDeviceDepthClipEnableFeaturesEXT                extDepthClipEnable;
    VkPhysicalDeviceDepthBiasControlFeaturesEXT               extDepthBiasControl;
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT          extExtendedDynamicState3;
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT        extFragmentShaderInterlock;
    VkBool32                                                  extFullScreenExclusive;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT        extGraphicsPipelineLibrary;
    VkBool32                                                  extHdrMetadata;
    VkPhysicalDeviceLineRasterizationFeaturesEXT              extLineRasterization;
    VkBool32                                                  extMemoryBudget;
    VkPhysicalDeviceMemoryPriorityFeaturesEXT                 extMemoryPriority;
    VkPhysicalDeviceMultiDrawFeaturesEXT                      extMultiDraw;
    VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT             extNonSeamlessCubeMap;
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT      extPageableDeviceLocalMemory;
    VkPhysicalDeviceRobustness2FeaturesEXT                    extRobustness2;
    VkBool32                                                  extSampleLocations;
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT         extShaderModuleIdentifier;
    VkBool32                                                  extShaderStencilExport;
    VkBool32                                                  extSwapchainColorSpace;
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR          khrSwapchainMaintenance1;
    VkPhysicalDeviceTransformFeedbackFeaturesEXT              extTransformFeedback;
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT         extVertexAttributeDivisor;
    VkBool32                                                  extCalibratedTimestamps;
    VkBool32                                                  khrExternalMemoryWin32;
    VkBool32                                                  khrExternalSemaphoreWin32;
    VkBool32                                                  extLoadStoreOpNone;
    VkPhysicalDeviceMaintenance5FeaturesKHR                   khrMaintenance5;
    VkPhysicalDeviceMaintenance7FeaturesKHR                   khrMaintenance7;
    VkPhysicalDevicePresentIdFeaturesKHR                      khrPresentId;
    VkPhysicalDevicePresentWaitFeaturesKHR                    khrPresentWait;
    VkPhysicalDevicePresentId2FeaturesKHR                     khrPresentId2;
    VkPhysicalDevicePresentWait2FeaturesKHR                   khrPresentWait2;
    VkBool32                                                  khrSwapchainMutableFormat;
    VkPhysicalDeviceDescriptorPoolOverallocationFeaturesNV    nvDescriptorPoolOverallocation;
    VkBool32                                                  nvLowLatency2;
    VkPhysicalDeviceRawAccessChainsFeaturesNV                 nvRawAccessChains;
    VkBool32                                                  nvxBinaryImport;
    VkBool32                                                  nvxImageViewHandle;
    VkBool32                                                  khrWin32KeyedMutex;
  };

}
