// editor_inspector_sections_render.inl - extracted from editor_inspector_panel.cpp (T9)
// DO NOT include directly; #include'd from editor_inspector_panel.cpp
void DrawSpriteRendererSection(EditorContext& context) {
    if (!context.registry.all_of<SpriteRendererComponent>(context.selected_entity)) {
        return;
    }

    auto& sprite = context.registry.get<SpriteRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_PALETTE "  Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "spriterenderer_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Shader");
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(-1);
    ImGui::Button(sprite.shader_variant.empty() ? "None" : sprite.shader_variant.c_str(), ImVec2(-1, 0));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            sprite.shader_variant = path;
            MarkSpriteRendererDirty(sprite);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::NextColumn();

    float color[4] = {sprite.color.r, sprite.color.g, sprite.color.b, sprite.color.a};
    INSPECTOR_PROPERTY_U("Color",
        if (ImGui::ColorEdit4("##color", color)) {
            sprite.color = glm::vec4(color[0], color[1], color[2], color[3]);
            MarkSpriteRendererDirty(sprite);
        },
        "Sprite.Color", sprite.color, context.selected_entity,
        MakeCompSetter<SpriteRendererComponent>(context.registry, context.selected_entity, &SpriteRendererComponent::color));

    ImGui::Columns(1);
}

void DrawRigidBody2DSection(EditorContext& context) {
    if (!context.registry.all_of<RigidBody2DComponent>(context.selected_entity)) {
        return;
    }

    auto& rb2d = context.registry.get<RigidBody2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_RUN "  RigidBody 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "rb2d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);

    const char* body_types[] = { "Static", "Kinematic", "Dynamic" };
    int current_type = static_cast<int>(rb2d.type);
    INSPECTOR_PROPERTY("Body Type", if (ImGui::Combo("##type", &current_type, body_types, IM_ARRAYSIZE(body_types))) {
        rb2d.type = static_cast<RigidBody2DType>(current_type);
        MarkRigidBody2DDirty(rb2d);
    });

    float vel[2] = {rb2d.velocity.x, rb2d.velocity.y};
    INSPECTOR_PROPERTY("Velocity", if (ImGui::DragFloat2("##vel", vel, 0.1f)) {
        rb2d.velocity = glm::vec2(vel[0], vel[1]);
        MarkRigidBody2DDirty(rb2d);
    });

    INSPECTOR_PROPERTY_U("Gravity",
        if (ImGui::DragFloat("##grav", &rb2d.gravity_scale, 0.1f)) { MarkRigidBody2DDirty(rb2d); },
        "RigidBody2D.Gravity", rb2d.gravity_scale, context.selected_entity,
        MakeCompSetter<RigidBody2DComponent>(context.registry, context.selected_entity, &RigidBody2DComponent::gravity_scale));
    ImGui::Columns(1);
}

void DrawMeshRendererSection(EditorContext& context) {
    if (!context.registry.all_of<dse::MeshRendererComponent>(context.selected_entity)) {
        return;
    }

    auto& mesh = context.registry.get<dse::MeshRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SPHERE "  Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "mesh_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);

    char mesh_buf[256] = {};
    std::strncpy(mesh_buf, mesh.mesh_path.c_str(), sizeof(mesh_buf) - 1);
    INSPECTOR_PROPERTY("Mesh Path", if (ImGui::InputText("##mesh_path", mesh_buf, sizeof(mesh_buf))) {
        mesh.mesh_path = mesh_buf;
    });
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            std::string p(path);
            if (p.ends_with(".obj") || p.ends_with(".fbx") || p.ends_with(".gltf") || p.ends_with(".glb") || p.ends_with(".dae")) {
                mesh.mesh_path = p;
            }
        }
        ImGui::EndDragDropTarget();
    }

    INSPECTOR_PROPERTY("Material Instance", ImGui::DragScalar("##mat_inst", ImGuiDataType_U32, &mesh.material_instance_id, 1.0f, nullptr, nullptr, "%u"));
    ImGui::Separator();
    ImGui::Text("Material Overrides (PBR)");
    INSPECTOR_PROPERTY_U("Albedo Tint", ImGui::ColorEdit4("##mesh_color", glm::value_ptr(mesh.color)),
        "Mesh.AlbedoTint", mesh.color, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::color));
    INSPECTOR_PROPERTY_U("Emissive", ImGui::ColorEdit3("##mesh_emissive", glm::value_ptr(mesh.emissive)),
        "Mesh.Emissive", mesh.emissive, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::emissive));
    INSPECTOR_PROPERTY_U("Metallic", ImGui::SliderFloat("##mesh_metallic", &mesh.metallic, 0.0f, 1.0f),
        "Mesh.Metallic", mesh.metallic, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::metallic));
    INSPECTOR_PROPERTY_U("Roughness", ImGui::SliderFloat("##mesh_roughness", &mesh.roughness, 0.0f, 1.0f),
        "Mesh.Roughness", mesh.roughness, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::roughness));
    INSPECTOR_PROPERTY_U("Ambient Occlusion", ImGui::SliderFloat("##mesh_ao", &mesh.ao, 0.0f, 1.0f),
        "Mesh.AO", mesh.ao, context.selected_entity,
        MakeCompSetter<dse::MeshRendererComponent>(context.registry, context.selected_entity, &dse::MeshRendererComponent::ao));
    ImGui::Separator();
    INSPECTOR_PROPERTY("Receive Shadow", ImGui::Checkbox("##receive_shadow", &mesh.receive_shadow));

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// 通用反射 Inspector：辅助构造"取得组件实例指针"的回调，组件被移除时返回 nullptr。
template <typename Comp>
std::function<void*()> MakeReflectResolver(EditorContext& context) {
    return [&context]() -> void* {
        entt::entity e = context.selected_entity;
        if (context.registry.valid(e) && context.registry.all_of<Comp>(e))
            return &context.registry.get<Comp>(e);
        return nullptr;
    };
}

