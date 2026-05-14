#include "compute_particle.hlsli"
// main はリボン描画用の入力頂点を後段のジオメトリシェーダーへ渡す形式へ整える。

GS_IN main(uint vertex_id : SV_VertexID)
{
    GS_IN vout;
    vout.vertex_id = vertex_id;
    return vout;
}
