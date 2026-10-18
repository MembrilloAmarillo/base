# Biblioteca gráfica

Backend Vulkan 1.3. Las operaciones de creación, grabación, actualización y
destrucción se realizan desde el hilo gráfico. Los objetos propietarios se
inicializan a cero y no se copian para transferir su propiedad.

## Organización

- `graphics.c`: dispositivo, swapchain, pipelines, descriptores y grabación.
- `graphics_memory.c`: buffers, texturas y subasignación de memoria.
- `graphics_upload.c`: staging reutilizable, uploads y sincronización por fence.
- `graphics_lifetime.c`: destrucción y apagado.
- `graphics_internal.h`: contratos internos entre módulos.

## Creación y destrucción

Las funciones de creación de buffers, texturas y descriptores reciben un
descriptor y un objeto de salida vacío. Devuelven `Graphics_Result`; un error
no sobrescribe un objeto que ya contiene un recurso.

```c
Gpu_Texture_t atlas = {0};
Gpu_Texture_Desc_t desc = {
    .width = 512, .height = 512,
    .format = FORMAT_R8_UNORM,
    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
};
Graphics_Result result = graphics_create_texture(device, &desc, &atlas);
```

La API de texturas soporta imágenes 2D de un mip y una capa: R8_UNORM,
RG8_UNORM, RGBA8/BGRA8_UNORM y RGBA8/BGRA8_SRGB. Rechaza otros formatos.
El tamaño de un upload debe coincidir exactamente con los píxeles empaquetados.
Para fuentes, R8_UNORM evita la conversión a RGBA; el shader lee `.r`.

`graphics_create_resources` es la única entrada de creación de descriptor sets.
Recibe un array de `Gpu_Binding_Desc_t` con tipo, etapas y cantidad por binding.
Los bindings se numeran en orden. `graphics_update_resources` comprueba el
binding, tipo, rango y uso del recurso, y accede únicamente al array pertinente
(`textures` o `buffers`). La actualización raw deja esas comprobaciones al usuario.

La textura **no posee** `texture.sampler`. Varios atlas pueden compartir uno;
se destruye explícitamente con `graphics_destroy_sampler`.

Después del último uso GPU, llamar a `graphics_destroy_buffer`,
`graphics_destroy_texture`, `graphics_destroy_resources`,
`graphics_destroy_pipeline` y `graphics_destroy_sampler`. Estas funciones no
introducen esperas globales. `graphics_shutdown` libera los pools y bloques
compartidos y destruye el dispositivo y la ventana; rechaza allocations o sets
que todavía estén vivos. Los batches también deben destruirse antes del apagado.

No llamar directamente a `vkFreeMemory`/`vkUnmapMemory` sobre buffers o texturas
de la biblioteca, ni destruir `resource.pool`: ahora son almacenamiento compartido.
`graphics_alloc` se conserva como conveniencia; `graphics_create_buffer` permite
solicitar solamente los flags de uso necesarios.

## Uploads agrupados

Secuencia para un batch reutilizable:

1. `graphics_create_upload_batch`: reservar staging mapeado y command pool/fence.
2. `graphics_begin_uploads`.
3. Una o varias llamadas a `graphics_upload_texture`/`graphics_upload_buffer`.
4. `graphics_submit_uploads`: un solo submit asíncrono en la cola gráfica.
5. `graphics_wait_uploads` antes de reutilizar staging o destruir los destinos.
6. Repetir desde el paso 2 o llamar a `graphics_destroy_upload_batch`.

Comprobar cada resultado. Si se excede la capacidad, el upload rechazado no
modifica el batch: puede enviarse lo ya grabado o descartarse mediante
`graphics_cancel_uploads`. Cancelar no cambia los layouts publicados.
Las texturas referenciadas deben conservar su dirección hasta el submit.
Grabar y enviar un batch antes de grabar otro que toque los mismos recursos.
Las transiciones externas deben mantener `texture.layout` coherente.

