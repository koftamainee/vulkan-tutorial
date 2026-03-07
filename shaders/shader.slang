struct VSInput {
    float2 in_position;
    float3 in_color;
    float2 in_tex_coord;
};

struct UniformBuffer {
    float time;
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float3 color;
    float4 sv_position : SV_Position;
    float2 frag_tex_coord;
};

[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    float4 world_pos = mul(ubo.model, float4(input.in_position, 0.0, 1.0));
    output.sv_position = mul(ubo.proj, mul(ubo.view, world_pos));

    float3 color;
    float t =  sqrt(world_pos.x * world_pos.x + world_pos.y * world_pos.y) * 3.0 + ubo.time;

    color.r = 0.5  + 0.5 * sin(t);
    color.g = 0.5 + 0.5 * sin(t + 2.094);
    color.b = 0.5 + 0.5 * sin(t + 4.188);

    output.color = color + 0.5 * input.in_color;
    output.frag_tex_coord = input.in_tex_coord;

    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 frag_main(VSOutput vert_in) : SV_Target {

    return float4(vert_in.color * texture.Sample(vert_in.frag_tex_coord).rgb, 1.0);
}
