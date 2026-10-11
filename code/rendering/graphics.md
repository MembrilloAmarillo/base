# Graphics API

`graphics.h` is the application-facing C API. It also works from C++.
`graphics.c` contains the Vulkan implementation; applications do not include
Vulkan headers, access command buffers, or manage GPU synchronization.

The cube test in `code/test/vulkan_cube_test.cpp` is a complete example. It uses
SDL for input and the graphics API for all rendering.

## What the backend handles

- Device, surface, swapchain, depth target, and command-buffer creation.
- Resize/minimize/restore, frame fences, acquisition, and presentation.
- GPU memory, mapping, coherent uploads, staging copies, and texture transitions.
- Per-frame copies of dynamic buffers, including automatic capacity growth.
- Shader modules, pipeline layouts, descriptor sets, and resource bindings.
- Depth/blend/rasterization state and the initial viewport/scissor.
- Resource cleanup, including cleanup after initialization or creation fails.

The application still supplies vertex/index data, shader code, constants, drawing
order, and UI clipping rectangles. These are rendering decisions, not backend work.

## Initialize and shut down

```c
Graphics_Device_t *device = NULL;
Graphics_Device_Desc_t desc = graphics_device_desc_default();
desc.title = "My application";
desc.width = 1280;
desc.height = 720;
desc.validation = true; /* Development only; requires installed validation layers. */

Graphics_Result result = graphics_init(&device, &desc);
if (result != Graphics_NONE) {
    fprintf(stderr, "%s\n", graphics_result_string(result));
    return 1;
}

/* Create resources and render... */
graphics_shutdown(&device); /* Waits, releases all resources, sets device to NULL. */
```

The default device description disables validation. Passing `NULL` for the device
description uses defaults. Initialization failure leaves the output handle `NULL`.

`graphics_get_window()` returns a borrowed SDL window for event/input handling.
Do not destroy this window yourself. The API initializes/releases its SDL video
subsystem reference; it does not shut down unrelated SDL subsystems.

## Ownership rules

Initialize every handle to `NULL`. Resources belong to the device that created
them. Descriptors and CPU upload data are consumed/copied during the call; you
can free those arrays afterward.

Creation requires an empty output handle. Explicit destruction takes a pointer to
the handle, waits for outstanding work, and clears it on success. Destroying an
already-NULL handle succeeds.

Resource groups retain references to their buffers/textures/samplers. Pipelines
retain references to their resource groups. Explicit destruction rejects objects
that are still referenced. Destroy in this order:

1. Pipelines.
2. Resource groups.
3. Samplers, textures, and buffers.

Alternatively, call `graphics_shutdown()`: the device performs this ordering for
all remaining resources. Other copies of those handle pointers become invalid;
do not use them after destruction or shutdown. The API is single-threaded.

## Buffers

```c
Gpu_Buffer_t *vertices = NULL;
Gpu_Buffer_Desc_t desc = {
    .size = vertex_count * sizeof(Vertex),
    .usage = GPU_BUFFER_VERTEX,
    .memory = GPU_ONLY,
    .dynamic = false
};
result = graphics_create_buffer(device, &vertices, &desc, vertex_data);
```

Combine usage flags when necessary. Vertex, index, uniform, and storage buffers
are supported. Initial data, when provided, must contain `desc.size` bytes.

| Buffer kind | When to update | Behavior |
| --- | --- | --- |
| Static `CPU_VISIBLE` / `CACHED` | Outside a frame | Waits for outstanding work, then copies into coherent memory. |
| Static `GPU_ONLY` | Outside a frame | Uploads through a temporary staging buffer; finishes before returning. |
| Dynamic, CPU-visible | After successful `start_frame`, before `begin_render` | Updates only the safe current frame copy; grows that copy if needed. |

Dynamic buffers cannot use `GPU_ONLY` memory. The application uses one handle, not
one handle per frame. Initial data fills every copy. Later updates affect only
the current copy, so upload changing data every frame that needs it. Growth keeps
that copy's existing bytes, but newly allocated/unwritten bytes are unspecified.

Static updates must fit the original size. `GPU_ONLY` update offsets must be
four-byte aligned. Resource bindings automatically select the appropriate dynamic
copy, even after it grows. No application memory mapping or flushing is required.

## Textures, samplers, and resource groups

```c
Gpu_Texture_t *atlas = NULL;
Gpu_Sampler_t *sampler = NULL;
Gpu_Resource_t *font_resources = NULL;

Gpu_Texture_Desc_t texture = {width, height, FORMAT_R8_UNORM};
result = graphics_create_texture(device, &atlas, &texture, pixels, width * height);
/* NULL sampler description: linear filtering, clamp to edge. */
result = graphics_create_sampler(device, &sampler, NULL);

Gpu_Resource_Binding_t binding = {0, GPU_RESOURCE_TEXTURE, GPU_STAGE_FRAGMENT};
Gpu_Resource_Desc_t group = {&binding, 1};
result = graphics_create_resources(device, &font_resources, &group);

Gpu_Resource_Write_t write = {
    .binding = 0, .texture = atlas, .sampler = sampler
};
result = graphics_update_resources(device, font_resources, &write, 1);
```

Texture uploads are tightly packed, single-mip 2D images. Supported upload formats
are `R8_UNORM`, `RG8_UNORM`, `RGBA8_UNORM`, `BGRA8_UNORM`, `RGBA8_SRGB`, and
`BGRA8_SRGB`. The byte count must match the dimensions and format exactly.

