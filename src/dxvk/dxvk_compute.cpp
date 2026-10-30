#include <cstring>
#include <iomanip>
#include <sstream>

#include "../util/util_time.h"

#include "dxvk_compute.h"
#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipemanager.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
          DxvkDevice*                 device,
          DxvkPipelineManager*        pipeMgr,
          DxvkComputePipelineShaders  shaders)
  : m_device        (device),
    m_stats         (&pipeMgr->m_stats),
    m_shaders       (std::move(shaders)),
    m_layout        (pipeMgr, m_shaders.cs->getLayout()),
    m_debugName     (createDebugName()) {

  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    m_pipelines.forEach([this] (const DxvkComputePipelineInstance& instance) {
      this->destroyPipeline(instance.handle);
    });
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) {
      // Slow path for compute shaders that do use spec constants
      DxvkComputePipelineInstance* instance = this->findInstance(state);

      if (unlikely(!instance)) {
        std::lock_guard<dxvk::mutex> lock(m_mutex);
        instance = this->findInstance(state);

        if (!instance)
          instance = this->createInstance(state);
      }

      return instance->handle;

  }


  void DxvkComputePipeline::compilePipeline(
    const DxvkComputePipelineStateInfo& state) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      if (!this->findInstance(state))
        this->createInstance(state);
  }


  DxvkComputePipelineInstance* DxvkComputePipeline::createInstance(
    const DxvkComputePipelineStateInfo& state) {
    VkPipeline newPipelineHandle = this->createPipeline(state);

    m_stats->numComputePipelines += 1;
    return m_pipelines.add(state, newPipelineHandle);
  }

  
  DxvkComputePipelineInstance* DxvkComputePipeline::findInstance(
    const DxvkComputePipelineStateInfo& state) {
    return m_pipelines.find(state);
  }
  
  
  VkPipeline DxvkComputePipeline::createPipeline(
    const DxvkComputePipelineStateInfo& state) const {
    auto vk = m_device->vkd();

    DxvkPipelineSpecConstantState scState(m_shaders.cs->getSpecConstantMask(), state.sc);
    
    DxvkShaderStageInfo stageInfo(m_device);
    stageInfo.addStage(VK_SHADER_STAGE_COMPUTE_BIT, 
      m_shaders.cs->getCode(m_layout.getBindingMap(), DxvkShaderModuleCreateInfo()),
      &scState.scInfo);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage                = *stageInfo.getStageInfos();
    info.layout               = m_layout.getLayout()->getPipelineLayout(false);
    info.basePipelineIndex    = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateComputePipelines(vk->device(),
          VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkComputePipeline: Failed to compile pipeline: ", vr));
      this->logPipelineState(LogLevel::Error, state);
      return VK_NULL_HANDLE;
    }

    return pipeline;
  }


  void DxvkComputePipeline::destroyPipeline(VkPipeline pipeline) {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), pipeline, nullptr);
  }
  
  
  void DxvkComputePipeline::logPipelineState(
          LogLevel                      level,
    const DxvkComputePipelineStateInfo& state) const {
    std::stringstream sstr;
    sstr << "  cs  : " << m_shaders.cs->debugName() << std::endl;

    bool hasSpecConstants = false;

    for (uint32_t i = 0; i < MaxNumSpecConstants; i++) {
      if (state.sc.specConstants[i]) {
        if (!hasSpecConstants) {
          sstr << "Specialization constants:" << std::endl;
          hasSpecConstants = true;
        }

        sstr << "  " << i << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << state.sc.specConstants[i] << std::dec << std::endl;
      }
    }

    Logger::log(level, sstr.str());
  }


  std::string DxvkComputePipeline::createDebugName() const {
    std::string shaderName = m_shaders.cs->debugName();
    size_t len = std::min(shaderName.size(), size_t(10));

    return str::format("[", shaderName.substr(0, len), "]");
  }

}
