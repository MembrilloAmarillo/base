fn_internal VkFormat 
R_FormatToVkFormat(r_format format) {
    switch (format) {
        case R_FORMAT_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case R_FORMAT_VEC2:  return VK_FORMAT_R32G32_SFLOAT;
        case R_FORMAT_VEC3:  return VK_FORMAT_R32G32B32_SFLOAT;
        case R_FORMAT_VEC4:  return VK_FORMAT_R32G32B32A32_SFLOAT;
		case R_FORMAT_UNORM8:return VK_FORMAT_R8_UNORM;
		case R_FORMAT_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
        default:             return VK_FORMAT_UNDEFINED;
    }
}

fn_internal u64
CustomXXHash( const u8* buffer, u64 len, u64 seed ) {
    return XXH3_64bits_withSeed(buffer, len, seed);
}

fn_internal void
R_RenderInit(r_render* Render, vulkan_base* Base, Stack_Allocator* Allocator) {
	Render->VulkanBase     = Base;
	Render->Allocator      = Allocator;
	Render->DescriptorPool = VK_NULL_HANDLE;
	
	stack_init(&Render->PerFrameAllocator, stack_push(Allocator, u8, mebibyte(12)), mebibyte(12));

	HashTableInit(&Render->Textures,   Allocator, 1024, CustomXXHash);
	HashTableInit(&Render->Buffers,    Allocator, 1024, CustomXXHash);
	HashTableInit(&Render->Pipelines,  Allocator, 1024, CustomXXHash);

	Render->CurrentPipeline     = R_HANDLE_INVALID;
	//Render->PendingBindings      = { 0 };
	Render->PendingBindingCount = 0;

	pool_size_ratio sizes[5];
    sizes[0].Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sizes[0].Ratio = 64;
    sizes[1].Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; sizes[1].Ratio = 64;
    sizes[2].Type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[2].Ratio = 64;
    sizes[3].Type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[3].Ratio = 64;
    sizes[4].Type = VK_DESCRIPTOR_TYPE_SAMPLER; sizes[4].Ratio = 64;
    InitDescriptorPool(Render->VulkanBase, &Render->DescriptorPool, Render->VulkanBase->Device, 64 * 5, sizes, ArrayCount(sizes));
}

fn_internal void R_Begin(r_render* Render) {
    vulkan_base* base = Render->VulkanBase;

    bool resized = PrepareFrame(base);
	Render->SetExternalResize = resized;
    if (resized) {
        // @todo How to handle this in a better way? Maybe set a customizable resizing function?
		return;
	}

	Render->SetScreenToClear = false;

    Render->CurrentCommandBuffer = BeginRender(base);
}

fn_internal void R_ClearScreen(r_render* Render, vec4 Color ) {
	Render->ClearColorScreen = (VkClearValue) {
		Color.r, Color.g, Color.b, Color.a
	};
	Render->SetScreenToClear = true;
}

fn_internal void R_BeginRenderPass(r_render* Render) {
	vulkan_base* base = Render->VulkanBase;
    
	VkRenderingAttachmentInfo colorAttachment; 
	VkRenderingInfo renderingInfo;

	if (Render->SetScreenToClear) {
		colorAttachment = AttachmentInfo(
			base->Swapchain.ImageViews[base->SwapchainImageIdx],
			&Render->ClearColorScreen,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);
		Render->SetScreenToClear = false;
	} else {
		colorAttachment = AttachmentInfo(
			base->Swapchain.ImageViews[base->SwapchainImageIdx],
			NULL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		);
	}
     
	renderingInfo = RenderingInfo(base->Swapchain.Extent, &colorAttachment, NULL);
    
	vkCmdBeginRendering(Render->CurrentCommandBuffer, &renderingInfo);

	VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)base->Swapchain.Extent.width;
    viewport.height = (float)base->Swapchain.Extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(Render->CurrentCommandBuffer , 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = base->Swapchain.Extent;
    vkCmdSetScissor(Render->CurrentCommandBuffer , 0, 1, &scissor);
}

fn_internal void
R_RenderPassEnd(r_render* Render) {
	vkCmdEndRendering(Render->CurrentCommandBuffer);
}

