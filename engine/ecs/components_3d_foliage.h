#ifndef DSE_COMPONENTS_3D_FOLIAGE_H
#define DSE_COMPONENTS_3D_FOLIAGE_H

namespace dse {

struct FoliageComponent {
    bool enabled = true;

    float wind_strength = 1.0f;       ///< 风弯曲强度倍数 [0,∞)
    float stiffness = 0.5f;           ///< 刚度（0=完全柔软, 1=较硬）
    float phase_offset = 0.0f;        ///< 相位偏移（实例间差异）

    float push_response = 1.0f;       ///< 对角色推力场的响应系数
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_FOLIAGE_H
