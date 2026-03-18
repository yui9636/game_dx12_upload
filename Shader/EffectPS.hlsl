//#include "Effect.hlsl"

//// ---------------------------------------------------------
//// 汎用テクスチャスロット (t0 ~ t9)
//// ---------------------------------------------------------
//Texture2D TexSlot0 : register(t0);
//Texture2D TexSlot1 : register(t1);
//Texture2D TexSlot2 : register(t2);
//Texture2D TexSlot3 : register(t3);
//Texture2D TexSlot4 : register(t4);
//Texture2D TexSlot5 : register(t5);
//Texture2D TexSlot6 : register(t6);
//Texture2D TexSlot7 : register(t7);
//Texture2D TexSlot8 : register(t8);
//Texture2D TexSlot9 : register(t9);



//SamplerState LinearWrap : register(s0);
//SamplerState LinearClamp : register(s1);

//float4 SampleSlot(int index, float2 uv)
//{
//    // ★追加: One-Shot (Clamp) モードの処理
//    if (uvScrollMode == 1)
//    {
//        // UVが 0.0～1.0 の範囲外なら描画しない（透明にする）
//        // ※ 0.001 のマージンは、浮動小数点の誤差で端がチラつくのを防ぐため
//        if (uv.x < 0.001f || uv.x > 0.999f || uv.y < 0.001f || uv.y > 0.999f)
//        {
//            return float4(0, 0, 0, 0);
//        }

//        // Clampサンプラーを使ってサンプリング
//        [branch]
//        switch (index)
//        {
//            case 0:
//                return TexSlot0.Sample(LinearClamp, uv);
//            case 1:
//                return TexSlot1.Sample(LinearClamp, uv);
//            case 2:
//                return TexSlot2.Sample(LinearClamp, uv);
//            case 3:
//                return TexSlot3.Sample(LinearClamp, uv);
//            case 4:
//                return TexSlot4.Sample(LinearClamp, uv);
//            case 5:
//                return TexSlot5.Sample(LinearClamp, uv);
//            case 6:
//                return TexSlot6.Sample(LinearClamp, uv);
//            case 7:
//                return TexSlot7.Sample(LinearClamp, uv);
//            case 8:
//                return TexSlot8.Sample(LinearClamp, uv);
//            case 9:
//                return TexSlot9.Sample(LinearClamp, uv);
//        }
//    }
//    else
//    {
//        // ★通常 (Loop) モード - 既存の LinearWrap を使用
//        [branch]
//        switch (index)
//        {
//            case 0:
//                return TexSlot0.Sample(LinearWrap, uv);
//            case 1:
//                return TexSlot1.Sample(LinearWrap, uv);
//            case 2:
//                return TexSlot2.Sample(LinearWrap, uv);
//            case 3:
//                return TexSlot3.Sample(LinearWrap, uv);
//            case 4:
//                return TexSlot4.Sample(LinearWrap, uv);
//            case 5:
//                return TexSlot5.Sample(LinearWrap, uv);
//            case 6:
//                return TexSlot6.Sample(LinearWrap, uv);
//            case 7:
//                return TexSlot7.Sample(LinearWrap, uv);
//            case 8:
//                return TexSlot8.Sample(LinearWrap, uv);
//            case 9:
//                return TexSlot9.Sample(LinearWrap, uv);
//        }
//    }

//    return float4(0, 0, 0, 0);
//}


//// ---------------------------------------------------------
//// Main
//// ---------------------------------------------------------
//float4 main(VS_OUT pin) : SV_TARGET
//{
//    float2 uv = pin.texcoord;
    
//    // --- 1. Distortion (歪み) ---
//    // インデックスが 0以上 の場合のみ計算する (C++側でフラグ制御も併用するとなお良し)
//#ifdef USE_DISTORT
//  if (distortionTexIndex >= 0)
//    {
//        float2 distUV = uv + (distortionUvScrollSpeed * currentTime);
//        float4 noise = SampleSlot(distortionTexIndex, distUV);
        
//        // RとGを別々に使い、0.5を引くことで「上下左右」に散らす
//        // もし白黒画像(R=G)だったとしても、片方の符号を反転させるだけで
//        // 歪みのバリエーションが劇的に増えます
//        float2 offset;
//        offset.x = (noise.r - 0.5f);
//        offset.y = (noise.g - 0.5f);
        
//        // 白黒画像対策：RとGがほぼ同じなら、Y方向を反転させて「対角線移動」を防ぐ
//        if(abs(noise.r - noise.g) < 0.01f) {
//            offset.y = -offset.x; 
//        }

//        uv += offset * distortionStrength;
//    }
//#endif

//    // --- 2. Main Color ---
//    // 歪んだ後のUVにスクロールを足す
//    uv += mainUvScrollSpeed * currentTime;
//    float2 finalUV = uv;
    
    
//    float4 color = baseColor * pin.color;
    
  
  
//#ifdef USE_FLIPBOOK
//    // --- フリップブック計算 ---
//    if (flipbookWidth > 1.0f || flipbookHeight > 1.0f)
//    {
//        // 1. 総コマ数
//        float totalCells = flipbookWidth * flipbookHeight;
        
