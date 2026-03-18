#include "compute_particle.hlsli"
StructuredBuffer<particle_header> particle_header_buffer : register(t1);

//  拡縮行列生成
float4x4 matrix_scaling(float3 scale)
{
    float4x4 m;
    m._11 = scale.x;
    m._12 = 0.0f;
    m._13 = 0.0f;
    m._14 = 0.0f;

    m._21 = 0.0f;
    m._22 = scale.y;
    m._23 = 0.0f;
    m._24 = 0.0f;

    m._31 = 0.0f;
    m._32 = 0.0f;
    m._33 = scale.z;
    m._34 = 0.0f;

    m._41 = 0.0f;
    m._42 = 0.0f;
    m._43 = 0.0f;
    m._44 = 1.0f;
    return m;
}

//  回転行列生成
float4x4 matrix_rotation_roll_pitch_yaw(float3 rotation)
{
    float cp, sp;
    float cy, sy;
    float cr, sr;
    sincos(rotation.x, sp, cp);
    sincos(rotation.y, sy, cy);
    sincos(rotation.z, sr, cr);

    float4x4 m;
    m._11 = cr * cy + sr * sp * sy;
    m._12 = sr * cp;
    m._13 = sr * sp * cy - cr * sy;
    m._14 = 0.0f;

    m._21 = cr * sp * sy - sr * cy;
    m._22 = cr * cp;
    m._23 = sr * sy + cr * sp * cy;
    m._24 = 0.0f;

    m._31 = cp * sy;
    m._32 = -sp;
    m._33 = cp * cy;
    m._34 = 0.0f;

    m._41 = 0.0f;
    m._42 = 0.0f;
    m._43 = 0.0f;
    m._44 = 1.0f;
    return m;
}

//  移動行列生成
float4x4 matrix_translation(float3 translation)
{
    float4x4 m;
    m._11 = 1.0f;
    m._12 = 0.0f;
    m._13 = 0.0f;
    m._14 = 0.0f;

    m._21 = 0.0f;
    m._22 = 1.0f;
    m._23 = 0.0f;
    m._24 = 0.0f;

    m._31 = 0.0f;
    m._32 = 0.0f;
    m._33 = 1.0f;
    m._34 = 0.0f;

    m._41 = translation.x;
    m._42 = translation.y;
    m._43 = translation.z;
    m._44 = 1.0f;
    return m;
}

StructuredBuffer<particle_data> particle_data_buffer : register(t0); //  パーティクル管理バッファ

[maxvertexcount(4)]
void main(point GS_IN gin[1], inout TriangleStream<PS_IN> output)
{
    // 元の取得
    uint vertex_id = gin[0].vertex_id;

    // ヘッダーから生存/インデックス
    bool is_alive = (particle_header_buffer[vertex_id].alive != 0);
    vertex_id = particle_header_buffer[vertex_id].particle_index;

    // 非生存ならスケールゼロ（破棄）
    float3 scale = is_alive ? particle_data_buffer[vertex_id].scale.xyz : float3(0, 0, 0);

    // ビルボード基底（ビュー行列の逆を想定、移動成分は殺す）
    float4x4 billboard_matrix = inverseviewProjection;
    billboard_matrix._41_42_43 = float3(0, 0, 0);
    billboard_matrix._44 = 1.0f;

    // === 速度ストレッチ ===
    float3 velocity_world = particle_data_buffer[vertex_id].velocity.xyz;
    float speed = length(velocity_world);

    // 伸ばし係数（既定は1）
    float stretch_factor = 1.0f;
    float additional_roll = 0.0f;

    if (enable_velocity_stretch != 0 && is_alive)
    {
        // カメラ平面上の速度方向を求めてロールを加える（ビルボード内でY軸を速度方向へ）
        // row_major前提：行ベクトルが基底
        float3 camera_right = normalize(float3(billboard_matrix._11, billboard_matrix._12, billboard_matrix._13));
        float3 camera_up = normalize(float3(billboard_matrix._21, billboard_matrix._22, billboard_matrix._23));

        float vx = dot(velocity_world, camera_right);
        float vy = dot(velocity_world, camera_up);
        additional_roll = (abs(vx) + abs(vy) > 1e-6f) ? atan2(vx, vy) : 0.0f;

        // 速度→縦スケール
        float k = max(0.0f, speed - velocity_stretch_min_speed);
        stretch_factor = min(velocity_stretch_max_aspect, 1.0f + k * velocity_stretch_scale);
        scale.y *= stretch_factor; // 縦方向だけ伸ばす
    }

    // 行列（回転：元のZロールに追加ロールを加算）
    float3 euler = particle_data_buffer[vertex_id].rotation.xyz;
    float4x4 scale_matrix = matrix_scaling(scale);
    float4x4 rotation_matrix = mul(billboard_matrix,
                                      matrix_rotation_roll_pitch_yaw(float3(euler.x, euler.y, euler.z + additional_roll)));
    float4x4 translation_matrix = matrix_translation(particle_data_buffer[vertex_id].position.xyz);
    float4x4 world_matrix = mul(mul(scale_matrix, rotation_matrix), translation_matrix);
    float4x4 world_view_projection_matrix = mul(world_matrix, viewProjection);

    // 既存のテクスチャ座標・色
    float4 texcoord = particle_data_buffer[vertex_id].texcoord;
    float4 color = particle_data_buffer[vertex_id].color;

    // 板ポリ4頂点
    static const float4 vertex_positions[4] =
    {
        float4(-0.5f, -0.5f, 0, 1),
        float4(+0.5f, -0.5f, 0, 1),
        float4(-0.5f, +0.5f, 0, 1),
        float4(+0.5f, +0.5f, 0, 1),
    };
    static const float2 vertex_texcoord[4] =
    {
        float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1),
    };

    // 出力
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        PS_IN o;
        o.position = mul(vertex_positions[i], world_view_projection_matrix);
        o.texcoord = texcoord.xy + texcoord.zw * vertex_texcoord[i];
        o.color = color;
        output.Append(o);
    }
    output.RestartStrip();
}