#pragma once

class Registry;
struct RenderContext;
class RenderQueue;

// EffectSpawnRequestComponent �� autoPlay ��Ԃ����āA
// ���ۂ� runtime instance �𐶐�����V�X�e���B
class EffectSpawnSystem
{
public:
    // effect spawn �v������������B
    static void Update(Registry& registry, float dt);
};

// �Đ��� effect �� currentTime �i�s��S������V�X�e���B
class EffectPlaybackSystem
{
public:
    // playback �� dt �����i�߂�B
    static void Update(Registry& registry, float dt);
};

// �e entity �� socket �֒Ǐ]���� effect �� transform ���X�V����V�X�e���B
class EffectAttachmentSystem
{
public:
    // attachment world transform / world velocity update.
    static void Update(Registry& registry, float dt);
};

// effect �̃V�~�����[�V�����⏕�X�V���s���V�X�e���B
// ����� lifetime fade �Ȃǂ̕⏕�l�X�V��S������B
class EffectSimulationSystem
{
public:
    // �V�~�����[�V�����֘A�̏�Ԃ��X�V����B
    static void Update(Registry& registry, float dt);
};

// ��~�v��������I�������āAeffect �̏I���m����s���V�X�e���B
class EffectLifetimeSystem
{
public:
    // effect �̎�������ƒ�~�m����s���B
    static void Update(Registry& registry, float dt);
};

// �v���r���[��p effect ���X�V����V�X�e���B
// �G�f�B�^�m�F�����̓��ʂ� loop �����Ȃǂ����B
class EffectPreviewSystem
{
public:
    // preview effect �� playback ���X�V����B
    static void Update(Registry& registry, float dt);
};

// �Đ��� effect ����`��p packet �𒊏o���� RenderQueue �֐ςރV�X�e���B
class EffectExtractSystem
{
public:
    // mesh effect / particle effect �̕`�� packet �𒊏o����B
    static void Extract(Registry& registry, RenderContext& rc, RenderQueue& queue);
};