//        // 2. 現在のフレーム番号 (時間を速度で進める)
//        // floorで整数化し、総コマ数で剰余(%)をとってループさせる
//        float currentFrame = floor(currentTime * flipbookSpeed);
//        currentFrame = fmod(currentFrame, totalCells);
        
//        // 3. 現在の行と列 (Row, Col) を計算
//        // Col = Frame % Width
//        // Row = Frame / Width
//        float col = fmod(currentFrame, flipbookWidth);
//        float row = floor(currentFrame / flipbookWidth);
        
//        // 4. UVを1コマ分のサイズに縮小
//        finalUV.x /= flipbookWidth;
//        finalUV.y /= flipbookHeight;
        
//        // 5. オフセットを加算して該当コマへ移動
//        finalUV.x += col / flipbookWidth;
//        finalUV.y += row / flipbookHeight;
        
//        // ※フリップブック時は通常のスクロール(mainUvScrollSpeed)は行わないのが一般的
//    }
//    else
//    {
//        // フリップブック無効時のみスクロール
//        finalUV += mainUvScrollSpeed * currentTime;
//    }
//#else
//    // バリアント無効時は単純スクロール
//    finalUV += mainUvScrollSpeed * currentTime;
//#endif  

    
//#ifdef USE_TEXTURE
//    if (mainTexIndex >= 0)
//    {
//        // 最終的なテクスチャ色を格納する変数
//        float4 finalTexColor = float4(1, 1, 1, 1);

//        // -----------------------------------------------------------
//        // 1. フローマップによるUV計算 (Flow Map UV Calculation)
//        // -----------------------------------------------------------
//        // ブレンド用の変数を初期化（フローマップOFFならそのまま）
//        float2 uv0 = finalUV;
//        float2 uv1 = finalUV;
//        float flowLerp = 0.0f;
//        bool useFlow = false;

//#ifdef USE_FLOW_MAP
//        if (flowTexIndex >= 0)
//        {
//            useFlow = true;

//            // フローマップからベクトルを取得 (0~1 -> -1~1)
//            float4 flowSample = SampleSlot(flowTexIndex, pin.texcoord);
//            float2 flowVector = flowSample.rg * 2.0f - 1.0f;
//            flowVector *= flowStrength;

//            // タイムサイクルの計算 (位相をずらした2つの波)
//            float timeScale = currentTime * flowSpeed;
//            float phase0 = frac(timeScale);
//            float phase1 = frac(timeScale + 0.5f);

//            // 2つのUVを計算
//            uv0 = finalUV + flowVector * phase0;
//            uv1 = finalUV + flowVector * phase1;

//            // ブレンド係数 (Triangle Wave: 0->1->0)
//            flowLerp = abs((0.5f - phase0) * 2.0f);
//        }
//#endif

//        // -----------------------------------------------------------
//        // 2. サンプリング処理 (Sampling with Chromatic Aberration)
//        // -----------------------------------------------------------
//        // 共通のサンプリング処理を行うためのループ（フローマップOFFなら1回、ONなら2回）
//        // ※HLSLの仕様上、関数化が面倒なためループで処理します
        
//        float4 sampledColors[2];
//        sampledColors[0] = float4(1, 1, 1, 1);
//        sampledColors[1] = float4(1, 1, 1, 1);

//        int iterations = useFlow ? 2 : 1;
        
//        [unroll] // ループ展開ヒント
//        for (int i = 0; i < iterations; ++i)
//        {
//            float2 currentUV = (i == 0) ? uv0 : uv1;
//            float4 colResult = float4(1, 1, 1, 1);

//#ifdef USE_CHROMATIC_ABERRATION
//            // --- 色収差ありのサンプリング ---
//            float2 shift = float2(chromaticAberrationStrength, 0.0f);

//            // SampleSlotClampかWrapかは、元のコードの設定に従います
//            float4 colR = SampleSlot(mainTexIndex, currentUV - shift);
//            float4 colG = SampleSlot(mainTexIndex, currentUV);
//            float4 colB = SampleSlot(mainTexIndex, currentUV + shift);
    

//            // RGBを合成
//            colResult.r = colR.r;
//            colResult.g = colG.g;
//            colResult.b = colB.b;
//            colResult.a = colG.a; // アルファはGを使用
//#else
//            // --- 通常のサンプリング ---
//            colResult = SampleSlot(mainTexIndex, currentUV);
//#endif
//            sampledColors[i] = colResult;
//        }

//        // -----------------------------------------------------------
//        // 3. 最終合成 (Final Composition)
//        // -----------------------------------------------------------
//        if (useFlow)
//        {
//            // 2つの結果をブレンド
//            finalTexColor = lerp(sampledColors[0], sampledColors[1], flowLerp);
//        }
//        else
//        {
//            // フローマップなし
//            finalTexColor = sampledColors[0];
//        }

//        // ベースカラーに乗算
//        color *= finalTexColor;
//    }
//#endif 
    
    
// #ifdef USE_SUB_TEXTURE
//    if (subTexIndex >= 0)
//    {
//        float2 subUV = pin.texcoord;