fn_internal void
R_RenderEnd(r_render* Render) {
	TransitionImageDefault(
		Render->CurrentCommandBuffer,
		Render->VulkanBase->Swapchain.Images[Render->VulkanBase->SwapchainImageIdx],
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	);

	bool do_resize = EndRender(Render->VulkanBase, Render->CurrentCommandBuffer);
	Render->SetExternalResize = do_resize;
	if (do_resize) {
		
	}
	stack_free_all(&Render->PerFrameAllocator);
}

fn_internal VkDescriptorSetLayout 
R_CreateDescriptorSetLayout(r_render* Render, u32 NSets, VkDescriptorType* Types, VkShaderStageFlagBits StageFlags) {
	vk_descriptor_set builder = {};
  InitDescriptorSet(&builder, NSets, &Render->PerFrameAllocator);
	for (i32 i = 0; i < NSets; i += 1) {
		VkDescriptorType type = Types[i];
		AddBindingDescriptorSet(
			&builder,
			i,
			type
		);
	}

	VkDescriptorSetLayout SetLayout = BuildDescriptorSet(
    &builder,
    Render->VulkanBase->Device,
    StageFlags,
    0,
    0
  );

	return SetLayout;
}

fn_internal R_Handle
R_CreatePipelineFromBuilder(
  r_render* Render, 
	const char* Id,
	const char* VertPath, 
	const char* FragPath, 
  pipeline_builder* p_Build,
	r_vertex_input_description* Description,
  VkDescriptorSetLayout* DescriptorSetLayout,
	u32 LayoutCount
) {

  {
		VkVertexInputBindingDescription binding_description = {};
		memset(&binding_description, 0, sizeof(VkVertexInputBindingDescription));
    binding_description.binding   = 0;
    binding_description.inputRate = Description->Rate;
    binding_description.stride    = Description->Stride;

		VkVertexInputAttributeDescription *AttributeDescriptions = stack_push(&Render->PerFrameAllocator, VkVertexInputAttributeDescription, Description->AttributeCount);

		for (i32 i = 0; i < Description->AttributeCount; i += 1) {
			AttributeDescriptions[i].location = Description->Attributes[i].Location;
			AttributeDescriptions[i].binding = 0;
			AttributeDescriptions[i].format = R_FormatToVkFormat(Description->Attributes[i].Format);
			AttributeDescriptions[i].offset = Description->Attributes[i].Offset;
		}
		SetVertexInputBindingDescription(p_Build, &binding_description, 1);
		SetVertexInputAttributeDescription(p_Build, AttributeDescriptions, Description->AttributeCount);
	}
  if (DescriptorSetLayout != NULL) {
    SetDescriptorLayout(p_Build, DescriptorSetLayout, LayoutCount);
  }
	vk_pipeline NewPipeline = AddPipeline(Render->VulkanBase, p_Build, VertPath, FragPath);
  DestroyPipelineBuilder(p_Build);

	vk_pipeline* Pipeline = stack_push(Render->Allocator, vk_pipeline, 1);
	*Pipeline = NewPipeline;

  if (DescriptorSetLayout != NULL) {
    Pipeline->Descriptors = stack_push(Render->Allocator, VkDescriptorSet, 2);
    Pipeline->DescriptorCount = 2;
    Pipeline->Descriptors[0] = DescriptorSetAllocate(&Render->DescriptorPool, Render->VulkanBase->Device, &DescriptorSetLayout[0]);
    Pipeline->Descriptors[1] = DescriptorSetAllocate(&Render->DescriptorPool, Render->VulkanBase->Device, &DescriptorSetLayout[0]);
  }
	entry* Entry = HashTableAdd(&Render->Pipelines, Id, Pipeline, 0);

	return (R_Handle)Entry->HashId;
}

