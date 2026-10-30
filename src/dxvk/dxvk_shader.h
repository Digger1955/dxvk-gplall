#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_shader_key.h"

#include "../spirv/spirv_code_buffer.h"
#include "../spirv/spirv_compression.h"
#include "../spirv/spirv_module.h"

namespace dxvk {
  
  class DxvkShader;
  class DxvkShaderModule;
  class DxvkPipelineManager;
  struct DxvkPipelineStats;
  
  /**
   * \brief Shader flags
   *
   * Provides extra information about the features
   * used by a shader.
   */
  enum DxvkShaderFlag : uint64_t {
    HasSampleRateShading,
    HasTransformFeedback,
    ExportsPosition,
    ExportsStencilRef,
    ExportsViewportIndexLayerFromVertexStage,
    ExportsSampleMask,
    UsesFragmentCoverage,
    UsesSparseResidency,
    TessellationPoints,
  };

  using DxvkShaderFlags = Flags<DxvkShaderFlag>;
  
  /**
   * \brief Shader info
   */
  struct DxvkShaderCreateInfo {
    /// Shader stage
    VkShaderStageFlagBits stage;
    /// Descriptor info
    uint32_t bindingCount = 0;
    const DxvkBindingInfo* bindings = nullptr;
    /// Input and output register mask
    uint32_t inputMask = 0;
    uint32_t outputMask = 0;
    /// Flat shading input mask
    uint32_t flatShadingInputs = 0;
    /// Push constant range
    uint32_t pushConstSize = 0;
    /// Rasterized stream, or -1
    int32_t xfbRasterizedStream = 0;
    /// Tess control patch vertex count
    uint32_t patchVertexCount = 0;
    /// Transform feedback vertex strides
    uint32_t xfbStrides[MaxNumXfbBuffers] = { };
    /// Input primitive topology for geometry shaders
    VkPrimitiveTopology inputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    /// Output primitive topology
    VkPrimitiveTopology outputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
  };


  /**
   * \brief Shader module create info
   */
  struct DxvkShaderModuleCreateInfo {
    bool      fsDualSrcBlend  = false;
    bool      fsFlatShading   = false;
    uint32_t  undefinedInputs = 0;
    VkPrimitiveTopology inputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;

    std::array<VkComponentMapping, MaxNumRenderTargets> rtSwizzles = { };

    bool eq(const DxvkShaderModuleCreateInfo& other) const;

    size_t hash() const;
  };
  
  
  /**
   * \brief Shader object
   * 
   * Stores a SPIR-V shader and information on the
   * bindings that the shader uses. In order to use
   * the shader with a pipeline, a shader module
   * needs to be created from he shader object.
   */
  class DxvkShader : public RcObject {
    
  public:
    
    DxvkShader(
      const DxvkShaderCreateInfo&   info,
            SpirvCodeBuffer&&       spirv);

    ~DxvkShader();
    
    /**
     * \brief Shader info
     * \returns Shader info
     */
    const DxvkShaderCreateInfo& info() const {
      return m_info;
    }

    /**
     * \brief Retrieves shader flags
     * \returns Shader flags
     */
    DxvkShaderFlags flags() const {
      return m_flags;
    }

    /**
     * \brief Queries shader binding layout
     * \returns Pipeline layout builder
     */
    DxvkPipelineLayoutBuilder getLayout() const {
      return m_layout;
    }

    /**
     * \brief Retrieves spec constant mask
     * \returns Bit mask of used spec constants
     */
    uint32_t getSpecConstantMask() const {
      return m_specConstantMask;
    }

    /**
     * \brief Gets raw code without modification
     */
    SpirvCodeBuffer getRawCode() const {
      return m_code.decompress();
    }

// Library Parts ?

    /**
     * \brief Patches code using given info
     *
     * Rewrites binding IDs and potentially fixes up other
     * parts of the code depending on pipeline state.
     * \param [in] bindings Biding map
     * \param [in] state Pipeline state info
     * \returns Uncompressed SPIR-V code buffer
     */

    SpirvCodeBuffer getCode(
      const DxvkShaderBindingMap*       bindings,
      const DxvkShaderModuleCreateInfo& state) const;

    /**
     * \brief Dumps SPIR-V shader
     * 
     * Can be used to store the SPIR-V code in a file.
     * \param [in] outputStream Stream to write to 
     */
    void dump(std::ostream& outputStream) const;

    /**
     * \brief Sets the shader key
     * \param [in] key Unique key
     */
    void setShaderKey(const DxvkShaderKey& key) {
      m_key = key;
      m_hash = key.hash();
    }

    /**
     * \brief Retrieves shader key
     * \returns The unique shader key
     */
    DxvkShaderKey getShaderKey() const {
      return m_key;
    }

    /**
     * \brief Get lookup hash
     * 
     * Retrieves a non-unique hash value derived from the
     * shader key which can be used to perform lookups.
     * This is better than relying on the pointer value.
     * \returns Hash value for map lookups
     */
    size_t getHash() const {
      return m_hash;
    }
    
