struct VSInput {
    float2 in_position;
    float3 in_color;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.sv_position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.in_position, 0.0, 1.0))));
    output.color = input.in_color;
    return output;
}

[shader("fragment")]
float4 frag_main(VSOutput in_vert) : SV_Target {
    float3 color = in_vert.color;
    return float4(color, 1.0);
}