//        // ---------------------------------------------------------
//        // ★ 1. 極座標 (Polar) か 直交 (Cartesian) かの分岐
//        // ---------------------------------------------------------
//        if (usePolarCoords > 0.5f)
//        {
//            // [ Polar Mode ]
//            // 中心から外へ向かう「円形」のUVに変換
//            float2 delta = subUV - 0.5f;
//            float radius = length(delta) * 2.0f;     // 中心0 -> 外1
//            float angle = atan2(delta.y, delta.x);   // -PI ~ PI

//            subUV.x = angle / 6.2831853f + 0.5f; // 角度をU座標へ
//            subUV.y = radius;                    // 距離をV座標へ
            
//            // ※ 極座標モードでは「UVスクロールのX」がそのまま「回転」になります
//        }
//        else
//        {
//            // [ Cartesian Mode ]
//            // 通常モードの時だけ、回転行列による回転を行う
//            if (abs(subUvRotationSpeed) > 0.0001f)
//            {
//                float2 center = float2(0.5f, 0.5f);
//                float2 uvCentered = subUV - center;
                
//                float angle = subUvRotationSpeed * currentTime;
//                float c = cos(angle);
//                float s = sin(angle);
                
//                subUV.x = uvCentered.x * c - uvCentered.y * s;
//                subUV.y = uvCentered.x * s + uvCentered.y * c;
                
//                subUV += center;
//            }
//        }

//        // ---------------------------------------------------------
//        // 2. スクロール適用 (Scroll)
//        // ---------------------------------------------------------
//        // Polarモードの場合: X=回転(Spin), Y=拡大縮小(Expand)
//        // Normalモードの場合: X=横移動, Y=縦移動
//        subUV += (subUvScrollSpeed * currentTime);

//        // ---------------------------------------------------------
//        // 3. サンプリング & 合成 (Blend)
//        // ---------------------------------------------------------
//        float4 subColor = SampleSlot(subTexIndex, subUV);

//        // 合成モード分岐
//        if (subBlendMode == 0)      color.rgb = lerp(color.rgb, color.rgb * subColor.rgb, subTexStrength); // Multiply
//        else if (subBlendMode == 1) color.rgb += subColor.rgb * subTexStrength; // Add
//        else if (subBlendMode == 2) color.rgb -= subColor.rgb * subTexStrength; // Subtract
//        else if (subBlendMode == 3) color.a *= lerp(1.0f, subColor.r, subTexStrength); // AlphaMask
//    }
//#endif   
    

// // =========================================================
//    // ★ Toon Shading (Ramp Only)
//    // =========================================================
//#ifdef USE_TOON
//    {
//        // Rampテクスチャが設定されている場合のみ動作
//        if (toonRampTexIndex >= 0)
//        {
//            float inputAlpha = color.a;

//            // -----------------------------------------------------
//            // 1. Boundary Noise (境界ノイズ)
//            // -----------------------------------------------------
//            // ノイズテクスチャがあれば適用
//            if (toonNoiseStrength > 0.001f && toonNoiseTexIndex >= 0)
//            {
//                float2 noiseUV = pin.texcoord + (float2(1.0f, 0.5f) * currentTime * toonNoiseSpeed);
                
//                // ノイズは通常サンプリング(Wrap)でOK
//                float noiseVal = SampleSlot(toonNoiseTexIndex, noiseUV).r; 

//                float noiseOffset = (noiseVal - 0.5f) * toonNoiseStrength;
//                inputAlpha = saturate(inputAlpha + noiseOffset);
//            }

//            // -----------------------------------------------------
//            // 2. 階調計算 (Ramp参照位置の決定)
//            // -----------------------------------------------------
//            float biasedAlpha = saturate(inputAlpha + (toonThreshold - 0.5f));
//            float steps = max(1.0f, toonSteps);
//            float val = biasedAlpha * steps;
            
//            // 階段状にする処理
//            float i = floor(val);
//            float f = frac(val);
//            float feather = max(0.001f, toonSmoothing * steps); 
//            float smooth_f = smoothstep(0.5f - feather, 0.5f + feather, f);
            
//            // 0.0 ~ 1.0 の範囲に正規化 (これがRampのU座標になる)
//            float stepRatio = (i + smooth_f) / steps;

//            // アルファ値を更新
//            color.a = stepRatio;

//            // -----------------------------------------------------
//            // 3. Ramp Coloring (色付け)
//            // -----------------------------------------------------
//            // U: 明るさ(stepRatio), V: 0.5(中央)
//            float2 rampUV = float2(stepRatio, 0.5f);
            
//            // ★指定通り Clamp でサンプリング
//            // これにより、0.0未満や1.0超過のUVがきても端の色が伸びるだけで、反対側の色が混ざらない
//            float4 rampColor = SampleSlotClamp(toonRampTexIndex, rampUV);

//            // 色を乗算 (テクスチャの色 x Rampの色)
//            color.rgb *= rampColor.rgb;
//        }
//    }
//#endif

    
    
  
    
//#ifdef USE_SIDE_FADE
//    // UVのX座標（横方向）を見て、0に近い側と1に近い側を透明にする
//    if (sideFadeWidth > 0.001f)
//    {
//        // 左端 (0.0 -> width) のフェード係数 (0->1)
//        float fadeLeft = smoothstep(0.0f, sideFadeWidth, finalUV.x);
        