// Camera3D / 各类光照 / Free Camera Controller 改由通用反射驱动渲染（纯标量/向量字段，
// 无拖拽或非反射字段）。字段集合来自反射注册（单一事实来源）；A->C 切换后端时此处不变。
void DrawCamera3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::Camera3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::Camera3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_VIDEO "  Camera 3D", *ti,
                         MakeReflectResolver<dse::Camera3DComponent>(context));
}

void DrawDirectionalLightSection(EditorContext& context) {
    if (!context.registry.all_of<dse::DirectionalLight3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::DirectionalLight3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_WEATHER_SUNNY "  Directional Light", *ti,
                         MakeReflectResolver<dse::DirectionalLight3DComponent>(context));
}

void DrawPointLightSection(EditorContext& context) {
    if (!context.registry.all_of<dse::PointLightComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::PointLightComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_LIGHTBULB "  Point Light", *ti,
                         MakeReflectResolver<dse::PointLightComponent>(context));
}

void DrawSpotLightSection(EditorContext& context) {
    if (!context.registry.all_of<dse::SpotLightComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::SpotLightComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_LIGHTBULB "  Spot Light", *ti,
                         MakeReflectResolver<dse::SpotLightComponent>(context));
}

void DrawSkyLightSection(EditorContext& context) {
    if (!context.registry.all_of<dse::SkyLightComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::SkyLightComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_WEATHER_SUNNY "  Sky Light", *ti,
                         MakeReflectResolver<dse::SkyLightComponent>(context));
}

void DrawSkyboxSection(EditorContext& context) {
    if (!context.registry.all_of<dse::SkyboxComponent>(context.selected_entity)) {
        return;
    }

    auto& skybox = context.registry.get<dse::SkyboxComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_VIEW_IN_AR "  Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "skybox_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##skybox_enabled", &skybox.enabled));

    char path_buf[256] = {};
    std::strncpy(path_buf, skybox.cubemap_path.c_str(), sizeof(path_buf) - 1);
    INSPECTOR_PROPERTY("Cubemap Path", if (ImGui::InputText("##skybox_path", path_buf, sizeof(path_buf))) {
        skybox.cubemap_path = path_buf;
    });
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            skybox.cubemap_path = static_cast<const char*>(payload->Data);
        }
        ImGui::EndDragDropTarget();
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawSubSceneSection(EditorContext& context) {
    if (!context.registry.all_of<dse::SubSceneComponent>(context.selected_entity)) return;
    auto& sub = context.registry.get<dse::SubSceneComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_IMAGE_MULTIPLE "  Sub-Scene", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "subscene_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##sub_en", &sub.enabled));

    static char s_sub_path[512] = {};
    static entt::entity s_sub_last = entt::null;
    if (s_sub_last != context.selected_entity) {
        s_sub_last = context.selected_entity;
        std::strncpy(s_sub_path, sub.scene_path.c_str(), sizeof(s_sub_path) - 1);
        s_sub_path[sizeof(s_sub_path) - 1] = '\0';
    }
    INSPECTOR_PROPERTY("Scene Path", if (ImGui::InputText("##sub_path", s_sub_path, sizeof(s_sub_path))) {
        sub.scene_path = s_sub_path;
    });
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string p(static_cast<const char*>(payload->Data));
            if (p.size() > 7 && p.substr(p.size() - 7) == ".dscene") {
                sub.scene_path = p;
                std::strncpy(s_sub_path, p.c_str(), sizeof(s_sub_path) - 1);
            }
        }
        ImGui::EndDragDropTarget();
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);

    if (!sub.scene_path.empty()) {
        ImGui::TextDisabled("Drag to re-order or reassign scene path.");
    }
}