fn_internal R_Handle 
R_CreatePipelineEx(
	r_render* Render, 
	const char* Id,
	const char* VertPath, 
	const char* FragPath, 
	r_vertex_input_description* Description,
	VkPrimitiveTopology Topology,
	VkDescriptorSetLayout* DescriptorSetLayout,
	u32 LayoutCount
) {
	pipeline_builder p_Build = InitPipelineBuilder(2, &Render->PerFrameAllocator);
    SetInputTopology(&p_Build, Topology);
    SetPolygonMode(&p_Build, VK_POLYGON_MODE_FILL);
    SetCullMode(&p_Build, 0, VK_FRONT_FACE_CLOCKWISE);
    EnableBlendingAlphaBlend(&p_Build);
    DisableDepthTest(&p_Build);
    SetMultisamplingNone(&p_Build);

	{
		VkVertexInputBindingDescription binding_description = {};
		memset(&binding_description, 0, sizeof(VkVertexInputBindingDescription));
        binding_description.binding   = 0;
        binding_description.inputRate = Description->Rate;
        binding_description.stride    = Description->Stride;

		VkVertexInputAttributeDescription *AttributeDescriptions = stack_push(&Render->PerFrameAllocator, VkVertexInputAttributeDescription, Description->AttributeCount);

		for (i32 i = 0; i < Description->AttributeCount; i += 1) {
			AttributeDescriptions[i].location = Description->Attributes[i].Location;
			AttributeDescriptions[i].binding = 0;
			AttributeDescriptions[i].format = R_FormatToVkFormat(Description->Attributes[i].Format);
			AttributeDescriptions[i].offset = Description->Attributes[i].Offset;
		}
		SetVertexInputBindingDescription(&p_Build, &binding_description, 1);
		SetVertexInputAttributeDescription(&p_Build, AttributeDescriptions, Description->AttributeCount);
	}

	SetDescriptorLayout(&p_Build, DescriptorSetLayout, LayoutCount);

	vk_pipeline NewPipeline = AddPipeline(Render->VulkanBase, &p_Build, VertPath, FragPath);
  DestroyPipelineBuilder(&p_Build);

	vk_pipeline* Pipeline = stack_push(Render->Allocator, vk_pipeline, 1);
	*Pipeline = NewPipeline;
	Pipeline->Descriptors = stack_push(Render->Allocator, VkDescriptorSet, 2);
	Pipeline->DescriptorCount = 2;
	Pipeline->Descriptors[0] = DescriptorSetAllocate(&Render->DescriptorPool, Render->VulkanBase->Device, &DescriptorSetLayout[0]);
	Pipeline->Descriptors[1] = DescriptorSetAllocate(&Render->DescriptorPool, Render->VulkanBase->Device, &DescriptorSetLayout[0]);

	entry* Entry = HashTableAdd(&Render->Pipelines, Id, Pipeline, 0);

	return (R_Handle)Entry->HashId;
}

fn_internal R_Handle 
R_CreateComputePipeline(
	r_render* Render,
	const char* Id,
	const char* ComputeShaderPath,
	VkDescriptorSetLayout* DescriptorSetLayout,
	u32 LayoutCount
) {
	VkPipelineLayoutCreateInfo computeLayout = {};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = NULL;
	computeLayout.setLayoutCount = LayoutCount;
	computeLayout.pSetLayouts = DescriptorSetLayout;

	vk_pipeline* Pipeline = stack_push(Render->Allocator, vk_pipeline, 1);

	VK_CHECK(vkCreatePipelineLayout(
		Render->VulkanBase->Device, 
		&computeLayout, 
		NULL, &Pipeline->Layout)
	);

	VkShaderModule computeDrawShader;
    if (!LoadShaderModule(ComputeShaderPath, Render->VulkanBase->Device, &computeDrawShader)) {
        fprintf(stderr, "[ERROR] Could not find shader!\n");
        exit(EXIT_FAILURE);
    }

	VkPipelineShaderStageCreateInfo stageInfo = {};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.pNext = NULL;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = computeDrawShader;
	stageInfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = NULL;
	computePipelineCreateInfo.layout = Pipeline->Layout;
	computePipelineCreateInfo.stage = stageInfo;

	VK_CHECK(
		vkCreateComputePipelines(
			Render->VulkanBase->Device, 
			VK_NULL_HANDLE, 1,
			&computePipelineCreateInfo, NULL,
			&Pipeline->Pipeline
		)
	);

    vkDestroyShaderModule(Render->VulkanBase->Device, computeDrawShader, NULL);

	Pipeline->Descriptors = stack_push(Render->Allocator, VkDescriptorSet, MAX_FRAMES_IN_FLIGHT);
	Pipeline->DescriptorCount = MAX_FRAMES_IN_FLIGHT;
	Pipeline->Descriptors[0] = DescriptorSetAllocate(&Render->DescriptorPool, Render->VulkanBase->Device, &DescriptorSetLayout[0]);
	Pipeline->Descriptors[1] = DescriptorSetAllocate(&Render->DescriptorPool, Render->VulkanBase->Device, &DescriptorSetLayout[0]);

	entry* Entry = HashTableAdd(&Render->Pipelines, Id, Pipeline, 0);

	return (R_Handle)Entry->HashId;
}


