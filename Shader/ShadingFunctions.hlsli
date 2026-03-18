//--------------------------------------------
//	�����o�[�g�g�U���ˌv�Z�֐�
//--------------------------------------------
// N:�@��(���K���ς�)
// L:���˃x�N�g��(���K���ς�)
// C:���ˌ�(�F�E����)
// K:���˗�
float3 CalcLambert(float3 N, float3 L, float3 C, float3 K)
{
    float power = saturate(dot(N, -L));
    return C * power * K;
}


//--------------------------------------------
//	�t�H���̋��ʔ��ˌv�Z�֐�
//--------------------------------------------
// N:�@��(���K���ς�)
// L:���˃x�N�g��(���K���ς�)
// E:�����x�N�g��(���K���ς�)
// C:���ˌ�(�F�E����)
// K:���˗�

float3 CalcPhongSpecular(float3 N, float3 L, float3 E, float3 C, float3 K)
{
    float3 R = reflect(L, N);
    float power = max(dot(-E, R), 0);
    power = pow(power, 128);
    return C * power * K;
}

//--------------------------------------------
//	�n�[�t�����o�[�g�g�U���ˌv�Z�֐�
//--------------------------------------------
// N:�@��(���K���ς�)
// L:���˃x�N�g��(���K���ς�)
// C:���ˌ�(�F�E����)
// K:���˗�
float3 CalcHalfLambert(float3 N, float3 L, float3 C, float3 K)
{
    float D = saturate(dot(N, -L) * 0.5f + 0.5f);
    return C * D * K;
}

//--------------------------------------------
// �������C�g
//--------------------------------------------
// N:�@��(���K���ς�)
// E:���_�����x�N�g��(���K���ς�)
// L:���˃x�N�g��(���K���ς�)
// C :���C�g�F
// RimPower : �������C�g�̋���(�����l�̓e�L�g�[�Ȃ̂Ŏ����Őݒ肷�邪�g)
float3 CalcRimLight(float3 N, float3 E, float3 L, float3 C, float RimPower = 3.0f)
{
    float rim = 1.0f - saturate(dot(N, -E));
    return C * pow(rim, RimPower) * saturate(dot(L, -E));

}

//--------------------------------------------
// �����v�V�F�[�f�B���O
//--------------------------------------------
// tex:�����v�V�F�[�f�B���O�p�e�N�X�`��
// samp:�����v�V�F�[�f�B���O�p�T���v���X�e�[�g
// N:�@��(���K���ς�)
// L:���˃x�N�g��(���K���ς�)
// C:���ˌ�(�F�E����)
// K:���˗�
float3 CalcRampShading(Texture2D tex, SamplerState samp, float3 N, float3 L, float3 C, float3 K)
{
    float D = saturate(dot(N, -L) * 0.5f + 0.5f);
    float Ramp = tex.Sample(samp, float2(D, 0.5f)).r;
    return C * Ramp * K.rgb;
}



//	�~����
static const float PI = 3.141592654f;

//--------------------------------------------
//	�t���l����
//--------------------------------------------
//F0	: �������ˎ��̔��˗�
//VdotH	: �����x�N�g���ƃn�[�t�x�N�g���i�����ւ̃x�N�g���Ǝ��_�ւ̃x�N�g���̒��ԃx�N�g��
float3 CalcFresnel(float3 F0, float VdotH)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - VdotH, 0.0f, 1.0f), 5.0f);
}

//--------------------------------------------
//	�g�U����BRDF(���K�������o�[�g�̊g�U����)
//--------------------------------------------
//VdotH		: �����ւ̃x�N�g���ƃn�[�t�x�N�g���Ƃ̓��
//fresnelF0	: �������ˎ��̃t���l�����ːF
//diffuseReflectance	: ���ˌ��̂����g�U���˂ɂȂ銄��
float3 DiffuseBRDF(float VdotH, float3 fresnelF0, float3 diffuseReflectance)
{
    return (1.0f - CalcFresnel(fresnelF0, VdotH)) * (diffuseReflectance / PI);
}