    /**
     * \brief Retrieves debug name
     * \returns The shader's name
     */
    std::string debugName() const {
      return m_key.toString();
    }

    /**
     * \brief Get lookup hash for a shader
     *
     * Convenience method that returns \c 0 for a null
     * pointer, and the shader's lookup hash otherwise.
     * \param [in] shader The shader
     * \returns The shader's lookup hash, or 0
     */
    static size_t getHash(const Rc<DxvkShader>& shader) {
      return shader != nullptr ? shader->getHash() : 0;
    }
    
  private:

    struct BindingOffsets {
      uint32_t bindingIndex = 0u;
      uint32_t bindingOffset = 0u;
      uint32_t setIndex = 0u;
      uint32_t setOffset = 0u;
    };

    DxvkShaderCreateInfo          m_info;
    SpirvCompressedBuffer         m_code;
    
    DxvkShaderFlags               m_flags;
    DxvkShaderKey                 m_key;
    size_t                        m_hash = 0;

    size_t                        m_o1IdxOffset = 0;
    size_t                        m_o1LocOffset = 0;

    uint32_t                      m_specConstantMask = 0;

    std::vector<BindingOffsets>   m_bindingOffsets;

    DxvkPipelineLayoutBuilder     m_layout;

    static void eliminateInput(
            SpirvCodeBuffer&          code,
            uint32_t                  location);

    static void emitOutputSwizzles(
            SpirvCodeBuffer&          code,
            uint32_t                  outputMask,
            const VkComponentMapping* swizzles);

    static void emitFlatShadingDeclarations(
            SpirvCodeBuffer&          code,
            uint32_t                  inputMask);

    static void patchInputTopology(
            SpirvCodeBuffer&          code,
            VkPrimitiveTopology       topology);

  };
  

  /**
   * \brief Shader module object
   * 
   * Manages a Vulkan shader module. This will not
   * perform any shader compilation. Instead, the
   * context will create pipeline objects on the
   * fly when executing draw calls.
   */
  class DxvkShaderStageInfo {
    
  public:

    DxvkShaderStageInfo(const DxvkDevice* device);

    DxvkShaderStageInfo             (DxvkShaderStageInfo&& other) = delete;
    DxvkShaderStageInfo& operator = (DxvkShaderStageInfo&& other) = delete;

    ~DxvkShaderStageInfo();

    /**
     * \brief Counts shader stages
     * \returns Shader stage count
     */
    uint32_t getStageCount() const {
      return m_stageCount;
    }

    /**
     * \brief Queries shader stage infos
     * \returns Pointer to shader stage infos
     */
    const VkPipelineShaderStageCreateInfo* getStageInfos() const {
      return m_stageInfos.data();
    }

    /**
     * \brief Adds a shader stage with specialization info
     *
     * \param [in] stage Shader stage
     * \param [in] code SPIR-V code
     * \param [in] specinfo Specialization info
     */
    void addStage(
            VkShaderStageFlagBits   stage,
            SpirvCodeBuffer&&       code,
      const VkSpecializationInfo*   specInfo);

    /**
     * \brief Adds stage using a module identifier
     *
     * \param [in] stage Shader stage
     * \param [in] identifier Shader module identifier
     * \param [in] specinfo Specialization info
     */
    void addStage(
            VkShaderStageFlagBits   stage,
      const VkShaderModuleIdentifierEXT& identifier,
      const VkSpecializationInfo*   specInfo);

  private:

    const DxvkDevice* m_device;

    struct ShaderModuleIdentifier {
      VkPipelineShaderStageModuleIdentifierCreateInfoEXT createInfo;
      std::array<uint8_t, VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT> data;
    };

    union ShaderModuleInfo {
      ShaderModuleIdentifier    moduleIdentifier;
      VkShaderModuleCreateInfo  moduleInfo;
    };

    std::array<SpirvCodeBuffer,                 5>  m_codeBuffers;
    std::array<ShaderModuleInfo,                5>  m_moduleInfos = { };
    std::array<VkPipelineShaderStageCreateInfo, 5>  m_stageInfos  = { };
    uint32_t                                        m_stageCount  = 0;

  };

// Library Parts ?

  /**
   * \brief Shader set
   *
   * Stores a set of shader pointers
   * for use in a pipeline library.
   */


  struct DxvkShaderSet {
    DxvkShader* vs = nullptr;
    DxvkShader* tcs = nullptr;
    DxvkShader* tes = nullptr;
    DxvkShader* gs = nullptr;
    DxvkShader* fs = nullptr;
    DxvkShader* cs = nullptr;
  };


// Library Parts ?

  /**
   * \brief Shader identifer set
   *
   * Stores a set of shader module identifiers
   * for use in a pipeline library.
   */


  struct DxvkShaderIdentifierSet {
    VkShaderModuleIdentifierEXT vs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT tcs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT tes = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT gs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT fs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT cs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
  };


}