fn_internal R_Handle 
R_CreateBuffer(r_render* Render, const char* BufferId, u64 Size, r_buffer_type Type) {
	vulkan_base* base = Render->VulkanBase;
    VkBufferUsageFlags usage = 0;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;

    // Traducir el tipo de buffer abstracto a los flags de Vulkan
    switch (Type) {
        case R_BUFFER_TYPE_VERTEX:
            usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY; // O GPU_ONLY si no se modifica
            break;
        case R_BUFFER_TYPE_INDEX:
            usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        case R_BUFFER_TYPE_UNIFORM:
            usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Mapeado para actualizaciones fáciles
            break;
		case R_BUFFER_TYPE_STAGING: {
			usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Mapeado para actualizaciones fáciles
		} break;
        // Añadir SSBO, etc.
    }

    // Llamar a la función de bajo nivel para crear el buffer real
    allocated_buffer newVkBuffer = CreateBuffer(base->GPUAllocator, Size, usage, memoryUsage);

	allocated_buffer* NewBuffer = stack_push(Render->Allocator, allocated_buffer, 1);
	*NewBuffer = newVkBuffer;

    entry* Entry = HashTableAdd(&Render->Buffers, BufferId, NewBuffer, 0);
	R_Handle Handle = (R_Handle)Entry->HashId;

    return Handle;
}

fn_internal void R_DestroyBuffer(r_render* Render, const char* Id) {
	entry* Entry = HashTableFindPointer(&Render->Buffers, Id, 0);
	if (Entry != NULL) {
		allocated_buffer* Buffer = (allocated_buffer*)Entry->Value;

		vmaDestroyBuffer(Render->VulkanBase->GPUAllocator, Buffer->Buffer, Buffer->Allocation);
	}
}

fn_internal void 
R_BindVertexBuffer(r_render* Render, R_Handle Handle) {
	Render->CurrentVertexBuffer = Handle;
}

fn_internal void 
R_BindIndexBuffer(r_render* Render, R_Handle Handle) {
	Render->CurrentIndexBuffer = Handle;
}

fn_internal void 
R_SetPipeline(r_render* Render, R_Handle Handle) {
	Render->CurrentPipeline = Handle;
}

fn_internal void 
R_UpdateUniformBuffer(r_render* Render, const char* Id, u32 Binding, void* Data, size_t DataSize) {

	entry* UboEntry = HashTableFindPointer(&Render->Buffers, Id, 0);
	assert(UboEntry != NULL && "Invalid Id, Not Found");
	allocated_buffer* Buffer = (allocated_buffer*)UboEntry->Value;
	
    VkMappedMemoryRange flushRange = {};
    flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    flushRange.memory = Buffer->Info.deviceMemory;
    flushRange.offset = 0;
    flushRange.size = DataSize;
    //vkFlushMappedMemoryRanges(Render->VulkanBase->Device, 1, &flushRange);
	memcpy(
        Buffer->Info.pMappedData,
        Data,
        DataSize 
    );

    assert(Render->PendingBindingCount < MAX_PENDING_BINDINGS);
    pending_binding* p = &Render->PendingBindings[Render->PendingBindingCount++];
    p->Handle       = (R_Handle)UboEntry->HashId;
    p->BindingPoint = Binding;
    p->Type         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	p->Range        = DataSize;
}