//        // 右端 (1.0 -> 1.0-width) のフェード係数 (0->1)
//        // HLSLのsmoothstepは min > max の場合、逆補間(1->0)をしてくれる
//        float fadeRight = smoothstep(1.0f, 1.0f - sideFadeWidth, finalUV.x);
        
//        // 両方を掛け合わせてアルファに適用
//        color.a *= (fadeLeft * fadeRight);
//    }
//#endif    
    
    
    
    
// #ifdef USE_GRADIENT_MAP
//    if (gradientTexIndex >= 0 && gradientStrength > 0.001f)
//    {
//        // メイン画像(color)の赤成分を輝度として取得
//        float grayscale = color.r;
        
//        // ★修正1: SampleSlotClamp を使用 (色の巻き込み防止)
//        // ★修正2: もし「縦読み」されていると確信があるなら、
//        //          float2(0.5f, grayscale) に入れ替えてみてください。
//        //          基本は float2(grayscale, 0.5f) で「横読み」になります。
        
//        float4 gradColor = SampleSlotClamp(gradientTexIndex, float2(grayscale, 0.5f));
        
//        // 合成
//        color.rgb = lerp(color.rgb, gradColor.rgb, gradientStrength);
//    }
//#endif

// #ifdef USE_NORMAL_MAP
//    if (normalTexIndex >= 0)
//    {
//        // 1. ノーマルマップのサンプリング (0.0~1.0)
//        // MainTextureと同じUV座標(finalUV)を使うのが基本です
//        float4 normalMap = SampleSlot(normalTexIndex, finalUV);

//        // 2. アンパック: (0.0~1.0) -> (-1.0~1.0) に変換
//        float3 localNormal = normalMap.rgb * 2.0f - 1.0f;

//        // 3. 強度の適用
//        // XY成分(傾き)に強度を掛け、Z成分(高さ)はそのまま保つことで、見た目の深さを調整
//        localNormal.xy *= normalStrength;
//        localNormal = normalize(localNormal);

//        // 4. TBN行列の構築 (Tangent Space -> World Space 変換用)
//        // VSから渡された World法線(normal) と World接線(tangent) を使用
//        float3 N = normalize(pin.normal);
//        float3 T = normalize(pin.tangent);
        
//        // 従法線(Binormal)は外積で求める (左手系/右手系に合わせて符号反転が必要な場合あり)
//        float3 B = normalize(cross(N, T));

//        // TBN行列を作成
//        float3x3 TBN = float3x3(T, B, N);

//        // 5. 法線の変換
//        // ノーマルマップの法線をワールド空間へ変換し、pin.normal を上書きする！
//        pin.normal = normalize(mul(localNormal, TBN));
//    }
//#endif  
    
    

//#ifdef USE_MATCAP
//    if (matCapTexIndex >= 0 && matCapStrength > 0.001f)
//    {
//        // 1. ワールド法線をスクリーン空間の向きに変換 (w=0で方向のみ投影)
//        // row_major なので mul(vector, matrix) の順序
//        float3 screenNormal = mul(float4(pin.normal, 0.0f), viewProjection).xyz;

//        // 2. 正規化
//        screenNormal = normalize(screenNormal);

//        // 3. UV座標の計算 (XYを 0.0~1.0 に変換)
//        float2 matCapUV = screenNormal.xy * 0.5f + 0.5f;
//        matCapUV.y = 1.0f - matCapUV.y; // 上下反転

//        // 4. サンプリング (境界線を考慮して Clamp を使用)
//        float4 matCapSample = SampleSlotClamp(matCapTexIndex, matCapUV);
        
//        // 5. 合成 (ベース色に質感を乗算し、強度でブレンド)
//        // * 2.0 は、MatCapのグレー(0.5)部分を「変化なし」にするための調整です
//        float3 blended = color.rgb * matCapSample.rgb * 2.0f * matCapColor;
//        color.rgb = lerp(color.rgb, blended, matCapStrength);
//    }
//#endif
    
    
    
    


//#ifdef USE_MASK
//{
//    // 1. マスクテクスチャをサンプリング
//    float4 maskSample = SampleSlot(maskTexIndex, pin.texcoord);
//    float maskVal = max(maskSample.r, maskSample.a); // RかAの強い方を取る

//    // 2. ★暗さ対策: Intensity と Contrast の適用
//    // Intensity で全体の輝度を上げ、Contrast で「中間色（グレー）」を削ぎ落とす
//    // 数式: saturate((maskVal - 0.5) * contrast + 0.5) * intensity
//    maskVal = saturate((maskVal - 0.5f) * maskContrast + 0.5f);
//    maskVal *= maskIntensity;

//    // 3. オプション: プロシージャルフェード (端をさらに絞る)
//    if (maskEdgeFade > 0.001f)
//    {
//        float2 rawUV = pin.texcoord;
//        float2 fadeFactor;
//        fadeFactor.x = smoothstep(0.0f, maskEdgeFade, rawUV.x) * smoothstep(0.0f, maskEdgeFade, 1.0f - rawUV.x);
//        fadeFactor.y = smoothstep(0.0f, maskEdgeFade, rawUV.y) * smoothstep(0.0f, maskEdgeFade, 1.0f - rawUV.y);
        
