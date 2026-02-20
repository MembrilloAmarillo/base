// Minimal vertex + pixel shaders for basic 3D rendering with bindless textures

// Push constants for per-draw data
struct PushConstants {
    uint texture_index;      // Index into bindless texture array
    uint sampler_index;      // Index into bindless sampler array
    uint padding0;
    uint padding1;
};
[[vk::push_constant]] PushConstants pc;

// Bindless resources (set 0 from descriptor indexing)
[[vk::binding(0, 0)]] Texture2D    textures[];     // Unbounded array of textures
[[vk::binding(1, 0)]] SamplerState samplers[];     // Unbounded array of samplers

// Object constants (set 1, register b0)
[[vk::binding(0, 1)]] cbuffer ObjectCB : register(b0)
{
    float4x4 World;         // object -> world
    float4   Albedo;        // rgba color (alpha used as output alpha)
};

// Frame/constants (set 1, register b1)
[[vk::binding(1, 1)]] cbuffer FrameCB : register(b1)
{
    float4x4 ViewProj;      // view * projection
    float3   LightDir;      // direction TO the light (should be normalized)
    float    padding0;
    float3   EyePos;        // world-space camera position
    float    Shininess;     // specular exponent (e.g. 32.0)
};

// Vertex input
struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};
// Vertex -> Pixel data
struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float3 worldPos : TEXCOORD1;
    float2 uv       : TEXCOORD0;
};
// Vertex shader: transform position, compute world normal & world position
[shader("vertex")]
VSOutput VSMain(VSInput vin)
{
    VSOutput out;
    float4 worldPos4 = mul(float4(vin.position, 1.0f), World);
    out.worldPos = worldPos4.xyz;
    // Transform normal: multiply by world (use 3x3 part). For non-uniform scale, use inverse-transpose.
    float3 worldNormal = normalize( (mul(float4(vin.normal, 0.0f), World)).xyz );
    out.normal = worldNormal;
    out.uv = vin.uv;
    out.position = mul(worldPos4, ViewProj);
    return out;
}
// Pixel shader: simple Blinn-Phong, sample albedo from bindless texture array
[shader("fragment")]
float4 PSMain(VSOutput in) : SV_TARGET
{
    // Fetch albedo from bindless texture array using push constant index
    float3 baseColor = Albedo.xyz;
    
    // Use bindless texture indexing if a valid texture index is provided (non-zero or explicit check)
    if (pc.texture_index < 16384) {  // MAX_TEXTURES constant
        baseColor *= textures[pc.texture_index].Sample(samplers[pc.sampler_index], in.uv).rgb;
    }
    
    float3 N = normalize(in.normal);
    float3 L = normalize(LightDir);                 // direction TO the light
    float3 V = normalize(EyePos - in.worldPos);     // view direction (toward eye)
    float3 H = normalize(L + V);
    float  NdotL = saturate(dot(N, L));
    float  NdotH = saturate(dot(N, H));
    float3 ambient = 0.08 * baseColor;
    float3 diffuse = NdotL * baseColor;
    float3 specular = pow(NdotH, Shininess) * float3(1.0, 1.0, 1.0);
    float3 color = ambient + diffuse + specular;
    //color = float3(1.0, 1.0, 1.0);
    return float4(color, Albedo.w);
}