fn_internal void 
R_BindTexture(r_render* Render, const char* Id, u32 Binding, VkDescriptorType Type) {

	entry* TexEntry = HashTableFindPointer(&Render->Textures, Id, 0);
	assert(TexEntry != NULL && "Invalid Id, Not Found");

	assert(Render->PendingBindingCount < MAX_PENDING_BINDINGS);
    pending_binding* p = &Render->PendingBindings[Render->PendingBindingCount++];
    p->Handle       = (R_Handle)TexEntry->HashId;
    p->BindingPoint = Binding;
    p->Type         = Type;
}

fn_internal R_Handle 
R_PushTexture(r_render* Render, const char* Id, vk_image* Image) {
	entry* Entry = HashTableAdd(&Render->Textures, Id, Image, 0);

	assert(Entry != NULL && "Could set new entry for image on the Hash table");

	return (R_Handle)Entry->HashId;
}

void R_Draw(r_render* Render, u32 VertexCount, u32 InstanceCount) {
    assert(Render->CurrentPipeline != R_HANDLE_INVALID && "R_Draw called without a pipeline being bound.");
    
    vulkan_base* base = Render->VulkanBase;
    VkCommandBuffer cmd = Render->CurrentCommandBuffer;

    vk_pipeline* pipeline = (vk_pipeline*)HashTableGet(&Render->Pipelines, Render->CurrentPipeline, 0);
    assert(pipeline != NULL && "CurrentPipeline handle is invalid or not found in resource manager.");

    if (Render->PendingBindingCount > 0) {
      VkDescriptorSet set_for_this_frame = pipeline->Descriptors[Render->VulkanBase->CurrentFrame];

        descriptor_writer writer = DescriptorWriterInit(Render->PendingBindingCount, &Render->PerFrameAllocator);

        for (u32 i = 0; i < Render->PendingBindingCount; ++i) {
            pending_binding* p = &Render->PendingBindings[i];

            switch (p->Type) {
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				{
					allocated_buffer* buf = (allocated_buffer*)HashTableGet(&Render->Buffers, p->Handle, 0);
					assert(buf != NULL && "Buffer handle for pending binding not found.");
                    
					WriteBuffer(&writer, p->BindingPoint, buf->Buffer, buf->Info.size, 0, p->Type);
				} break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                {
                    allocated_buffer* buf = (allocated_buffer*)HashTableGet(&Render->Buffers, p->Handle, 0);
                    assert(buf != NULL && "Buffer handle for pending binding not found.");
                    
                    WriteBuffer(&writer, p->BindingPoint, buf->Buffer, buf->Info.size, 0, p->Type);
                } break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                {
                    vk_image* tex = (vk_image*)HashTableGet(&Render->Textures, p->Handle, 0);
                    assert(tex != NULL && "Texture handle for pending binding not found.");

                    WriteImage(&writer, p->BindingPoint, tex->ImageView, tex->Sampler, 
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, p->Type);
                } break;
                default:
                    assert(0 && "Unsupported descriptor type in pending bindings!");
                    break;
            }
        }
        
        UpdateDescriptorSet(&writer, base->Device, set_for_this_frame);
        
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                pipeline->Layout, 0, 1, &set_for_this_frame, 0, NULL);
    }
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Pipeline);

    allocated_buffer* vb = (allocated_buffer*)HashTableGet(&Render->Buffers, Render->CurrentVertexBuffer, 0);
    VkBuffer Buffers[] = {vb->Buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, Buffers, offsets);

    vkCmdDraw(cmd, VertexCount, InstanceCount, 0, 0);

    Render->PendingBindingCount = 0;
}