//        maskVal *= (fadeFactor.x * fadeFactor.y);
//    }

//    // 4. 最終合成
//    color.a *= saturate(maskVal);
//}
//#endif
    
    
// #ifdef USE_DISSOLVE
//    if (dissolveTexIndex >= 0)
//    {
//        // 1. ノイズサンプリング
//        float dissolveVal = SampleSlotClamp(dissolveTexIndex, finalUV).r;
        
//        // 2. クリップ (ここまでは既存と同じ)
//        // 閾値より低いピクセルは描画しない
//        if (dissolveVal < dissolveThreshold) discard;

//        // ---------------------------------------------------
//        // ★追加: USE_DISSOLVE_GLOW logic
//        // ---------------------------------------------------
//#ifdef USE_DISSOLVE_GLOW
//        // dissolveVal が threshold に近いほど (ギリギリ消えていない場所ほど) 強く光らせる
        
//        // 「閾値」から「閾値 + Range」までの範囲を 1.0 ~ 0.0 に変換
//        // smoothstep(min, max, value): valueがmin以下なら0、max以上なら1、間は滑らかに補間
//        // ここでは逆にしたいので 1.0 - ... を使う、あるいは引数を逆にする
        
//        // 閾値(threshold)付近が 1.0、そこから range 分離れると 0.0 になる計算
//        float glowFactor = 1.0f - smoothstep(dissolveThreshold, dissolveThreshold + dissolveGlowRange, dissolveVal);
        
//        // 指数関数でカーブを調整 (熱源のような急激な減衰を作る)
//        glowFactor = pow(glowFactor, 3.0f); 

//        // 加算合成 (Emissive)
//        // 元の色に「発光色 * 強さ * 係数」を足し込む
//        color.rgb += dissolveGlowColor * dissolveGlowIntensity * glowFactor;
//#else
//        // (Glowを使わない場合の、既存の単色エッジ処理があればここに入れる)
//        // float edge = step(dissolveVal, dissolveThreshold + 0.05f);
//        // color.rgb = lerp(color.rgb, dissolveEdgeColor, edge);
//#endif
//    }
//#endif  
    
    
    
  
//#ifdef USE_LIGHTING
//    // ライティング計算 (省略)
//    // 必要なら法線マップも normalTexIndex を作ってルーティングする
//#endif

//    // 色計算が終わった後、最終出力の直前にフレネルを加算
//    color.rgb *= emissiveIntensity;
  
//#ifdef USE_FRESNEL
//    if (fresnelPower > 0.01f)
//    {
  
//        float v = (pin.texcoord.y - 0.5f) * 2.0f;
//        float uvRim = pow(abs(v), fresnelPower);
    
//        float finalRim = uvRim * color.a;

//        // 3. 加算
//        color.rgb += fresnelColor * finalRim;
    
//    }
//#endif 

//#ifdef USE_ALPHA_FADE
//    // -----------------------------------------------------------
//    // ★ 改修: 空間的な消込 (UV Wipe) と 固定端フェード
//    // -----------------------------------------------------------
    
//    // ★修正: スクロールや歪みの影響を受けていない、メッシュ本来のUVを使用
//    // これにより、模様がどれだけ流れても「斬撃の形」は崩れません。
//    float wipeUV = pin.texcoord.x; 

//    // 1. ボケ足 (Softness)
//    float softness = max(0.001f, clipSoftness);
    
//    // 2. Wipe マスク計算
//    // finalUV.x ではなく wipeUV を使う
//    float wipeMask = smoothstep(clipStart - softness, clipStart, wipeUV) 
//                   * smoothstep(clipEnd + softness,   clipEnd,   wipeUV);

//    // 3. 固定端フェード (Start/End Fade)
//    // ここも wipeUV を使い、メッシュの両端を常にフェードさせる
//    float startFade = smoothstep(0.0f, startEndFadeWidth, wipeUV);
//    float endFade   = 1.0f - smoothstep(1.0f - startEndFadeWidth, 1.0f, wipeUV);

//    // 全てを乗算してアルファに適用
//    color.a *= wipeMask * startFade * endFade;

//    // 最後に全体の透明度(visibility)を適用
//    color.a *= visibility;

//    // 完全に透明なら描画をスキップ
//    if (color.a <= 0.0f) discard;
//#endif    
    
    
    
    
   
//    return color;
//}


#include "Effect.hlsl"

// ---------------------------------------------------------
// 汎用テクスチャスロット (t0 ~ t9)
// ---------------------------------------------------------
Texture2D TexSlot0 : register(t0);
Texture2D TexSlot1 : register(t1);
Texture2D TexSlot2 : register(t2);
Texture2D TexSlot3 : register(t3);
Texture2D TexSlot4 : register(t4);
Texture2D TexSlot5 : register(t5);
Texture2D TexSlot6 : register(t6);
Texture2D TexSlot7 : register(t7);
Texture2D TexSlot8 : register(t8);
Texture2D TexSlot9 : register(t9);

SamplerState LinearWrap : register(s0);
SamplerState LinearClamp : register(s1);