Los draws enviados posteriormente en la misma cola ven los uploads gracias a
las barreras grabadas; no necesitan una espera CPU. El fence protege la
reutilización del staging. El demo sube atlas y vértices estáticos en un batch.

`graphics_texture_data` sigue disponible como conveniencia bloqueante para una
textura. Crea un batch temporal; usar un batch persistente para cargas repetidas.
`graphics_copy` graba en el command buffer proporcionado o, si se pasa
`VK_NULL_HANDLE`, envía y espera mediante un fence. El mismo offset se aplica a
ambos buffers. Las escrituras CPU en memoria mapeada requieren que haya acabado
su uso previo por GPU.

## Pools y memoria

- Hasta 32 bloques de memoria y 512 subasignaciones activas por dispositivo.
- Bloques normales de 16 MiB, separados por tipo de memoria y por buffer/imagen
  óptima, respetando alignment y `bufferImageGranularity`.
- Se reutilizan huecos liberados. Los bloques normales vacíos se retienen hasta
  shutdown; recursos grandes o que requieran memoria dedicada se asignan aparte.
- Hasta 16 pools de descriptores, 64 sets por pool. Cada pool reserva inicialmente
  128 descriptores de cada tipo soportado, o más si el set inicial lo requiere.
- Los pools se amplían al agotarse; los sets se liberan individualmente.
- Estos límites están declarados en `graphics.h`; agotarlos devuelve un error.

El allocator es first-fit con tablas acotadas. Es sencillo y no usa heap de CPU,
pero debe medirse antes de utilizarlo para creación/destrucción masiva por frame.
Los límites efectivos de layouts y descriptores también dependen de la GPU.
El pipeline cache es compartido por dispositivo y reside en memoria.

## Estado de comandos

El primer bind de cada grabación inicializa depth/stencil: tests desactivados,
depth write activo, comparación LESS, stencil ALWAYS/KEEP. Los binds posteriores
preservan el estado dinámico. Los setters y binds redundantes no emiten comandos.

`graphics_start_frame` invalida la caché del command buffer que va a reutilizar.
Tras resets o cambios raw de pipeline/depth/stencil, llamar a
`graphics_invalidate_state` antes del siguiente bind de la biblioteca. Ese bind
restablece los defaults; aplicar después los ajustes deseados. Los binds raw de
descriptor sets no requieren invalidación.

## Verificación y medición

Desde la raíz, con el entorno del compilador configurado:

```bat
build.bat graphics_resource_test
graphics_resource_test.exe
build.bat vulkan_cube_test
vulkan_cube_test.exe --frames 120
vulkan_cube_test.exe --frames 120 --resize-test
vulkan_cube_test.exe --frames 120 --bind-repeat 1000
```

En Linux están disponibles los mismos targets mediante `./build.sh`.
La prueba headless activa la validación de sincronización y comprueba límites,
copias mediante readback GPU, reutilización y agotamiento de memoria, uploads
agrupados/cancelados, formatos y rollover de pools. El demo verifica pipelines,
presentación y texto; `--resize-test` recrea el swapchain dos veces.

`device.stats` expone asignaciones de memoria gestionada, creación de pools,
submits/bytes de staging, binds y comandos de estado. No cuenta la memoria del
swapchain ni su profundidad, que siguen siendo recursos dedicados.

Medición Windows, Intel UHD, build debug y validación activa, 120 frames con
1.000 binds idénticos por frame:

| | Antes de cachear estado | Después |
|---|---:|---:|
| Binds Vulkan emitidos | 120.000 | 120 |
| Comandos depth/stencil | 960.240 | 960 |
| CPU acumulada en el bucle de bind | 1.650 ms | 9,1 ms |

Es una prueba de redundancia de comandos, no una estimación del aumento de FPS.
El demo normal realiza un draw por frame. El atlas ocupa 262.144 bytes de datos
en vez de 1.048.576; junto con los vértices, el submit utiliza 275.008 bytes.
La reserva de memoria física sigue la granularidad de los bloques del allocator.