fn_internal void 
R_DrawIndexed(r_render* Render, u32 IndexCount, u32 InstanceCount) {
    assert(Render->CurrentPipeline     != R_HANDLE_INVALID && "R_DrawIndexed called without a pipeline being bound.");
    assert(Render->CurrentVertexBuffer != R_HANDLE_INVALID && "R_DrawIndexed called without a vertex buffer being bound.");
    assert(Render->CurrentIndexBuffer  != R_HANDLE_INVALID && "R_DrawIndexed called without an index buffer being bound.");
    
    vulkan_base*   base = Render->VulkanBase;
    VkCommandBuffer cmd = Render->CurrentCommandBuffer;

    vk_pipeline* pipeline = (vk_pipeline*)HashTableGet(&Render->Pipelines, Render->CurrentPipeline, 0);
    assert(pipeline != NULL && "CurrentPipeline handle is invalid or not found.");
	
	VkDescriptorSet Set = pipeline->Descriptors[Render->VulkanBase->CurrentFrame];

    if (Render->PendingBindingCount > 0) {
        descriptor_writer writer = DescriptorWriterInit(Render->PendingBindingCount, &Render->PerFrameAllocator);

        for (u32 i = 0; i < Render->PendingBindingCount; ++i) {
            pending_binding* p = &Render->PendingBindings[i];
            switch (p->Type) {
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				{
					allocated_buffer* buf = (allocated_buffer*)HashTableGet(&Render->Buffers, p->Handle, 0);
					assert(buf != NULL && "Buffer handle for pending binding not found.");
                    
					WriteBuffer(&writer, p->BindingPoint, buf->Buffer, p->Range, 0, p->Type);
				} break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                {
                    allocated_buffer* buf = (allocated_buffer*)HashTableGet(&Render->Buffers, p->Handle, 0);
                    assert(buf != NULL && "Buffer handle for pending binding not found.");
                    
                    WriteBuffer(&writer, p->BindingPoint, buf->Buffer, buf->Info.size, 0, p->Type);
                } break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                {
                    vk_image* tex = (vk_image*)HashTableGet(&Render->Textures, p->Handle, 0);
                    assert(tex != NULL && "Texture handle for pending binding not found.");

                    WriteImage(&writer, p->BindingPoint, tex->ImageView, tex->Sampler, 
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, p->Type);
                } break;
				default: 
				{
					assert(0 && "Unsupported descriptor type in pending bindings!");
				} break;
            }
        }
        
        UpdateDescriptorSet(&writer, base->Device, Set);
    }
    
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, 0, 1, &Set, 0, NULL);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Pipeline);
    
    allocated_buffer* vb = (allocated_buffer*)HashTableGet(&Render->Buffers, Render->CurrentVertexBuffer, 0);
	VkBuffer Buffers[] = {vb->Buffer};
	VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, Buffers, offsets);
    
    allocated_buffer* ib = (allocated_buffer*)HashTableGet(&Render->Buffers, Render->CurrentIndexBuffer, 0);
    vkCmdBindIndexBuffer(cmd, ib->Buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, IndexCount, InstanceCount, 0, 0, 0);

    Render->PendingBindingCount = 0;
}

fn_internal void 
R_SendDataToBuffer(r_render* Render, R_Handle Buffer, void* Data, u64 Size, u64 offset) {
  allocated_buffer* vb = (allocated_buffer*)HashTableGet(&Render->Buffers, Buffer, 0);

  memcpy(
    (u8*)vb->Info.pMappedData + offset,
    Data,
    Size
  );

	VkMappedMemoryRange flushRange = {};
  flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  flushRange.memory = vb->Info.deviceMemory;
  flushRange.offset = 0;
  flushRange.size = Size;
  //vkFlushMappedMemoryRanges(Render->VulkanBase->Device, 1, &flushRange);
}

fn_internal void
R_CopyStageToBuffer(r_render* Render, R_Handle StageBuffer, R_Handle Buffer, VkBufferCopy Copy) {
	allocated_buffer* sb = (allocated_buffer*)HashTableGet(&Render->Buffers, StageBuffer, 0);
	allocated_buffer* vb = (allocated_buffer*)HashTableGet(&Render->Buffers, Buffer, 0);

	vkCmdCopyBuffer(
		Render->CurrentCommandBuffer,
		sb->Buffer,
		vb->Buffer,
		1,
		&Copy
	);
}

