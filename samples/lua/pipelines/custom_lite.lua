return {
    name = "CustomLiteLua",

    settings = {
        gpu_driven = true,
        shadows = true,
        shadow_quality = "medium",
        postprocess_quality = "lite",
    },

    passes = {
        { name = "pre_z", enabled = true },
        { name = "hiz_build", enabled = true },
        { name = "hiz_cull", enabled = true },
        { name = "csm_shadow", enabled = true },
        { name = "spot_shadow", enabled = false },
        { name = "point_shadow", enabled = false },
        { name = "gpu_cull", enabled = true },
        { name = "forward_scene", enabled = true },
        { name = "wboit", enabled = true },
        { name = "water", enabled = true },
        { name = "bloom", enabled = true, intensity = 0.5 },
        { name = "ssao", enabled = false },
        { name = "contact_shadow", enabled = false },
        { name = "auto_exposure", enabled = true },
        { name = "ui", enabled = true },
        { name = "composite", enabled = true },
        { name = "fxaa", enabled = true },
        { name = "taa", enabled = false },
        { name = "present", enabled = true },
    }
}