REGISTER_INSPECTOR_CUSTOM(
    "Sub-Scene",
    DrawSubSceneSection,
    "3D",
    35,
    [](entt::registry& r, entt::entity e) -> bool { return r.all_of<dse::SubSceneComponent>(e); },
    [](entt::registry& r, entt::entity e) { if (!r.all_of<dse::SubSceneComponent>(e)) r.emplace<dse::SubSceneComponent>(e); }
);

void DrawAnimator3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::Animator3DComponent>(context.selected_entity)) {
        return;
    }

    auto& animator = context.registry.get<dse::Animator3DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_ANIMATION "  Animator 3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "anim_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##anim_enabled", &animator.enabled));

    char skel_buf[256] = {};
    std::strncpy(skel_buf, animator.dskel_path.c_str(), sizeof(skel_buf) - 1);
    INSPECTOR_PROPERTY("Skeleton Path", if (ImGui::InputText("##anim_skel", skel_buf, sizeof(skel_buf))) {
        animator.dskel_path = skel_buf;
    });
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string p(static_cast<const char*>(payload->Data));
            if (p.ends_with(".dskel")) animator.dskel_path = p;
        }
        ImGui::EndDragDropTarget();
    }

    if (animator.state_machine) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "[FSM Active]");
        INSPECTOR_PROPERTY("Current State", ImGui::Text("%s", animator.current_state_name.c_str()));
        INSPECTOR_PROPERTY("Is Transitioning", ImGui::Text("%s (%.2f%%)", animator.is_transitioning ? "Yes" : "No", animator.transition_progress * 100.0f));
        if (animator.is_transitioning) {
            INSPECTOR_PROPERTY("Next State", ImGui::Text("%s", animator.next_state_name.c_str()));
        }

        ImGui::Separator();
        ImGui::Text("FSM Parameters:");
        for (const auto& kv : animator.state_machine->GetParameters()) {
            if (kv.second.type == dse::gameplay3d::AnimParamType::Float) {
                float val = std::get<float>(kv.second.value);
                INSPECTOR_PROPERTY(kv.first.c_str(), if (ImGui::DragFloat((std::string("##fsm_") + kv.first).c_str(), &val, 0.1f)) {
                    animator.state_machine->SetFloat(kv.first, val);
                });
            } else if (kv.second.type == dse::gameplay3d::AnimParamType::Trigger) {
                INSPECTOR_PROPERTY(kv.first.c_str(), if (ImGui::Button((std::string("Trigger##fsm_") + kv.first).c_str())) {
                    animator.state_machine->SetTrigger(kv.first);
                });
            }
        }
    } else {
        char anim_buf[256] = {};
        std::strncpy(anim_buf, animator.danim_path.c_str(), sizeof(anim_buf) - 1);
        INSPECTOR_PROPERTY("Anim Path", if (ImGui::InputText("##anim_path", anim_buf, sizeof(anim_buf))) {
            animator.danim_path = anim_buf;
        });
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string p(static_cast<const char*>(payload->Data));
                if (p.ends_with(".danim")) animator.danim_path = p;
            }
            ImGui::EndDragDropTarget();
        }
        INSPECTOR_PROPERTY("Speed", ImGui::DragFloat("##anim_speed", &animator.speed, 0.1f, 0.0f, 10.0f));
        INSPECTOR_PROPERTY("Loop", ImGui::Checkbox("##anim_loop", &animator.loop));
        INSPECTOR_PROPERTY("Use Blend Tree", ImGui::Checkbox("##anim_tree", &animator.use_anim_tree));
        INSPECTOR_PROPERTY("Blend Value", ImGui::DragFloat("##anim_blend_value", &animator.blend_parameter_value, 0.05f, -100.0f, 100.0f));

        char blend_param_buf[128] = {};
        std::strncpy(blend_param_buf, animator.blend_parameter.c_str(), sizeof(blend_param_buf) - 1);
        INSPECTOR_PROPERTY("Blend Param", if (ImGui::InputText("##anim_blend_param", blend_param_buf, sizeof(blend_param_buf))) {
            animator.blend_parameter = blend_param_buf;
        });

        if (animator.use_anim_tree) {
            ImGui::Separator();
            ImGui::Text("Blend Nodes");
            for (size_t i = 0; i < animator.blend_nodes.size(); ++i) {
                auto& node = animator.blend_nodes[i];
                ImGui::PushID(static_cast<int>(i));

                char node_name_buf[128] = {};
                std::strncpy(node_name_buf, node.name.c_str(), sizeof(node_name_buf) - 1);
                INSPECTOR_PROPERTY("Node Name", if (ImGui::InputText("##blend_name", node_name_buf, sizeof(node_name_buf))) {
                    node.name = node_name_buf;
                });

                char node_path_buf[256] = {};
                std::strncpy(node_path_buf, node.danim_path.c_str(), sizeof(node_path_buf) - 1);
                INSPECTOR_PROPERTY("Node Anim", if (ImGui::InputText("##blend_path", node_path_buf, sizeof(node_path_buf))) {
                    node.danim_path = node_path_buf;
                });

                INSPECTOR_PROPERTY("Node Speed", ImGui::DragFloat("##blend_speed", &node.speed, 0.05f, 0.0f, 10.0f));
                INSPECTOR_PROPERTY("Node Loop", ImGui::Checkbox("##blend_loop", &node.loop));
                INSPECTOR_PROPERTY("Weight", ImGui::DragFloat("##blend_weight", &node.weight, 0.01f, 0.0f, 1.0f));
                INSPECTOR_PROPERTY("Threshold", ImGui::DragFloat("##blend_threshold", &node.threshold, 0.05f, -100.0f, 100.0f));

                if (ImGui::Button("Remove Node")) {
                    animator.blend_nodes.erase(animator.blend_nodes.begin() + static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    break;
                }
                ImGui::Separator();
                ImGui::PopID();
            }

            if (ImGui::Button("Add Blend Node")) {
                animator.blend_nodes.push_back(dse::AnimBlendNode{});
            }
        }
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawFreeCameraControllerSection(EditorContext& context) {
    if (!context.registry.all_of<dse::FreeCameraControllerComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::FreeCameraControllerComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_VIDEO "  Free Camera Controller", *ti,
                         MakeReflectResolver<dse::FreeCameraControllerComponent>(context));
}

void DrawTerrainSection(EditorContext& context) {
    if (!context.registry.all_of<dse::TerrainComponent>(context.selected_entity)) {
        return;
    }

    auto& terrain = context.registry.get<dse::TerrainComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_TERRAIN "  Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "terrain_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##terrain_enabled", &terrain.enabled));

    char path_buf[256] = {};
    std::strncpy(path_buf, terrain.heightmap_path.c_str(), sizeof(path_buf) - 1);
    INSPECTOR_PROPERTY("Heightmap Path", if (ImGui::InputText("##terrain_path", path_buf, sizeof(path_buf))) {
        terrain.heightmap_path = path_buf;
        terrain.is_dirty = true;
    });

    INSPECTOR_PROPERTY("Width", if (ImGui::DragFloat("##terrain_width", &terrain.width, 1.0f, 10.0f, 1000.0f)) { terrain.is_dirty = true; });
    INSPECTOR_PROPERTY("Depth", if (ImGui::DragFloat("##terrain_depth", &terrain.depth, 1.0f, 10.0f, 1000.0f)) { terrain.is_dirty = true; });
    INSPECTOR_PROPERTY("Max Height", if (ImGui::DragFloat("##terrain_height", &terrain.max_height, 0.5f, 1.0f, 200.0f)) { terrain.is_dirty = true; });
    INSPECTOR_PROPERTY("Dynamic LOD", ImGui::Checkbox("##terrain_lod", &terrain.use_dynamic_lod));

    if (terrain.use_dynamic_lod) {
        INSPECTOR_PROPERTY("LOD Levels", ImGui::DragInt("##terrain_lod_levels", &terrain.max_lod_levels, 0.1f, 1, 8));
        INSPECTOR_PROPERTY("LOD Dist Factor", ImGui::DragFloat("##terrain_lod_dist", &terrain.lod_distance_factor, 1.0f, 5.0f, 500.0f));

        ImGui::Separator();
        ImGui::Text("LOD Preview"); ImGui::NextColumn(); ImGui::NextColumn();
        INSPECTOR_PROPERTY("Current LOD", ImGui::Text("%d / %d", terrain.current_lod, terrain.max_lod_levels - 1));

        // Show triangle count for current LOD
        unsigned int tri_count = 0;
        if (terrain.current_lod < static_cast<int>(terrain.lod_index_counts.size())) {
            tri_count = terrain.lod_index_counts[terrain.current_lod] / 3;
        } else if (terrain.index_count > 0) {
            tri_count = terrain.index_count / 3;
        }
        INSPECTOR_PROPERTY("Triangles", ImGui::Text("%u", tri_count));
        INSPECTOR_PROPERTY("Resolution", ImGui::Text("%d x %d", terrain.resolution_x, terrain.resolution_z));

        // LOD level bar visualization
        ImGui::NextColumn(); ImGui::NextColumn();
        float lod_frac = terrain.max_lod_levels > 1
            ? static_cast<float>(terrain.current_lod) / static_cast<float>(terrain.max_lod_levels - 1)
            : 0.0f;
        ImGui::ProgressBar(lod_frac, ImVec2(-1, 0),
            (std::string("LOD ") + std::to_string(terrain.current_lod)).c_str());
        ImGui::NextColumn();
    }

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}
