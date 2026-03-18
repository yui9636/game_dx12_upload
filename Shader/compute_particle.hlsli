
//  パーティクルスレッド数
static const int NumParticleThread = 1024;

//  indirect_data_bufferへのアクセス用バイトオフセット
static const uint IndirectArgumentsNumCurrentParticle = 0;
static const uint IndirectArgumentsNumPreviousParticle = 4;
static const uint IndirectArgumentsNumDeadParticle = 8;
static const uint IndirectArgumentsEmitParticleDispatchIndirect = 12;

static const uint IndirectArgumentsUpdateParticleDispatchIndirect = 24;
static const uint IndirectArgumentsNumEmitParticleIndex = 36;
static const uint IndirectArgumentsDrawIndirect = 40;

static const int MaxGradientKeys = 4;




struct gradient_color
{
    float4 color;
    float time;
    float3 pad;
};



//  生成パーティクル構造体
struct emit_particle_data
{
    float4 parameter; // x : パーティクル処理タイプ, y : 生存時間, zw : 空き

    float4 position; // 生成座標
    float4 rotation; // 拡縮情報
    float4 scale; // 回転情報

    float4 velocity; // 初速
    float4 acceleration; // 加速度
    
    //float4 color; // 色情報
    
    gradient_color gradientColors[MaxGradientKeys];
    int gradientCount;
    float3 pad;
    
    float4 scale_begin; // ★追加
    float4 scale_end; // ★追加
    
    float4 angular_velocity;
    float2 fade;
    
};

//  パーティクル構造体
struct particle_data
{
    float4 parameter; // x : パーティクル処理タイプ, y : 生存時間, w : 空き

    float4 position; // 生成座標
    float4 rotation; // 回転情報
    float4 scale; // 拡縮情報

    float4 scale_begin; // ★追加
    float4 scale_end; // ★追加
    
    float4 velocity; // 初速
    float4 acceleration; // 加速度

    float4 texcoord; //  UV座標
    
    gradient_color gradientColors[MaxGradientKeys];
    int gradientCount;
    float3 pad;
    float4 color; // 色情報
    

    float4 angular_velocity;
    
    
    
    float2 fade;

};


//	パーティクルヘッダー構造体
struct particle_header
{
    uint alive; // 生存フラグ
    uint particle_index; // パーティクル番号
    float depth; // 深度
    uint dummy;
};



//	DrawInstanced用DrawIndirect用構造体
struct draw_indirect
{
    uint vertex_count_per_instance;
    uint instance_count;
    uint start_vertex_location;
    uint start_instance_location;
};



//=========================================================================================
//  汎用情報
cbuffer COMPUTE_PARTICLE_COMMON_CONSTANT_BUFFER : register(b10)
{
    float elapsed_time;
    uint2 texture_split_count;
    uint system_num_particles;
    uint total_emit_count; // 生成予定のパーティクル数
    uint common_dummy[3];

    
};

//	バイトニックソート情報
cbuffer COMPUTE_PARTICLE_BITONIC_SORT_CONSTANT_BUFFER : register(b11)
{
    uint increment;
    uint direction;
    uint sort_dummy[2];
};

static const uint BitonicSortB2Thread = 256;
static const uint BitonicSortC2Thread = 512;




#ifndef _COMPUTE_PARTICLE_DISABLE_CBSCENE_

cbuffer CbScene : register(b0)
{
    row_major float4x4 viewProjection;
    row_major float4x4 inverseviewProjection;
    float4 lightDirection;
    float4 lightColor;
    float4 cameraPosition;
    row_major float4x4 lightViewProjection;
};

#endif //_COMPUTE_PARTICLE_DISABLE_CBSCENE_


cbuffer COMPUTE_PARTICLE_RENDER_CONSTANT_BUFFER : register(b2)
{
    uint enable_velocity_stretch; // 0=off, 1=on
    float velocity_stretch_scale; // 速度→縦倍率への係数（例 0.05）
    float velocity_stretch_max_aspect; // 縦横比の上限（例 8.0）
    float velocity_stretch_min_speed; // これ以下の速度はストレッチしない（例 0.0）
    
    float global_alpha;
    
    float curl_noise_strength; // 強さ (0なら無効)
    float curl_noise_scale; // ノイズの粗さ (座標スケール)
    float curl_move_speed; // ノイズのスクロール速度
};





//  頂点シェーダーからジオメトリシェーダーに転送する情報
struct GS_IN
{
    uint vertex_id : VERTEX_ID;
};

//  ジオメトリシェーダーからピクセルシェーダーに転送する情報
struct PS_IN
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};