// =========================================================
// ★共通コア関数 (The Core)
// 全サンプリング処理の基盤。サンプラーを引数で受け取る。
// =========================================================
float4 SampleTextureCore(int index, SamplerState ss, float2 uv)
{
    [branch]
    switch (index)
    {
        case 0:
            return TexSlot0.Sample(ss, uv);
        case 1:
            return TexSlot1.Sample(ss, uv);
        case 2:
            return TexSlot2.Sample(ss, uv);
        case 3:
            return TexSlot3.Sample(ss, uv);
        case 4:
            return TexSlot4.Sample(ss, uv);
        case 5:
            return TexSlot5.Sample(ss, uv);
        case 6:
            return TexSlot6.Sample(ss, uv);
        case 7:
            return TexSlot7.Sample(ss, uv);
        case 8:
            return TexSlot8.Sample(ss, uv);
        case 9:
            return TexSlot9.Sample(ss, uv);
    }
    return float4(0, 0, 0, 0);
}

// ---------------------------------------------------------
// ラッパー1: 通常サンプリング (Wrap / Loop)
// ノイズ、フロー、サブテクスチャなど無限ループ用
// ---------------------------------------------------------
float4 SampleSlot(int index, float2 uv)
{
    return SampleTextureCore(index, LinearWrap, uv);
}

// ---------------------------------------------------------
// ラッパー2: クランプサンプリング (Clamp / No Repeat)
// グラデーション、トゥーン、MatCap用 (端の色を維持)
// ---------------------------------------------------------
float4 SampleSlotClamp(int index, float2 uv)
{
    float4 c = SampleTextureCore(index, LinearClamp, uv);
    return float4(c.rgb, 1.0f); // アルファを無視して不透明にする
   
}

// ---------------------------------------------------------
// ラッパー3: メインテクスチャ専用 (One-Shot Logic)
// ★ここだけ uvScrollMode (0:Loop, 1:Clamp+Vanish) の判定を行う
// ---------------------------------------------------------
float4 SampleMainTexture(int index, float2 uv)
{
    // One-Shot (Clamp) モードの場合
    if (uvScrollMode == 1)
    {
        // 範囲外なら描画しない（透明にする）
        // ※HLSLのclipを使わず透明色を返すことで、ブレンドステートに任せる
        // ※0.001のマージンは計算誤差による端のチラつき防止
        if (uv.x < 0.001f || uv.x > 0.999f || uv.y < 0.001f || uv.y > 0.999f)
        {
            return float4(0, 0, 0, 0);
        }
        
        // 範囲内なら Clamp でサンプリング (Coreを呼ぶ)
        // 反対側の色が混ざらないように Clamp を指定
        return SampleTextureCore(index, LinearClamp, uv);
    }
    
    // Loopモードなら通常サンプリング (Wrap)
    return SampleTextureCore(index, LinearWrap, uv);
}