fn_internal void 
R_DispatchCompute(r_render* Render, R_Handle PipelineHandle, u32 GroupCountX, u32 GroupCountY, u32 GroupCountZ) {
    // 1. Obtener el contexto y el pipeline
    assert(PipelineHandle != R_HANDLE_INVALID && "Pipeline de cómputo inválido");
    vulkan_base* base = Render->VulkanBase;
    VkCommandBuffer cmd = Render->CurrentCommandBuffer;
    vk_pipeline* pipeline = (vk_pipeline*)HashTableGet(&Render->Pipelines, PipelineHandle, 0);
    assert(pipeline != NULL && "Pipeline de cómputo no encontrado.");

    // 2. Bindear el pipeline de CÓMPUTO
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->Pipeline);

    // 3. Actualizar y bindear descriptores (¡lógica 100% idéntica a R_DrawIndexed!)
    VkDescriptorSet set_for_this_frame = pipeline->Descriptors[base->CurrentFrame];

    if (Render->PendingBindingCount > 0) {
        descriptor_writer writer = DescriptorWriterInit(Render->PendingBindingCount, &Render->PerFrameAllocator);

        for (u32 i = 0; i < Render->PendingBindingCount; ++i) {
            pending_binding* p = &Render->PendingBindings[i];
            switch (p->Type) {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                {
                    allocated_buffer* buf = (allocated_buffer*)HashTableGet(&Render->Buffers, p->Handle, 0);
                    WriteBuffer(&writer, p->BindingPoint, buf->Buffer, p->Range, 0, p->Type);
                    break;
                }
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: // Importante para cómputo
                {
                    vk_image* tex = (vk_image*)HashTableGet(&Render->Textures, p->Handle, 0);
                    // (Asegúrate de que WriteImage maneje STORAGE_IMAGE y el layout correcto)
                    WriteImage(&writer, p->BindingPoint, tex->ImageView, tex->Sampler, 
                               VK_IMAGE_LAYOUT_GENERAL, p->Type);
                    break;
                }
                default:
                    assert(0 && "Tipo de descriptor no soportado");
                    break;
            }
        }
        UpdateDescriptorSet(&writer, base->Device, set_for_this_frame);
    }
    
    // 4. Bindear el descriptor set al punto de CÓMPUTO
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, 
                            pipeline->Layout, 0, 1, &set_for_this_frame, 0, NULL);

    // 5. ¡Despachar!
    vkCmdDispatch(cmd, GroupCountX, GroupCountY, GroupCountZ);

    // 6. Resetear los bindings
    Render->PendingBindingCount = 0;

	R_AddComputeToGraphicsBarrier(Render);
}

fn_internal void 
R_AddComputeToGraphicsBarrier(r_render* Render) {
    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // Escrituras del Compute
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // Lecturas del Gráfico (UBO, SSBO)
	// O usa VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
	// si el compute escribió en un vertex buffer.

    vkCmdPipelineBarrier(
        Render->CurrentCommandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // Espera a que termine el Compute
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |   // Despierta al Vertex
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // y al Fragment
        0,
        1, &barrier,
        0, NULL,
        0, NULL
    );
}


fn_internal void 
R_SendImageToSwapchain(r_render* Render, R_Handle ImageHandle) {
	vk_image* computeImage = (vk_image*)HashTableGet(&Render->Textures, ImageHandle, 0);
    if (computeImage == NULL) {
        return;
    }

    vulkan_base* base = Render->VulkanBase;
    VkCommandBuffer cmd = Render->CurrentCommandBuffer; // Usa el command buffer del frame actual

    TransitionImageDefault(
        cmd,
        computeImage->Image,
        VK_IMAGE_LAYOUT_GENERAL,              // Estaba en GENERAL (para escritura del shader)
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );

    computeImage->Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; 

    TransitionImageDefault(
        cmd,
        base->Swapchain.Images[base->SwapchainImageIdx],
        VK_IMAGE_LAYOUT_UNDEFINED,              // Su estado después de AcquireNextImage
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    CopyImageToImage(
        cmd,
        computeImage->Image,
        base->Swapchain.Images[base->SwapchainImageIdx],
        (VkExtent2D) { computeImage->Extent.width, computeImage->Extent.height },
        base->Swapchain.Extent
    );

    TransitionImageDefault(
        cmd,
        base->Swapchain.Images[base->SwapchainImageIdx],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    );
}

fn_internal void
R_SendCopyToGpu(r_render* Render) {
	// Add pipeline barrier before vertex input
	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

	vkCmdPipelineBarrier(Render->CurrentCommandBuffer,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
						 0, 1, &barrier, 0, 0, 0, 0);
}