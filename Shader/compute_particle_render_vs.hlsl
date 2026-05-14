#include "compute_particle.hlsli"

GS_IN main(uint vertex_id : SV_VertexID)
{
    //  頂点番号を送るだけ
    GS_IN vout;
    vout.vertex_id = vertex_id;
    return vout;
}