// ---------------------------------------------------------
// Main Pixel Shader
// ---------------------------------------------------------
float4 main(VS_OUT pin) : SV_TARGET
{
    float2 uv = pin.texcoord;
    
    // -----------------------------------------------------
    // 1. Distortion (歪み)
    // -----------------------------------------------------
#ifdef USE_DISTORT
    if (distortionTexIndex >= 0)
    {
        float2 distUV = uv + (distortionUvScrollSpeed * currentTime);
        // ノイズはループしてほしいので SampleSlot (Wrap)
        float4 noise = SampleSlot(distortionTexIndex, distUV);
        
        float2 offset;
        offset.x = (noise.r - 0.5f);
        offset.y = (noise.g - 0.5f);
        
        // 白黒画像対策
        if(abs(noise.r - noise.g) < 0.01f) {
            offset.y = -offset.x; 
        }

        uv += offset * distortionStrength;
    }
#endif

    // -----------------------------------------------------
    // 2. Main Color Calculation
    // -----------------------------------------------------
    // 歪んだ後のUVにスクロールを足す
    uv += mainUvScrollSpeed * currentTime;
    float2 finalUV = uv;
    
    float4 color = baseColor * pin.color;
    
#ifdef USE_FLIPBOOK
    // フリップブック計算
    if (flipbookWidth > 1.0f || flipbookHeight > 1.0f) {
        float totalCells = flipbookWidth * flipbookHeight;
        float currentFrame = floor(currentTime * flipbookSpeed);
        currentFrame = fmod(currentFrame, totalCells);
        float col = fmod(currentFrame, flipbookWidth);
        float row = floor(currentFrame / flipbookWidth);
        
        finalUV.x /= flipbookWidth;
        finalUV.y /= flipbookHeight;
        finalUV.x += col / flipbookWidth;
        finalUV.y += row / flipbookHeight;
    } else {
        // バリアント有効だがサイズ1x1の場合は通常のスクロールが適用される
        // (上の uv += ... ですでに適用済み)
    }
#endif  

    
#ifdef USE_TEXTURE
    if (mainTexIndex >= 0)
    {
        float4 finalTexColor = float4(1, 1, 1, 1);

        // --- Flow Map ---
        float2 uv0 = finalUV;
        float2 uv1 = finalUV;
        float flowLerp = 0.0f;
        bool useFlow = false;

#ifdef USE_FLOW_MAP
        if (flowTexIndex >= 0)
        {
            useFlow = true;
            // フローマップはループ前提 -> SampleSlot (Wrap)
            float4 flowSample = SampleSlot(flowTexIndex, pin.texcoord);
            float2 flowVector = flowSample.rg * 2.0f - 1.0f;
            flowVector *= flowStrength;

            float timeScale = currentTime * flowSpeed;
            float phase0 = frac(timeScale);
            float phase1 = frac(timeScale + 0.5f);

            uv0 = finalUV + flowVector * phase0;
            uv1 = finalUV + flowVector * phase1;
            flowLerp = abs((0.5f - phase0) * 2.0f);
        }
#endif

        // --- Sampling Main Texture ---
        // ★修正: SampleMainTexture を使用 (Loop/Clamp切替対応)
        float4 sampledColors[2];
        sampledColors[0] = float4(1, 1, 1, 1);
        sampledColors[1] = float4(1, 1, 1, 1);

        int iterations = useFlow ? 2 : 1;
        
        [unroll]
        for (int i = 0; i < iterations; ++i)
        {
            float2 currentUV = (i == 0) ? uv0 : uv1;
            float4 colResult = float4(1, 1, 1, 1);

#ifdef USE_CHROMATIC_ABERRATION
            float2 shift = float2(chromaticAberrationStrength, 0.0f);

            // メインテクスチャなので SampleMainTexture
            float4 colR = SampleMainTexture(mainTexIndex, currentUV - shift);
            float4 colG = SampleMainTexture(mainTexIndex, currentUV);
            float4 colB = SampleMainTexture(mainTexIndex, currentUV + shift);
    
            colResult.r = colR.r;
            colResult.g = colG.g;
            colResult.b = colB.b;
            colResult.a = colG.a;
#else
            // メインテクスチャなので SampleMainTexture
            colResult = SampleMainTexture(mainTexIndex, currentUV);
#endif
            sampledColors[i] = colResult;
        }

        if (useFlow) finalTexColor = lerp(sampledColors[0], sampledColors[1], flowLerp);
        else         finalTexColor = sampledColors[0];

        color *= finalTexColor;
    }
#endif 
    
    
#ifdef USE_SUB_TEXTURE
    if (subTexIndex >= 0)
    {
        float2 subUV = pin.texcoord;
        
        // Polar / Cartesian
        if (usePolarCoords > 0.5f) {
            float2 delta = subUV - 0.5f;
            float radius = length(delta) * 2.0f;
            float angle = atan2(delta.y, delta.x);
            subUV.x = angle / 6.2831853f + 0.5f; 
            subUV.y = radius;
        } else {
            if (abs(subUvRotationSpeed) > 0.0001f) {
                float2 center = float2(0.5f, 0.5f);
                float2 uvCentered = subUV - center;
                float angle = subUvRotationSpeed * currentTime;
                float c = cos(angle);
                float s = sin(angle);
                subUV.x = uvCentered.x * c - uvCentered.y * s;
                subUV.y = uvCentered.x * s + uvCentered.y * c;
                subUV += center;
            }
        }
        subUV += (subUvScrollSpeed * currentTime);

        // サブテクスチャは通常 SampleSlot (Loop) でOK
        float4 subColor = SampleSlot(subTexIndex, subUV);

        // Blend Modes
        if (subBlendMode == 0)      color.rgb = lerp(color.rgb, color.rgb * subColor.rgb, subTexStrength);
        else if (subBlendMode == 1) color.rgb += subColor.rgb * subTexStrength;
        else if (subBlendMode == 2) color.rgb -= subColor.rgb * subTexStrength;
        else if (subBlendMode == 3) color.a *= lerp(1.0f, subColor.r, subTexStrength);
    }
#endif    
    

 // =========================================================
 // Toon Shading (Ramp Only)
 // =========================================================
#ifdef USE_TOON
    {
        if (toonRampTexIndex >= 0)
        {
            float inputAlpha = color.a;

            // 1. Noise
            if (toonNoiseStrength > 0.001f && toonNoiseTexIndex >= 0)
            {
                float2 noiseUV = pin.texcoord + (float2(1.0f, 0.5f) * currentTime * toonNoiseSpeed);
                // ノイズは SampleSlot (Wrap)
                float noiseVal = SampleSlot(toonNoiseTexIndex, noiseUV).r; 
                float noiseOffset = (noiseVal - 0.5f) * toonNoiseStrength;
                inputAlpha = saturate(inputAlpha + noiseOffset);
            }

            // 2. Ramp Coord
            float biasedAlpha = saturate(inputAlpha + (toonThreshold - 0.5f));
            float steps = max(1.0f, toonSteps);
            float val = biasedAlpha * steps;
            
            float i = floor(val);
            float f = frac(val);
            float feather = max(0.001f, toonSmoothing * steps); 
            float smooth_f = smoothstep(0.5f - feather, 0.5f + feather, f);
            float stepRatio = (i + smooth_f) / steps;

            color.a = stepRatio;

            // 3. Ramp Coloring
            float2 rampUV = float2(stepRatio, 0.5f);
            
            // ★修正: グラデーションは消えてほしくないので SampleSlotClamp を使う！
            float4 rampColor = SampleSlotClamp(toonRampTexIndex, rampUV);

            color.rgb *= rampColor.rgb;
        }
    }
#endif

    
#ifdef USE_GRADIENT_MAP
    if (gradientTexIndex >= 0 && gradientStrength > 0.001f)
    {
        float grayscale = color.r;
        
      
     float4 gradColor = SampleSlotClamp(gradientTexIndex, float2(grayscale, 0.5f));
        
        color.rgb = lerp(color.rgb, gradColor.rgb, gradientStrength);
    }
#endif

#ifdef USE_NORMAL_MAP
    if (normalTexIndex >= 0)
    {
        // ノーマルマップは通常タイリングするので SampleSlot (Wrap)
        float4 normalMap = SampleSlot(normalTexIndex, finalUV);
        
        float3 localNormal = normalMap.rgb * 2.0f - 1.0f;
        localNormal.xy *= normalStrength;
        localNormal = normalize(localNormal);
        
        float3 N = normalize(pin.normal);
        float3 T = normalize(pin.tangent);
        float3 B = normalize(cross(N, T));
        float3x3 TBN = float3x3(T, B, N);
        
        pin.normal = normalize(mul(localNormal, TBN));
    }
#endif  
    
#ifdef USE_MATCAP
    if (matCapTexIndex >= 0 && matCapStrength > 0.001f)
    {
        float3 screenNormal = mul(float4(pin.normal, 0.0f), viewProjection).xyz;
        screenNormal = normalize(screenNormal);
        float2 matCapUV = screenNormal.xy * 0.5f + 0.5f;
        matCapUV.y = 1.0f - matCapUV.y;

        // ★MatCapも端の色を保持したいので SampleSlotClamp
        float4 matCapSample = SampleSlotClamp(matCapTexIndex, matCapUV);
        
        float3 blended = color.rgb * matCapSample.rgb * 2.0f * matCapColor;
        color.rgb = lerp(color.rgb, blended, matCapStrength);
    }
#endif

#ifdef USE_MASK
{
    // マスクはタイリングしないことが多いので SampleSlotClamp が安全
    float4 maskSample = SampleSlotClamp(maskTexIndex, pin.texcoord);
    float maskVal = max(maskSample.r, maskSample.a);

    maskVal = saturate((maskVal - 0.5f) * maskContrast + 0.5f);
    maskVal *= maskIntensity;

    if (maskEdgeFade > 0.001f)
    {
        float2 rawUV = pin.texcoord;
        float2 fadeFactor;
        fadeFactor.x = smoothstep(0.0f, maskEdgeFade, rawUV.x) * smoothstep(0.0f, maskEdgeFade, 1.0f - rawUV.x);
        fadeFactor.y = smoothstep(0.0f, maskEdgeFade, rawUV.y) * smoothstep(0.0f, maskEdgeFade, 1.0f - rawUV.y);
        maskVal *= (fadeFactor.x * fadeFactor.y);
    }
    color.a *= saturate(maskVal);
}
#endif

#ifdef USE_DISSOLVE
    if (dissolveTexIndex >= 0)
    {
        // ディゾルブ用ノイズはループしてほしいので SampleSlot (Wrap)
        float dissolveVal = SampleSlot(dissolveTexIndex, finalUV).r;
        
        if (dissolveVal < dissolveThreshold) discard;

#ifdef USE_DISSOLVE_GLOW
        float glowFactor = 1.0f - smoothstep(dissolveThreshold, dissolveThreshold + dissolveGlowRange, dissolveVal);
        glowFactor = pow(glowFactor, 3.0f); 
        color.rgb += dissolveGlowColor * dissolveGlowIntensity * glowFactor;
#endif
    }
#endif  
    
#ifdef USE_SIDE_FADE
    if (sideFadeWidth > 0.001f)
    {
        float fadeLeft = smoothstep(0.0f, sideFadeWidth, finalUV.x);
        float fadeRight = smoothstep(1.0f, 1.0f - sideFadeWidth, finalUV.x);
        color.a *= (fadeLeft * fadeRight);
    }
#endif

    // Emissive Intensity
    color.rgb *= emissiveIntensity;

#ifdef USE_FRESNEL
    if (fresnelPower > 0.01f)
    {
        float v = (pin.texcoord.y - 0.5f) * 2.0f;
        float uvRim = pow(abs(v), fresnelPower);
        float finalRim = uvRim * color.a;
        color.rgb += fresnelColor * finalRim;
    }
#endif 

#ifdef USE_ALPHA_FADE
    // 空間的な消込 (UV Wipe) と 固定端フェード
    // メッシュ本来のUV (pin.texcoord.x) を使用する
    float wipeUV = pin.texcoord.x; 
    float softness = max(0.001f, clipSoftness);
    
    float wipeMask = smoothstep(clipStart - softness, clipStart, wipeUV) 
                   * smoothstep(clipEnd + softness,    clipEnd,    wipeUV);
                   
    float startFade = smoothstep(0.0f, startEndFadeWidth, wipeUV);
    float endFade   = 1.0f - smoothstep(1.0f - startEndFadeWidth, 1.0f, wipeUV);

    color.a *= wipeMask * startFade * endFade;
    color.a *= visibility;

    if (color.a <= 0.0f) discard;
#endif    

    return color;
}
