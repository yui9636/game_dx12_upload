@echo off
setlocal EnableDelayedExpansion

set DXC="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"
set SRC=..\Shader\EffectMeshVariantPS.hlsl
set OUT=..\Data\Shader

if not exist %OUT% mkdir %OUT%

echo [CompileMeshVariants] Starting...

:: ----------------------------------------------------------------
:: フラグ定義 (ShaderFlags ビット対応)
:: ----------------------------------------------------------------
:: Texture=1, Dissolve=2, Distort=4, Lighting=8, Mask=16
:: Fresnel=32, Flipbook=64, GradientMap=128, ChromaticAberration=256
:: DissolveGlow=512, MatCap=1024, NormalMap=2048, FlowMap=4096
:: SideFade=8192, AlphaFade=16384, SubTexture=32768, Toon=65536
:: RimLight=131072, VertexColorBlend=262144, Emission=524288, Scroll=1048576

:: ----------------------------------------------------------------
:: 剣戟セット
:: ----------------------------------------------------------------
call :compile "Slash_Basic"    "USE_TEXTURE USE_DISSOLVE USE_ALPHA_FADE"                                               0x00004003
call :compile "Slash_Glow"     "USE_TEXTURE USE_DISSOLVE USE_DISSOLVE_GLOW USE_ALPHA_FADE"                             0x00006003
call :compile "Slash_Flow"     "USE_TEXTURE USE_FLOW_MAP USE_FRESNEL USE_ALPHA_FADE"                                   0x00014021
call :compile "Slash_Full"     "USE_TEXTURE USE_DISSOLVE USE_DISSOLVE_GLOW USE_FLOW_MAP USE_FRESNEL USE_SIDE_FADE USE_ALPHA_FADE" 0x00036023

:: ----------------------------------------------------------------
:: 魔法セット
:: ----------------------------------------------------------------
call :compile "Magic_Circle"   "USE_TEXTURE USE_FLOW_MAP USE_MASK USE_ALPHA_FADE"                                      0x00014011
call :compile "Magic_Summon"   "USE_TEXTURE USE_DISSOLVE USE_NORMAL_MAP USE_LIGHTING"                                  0x0000080B
call :compile "Magic_Aura"     "USE_TEXTURE USE_FRESNEL USE_ALPHA_FADE USE_RIM_LIGHT"                                  0x00024021
call :compile "Magic_Explosion" "USE_TEXTURE USE_CHROMATIC_ABERRATION USE_DISTORT USE_ALPHA_FADE"                      0x00004105

:: ----------------------------------------------------------------
:: 汎用セット
:: ----------------------------------------------------------------
call :compile "Universal_Glow" "USE_TEXTURE USE_FRESNEL USE_EMISSION USE_ALPHA_FADE"                                   0x00084021
call :compile "Universal_Flow" "USE_TEXTURE USE_FLOW_MAP USE_SUB_TEXTURE USE_ALPHA_FADE"                              0x00018001

echo.
echo [CompileMeshVariants] Done.
pause
exit /b 0

:: ----------------------------------------------------------------
:compile
:: %1 = 名前ラベル  %2 = defineリスト  %3 = variantKey(hex)
:: ----------------------------------------------------------------
set LABEL=%~1
set DEFINES=%~2
set KEY=%~3

set DEF_ARGS=
for %%D in (%DEFINES%) do (
    set DEF_ARGS=!DEF_ARGS! -D %%D=1
)

set OUTFILE=%OUT%\EffectMeshVariantPS_%KEY%.cso

echo   Compiling %LABEL% [%KEY%] ...
%DXC% -T ps_6_0 -E main %DEF_ARGS% -Fo "%OUTFILE%" "%SRC%"
if errorlevel 1 (
    echo   [ERROR] Failed: %LABEL%
) else (
    echo   [OK]    %OUTFILE%
)
exit /b 0