A texture binding combines a texture and sampler. Uniform/storage bindings use a
buffer, byte offset, and byte size; size zero means the remaining buffer capacity.
Storage buffers are read-only shader inputs; shader storage writes/atomics are not
enabled by this initial backend.
Offsets must meet the device's uniform/storage alignment requirements; zero is
always a valid alignment. Large buffer ranges may exceed device limits.

Binding numbers and stage masks must match the shader. Each declared binding has
one resource, and every binding must be initialized before drawing. Updates happen
outside a frame and wait for outstanding work. This simple API does not expose
descriptor pools, descriptor arrays, bindless heaps, or update-after-bind flags.

## Pipelines

Start with `graphics_pso_desc_default()`, then supply:

- Vertex and fragment SPIR-V arrays and their **word counts**, not byte counts.
- Shader entry names if not `main`.
- Vertex stride and attributes: location, format, and byte offset.
- Optional depth, blend, culling, and depth-bias settings.
- Resource group handles in shader set-number order.
- Optional push-constant byte ranges, aligned to four bytes.

`graphics_create_pso()` copies the description, creates the layout and pipeline,
and releases temporary shader modules. Shader arrays can be freed afterward.
The swapchain/depth formats are selected internally.

```c
Gpu_PSO_Desc_t desc = graphics_pso_desc_default();
/* Set shader code, vertex_stride, attributes, and attribute_count here. */
desc.depth = (Gpu_Depth_State_t){true, true, GPU_COMPARE_LESS};
Gpu_Range_t constants = {0, sizeof(Cube_Constants)};
desc.ranges = &constants;
desc.n_ranges = 1;
result = graphics_create_pso(device, &pipeline, &desc);
```

For a UI pipeline, leave depth disabled and use `desc.blend = graphics_alpha_blend()`.
Its resource array contains the font group at index zero, matching shader set zero.
Pipeline state is applied automatically when bound; no manual depth-state commands
or pipeline layouts are needed.

## Frame and draw sequence

Check every operation's result. Here `check()` stands for your application's
error handling, and `poll_input()` is independent of rendering:

```c
while (poll_input()) {
    result = graphics_start_frame(device);
    if (result == Graphics_FRAME_SKIPPED) {
        /* Optional short sleep; continue polling input while minimized. */
        continue;
    }
    check(result);

    check(graphics_update_buffer(device, ui_vertices, 0, vertex_data, vertex_bytes));
    check(graphics_update_buffer(device, ui_indices, 0, index_data, index_bytes));

    check(graphics_begin_render(device, NULL));
    check(graphics_bind_pso(device, cube_pipeline));
    check(graphics_bind_vertex_buffer(device, cube_vertices, 0));
    check(graphics_push_constant(device, &cube_constants, sizeof(cube_constants)));
    check(graphics_draw(device, 36, 0));

    check(graphics_bind_pso(device, ui_pipeline));
    check(graphics_bind_resources(device, 0, font_resources));
    check(graphics_bind_vertex_buffer(device, ui_vertices, 0));
    check(graphics_bind_index_buffer(device, ui_indices, GPU_INDEX_UINT32, 0));
    for (each_ui_batch) {
        check(graphics_set_scissor(device, batch.clip));
        check(graphics_draw_indexed(device, batch.count, batch.first_index, 0));
    }
    check(graphics_end_render(device));
    check(graphics_end_frame(device));
}
```

There is one swapchain render pass per frame. `begin_render(NULL)` clears to the
default color/depth and sets a full-target viewport and scissor. A pass description
can override clear values. `graphics_get_frame_size()` provides the pixel size of
the acquired target; use it for projection and UI layout.

Clipping rectangles are clamped to the target. A zero-area clip suppresses draws.
Indexed draws support 16- and 32-bit indices. Index contents and vertex offsets
remain the application's responsibility. Binding a pipeline requires rebinding
its resource groups; vertex/index bindings may be reused.

Push constants use the currently bound pipeline's layout. The singular helper
starts at byte zero; `graphics_push_constants()` also accepts an offset. Updates
must fit a declared range and be four-byte aligned.

Skipped frames need no `end_frame`. Resize/presentation changes are handled on the
next frame automatically. A backend resource failure is fatal for that device:
report it and shut down rather than retrying as if it were a skipped frame. Invalid
arguments are rejected before commands are recorded and can be corrected.

## Current scope and tests

The backend requires Vulkan 1.3, dynamic rendering, synchronization2, a combined
graphics/present queue, and an sRGB BGRA swapchain. It needs no buffer-address,
descriptor-heap, mesh-shader, or sampler-anisotropy extensions/features.

The intentionally small API supports triangle-list drawing, one vertex binding,
one color/depth target, up to 16 vertex attributes, 16 bindings per resource group,
8 resource groups per pipeline, and 8 constant ranges (subject to device limits).
Offscreen targets, compute, mesh shaders, instancing, and compressed texture uploads
are not implemented. Do not add placeholders until there is a real use case.

From a Visual Studio Developer Command Prompt, at the repository root:

```bat
build.bat vulkan_cube_test
vulkan_cube_test.exe --frames 120
build.bat graphics_api_test
set VK_LAYER_VALIDATE_SYNC=1
graphics_api_test.exe
```

The C-only API test includes no SDL/Vulkan headers. It checks invalid arguments,
ownership, static/dynamic uploads, growth, texture/resource creation, resource
references, pipeline state, indexed draws, repeated binding, and cleanup.
