// editor_inspector_sections_ui.inl - extracted from editor_inspector_panel.cpp (T9)
// DO NOT include directly; #include'd from editor_inspector_panel.cpp
void DrawUILabelSection(EditorContext& context) {
    if (!context.registry.all_of<UILabelComponent>(context.selected_entity)) {
        return;
    }

    auto& label = context.registry.get<UILabelComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_FORMAT_TEXT "  UI Label", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "uilabel_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);

    char text_buf[256] = {};
    std::strncpy(text_buf, label.text.c_str(), sizeof(text_buf) - 1);
    INSPECTOR_PROPERTY("Text", if (ImGui::InputText("##label_text", text_buf, sizeof(text_buf))) {
        label.text = text_buf;
        label.dirty = true;
    });

    ImGui::Separator();
    ImGui::Text("Localization");
    INSPECTOR_PROPERTY("Enable Loc", if (ImGui::Checkbox("##use_loc", &label.use_localization)) {
        label.dirty = true;
    });

    if (label.use_localization) {
        char key_buf[128] = {};
        std::strncpy(key_buf, label.localization_key.c_str(), sizeof(key_buf) - 1);
        INSPECTOR_PROPERTY("Loc Key", if (ImGui::InputText("##loc_key", key_buf, sizeof(key_buf))) {
            label.localization_key = key_buf;
            label.dirty = true;
        });

        char fallback_buf[256] = {};
        std::strncpy(fallback_buf, label.fallback_text.c_str(), sizeof(fallback_buf) - 1);
        INSPECTOR_PROPERTY("Fallback", if (ImGui::InputText("##fallback_text", fallback_buf, sizeof(fallback_buf))) {
            label.fallback_text = fallback_buf;
            label.dirty = true;
        });

        ImGui::Separator();
        ImGui::Text("Loc Parameters");

        std::string key_to_erase;
        for (auto& [param_key, param_value] : label.localization_params) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("{%s}", param_key.c_str());
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(-30.0f);

            char val_buf[128] = {};
            std::strncpy(val_buf, param_value.c_str(), sizeof(val_buf) - 1);
            if (ImGui::InputText((std::string("##val_") + param_key).c_str(), val_buf, sizeof(val_buf))) {
                param_value = val_buf;
                label.dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button((std::string("X##") + param_key).c_str(), ImVec2(24, 0))) {
                key_to_erase = param_key;
            }
            ImGui::NextColumn();
        }

        if (!key_to_erase.empty()) {
            label.localization_params.erase(key_to_erase);
            label.dirty = true;
        }

        static char new_param_name[64] = "";
        static char new_param_value[128] = "";
        static entt::entity s_last_label_entity = entt::null;
        if (s_last_label_entity != context.selected_entity) {
            s_last_label_entity = context.selected_entity;
            new_param_name[0] = '\0';
            new_param_value[0] = '\0';
        }
        INSPECTOR_PROPERTY("New Param", ImGui::InputTextWithHint("##new_param_name", "Key (e.g. name)", new_param_name, sizeof(new_param_name)));
        INSPECTOR_PROPERTY("New Value", ImGui::InputTextWithHint("##new_param_value", "Value", new_param_value, sizeof(new_param_value)));
        INSPECTOR_PROPERTY("", if (ImGui::Button("Add Parameter", ImVec2(-1, 0))) {
            if (std::strlen(new_param_name) > 0) {
                label.localization_params[new_param_name] = new_param_value;
                label.dirty = true;
                new_param_name[0] = '\0';
                new_param_value[0] = '\0';
            }
        });
    }

    ImGui::Separator();
    ImGui::Text("Appearance");
    float color[4] = {label.color.r, label.color.g, label.color.b, label.color.a};
    INSPECTOR_PROPERTY("Color", if (ImGui::ColorEdit4("##label_color", color)) {
        label.color = glm::vec4(color[0], color[1], color[2], color[3]);
        label.dirty = true;
    });
    INSPECTOR_PROPERTY("Glyph Size", if (ImGui::DragFloat2("##glyph_size", glm::value_ptr(label.glyph_size), 1.0f, 1.0f, 256.0f)) {
        label.dirty = true;
    });
    INSPECTOR_PROPERTY("Spacing", if (ImGui::DragFloat("##spacing", &label.spacing, 0.1f)) {
        label.dirty = true;
    });

    ImGui::Columns(1);
}

