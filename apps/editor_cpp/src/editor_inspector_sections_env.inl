// editor_inspector_sections_env.inl - extracted from editor_inspector_panel.cpp (T9)
// DO NOT include directly; #include'd from editor_inspector_panel.cpp
// ─── 3D Physics (existing) ──────────────────────────────────────────────────

void DrawRigidBody3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::RigidBody3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::RigidBody3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_RUN "  RigidBody 3D", *ti,
                         MakeReflectResolver<dse::RigidBody3DComponent>(context));
}

void DrawBoxCollider3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::BoxCollider3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::BoxCollider3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_CUBE_OUTLINE "  Box Collider 3D", *ti,
                         MakeReflectResolver<dse::BoxCollider3DComponent>(context));
}

void DrawSphereCollider3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::SphereCollider3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::SphereCollider3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SPHERE "  Sphere Collider 3D", *ti,
                         MakeReflectResolver<dse::SphereCollider3DComponent>(context));
}

void DrawParticleSystem3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::ParticleSystem3DComponent>(context.selected_entity)) {
        return;
    }

    auto& ps = context.registry.get<dse::ParticleSystem3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_CREATION "  Particle System 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "ps3d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##ps3d_en", &ps.enabled));
    INSPECTOR_PROPERTY("Max Particles", ImGui::Text("%d (Active: %d)", ps.max_particles, ps.active_particle_count));
    INSPECTOR_PROPERTY("Emission Rate", ImGui::DragFloat("##ps3d_rate", &ps.emission_rate, 1.0f, 0.0f, 10000.0f));

    ImGui::Separator();
    ImGui::Text("Life"); ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Min", ImGui::DragFloat("##ps3d_lmin", &ps.start_life_min, 0.1f, 0.1f, 10.0f));
    INSPECTOR_PROPERTY("Max", ImGui::DragFloat("##ps3d_lmax", &ps.start_life_max, 0.1f, 0.1f, 10.0f));

    ImGui::Separator();
    ImGui::Text("Size"); ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Min", ImGui::DragFloat("##ps3d_smin", &ps.start_size_min, 0.05f, 0.01f, 5.0f));
    INSPECTOR_PROPERTY("Max", ImGui::DragFloat("##ps3d_smax", &ps.start_size_max, 0.05f, 0.01f, 5.0f));

    ImGui::Separator();
    ImGui::Text("Speed"); ImGui::NextColumn(); ImGui::NextColumn();
    INSPECTOR_PROPERTY("Min", ImGui::DragFloat("##ps3d_spmin", &ps.start_speed_min, 0.1f, 0.0f, 50.0f));
    INSPECTOR_PROPERTY("Max", ImGui::DragFloat("##ps3d_spmax", &ps.start_speed_max, 0.1f, 0.0f, 50.0f));
    INSPECTOR_PROPERTY("Color", ImGui::ColorEdit4("##ps3d_color", glm::value_ptr(ps.start_color)));
    INSPECTOR_PROPERTY("Gravity", ImGui::DragFloat3("##ps3d_grav", glm::value_ptr(ps.gravity), 0.1f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// PostProcess / Tree / Grass 改由通用反射驱动渲染：字段集合来自反射注册（单一
// 事实来源）。给组件加字段不再需要改这里——A->C 切换后端时本入口同样不动。
void DrawPostProcessSection(EditorContext& context) {
    if (!context.registry.all_of<dse::PostProcessComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::PostProcessComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_POST_PROCESS "  Post Processing", *ti,
                         MakeReflectResolver<dse::PostProcessComponent>(context));
}

void DrawTreeSection(EditorContext& context) {
    if (!context.registry.all_of<dse::TreeComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::TreeComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_TERRAIN "  Tree", *ti,
                         MakeReflectResolver<dse::TreeComponent>(context));
}

void DrawGrassSection(EditorContext& context) {
    if (!context.registry.all_of<dse::GrassComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::GrassComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_TERRAIN "  Grass", *ti,
                         MakeReflectResolver<dse::GrassComponent>(context));
}

void DrawLightProbeSection(EditorContext& context) {
    if (!context.registry.all_of<dse::LightProbeComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::LightProbeComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_LIGHTBULB "  Light Probe", *ti,
                         MakeReflectResolver<dse::LightProbeComponent>(context));
}

void DrawReflectionProbeSection(EditorContext& context) {
    if (!context.registry.all_of<dse::ReflectionProbeComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::ReflectionProbeComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_CUBE_OUTLINE "  Reflection Probe", *ti,
                         MakeReflectResolver<dse::ReflectionProbeComponent>(context));
}

// ─── 反射驱动的 Inspector Section（Decal / Water / GIProbeVolume / LODGroup / Foliage / NavMesh 等） ───

void DrawDecalSection(EditorContext& context) {
    if (!context.registry.all_of<dse::DecalComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::DecalComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_LAYERS "  Decal", *ti,
                         MakeReflectResolver<dse::DecalComponent>(context));
}

void DrawWaterSection(EditorContext& context) {
    if (!context.registry.all_of<dse::WaterComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::WaterComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_WATER "  Water", *ti,
                         MakeReflectResolver<dse::WaterComponent>(context));
}

void DrawGIProbeVolumeSection(EditorContext& context) {
    if (!context.registry.all_of<dse::GIProbeVolumeComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::GIProbeVolumeComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_LIGHTBULB "  GI Probe Volume", *ti,
                         MakeReflectResolver<dse::GIProbeVolumeComponent>(context));
}

void DrawLODGroupSection(EditorContext& context) {
    if (!context.registry.all_of<dse::LODGroupComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::LODGroupComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_LAYERS "  LOD Group", *ti,
                         MakeReflectResolver<dse::LODGroupComponent>(context));
}

void DrawFoliageSection(EditorContext& context) {
    if (!context.registry.all_of<dse::FoliageComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::FoliageComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_TERRAIN "  Foliage", *ti,
                         MakeReflectResolver<dse::FoliageComponent>(context));
}

void DrawDynamicObstacleSection(EditorContext& context) {
    if (!context.registry.all_of<dse::DynamicObstacleComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::DynamicObstacleComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_CUBE_OUTLINE "  Dynamic Obstacle", *ti,
                         MakeReflectResolver<dse::DynamicObstacleComponent>(context));
}

void DrawNavMeshAutoRebakeSection(EditorContext& context) {
    if (!context.registry.all_of<dse::NavMeshAutoRebakeComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::NavMeshAutoRebakeComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_MAP "  NavMesh Auto Rebake", *ti,
                         MakeReflectResolver<dse::NavMeshAutoRebakeComponent>(context));
}

void DrawTerrainTileManagerSection(EditorContext& context) {
    if (!context.registry.all_of<dse::TerrainTileManagerComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::TerrainTileManagerComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_TERRAIN "  Terrain Tile Manager", *ti,
                         MakeReflectResolver<dse::TerrainTileManagerComponent>(context));
}

// ─── Sky ─────────────────────────────────────────────────────────────────────

void DrawAtmosphereSection(EditorContext& context) {
    if (!context.registry.all_of<dse::AtmosphereComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::AtmosphereComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_WEATHER_SUNNY "  Atmosphere", *ti,
                         MakeReflectResolver<dse::AtmosphereComponent>(context));
}

void DrawVolumetricCloudSection(EditorContext& context) {
    if (!context.registry.all_of<dse::VolumetricCloudComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::VolumetricCloudComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_WEATHER_SUNNY "  Volumetric Cloud", *ti,
                         MakeReflectResolver<dse::VolumetricCloudComponent>(context));
}

void DrawDayNightCycleSection(EditorContext& context) {
    if (!context.registry.all_of<dse::DayNightCycleComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::DayNightCycleComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_MOON "  Day/Night Cycle", *ti,
                         MakeReflectResolver<dse::DayNightCycleComponent>(context));
}

// ─── Hair / MorphTarget ─────────────────────────────────────────────────────

void DrawHairSection(EditorContext& context) {
    if (!context.registry.all_of<dse::HairComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::HairComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Hair", *ti,
                         MakeReflectResolver<dse::HairComponent>(context));
}

void DrawMorphTargetSection(EditorContext& context) {
    if (!context.registry.all_of<dse::MorphTargetComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::MorphTargetComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Morph Target", *ti,
                         MakeReflectResolver<dse::MorphTargetComponent>(context));
}

void DrawImpostorSection(EditorContext& context) {
    if (!context.registry.all_of<dse::ImpostorComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::ImpostorComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_IMAGE "  Impostor LOD", *ti,
                         MakeReflectResolver<dse::ImpostorComponent>(context));
}

// ─── GPU Particle Emitter (手动 Inspector) ──────────────────────────────

void DrawGpuParticleSection(EditorContext& context) {
    if (!context.registry.all_of<dse::render::GpuParticleComponent>(context.selected_entity)) return;
    auto& comp = context.registry.get<dse::render::GpuParticleComponent>(context.selected_entity);
    auto& cfg = comp.config;

    if (!ImGui::CollapsingHeader(MDI_ICON_CREATION "  GPU Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::Indent();

    ImGui::Checkbox("Enabled", &cfg.enabled);
    int max_p = static_cast<int>(cfg.max_particles);
    if (ImGui::DragInt("Max Particles", &max_p, 100, 100, 1000000)) cfg.max_particles = static_cast<uint32_t>(max_p);
    ImGui::DragFloat("Emission Rate", &cfg.emission_rate, 10.0f, 1.0f, 100000.0f, "%.0f /s");

    // Emission shape
    const char* shapes[] = {"Point", "Sphere", "Cone", "Ring", "Box"};
    int shape_idx = static_cast<int>(cfg.shape);
    if (ImGui::Combo("Shape", &shape_idx, shapes, 5)) cfg.shape = static_cast<dse::render::EmitterShape>(shape_idx);
    if (cfg.shape != dse::render::EmitterShape::Point) {
        ImGui::DragFloat("Shape Radius", &cfg.shape_radius, 0.1f, 0.0f, 100.0f);
    }
    if (cfg.shape == dse::render::EmitterShape::Cone) {
        ImGui::SliderFloat("Cone Angle", &cfg.cone_angle, 1.0f, 90.0f, "%.0f deg");
    }

    ImGui::Separator();
    ImGui::Text("Lifetime & Speed:");
    ImGui::DragFloatRange2("Life", &cfg.life_min, &cfg.life_max, 0.05f, 0.01f, 60.0f, "%.2f s");
    ImGui::DragFloatRange2("Speed", &cfg.speed_min, &cfg.speed_max, 0.1f, 0.0f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Size:");
    ImGui::DragFloat("Start Size", &cfg.size_start, 0.01f, 0.001f, 10.0f);
    ImGui::DragFloat("End Size", &cfg.size_end, 0.01f, 0.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Color:");
    ImGui::ColorEdit4("Start Color", &cfg.color_start.x);
    ImGui::ColorEdit4("End Color", &cfg.color_end.x);

    ImGui::Separator();
    ImGui::Text("Forces:");
    ImGui::DragFloat3("Gravity", &cfg.gravity.x, 0.1f);
    ImGui::DragFloat3("Wind", &cfg.wind.x, 0.1f);
    ImGui::DragFloat("Turbulence", &cfg.turbulence, 0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("Vortex", &cfg.vortex_strength, 0.1f, 0.0f, 50.0f);

    ImGui::Separator();
    ImGui::Text("Collision:");
    ImGui::Checkbox("Enable Collision", &cfg.collision_enabled);
    if (cfg.collision_enabled) {
        ImGui::DragFloat("Plane Y", &cfg.collision_plane_y, 0.1f);
        ImGui::SliderFloat("Bounce", &cfg.collision_bounce, 0.0f, 1.0f);
        ImGui::SliderFloat("Friction", &cfg.collision_friction, 0.0f, 1.0f);
    }

    ImGui::Separator();
    ImGui::Text("Rendering:");
    ImGui::Checkbox("Additive Blend", &cfg.additive_blend);

    ImGui::Unindent();
}

// ─── Lightmap Inspector ───────────────────────────────────────────

void DrawLightmapSection(EditorContext& context) {
    if (!context.registry.all_of<dse::render::LightmapComponent>(context.selected_entity)) return;
    auto& comp = context.registry.get<dse::render::LightmapComponent>(context.selected_entity);

    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::render::LightmapComponent>();
    if (ti) {
        DrawReflectedSection(context, MDI_ICON_LIGHTBULB "  Lightmap", *ti,
                             MakeReflectResolver<dse::render::LightmapComponent>(context));
    } else {
        if (!ImGui::CollapsingHeader(MDI_ICON_LIGHTBULB "  Lightmap", ImGuiTreeNodeFlags_DefaultOpen)) return;
        ImGui::Indent();
        char buf[256];
        strncpy(buf, comp.lightmap_path.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("Path", buf, sizeof(buf))) comp.lightmap_path = buf;
        ImGui::DragFloat("Intensity", &comp.intensity, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat4("ST Offset", &comp.st_offset.x, 0.01f);
        ImGui::Checkbox("Use AO", &comp.use_ao);
        ImGui::Unindent();
    }
}