//--------------------------------------------
//	�@�����z�֐�
//--------------------------------------------
//NdotH		: �@���x�N�g���ƃn�[�t�x�N�g���i�����ւ̃x�N�g���Ǝ��_�ւ̃x�N�g���̒��ԃx�N�g���j�̓��
//roughness : �e��
float CalcNormalDistributionFunction(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float b = (NdotH * NdotH) * (a - 1.0f) + 1.0f;
    return a / (PI * b * b);

}

//--------------------------------------------
//	�􉽌������̎Z�o
//--------------------------------------------
//NdotL		: �@���x�N�g���ƌ����ւ̃x�N�g���Ƃ̓��
//NdotV		: �@���x�N�g���Ǝ����ւ̃x�N�g���Ƃ̓��
//roughness : �e��
float CalcGeometryFunction(float NdotL, float NdotV, float roughness)
{
    float r = roughness * 0.5f;
    float shadowing = NdotL / (NdotL * (1.0 - r) + r);
    float masking = NdotV / (NdotV * (1.0 - r) + r);
    return shadowing * masking;
}

//--------------------------------------------
//	���ʔ���BRDF�i�N�b�N�E�g�����X�̃}�C�N���t�@�Z�b�g���f���j
//--------------------------------------------
//NdotV		: �@���x�N�g���Ǝ����ւ̃x�N�g���Ƃ̓��
//NdotL		: �@���x�N�g���ƌ����ւ̃x�N�g���Ƃ̓��
//NdotH		: �@���x�N�g���ƃn�[�t�x�N�g���Ƃ̓��
//VdotH		: �����ւ̃x�N�g���ƃn�[�t�x�N�g���Ƃ̓��
//fresnelF0	: �������ˎ��̃t���l�����ːF
//roughness	: �e��
float3 SpecularBRDF(float NdotV, float NdotL, float NdotH, float VdotH, float3 fresnelF0, float roughness)
{
	// D���i�@�����z�j
    float D = CalcNormalDistributionFunction(NdotH, roughness);
	// G���i�􉽌������j
    float G = CalcGeometryFunction(NdotL, NdotV, roughness);
	// F���i�t���l�����ˁj
    float3 F = CalcFresnel(fresnelF0, VdotH);

    return D * G * F / (NdotL * NdotV * 4.0f);
}

//--------------------------------------------
//	���ڌ��̕����x�[�X���C�e�B���O
//--------------------------------------------
//diffuseReflectance	: ���ˌ��̂����g�U���˂ɂȂ銄��
//F0					: �������ˎ��̃t���l�����ːF
//normal				: �@���x�N�g��(���K���ς�)
//eyeVector				: ���_�Ɍ������x�N�g��(���K���ς�)
//lightVector			: �����Ɍ������x�N�g��(���K���ς�)
//lightColor			: ���C�g�J���[
//roughness				: �e��
void DirectBDRF(float3 diffuseReflectance,
	float3 F0,
	float3 normal,
	float3 eyeVector,
	float3 lightVector,
	float3 lightColor,
	float roughness,
	out float3 outDiffuse,
	out float3 outSpecular)
{
    float3 N = normal;
    float3 L = -lightVector;
    float3 V = -eyeVector;
    float3 H = normalize(L + V);

    float NdotV = max(0.0001f, dot(N, V));
    float NdotL = max(0.0001f, dot(N, L));
    float NdotH = max(0.0001f, dot(N, H));
    float VdotH = max(0.0001f, dot(V, H));

    float3 irradiance = lightColor * NdotL;

	// �g�U����BRDF
    outDiffuse = DiffuseBRDF(VdotH, F0, diffuseReflectance) * irradiance;

	// ���ʔ���BRDF
    outSpecular = SpecularBRDF(NdotV, NdotL, NdotH, VdotH, F0, roughness) * irradiance;


}
//--------------------------------------------
//	���b�N�A�b�v�e�[�u������GGX����擾
//--------------------------------------------
//brdf_sample_point	: �T���v�����O�|�C���g
//lut_ggx_map       : GGX���b�N�A�b�v�e�[�u��
//state             : �Q�Ǝ��̃T���v���[�X�e�[�g
float4 SampleLutGGX(float2 brdf_sample_point, Texture2D lut_ggx_map, SamplerState state)
{
    return lut_ggx_map.Sample(state, brdf_sample_point);
}