void DrawParticleEmitterSection(EditorContext& context) {
    if (!context.registry.all_of<ParticleEmitterComponent>(context.selected_entity)) {
        return;
    }

    auto& emitter = context.registry.get<ParticleEmitterComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_CREATION "  Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Columns(2, "particle_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    INSPECTOR_PROPERTY("Emitting", if (ImGui::Checkbox("##emitting", &emitter.emitting)) { MarkParticleEmitterDirty(emitter); });
    INSPECTOR_PROPERTY("Max Particles", if (ImGui::DragInt("##max_particles", &emitter.max_particles, 1.0f, 1, 5000)) { MarkParticleEmitterDirty(emitter); });
    INSPECTOR_PROPERTY("Emit Rate", if (ImGui::DragFloat("##emit_rate", &emitter.emit_rate, 0.1f, 0.0f, 1000.0f)) { MarkParticleEmitterDirty(emitter); });
    INSPECTOR_PROPERTY("Burst", if (ImGui::Button("Emit 10", ImVec2(-1, 0))) { emitter.pending_burst += 10; });
    INSPECTOR_PROPERTY("Gravity", if (ImGui::DragFloat3("##gravity", glm::value_ptr(emitter.gravity), 0.05f)) { MarkParticleEmitterDirty(emitter); });

    ImGui::Separator();
    ImGui::Text("Randomize Params");
    INSPECTOR_PROPERTY("Enable Random", if (ImGui::Checkbox("##random_params", &emitter.use_random_params)) { MarkParticleEmitterDirty(emitter); });

    if (emitter.use_random_params) {
        INSPECTOR_PROPERTY("Life Time", if (ImGui::DragFloat2("##life_time_range", &emitter.life_time_min, 0.05f, 0.05f, 30.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Size", if (ImGui::DragFloat2("##size_range", &emitter.size_min, 0.05f, 0.01f, 100.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Velocity Min", if (ImGui::DragFloat3("##vel_min", glm::value_ptr(emitter.velocity_min), 0.1f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Velocity Max", if (ImGui::DragFloat3("##vel_max", glm::value_ptr(emitter.velocity_max), 0.1f)) { MarkParticleEmitterDirty(emitter); });
    } else {
        INSPECTOR_PROPERTY("Life Time", if (ImGui::DragFloat("##life_time", &emitter.start_life_time, 0.05f, 0.05f, 30.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Start Size", if (ImGui::DragFloat("##start_size", &emitter.start_size, 0.05f, 0.01f, 100.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Start Color", if (ImGui::ColorEdit4("##start_color", glm::value_ptr(emitter.start_color))) { MarkParticleEmitterDirty(emitter); });
    }

    auto draw_curve_inspector = [&emitter](const char* label_text, ParticleCurve& curve, float min_val, float max_val) {
        INSPECTOR_PROPERTY(label_text, if (ImGui::Checkbox((std::string("##enabled_") + label_text).c_str(), &curve.enabled)) { MarkParticleEmitterDirty(emitter); });
        if (curve.enabled) {
            const char* curve_types[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Custom" };
            int current_type = static_cast<int>(curve.type);
            INSPECTOR_PROPERTY("  Type", if (ImGui::Combo((std::string("##type_") + label_text).c_str(), &current_type, curve_types, IM_ARRAYSIZE(curve_types))) {
                curve.type = static_cast<ParticleCurveType>(current_type);
                MarkParticleEmitterDirty(emitter);
            });
            INSPECTOR_PROPERTY("  Start", if (ImGui::DragFloat((std::string("##start_") + label_text).c_str(), &curve.start_value, 0.05f, min_val, max_val)) { MarkParticleEmitterDirty(emitter); });
            INSPECTOR_PROPERTY("  End", if (ImGui::DragFloat((std::string("##end_") + label_text).c_str(), &curve.end_value, 0.05f, min_val, max_val)) { MarkParticleEmitterDirty(emitter); });
        }
    };

    ImGui::Separator();
    ImGui::Text("Curves over Lifetime");
    draw_curve_inspector("Size Curve", emitter.size_curve, 0.0f, 100.0f);
    draw_curve_inspector("Alpha Curve", emitter.alpha_curve, 0.0f, 1.0f);
    draw_curve_inspector("Speed Curve", emitter.speed_curve, 0.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("Collision");
    const char* collision_modes[] = { "None", "GroundPlane", "Box2D" };
    int collision_mode = static_cast<int>(emitter.collision_mode);
    INSPECTOR_PROPERTY("Mode", if (ImGui::Combo("##collision_mode", &collision_mode, collision_modes, IM_ARRAYSIZE(collision_modes))) {
        emitter.collision_mode = static_cast<ParticleCollisionMode>(collision_mode);
        MarkParticleEmitterDirty(emitter);
    });

    if (emitter.collision_mode != ParticleCollisionMode::None) {
        INSPECTOR_PROPERTY("Bounce", if (ImGui::DragFloat("##collision_bounce", &emitter.collision_bounce, 0.01f, 0.0f, 1.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Friction", if (ImGui::DragFloat("##collision_friction", &emitter.collision_friction, 0.01f, 0.0f, 1.0f)) { MarkParticleEmitterDirty(emitter); });
        INSPECTOR_PROPERTY("Life Loss", if (ImGui::DragFloat("##collision_life_loss", &emitter.collision_life_loss, 0.01f, 0.0f, 1.0f)) { MarkParticleEmitterDirty(emitter); });
        if (emitter.collision_mode == ParticleCollisionMode::GroundPlane) {
            INSPECTOR_PROPERTY("Ground Y", if (ImGui::DragFloat("##ground_y", &emitter.ground_y, 0.05f)) { MarkParticleEmitterDirty(emitter); });
        }
    }

    ImGui::Columns(1);
    ImGui::TextDisabled("Active Particles: %d", static_cast<int>(emitter.particles.size()));
}

// ─── Script ─────────────────────────────────────────────────────────────────

void DrawScriptSection(EditorContext& context) {
    if (!context.registry.all_of<ScriptComponent>(context.selected_entity)) return;
    auto& script = context.registry.get<ScriptComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_FILE "  Script", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "script_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);

    static char path_buf[256] = "";
    static entt::entity s_last_script_entity = entt::null;
    if (s_last_script_entity != context.selected_entity) {
        s_last_script_entity = context.selected_entity;
        std::strncpy(path_buf, script.script_path.c_str(), sizeof(path_buf) - 1);
        path_buf[sizeof(path_buf) - 1] = '\0';
    }
    INSPECTOR_PROPERTY("Script Path", if (ImGui::InputText("##script_path", path_buf, sizeof(path_buf))) {
        script.script_path = path_buf;
    });
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string p(static_cast<const char*>(payload->Data));
            if (p.ends_with(".lua")) {
                script.script_path = p;
                std::strncpy(path_buf, p.c_str(), sizeof(path_buf) - 1);
            }
        }
        ImGui::EndDragDropTarget();
    }
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##script_enabled", &script.enabled));

    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// ─── 2D Physics Colliders ───────────────────────────────────────────────────

void DrawBoxCollider2DSection(EditorContext& context) {
    if (!context.registry.all_of<BoxCollider2DComponent>(context.selected_entity)) return;
    auto& col = context.registry.get<BoxCollider2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SHAPE "  Box Collider 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "boxcol2d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Size", ImGui::DragFloat2("##bc2d_size", glm::value_ptr(col.size), 0.1f, 0.01f));
    INSPECTOR_PROPERTY("Offset", ImGui::DragFloat2("##bc2d_off", glm::value_ptr(col.offset), 0.1f));
    INSPECTOR_PROPERTY("Density", ImGui::DragFloat("##bc2d_dens", &col.density, 0.1f, 0.0f));
    INSPECTOR_PROPERTY("Friction", ImGui::DragFloat("##bc2d_fric", &col.friction, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Restitution", ImGui::DragFloat("##bc2d_rest", &col.restitution, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Is Trigger", ImGui::Checkbox("##bc2d_trigger", &col.is_trigger));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawCircleCollider2DSection(EditorContext& context) {
    if (!context.registry.all_of<CircleCollider2DComponent>(context.selected_entity)) return;
    auto& col = context.registry.get<CircleCollider2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_CIRCLE "  Circle Collider 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "circlecol2d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Radius", ImGui::DragFloat("##cc2d_rad", &col.radius, 0.1f, 0.01f));
    INSPECTOR_PROPERTY("Offset", ImGui::DragFloat2("##cc2d_off", glm::value_ptr(col.offset), 0.1f));
    INSPECTOR_PROPERTY("Density", ImGui::DragFloat("##cc2d_dens", &col.density, 0.1f, 0.0f));
    INSPECTOR_PROPERTY("Friction", ImGui::DragFloat("##cc2d_fric", &col.friction, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Restitution", ImGui::DragFloat("##cc2d_rest", &col.restitution, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Is Trigger", ImGui::Checkbox("##cc2d_trigger", &col.is_trigger));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawPolygonCollider2DSection(EditorContext& context) {
    if (!context.registry.all_of<PolygonCollider2DComponent>(context.selected_entity)) return;
    auto& col = context.registry.get<PolygonCollider2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SHAPE "  Polygon Collider 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "polycol2d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Vertices", ImGui::Text("%d", static_cast<int>(col.vertices.size())));
    for (int i = 0; i < static_cast<int>(col.vertices.size()); ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "V[%d]", i);
        char id[32];
        std::snprintf(id, sizeof(id), "##poly_v%d", i);
        INSPECTOR_PROPERTY(label, ImGui::DragFloat2(id, glm::value_ptr(col.vertices[i]), 0.1f));
    }
    INSPECTOR_PROPERTY("Offset", ImGui::DragFloat2("##poly_off", glm::value_ptr(col.offset), 0.1f));
    INSPECTOR_PROPERTY("Density", ImGui::DragFloat("##poly_dens", &col.density, 0.1f, 0.0f));
    INSPECTOR_PROPERTY("Friction", ImGui::DragFloat("##poly_fric", &col.friction, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Restitution", ImGui::DragFloat("##poly_rest", &col.restitution, 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Is Trigger", ImGui::Checkbox("##poly_trigger", &col.is_trigger));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawJoint2DSection(EditorContext& context) {
    if (!context.registry.all_of<Joint2DComponent>(context.selected_entity)) return;
    auto& joint = context.registry.get<Joint2DComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SHAPE "  Joint 2D", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "joint2d_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    const char* joint_types[] = { "Revolute", "Distance", "Prismatic", "Weld" };
    int type_idx = static_cast<int>(joint.type);
    INSPECTOR_PROPERTY("Type", if (ImGui::Combo("##j2d_type", &type_idx, joint_types, IM_ARRAYSIZE(joint_types))) {
        joint.type = static_cast<Joint2DType>(type_idx);
    });
    INSPECTOR_PROPERTY("Entity A", ImGui::Text("%u", static_cast<uint32_t>(joint.entity_a)));
    INSPECTOR_PROPERTY("Entity B", ImGui::Text("%u", static_cast<uint32_t>(joint.entity_b)));
    INSPECTOR_PROPERTY("Anchor A", ImGui::DragFloat2("##j2d_anc_a", glm::value_ptr(joint.anchor_a), 0.1f));
    INSPECTOR_PROPERTY("Anchor B", ImGui::DragFloat2("##j2d_anc_b", glm::value_ptr(joint.anchor_b), 0.1f));
    INSPECTOR_PROPERTY("Collide Conn.", ImGui::Checkbox("##j2d_collide", &joint.collide_connected));

    if (joint.type == Joint2DType::Revolute) {
        ImGui::Separator();
        INSPECTOR_PROPERTY("Enable Limit", ImGui::Checkbox("##j2d_limit", &joint.enable_limit));
        if (joint.enable_limit) {
            INSPECTOR_PROPERTY("Lower Angle", ImGui::DragFloat("##j2d_la", &joint.lower_angle, 1.0f, -360.0f, 360.0f));
            INSPECTOR_PROPERTY("Upper Angle", ImGui::DragFloat("##j2d_ua", &joint.upper_angle, 1.0f, -360.0f, 360.0f));
        }
        INSPECTOR_PROPERTY("Enable Motor", ImGui::Checkbox("##j2d_motor", &joint.enable_motor));
        if (joint.enable_motor) {
            INSPECTOR_PROPERTY("Motor Speed", ImGui::DragFloat("##j2d_ms", &joint.motor_speed, 1.0f));
            INSPECTOR_PROPERTY("Max Torque", ImGui::DragFloat("##j2d_mt", &joint.max_motor_torque, 1.0f, 0.0f));
        }
    } else if (joint.type == Joint2DType::Distance) {
        ImGui::Separator();
        INSPECTOR_PROPERTY("Min Length", ImGui::DragFloat("##j2d_minl", &joint.min_length, 0.1f, 0.0f));
        INSPECTOR_PROPERTY("Max Length", ImGui::DragFloat("##j2d_maxl", &joint.max_length, 0.1f, 0.0f));
        INSPECTOR_PROPERTY("Stiffness", ImGui::DragFloat("##j2d_stiff", &joint.stiffness, 0.1f, 0.0f));
        INSPECTOR_PROPERTY("Damping", ImGui::DragFloat("##j2d_damp", &joint.damping, 0.1f, 0.0f));
    } else if (joint.type == Joint2DType::Prismatic) {
        ImGui::Separator();
        INSPECTOR_PROPERTY("Axis", ImGui::DragFloat2("##j2d_axis", glm::value_ptr(joint.prismatic_axis), 0.1f));
        INSPECTOR_PROPERTY("Lower Trans.", ImGui::DragFloat("##j2d_lt", &joint.lower_translation, 0.1f));
        INSPECTOR_PROPERTY("Upper Trans.", ImGui::DragFloat("##j2d_ut", &joint.upper_translation, 0.1f));
        INSPECTOR_PROPERTY("Motor Speed", ImGui::DragFloat("##j2d_pms", &joint.prismatic_motor_speed, 1.0f));
        INSPECTOR_PROPERTY("Max Force", ImGui::DragFloat("##j2d_pmf", &joint.max_motor_force, 1.0f, 0.0f));
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// ─── 3D Physics Extended ────────────────────────────────────────────────────

void DrawCapsuleCollider3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::CapsuleCollider3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::CapsuleCollider3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Capsule Collider 3D", *ti,
                         MakeReflectResolver<dse::CapsuleCollider3DComponent>(context));
}

void DrawCharacterController3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::CharacterController3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::CharacterController3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_RUN "  Character Controller 3D", *ti,
                         MakeReflectResolver<dse::CharacterController3DComponent>(context));
}

void DrawMeshCollider3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::MeshCollider3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::MeshCollider3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Mesh Collider 3D", *ti,
                         MakeReflectResolver<dse::MeshCollider3DComponent>(context));
}

void DrawJoint3DSection(EditorContext& context) {
    if (!context.registry.all_of<dse::Joint3DComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::Joint3DComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Joint 3D", *ti,
                         MakeReflectResolver<dse::Joint3DComponent>(context));
}

// ─── Advanced Physics ───────────────────────────────────────────────────────

void DrawRagdollSection(EditorContext& context) {
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    if (!context.registry.all_of<dse::RagdollComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::RagdollComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_RUN "  Ragdoll", *ti,
                         MakeReflectResolver<dse::RagdollComponent>(context));
#endif
}

void DrawSoftBodySection(EditorContext& context) {
    if (!context.registry.all_of<dse::SoftBodyComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::SoftBodyComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Soft Body", *ti,
                         MakeReflectResolver<dse::SoftBodyComponent>(context));
}

void DrawVehicleSection(EditorContext& context) {
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    if (!context.registry.all_of<dse::VehicleComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::VehicleComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_RUN "  Vehicle", *ti,
                         MakeReflectResolver<dse::VehicleComponent>(context));
#endif
}

void DrawRopeSection(EditorContext& context) {
    if (!context.registry.all_of<dse::RopeComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::RopeComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_SHAPE "  Rope", *ti,
                         MakeReflectResolver<dse::RopeComponent>(context));
}

void DrawBuoyancySection(EditorContext& context) {
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    if (!context.registry.all_of<dse::BuoyancyComponent>(context.selected_entity)) return;
    dse::reflect::EnsureCoreReflectionRegistered();
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::BuoyancyComponent>();
    if (!ti) return;
    DrawReflectedSection(context, MDI_ICON_WATER "  Buoyancy", *ti,
                         MakeReflectResolver<dse::BuoyancyComponent>(context));
#endif
}

// ─── Spine ──────────────────────────────────────────────────────────────────

void DrawSpineRendererSection(EditorContext& context) {
    if (!context.registry.all_of<SpineRendererComponent>(context.selected_entity)) return;
    auto& spine = context.registry.get<SpineRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_ANIMATION "  Spine Renderer", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "spine_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Visible", ImGui::Checkbox("##spine_vis", &spine.visible));

    static char skel_buf[256] = "";
    static char atlas_buf[256] = "";
    static char anim_buf[128] = "";
    static entt::entity s_last_spine = entt::null;
    if (s_last_spine != context.selected_entity) {
        s_last_spine = context.selected_entity;
        std::strncpy(skel_buf, spine.skeleton_data_path.c_str(), sizeof(skel_buf) - 1);
        skel_buf[sizeof(skel_buf) - 1] = '\0';
        std::strncpy(atlas_buf, spine.atlas_path.c_str(), sizeof(atlas_buf) - 1);
        atlas_buf[sizeof(atlas_buf) - 1] = '\0';
        std::strncpy(anim_buf, spine.current_animation.c_str(), sizeof(anim_buf) - 1);
        anim_buf[sizeof(anim_buf) - 1] = '\0';
    }
    INSPECTOR_PROPERTY("Skeleton", if (ImGui::InputText("##spine_skel", skel_buf, sizeof(skel_buf))) {
        spine.skeleton_data_path = skel_buf;
    });
    INSPECTOR_PROPERTY("Atlas", if (ImGui::InputText("##spine_atlas", atlas_buf, sizeof(atlas_buf))) {
        spine.atlas_path = atlas_buf;
    });
    INSPECTOR_PROPERTY("Animation", if (ImGui::InputText("##spine_anim", anim_buf, sizeof(anim_buf))) {
        spine.current_animation = anim_buf;
        spine.dirty_animation = true;
    });
    INSPECTOR_PROPERTY("Loop", ImGui::Checkbox("##spine_loop", &spine.loop));
    INSPECTOR_PROPERTY("Time Scale", ImGui::DragFloat("##spine_ts", &spine.time_scale, 0.05f, 0.0f, 10.0f));
    INSPECTOR_PROPERTY("Sort Layer", ImGui::DragInt("##spine_sl", &spine.sorting_layer, 1));
    INSPECTOR_PROPERTY("Order", ImGui::DragInt("##spine_order", &spine.order_in_layer, 1));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// ─── UI Components ──────────────────────────────────────────────────────────

void DrawUIRendererSection(EditorContext& context) {
    if (!context.registry.all_of<UIRendererComponent>(context.selected_entity)) return;
    auto& ui = context.registry.get<UIRendererComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_IMAGE "  UI Renderer", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uirender_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Color", ImGui::ColorEdit4("##ui_color", glm::value_ptr(ui.color)));
    INSPECTOR_PROPERTY("Order", ImGui::DragInt("##ui_order", &ui.order, 1));
    INSPECTOR_PROPERTY("Visible", ImGui::Checkbox("##ui_visible", &ui.visible));
    INSPECTOR_PROPERTY("Interactable", ImGui::Checkbox("##ui_interact", &ui.interactable));
    ImGui::Separator();
    INSPECTOR_PROPERTY("Size", ImGui::DragFloat2("##ui_size", glm::value_ptr(ui.size), 1.0f, 0.0f));
    INSPECTOR_PROPERTY("Pivot", ImGui::DragFloat2("##ui_pivot", glm::value_ptr(ui.pivot), 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Anchor Min", ImGui::DragFloat2("##ui_anc_min", glm::value_ptr(ui.anchor_min), 0.05f, 0.0f, 1.0f));
    INSPECTOR_PROPERTY("Anchor Max", ImGui::DragFloat2("##ui_anc_max", glm::value_ptr(ui.anchor_max), 0.05f, 0.0f, 1.0f));
    ImGui::Separator();
    INSPECTOR_PROPERTY("Hover Scale", ImGui::DragFloat("##ui_hscale", &ui.hover_scale, 0.01f, 0.5f, 2.0f));
    INSPECTOR_PROPERTY("Pressed Scale", ImGui::DragFloat("##ui_pscale", &ui.pressed_scale, 0.01f, 0.5f, 2.0f));
    INSPECTOR_PROPERTY("Nine Slice", ImGui::Checkbox("##ui_9slice", &ui.nine_slice_enabled));
    if (ui.nine_slice_enabled) {
        INSPECTOR_PROPERTY("Border", ImGui::DragFloat4("##ui_9border", glm::value_ptr(ui.nine_slice_border), 0.01f, 0.0f, 0.5f));
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIButtonSection(EditorContext& context) {
    if (!context.registry.all_of<UIButtonComponent>(context.selected_entity)) return;
    auto& btn = context.registry.get<UIButtonComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_BUTTON "  UI Button", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uibtn_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Normal Color", ImGui::ColorEdit4("##btn_normal", glm::value_ptr(btn.normal_color)));
    INSPECTOR_PROPERTY("Hover Color", ImGui::ColorEdit4("##btn_hover", glm::value_ptr(btn.hover_color)));
    INSPECTOR_PROPERTY("Pressed Color", ImGui::ColorEdit4("##btn_pressed", glm::value_ptr(btn.pressed_color)));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIPanelSection(EditorContext& context) {
    if (!context.registry.all_of<UIPanelComponent>(context.selected_entity)) return;
    auto& panel = context.registry.get<UIPanelComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SHAPE "  UI Panel", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uipanel_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Blocks Input", ImGui::Checkbox("##panel_blocks", &panel.blocks_input));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIMaskSection(EditorContext& context) {
    if (!context.registry.all_of<UIMaskComponent>(context.selected_entity)) return;
    auto& mask = context.registry.get<UIMaskComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SHAPE "  UI Mask", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uimask_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Enabled", ImGui::Checkbox("##mask_en", &mask.enabled));
    INSPECTOR_PROPERTY("Size", ImGui::DragFloat2("##mask_size", glm::value_ptr(mask.size), 1.0f));
    INSPECTOR_PROPERTY("Offset", ImGui::DragFloat2("##mask_off", glm::value_ptr(mask.offset), 1.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIRichTextSection(EditorContext& context) {
    if (!context.registry.all_of<UIRichTextComponent>(context.selected_entity)) return;
    auto& rt = context.registry.get<UIRichTextComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_FILE "  UI Rich Text", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uirt_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    static char rt_buf[512] = "";
    static entt::entity s_last_rt_entity = entt::null;
    if (s_last_rt_entity != context.selected_entity) {
        s_last_rt_entity = context.selected_entity;
        std::strncpy(rt_buf, rt.text.c_str(), sizeof(rt_buf) - 1);
        rt_buf[sizeof(rt_buf) - 1] = '\0';
    }
    INSPECTOR_PROPERTY("Text", if (ImGui::InputTextMultiline("##rt_text", rt_buf, sizeof(rt_buf), ImVec2(-1, 60))) {
        rt.text = rt_buf;
    });
    INSPECTOR_PROPERTY("Default Color", ImGui::ColorEdit4("##rt_def_color", glm::value_ptr(rt.default_color)));
    INSPECTOR_PROPERTY("Shadow", ImGui::Checkbox("##rt_shadow", &rt.enable_shadow));
    if (rt.enable_shadow) {
        INSPECTOR_PROPERTY("Shadow Offset", ImGui::DragFloat2("##rt_sh_off", glm::value_ptr(rt.shadow_offset), 0.5f));
        INSPECTOR_PROPERTY("Shadow Color", ImGui::ColorEdit4("##rt_sh_color", glm::value_ptr(rt.shadow_color)));
    }
    INSPECTOR_PROPERTY("Outline", ImGui::Checkbox("##rt_outline", &rt.enable_outline));
    if (rt.enable_outline) {
        INSPECTOR_PROPERTY("Outline Color", ImGui::ColorEdit4("##rt_ol_color", glm::value_ptr(rt.outline_color)));
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIJoystickSection(EditorContext& context) {
    if (!context.registry.all_of<UIJoystickComponent>(context.selected_entity)) return;
    auto& joy = context.registry.get<UIJoystickComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_CIRCLE "  UI Joystick", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uijoy_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Max Radius", ImGui::DragFloat("##joy_rad", &joy.max_radius, 1.0f, 1.0f, 500.0f));
    INSPECTOR_PROPERTY("Follow Pointer", ImGui::Checkbox("##joy_follow", &joy.follow_pointer));
    INSPECTOR_PROPERTY("Reset On Release", ImGui::Checkbox("##joy_reset", &joy.reset_on_release));
    ImGui::Separator();
    INSPECTOR_PROPERTY("Direction", ImGui::Text("(%.2f, %.2f)", joy.direction.x, joy.direction.y));
    INSPECTOR_PROPERTY("Is Dragging", ImGui::Text(joy.is_dragging ? "Yes" : "No"));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

// ─── UI Layout (migrated from DrawUILayoutInspector callback) ───────────────

void DrawUIAnchorSection(EditorContext& context) {
    if (!context.registry.all_of<UIAnchorComponent>(context.selected_entity)) return;
    auto& anchor = context.registry.get<UIAnchorComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_IMAGE "  UI Anchor", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uianchor_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    const char* anchor_types[] = {
        "Center", "TopLeft", "TopCenter", "TopRight",
        "MiddleLeft", "MiddleRight", "BottomLeft", "BottomCenter",
        "BottomRight", "StretchAll", "StretchHorizontal", "StretchVertical"
    };
    int current_anchor = static_cast<int>(anchor.anchor);
    INSPECTOR_PROPERTY("Anchor Preset", if (ImGui::Combo("##ui_anchor_preset", &current_anchor, anchor_types, IM_ARRAYSIZE(anchor_types))) {
        anchor.anchor = current_anchor;
    });
    INSPECTOR_PROPERTY("Offset", ImGui::DragFloat2("##ui_anchor_off", glm::value_ptr(anchor.offset), 1.0f));
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIGridLayoutSection(EditorContext& context) {
    if (!context.registry.all_of<UIGridLayoutComponent>(context.selected_entity)) return;
    auto& grid = context.registry.get<UIGridLayoutComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_SHAPE "  UI Grid Layout", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uigrid_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Columns", ImGui::DragInt("##grid_cols", &grid.columns, 0.1f, 0, 100));
    INSPECTOR_PROPERTY("Rows", ImGui::DragInt("##grid_rows", &grid.rows, 0.1f, 0, 100));
    INSPECTOR_PROPERTY("Cell Size", ImGui::DragFloat2("##grid_cell", glm::value_ptr(grid.cell_size), 1.0f));
    INSPECTOR_PROPERTY("Spacing", ImGui::DragFloat2("##grid_space", glm::value_ptr(grid.spacing), 1.0f));
    const char* align_types[] = {
        "TopLeft", "TopCenter", "TopRight",
        "MiddleLeft", "MiddleCenter", "MiddleRight",
        "BottomLeft", "BottomCenter", "BottomRight"
    };
    int current_align = static_cast<int>(grid.alignment);
    INSPECTOR_PROPERTY("Alignment", if (ImGui::Combo("##grid_align", &current_align, align_types, IM_ARRAYSIZE(align_types))) {
        grid.alignment = current_align;
    });
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUICanvasScalerSection(EditorContext& context) {
    if (!context.registry.all_of<UICanvasScalerComponent>(context.selected_entity)) return;
    auto& scaler = context.registry.get<UICanvasScalerComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_RESIZE "  UI Canvas Scaler", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uiscaler_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    INSPECTOR_PROPERTY("Reference Res", ImGui::DragFloat2("##scaler_ref", glm::value_ptr(scaler.reference_resolution), 1.0f));
    INSPECTOR_PROPERTY("Match W/H", ImGui::Checkbox("##scaler_match", &scaler.match_width_or_height));
    if (scaler.match_width_or_height) {
        INSPECTOR_PROPERTY("Factor", ImGui::SliderFloat("##scaler_factor", &scaler.scale_factor, 0.0f, 1.0f));
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}

void DrawUIAnimationSection(EditorContext& context) {
    if (!context.registry.all_of<UIAnimationComponent>(context.selected_entity)) return;
    auto& anim = context.registry.get<UIAnimationComponent>(context.selected_entity);
    if (!ImGui::CollapsingHeader(MDI_ICON_ANIMATION "  UI Animation", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "uianim_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);
    BeginInspectorReadOnlyScope(context);
    if (anim.playing) {
        INSPECTOR_PROPERTY("Control", if (ImGui::Button("Stop##uianim", ImVec2(-1, 0))) anim.playing = false);
    } else {
        INSPECTOR_PROPERTY("Control", if (ImGui::Button("Play##uianim", ImVec2(-1, 0))) {
            anim.playing = true; anim.elapsed = 0.0f;
            anim.delay_remaining = anim.delay; anim.reverse = false;
        });
    }
    INSPECTOR_PROPERTY("Duration", ImGui::DragFloat("##uia_dur", &anim.duration, 0.01f, 0.0f, 10.0f));
    INSPECTOR_PROPERTY("Delay", ImGui::DragFloat("##uia_delay", &anim.delay, 0.01f, 0.0f, 10.0f));
    INSPECTOR_PROPERTY("Loop", ImGui::Checkbox("##uia_loop", &anim.loop));
    INSPECTOR_PROPERTY("Ping Pong", ImGui::Checkbox("##uia_pp", &anim.ping_pong));
    const char* easing_types[] = { "Linear", "Ease-In", "Ease-Out", "Ease-In-Out" };
    INSPECTOR_PROPERTY("Easing", ImGui::Combo("##uia_ease", &anim.easing, easing_types, IM_ARRAYSIZE(easing_types)));
    ImGui::Separator();
    INSPECTOR_PROPERTY("Anim Position", ImGui::Checkbox("##uia_pos", &anim.animate_position));
    if (anim.animate_position) {
        INSPECTOR_PROPERTY("Target Pos", ImGui::DragFloat2("##uia_tpos", glm::value_ptr(anim.target_position), 1.0f));
    }
    INSPECTOR_PROPERTY("Anim Scale", ImGui::Checkbox("##uia_scl", &anim.animate_scale));
    if (anim.animate_scale) {
        INSPECTOR_PROPERTY("Target Scale", ImGui::DragFloat2("##uia_tscl", glm::value_ptr(anim.target_scale), 0.05f));
    }
    INSPECTOR_PROPERTY("Anim Alpha", ImGui::Checkbox("##uia_alp", &anim.animate_alpha));
    if (anim.animate_alpha) {
        INSPECTOR_PROPERTY("Target Alpha", ImGui::DragFloat("##uia_talp", &anim.target_alpha, 0.05f, 0.0f, 1.0f));
    }
    INSPECTOR_PROPERTY("Anim Color", ImGui::Checkbox("##uia_col", &anim.animate_color));
    if (anim.animate_color) {
        INSPECTOR_PROPERTY("Target Color", ImGui::ColorEdit4("##uia_tcol", glm::value_ptr(anim.target_color)));
    }
    EndInspectorReadOnlyScope(context);
    ImGui::Columns(1);
}