//--------------------------------------------
//	�L���[�u�}�b�v����Ɠx��擾
//--------------------------------------------
//v                     : �T���v�����O����
//diffuse_iem_cube_map  : ���O�v�Z�g�U����IBL�L���[�u�}�b�v
//state                 : �Q�Ǝ��̃T���v���[�X�e�[�g
float4 SampleDiffuseIEM(float3 v, TextureCube diffuse_iem_cube_map, SamplerState state)
{
    return diffuse_iem_cube_map.Sample(state, v);
}

//--------------------------------------------
//	�L���[�u�}�b�v������ˋP�x��擾
//--------------------------------------------
//v	                        : �T���v�����O����
//roughness                 : �e��
//specular_pmrem_cube_map   : ���O�v�Z���ʔ���IBL�L���[�u�}�b�v
//state                     : �Q�Ǝ��̃T���v���[�X�e�[�g
float4 SampleSpecularPMREM(float3 v, float roughness, TextureCube specular_pmrem_cube_map, SamplerState state)
{
	// �~�b�v�}�b�v�ɂ���đe����\�����邽�߁A�i�K��Z�o
    uint width, height, mip_maps;
    specular_pmrem_cube_map.GetDimensions(0, width, height, mip_maps);
    float lod = roughness * float(mip_maps - 1);
    return specular_pmrem_cube_map.SampleLevel(state, v, lod);
}

//--------------------------------------------
//	�e����l�������t���l�����̋ߎ���
//--------------------------------------------
//F0	    : �������ˎ��̔��˗�
//VdotN 	: �����x�N�g���Ɩ@���x�N�g���Ƃ̓��
//roughness	: �e��
float3 CalcFresnelRoughness(float3 f0, float NdotV, float roughness)
{
    return f0 + (max((float3) (1.0f - roughness), f0) - f0) * pow(saturate(1.0f - NdotV), 5.0f);
}

//--------------------------------------------
//	�g�U����IBL
//--------------------------------------------
//normal                : �@��(���K���ς�)
//eye_vector		    : �����x�N�g��(���K���ς�)
//roughness				: �e��
//diffuse_reflectance	: ���ˌ��̂����g�U���˂ɂȂ銄��
//F0					: �������ˎ��̃t���l�����ːF
//diffuse_iem_cube_map  : ���O�v�Z�g�U����IBL�L���[�u�}�b�v
//state                 : �Q�Ǝ��̃T���v���[�X�e�[�g
float3 DiffuseIBL(float3 normal, float3 eye_vector, float roughness, float3 diffuse_reflectance, float3 f0, TextureCube diffuse_iem_cube_map, SamplerState state)
{
    float3 N = normal;
    float3 V = -eye_vector;

	//�@�Ԑڊg�U���ˌ��̔��˗��v�Z
    float NdotV = max(0.0001f, dot(N, V));
    float3 kD = 1.0f - CalcFresnelRoughness(f0, NdotV, roughness);

    float3 irradiance = SampleDiffuseIEM(normal, diffuse_iem_cube_map, state).rgb;
    return diffuse_reflectance * irradiance * kD;
}

//--------------------------------------------
//	���ʔ���IBL
//--------------------------------------------
//normal				    : �@���x�N�g��(���K���ς�)
//eye_vector			    : �����x�N�g��(���K���ς�)
//roughness				    : �e��
//F0					    : �������ˎ��̃t���l�����ːF
//lut_ggx_map               : GGX���b�N�A�b�v�e�[�u��
//specular_pmrem_cube_map   : ���O�v�Z���ʔ���IBL�L���[�u�}�b�v
//state                     : �Q�Ǝ��̃T���v���[�X�e�[�g
float3 SpecularIBL(float3 normal, float3 eye_vector, float roughness, float3 f0, Texture2D lut_ggx_map, TextureCube specular_pmrem_cube_map, SamplerState state)
{
    float3 N = normal;
    float3 V = -eye_vector;

    float NdotV = max(0.0001f, dot(N, V));
    float3 R = normalize(reflect(-V, N));
    float3 specular_light = SampleSpecularPMREM(R, roughness, specular_pmrem_cube_map, state).rgb;

    float2 brdf_sample_point = saturate(float2(NdotV, roughness));
    float2 env_brdf = SampleLutGGX(brdf_sample_point, lut_ggx_map, state).rg;
    return specular_light * (f0 * env_brdf.x + env_brdf.y);

}
