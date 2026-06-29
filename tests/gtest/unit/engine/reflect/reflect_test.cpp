#include <gtest/gtest.h>

#include <rapidjson/document.h>

#include <array>
#include <string>
#include <vector>

#include "engine/reflect/component_reflection.h"
#include "engine/reflect/reflect.h"
#include "engine/reflect/reflect_json.h"

#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_tree.h"

using namespace dse::reflect;

namespace {

const FieldInfo* FindField(const TypeInfo& ti, const std::string& name) {
    for (const FieldInfo& f : ti.fields) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

// ─── 枚举/嵌套结构测试类型（自包含，不依赖具体组件） ──────────────────────────
enum class TestColorMode { Off = 0, Linear = 1, SRGB = 2 };

struct TestInner {
    float a = 1.0f;
    int   b = 2;
};

struct TestOuter {
    bool          on = true;
    TestColorMode mode = TestColorMode::Linear;
    TestInner     inner;
    glm::vec3     tint{0.0f, 0.0f, 0.0f};
};

// 注册顺序：枚举与嵌套类型须先于引用它们的外层类型注册。
void RegisterTestTypes() {
    DSE_REFLECT_ENUM(TestColorMode)
        .value("Off", TestColorMode::Off)
        .value("Linear", TestColorMode::Linear)
        .value("SRGB", TestColorMode::SRGB);
    {
        auto t = DSE_REFLECT_TYPE(TestInner);
        t.field("a", &TestInner::a);
        t.field("b", &TestInner::b);
    }
    {
        auto t = DSE_REFLECT_TYPE(TestOuter);
        t.field("on", &TestOuter::on);
        t.field("mode", &TestOuter::mode);
        t.field("inner", &TestOuter::inner);
        t.field("tint", &TestOuter::tint).color();
    }
}

}  // namespace

class ReflectionTest : public ::testing::Test {
protected:
    void SetUp() override { EnsureCoreReflectionRegistered(); }
};

// ─── 注册表自省 ───────────────────────────────────────────────────────────────

TEST_F(ReflectionTest, FindsRegisteredTypesByTemplateAndName) {
    const TypeInfo* by_tpl = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(by_tpl, nullptr);
    EXPECT_EQ(by_tpl->name, "GrassComponent");

    const TypeInfo* by_name = Reflection::Find("GrassComponent");
    EXPECT_EQ(by_tpl, by_name);
}

TEST_F(ReflectionTest, UnregisteredTypeReturnsNull) {
    EXPECT_EQ(Reflection::Find<dse::WaterComponent>(), nullptr);
    EXPECT_EQ(Reflection::Find("NoSuchComponent"), nullptr);
}

TEST_F(ReflectionTest, FieldCarriesTypeAndAttributes) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);

    const FieldInfo* density = FindField(*ti, "density");
    ASSERT_NE(density, nullptr);
    EXPECT_EQ(density->type, FieldType::Float);
    EXPECT_TRUE(density->attr.has_range);
    EXPECT_DOUBLE_EQ(density->attr.min_value, 0.0);
    EXPECT_DOUBLE_EQ(density->attr.max_value, 16.0);

    const FieldInfo* seed = FindField(*ti, "seed");
    ASSERT_NE(seed, nullptr);
    EXPECT_EQ(seed->type, FieldType::UInt);

    const FieldInfo* base_color = FindField(*ti, "base_color");
    ASSERT_NE(base_color, nullptr);
    EXPECT_EQ(base_color->type, FieldType::Vec3);

    const FieldInfo* wind_dir = FindField(*ti, "wind_direction");
    ASSERT_NE(wind_dir, nullptr);
    EXPECT_EQ(wind_dir->type, FieldType::Vec2);
}

TEST_F(ReflectionTest, RegistrationIsIdempotent) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);
    const std::size_t count_before = ti->fields.size();

    EnsureCoreReflectionRegistered();  // 再次调用不应翻倍字段
    EXPECT_EQ(ti->fields.size(), count_before);
}

TEST_F(ReflectionTest, FieldResolverReadsAndWritesInstance) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);
    const FieldInfo* density = FindField(*ti, "density");
    ASSERT_NE(density, nullptr);

    dse::GrassComponent g;
    g.density = 3.5f;
    EXPECT_FLOAT_EQ(*static_cast<const float*>(density->cget(&g)), 3.5f);

    *static_cast<float*>(density->get(&g)) = 7.0f;
    EXPECT_FLOAT_EQ(g.density, 7.0f);
}

// ─── 通用 JSON round-trip ─────────────────────────────────────────────────────

TEST_F(ReflectionTest, GrassJsonRoundTrip) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);

    dse::GrassComponent src;
    src.enabled = false;
    src.density = 4.25f;
    src.seed = 9001u;
    src.chunk_size = 12.0f;
    src.base_color = glm::vec3(0.2f, 0.3f, 0.4f);
    src.wind_direction = glm::vec2(0.6f, -0.8f);
    src.cast_shadow = true;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    dse::GrassComponent dst;
    DeserializeReflected(*ti, &dst, obj);

    EXPECT_EQ(dst.enabled, src.enabled);
    EXPECT_FLOAT_EQ(dst.density, src.density);
    EXPECT_EQ(dst.seed, src.seed);
    EXPECT_FLOAT_EQ(dst.chunk_size, src.chunk_size);
    EXPECT_FLOAT_EQ(dst.base_color.r, src.base_color.r);
    EXPECT_FLOAT_EQ(dst.base_color.g, src.base_color.g);
    EXPECT_FLOAT_EQ(dst.base_color.b, src.base_color.b);
    EXPECT_FLOAT_EQ(dst.wind_direction.x, src.wind_direction.x);
    EXPECT_FLOAT_EQ(dst.wind_direction.y, src.wind_direction.y);
    EXPECT_EQ(dst.cast_shadow, src.cast_shadow);
}

TEST_F(ReflectionTest, TreeJsonRoundTripIncludingStrings) {
    const TypeInfo* ti = Reflection::Find<dse::TreeComponent>();
    ASSERT_NE(ti, nullptr);

    dse::TreeComponent src;
    src.enabled = true;
    src.mesh_path = "assets/trees/oak.mesh";
    src.lod1_mesh_path = "assets/trees/oak_lod1.mesh";
    src.density = 0.137f;
    src.seed = 777u;
    src.min_scale = 0.5f;
    src.max_scale = 2.5f;
    src.random_rotation = false;
    src.cull_distance = 321.0f;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    dse::TreeComponent dst;
    DeserializeReflected(*ti, &dst, obj);

    EXPECT_EQ(dst.enabled, src.enabled);
    EXPECT_EQ(dst.mesh_path, src.mesh_path);
    EXPECT_EQ(dst.lod1_mesh_path, src.lod1_mesh_path);
    EXPECT_FLOAT_EQ(dst.density, src.density);
    EXPECT_EQ(dst.seed, src.seed);
    EXPECT_FLOAT_EQ(dst.min_scale, src.min_scale);
    EXPECT_FLOAT_EQ(dst.max_scale, src.max_scale);
    EXPECT_EQ(dst.random_rotation, src.random_rotation);
    EXPECT_FLOAT_EQ(dst.cull_distance, src.cull_distance);
}

TEST_F(ReflectionTest, PostProcessJsonRoundTrip) {
    const TypeInfo* ti = Reflection::Find<dse::PostProcessComponent>();
    ASSERT_NE(ti, nullptr);

    dse::PostProcessComponent src;
    src.bloom_threshold = 2.5f;
    src.ssao_enabled = true;
    src.ssao_sample_count = 64;
    src.exposure = 1.7f;
    src.fog_enabled = true;
    src.fog_color = glm::vec3(0.1f, 0.2f, 0.3f);
    src.fog_steps = 24;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    dse::PostProcessComponent dst;
    DeserializeReflected(*ti, &dst, obj);

    EXPECT_FLOAT_EQ(dst.bloom_threshold, src.bloom_threshold);
    EXPECT_EQ(dst.ssao_enabled, src.ssao_enabled);
    EXPECT_EQ(dst.ssao_sample_count, src.ssao_sample_count);
    EXPECT_FLOAT_EQ(dst.exposure, src.exposure);
    EXPECT_EQ(dst.fog_enabled, src.fog_enabled);
    EXPECT_FLOAT_EQ(dst.fog_color.r, src.fog_color.r);
    EXPECT_EQ(dst.fog_steps, src.fog_steps);
}

// ─── 缺失/异常输入鲁棒性 ───────────────────────────────────────────────────────

TEST_F(ReflectionTest, MissingFieldKeepsDefault) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);

    rapidjson::Document doc;
    doc.SetObject();
    // 只提供 density，其余字段缺失
    doc.AddMember("density", 6.5, doc.GetAllocator());

    dse::GrassComponent dst;  // 默认值
    const float default_chunk = dst.chunk_size;
    DeserializeReflected(*ti, &dst, doc);

    EXPECT_FLOAT_EQ(dst.density, 6.5f);
    EXPECT_FLOAT_EQ(dst.chunk_size, default_chunk);  // 保持默认
}

TEST_F(ReflectionTest, WrongTypeFieldIsIgnored) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);

    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("density", "not a number", doc.GetAllocator());

    dse::GrassComponent dst;
    const float default_density = dst.density;
    DeserializeReflected(*ti, &dst, doc);

    EXPECT_FLOAT_EQ(dst.density, default_density);  // 类型不符被忽略
}

TEST_F(ReflectionTest, DeserializeIgnoresNonObject) {
    const TypeInfo* ti = Reflection::Find<dse::GrassComponent>();
    ASSERT_NE(ti, nullptr);

    rapidjson::Document doc;
    doc.SetArray();

    dse::GrassComponent dst;
    const float default_density = dst.density;
    DeserializeReflected(*ti, &dst, doc);  // 非 object，应安全无操作
    EXPECT_FLOAT_EQ(dst.density, default_density);
}

// ─── 扩展注册组件 round-trip ──────────────────────────────────────────────────

TEST_F(ReflectionTest, MeshRendererEnumFieldRoundTrips) {
    const TypeInfo* ti = Reflection::Find<dse::MeshRendererComponent>();
    ASSERT_NE(ti, nullptr);

    const FieldInfo* src_field = FindField(*ti, "material_data_source");
    ASSERT_NE(src_field, nullptr);
    EXPECT_EQ(src_field->type, FieldType::Enum);
    ASSERT_NE(src_field->enum_info, nullptr);
    EXPECT_EQ(src_field->enum_info->entries.size(), 2u);

    dse::MeshRendererComponent src;
    src.mesh_path = "assets/box.dmesh";
    src.metallic = 0.8f;
    src.roughness = 0.3f;
    src.color = glm::vec4(0.5f, 0.6f, 0.7f, 1.0f);
    src.material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    ASSERT_TRUE(obj.HasMember("material_data_source"));
    ASSERT_TRUE(obj["material_data_source"].IsString());
    EXPECT_STREQ(obj["material_data_source"].GetString(), "MaterialInstance");

    dse::MeshRendererComponent dst;
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_EQ(dst.mesh_path, src.mesh_path);
    EXPECT_FLOAT_EQ(dst.metallic, src.metallic);
    EXPECT_FLOAT_EQ(dst.roughness, src.roughness);
    EXPECT_FLOAT_EQ(dst.color.r, src.color.r);
    EXPECT_EQ(dst.material_data_source,
              dse::MeshRendererComponent::MaterialDataSource::MaterialInstance);
}

TEST_F(ReflectionTest, SpotLightRoundTrips) {
    const TypeInfo* ti = Reflection::Find<dse::SpotLightComponent>();
    ASSERT_NE(ti, nullptr);

    dse::SpotLightComponent src;
    src.enabled = false;
    src.color = glm::vec3(0.1f, 0.2f, 0.9f);
    src.direction = glm::vec3(0.0f, -0.7f, 0.7f);
    src.intensity = 3.5f;
    src.inner_cone_angle = 10.0f;
    src.outer_cone_angle = 25.0f;
    src.cast_shadow = true;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    dse::SpotLightComponent dst;
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_EQ(dst.enabled, src.enabled);
    EXPECT_FLOAT_EQ(dst.color.b, src.color.b);
    EXPECT_FLOAT_EQ(dst.direction.y, src.direction.y);
    EXPECT_FLOAT_EQ(dst.intensity, src.intensity);
    EXPECT_FLOAT_EQ(dst.inner_cone_angle, src.inner_cone_angle);
    EXPECT_FLOAT_EQ(dst.outer_cone_angle, src.outer_cone_angle);
    EXPECT_EQ(dst.cast_shadow, src.cast_shadow);
}

// ─── 枚举支持 ─────────────────────────────────────────────────────────────────

class ReflectionEnumStructTest : public ::testing::Test {
protected:
    void SetUp() override { RegisterTestTypes(); }
};

TEST_F(ReflectionEnumStructTest, EnumFieldCarriesEnumInfo) {
    const TypeInfo* ti = Reflection::Find<TestOuter>();
    ASSERT_NE(ti, nullptr);
    const FieldInfo* mode = FindField(*ti, "mode");
    ASSERT_NE(mode, nullptr);
    EXPECT_EQ(mode->type, FieldType::Enum);
    ASSERT_NE(mode->enum_info, nullptr);
    ASSERT_EQ(mode->enum_info->entries.size(), 3u);
    EXPECT_EQ(mode->enum_info->entries[0].name, "Off");
    EXPECT_EQ(mode->enum_info->entries[2].name, "SRGB");
    EXPECT_EQ(mode->enum_info->entries[2].value, 2);

    const EnumInfo* by_tpl = Reflection::FindEnum<TestColorMode>();
    EXPECT_EQ(by_tpl, mode->enum_info);
}

TEST_F(ReflectionEnumStructTest, EnumSerializesAsNameAndRoundTrips) {
    const TypeInfo* ti = Reflection::Find<TestOuter>();
    ASSERT_NE(ti, nullptr);

    TestOuter src;
    src.mode = TestColorMode::SRGB;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    ASSERT_TRUE(obj.HasMember("mode"));
    ASSERT_TRUE(obj["mode"].IsString());           // 写出枚举名而非整数
    EXPECT_STREQ(obj["mode"].GetString(), "SRGB");

    TestOuter dst;  // 默认 Linear
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_EQ(dst.mode, TestColorMode::SRGB);
}

TEST_F(ReflectionEnumStructTest, EnumDeserializesFromInteger) {
    const TypeInfo* ti = Reflection::Find<TestOuter>();
    ASSERT_NE(ti, nullptr);

    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("mode", 2, doc.GetAllocator());   // 整数回退路径

    TestOuter dst;
    DeserializeReflected(*ti, &dst, doc);
    EXPECT_EQ(dst.mode, TestColorMode::SRGB);
}

TEST_F(ReflectionEnumStructTest, UnknownEnumNameKeepsDefault) {
    const TypeInfo* ti = Reflection::Find<TestOuter>();
    ASSERT_NE(ti, nullptr);

    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("mode", "Bogus", doc.GetAllocator());

    TestOuter dst;  // 默认 Linear
    DeserializeReflected(*ti, &dst, doc);
    EXPECT_EQ(dst.mode, TestColorMode::Linear);
}

// ─── 嵌套结构支持 ─────────────────────────────────────────────────────────────

TEST_F(ReflectionEnumStructTest, NestedStructFieldCarriesStructInfo) {
    const TypeInfo* ti = Reflection::Find<TestOuter>();
    ASSERT_NE(ti, nullptr);
    const FieldInfo* inner = FindField(*ti, "inner");
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->type, FieldType::Struct);
    ASSERT_NE(inner->struct_info, nullptr);
    EXPECT_EQ(inner->struct_info->name, "TestInner");
    EXPECT_EQ(inner->struct_info->fields.size(), 2u);
}

TEST_F(ReflectionEnumStructTest, NestedStructSerializesAsObjectAndRoundTrips) {
    const TypeInfo* ti = Reflection::Find<TestOuter>();
    ASSERT_NE(ti, nullptr);

    TestOuter src;
    src.inner.a = 4.5f;
    src.inner.b = 17;
    src.mode = TestColorMode::Off;

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    ASSERT_TRUE(obj.HasMember("inner"));
    ASSERT_TRUE(obj["inner"].IsObject());           // 嵌套写出为对象
    ASSERT_TRUE(obj["inner"].HasMember("a"));
    EXPECT_FLOAT_EQ(obj["inner"]["a"].GetFloat(), 4.5f);

    TestOuter dst;
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_FLOAT_EQ(dst.inner.a, 4.5f);
    EXPECT_EQ(dst.inner.b, 17);
    EXPECT_EQ(dst.mode, TestColorMode::Off);
}

// ─── 容器（std::vector / C 数组 / std::array） ────────────────────────────────

namespace {

struct TestVecItem {
    float x = 0.0f;
    int   y = 0;
};

struct TestContainers {
    std::vector<int>           ints;
    std::vector<float>         floats;
    std::vector<std::string>   names;
    std::vector<glm::vec3>     points;
    std::vector<TestColorMode> modes;
    std::vector<TestVecItem>   items;
    float                      carr[3] = {1.0f, 2.0f, 3.0f};
    std::array<int, 4>         sarr = {0, 0, 0, 0};
    float                      clamped = 0.0f;
};

void RegisterContainerTypes() {
    RegisterTestTypes();  // 注册 TestColorMode 枚举（幂等）
    {
        auto t = DSE_REFLECT_TYPE(TestVecItem);
        t.field("x", &TestVecItem::x);
        t.field("y", &TestVecItem::y);
    }
    {
        auto t = DSE_REFLECT_TYPE(TestContainers);
        t.field("ints", &TestContainers::ints);
        t.field("floats", &TestContainers::floats);
        t.field("names", &TestContainers::names);
        t.field("points", &TestContainers::points);
        t.field("modes", &TestContainers::modes);
        t.field("items", &TestContainers::items);
        t.field("carr", &TestContainers::carr);
        t.field("sarr", &TestContainers::sarr);
        t.field("clamped", &TestContainers::clamped).range(0.0, 1.0);
    }
}

}  // namespace

class ReflectionContainerTest : public ::testing::Test {
protected:
    void SetUp() override { RegisterContainerTypes(); }
};

TEST_F(ReflectionContainerTest, ContainerFieldsCarryElementMetadata) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    const FieldInfo* ints = FindField(*ti, "ints");
    ASSERT_NE(ints, nullptr);
    EXPECT_EQ(ints->type, FieldType::Array);
    EXPECT_FALSE(ints->container_fixed);
    EXPECT_EQ(ints->element_type, FieldType::Int);

    const FieldInfo* names = FindField(*ti, "names");
    ASSERT_NE(names, nullptr);
    EXPECT_EQ(names->element_type, FieldType::String);

    const FieldInfo* modes = FindField(*ti, "modes");
    ASSERT_NE(modes, nullptr);
    EXPECT_EQ(modes->element_type, FieldType::Enum);
    ASSERT_NE(modes->element_enum_info, nullptr);

    const FieldInfo* items = FindField(*ti, "items");
    ASSERT_NE(items, nullptr);
    EXPECT_EQ(items->element_type, FieldType::Struct);
    ASSERT_NE(items->element_struct_info, nullptr);
    EXPECT_EQ(items->element_struct_info->name, "TestVecItem");

    const FieldInfo* carr = FindField(*ti, "carr");
    ASSERT_NE(carr, nullptr);
    EXPECT_EQ(carr->type, FieldType::Array);
    EXPECT_TRUE(carr->container_fixed);
    EXPECT_EQ(carr->element_type, FieldType::Float);
    EXPECT_EQ(carr->container_size(nullptr), 3u);  // 固定数组容量与实例无关
}

TEST_F(ReflectionContainerTest, ScalarVectorRoundTrip) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;
    src.ints = {1, 2, 3, 4};
    src.floats = {0.5f, 1.5f, 2.5f};
    src.names = {"alpha", "beta"};

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    ASSERT_TRUE(obj.HasMember("ints"));
    ASSERT_TRUE(obj["ints"].IsArray());
    EXPECT_EQ(obj["ints"].Size(), 4u);

    TestContainers dst;
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_EQ(dst.ints, src.ints);
    EXPECT_EQ(dst.floats, src.floats);
    EXPECT_EQ(dst.names, src.names);
}

TEST_F(ReflectionContainerTest, VectorOfVec3RoundTrip) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;
    src.points = {{1.0f, 2.0f, 3.0f}, {-4.0f, 5.0f, -6.0f}};

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    TestContainers dst;
    DeserializeReflected(*ti, &dst, obj);
    ASSERT_EQ(dst.points.size(), 2u);
    EXPECT_FLOAT_EQ(dst.points[1].x, -4.0f);
    EXPECT_FLOAT_EQ(dst.points[1].z, -6.0f);
}

TEST_F(ReflectionContainerTest, VectorOfEnumSerializesByName) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;
    src.modes = {TestColorMode::Off, TestColorMode::SRGB};

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    ASSERT_TRUE(obj["modes"].IsArray());
    ASSERT_TRUE(obj["modes"][1].IsString());
    EXPECT_STREQ(obj["modes"][1].GetString(), "SRGB");

    TestContainers dst;
    DeserializeReflected(*ti, &dst, obj);
    ASSERT_EQ(dst.modes.size(), 2u);
    EXPECT_EQ(dst.modes[0], TestColorMode::Off);
    EXPECT_EQ(dst.modes[1], TestColorMode::SRGB);
}

TEST_F(ReflectionContainerTest, VectorOfStructRoundTrip) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;
    src.items = {{1.5f, 10}, {2.5f, 20}, {3.5f, 30}};

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    ASSERT_TRUE(obj["items"].IsArray());
    ASSERT_TRUE(obj["items"][0].IsObject());
    EXPECT_FLOAT_EQ(obj["items"][0]["x"].GetFloat(), 1.5f);

    TestContainers dst;
    DeserializeReflected(*ti, &dst, obj);
    ASSERT_EQ(dst.items.size(), 3u);
    EXPECT_FLOAT_EQ(dst.items[2].x, 3.5f);
    EXPECT_EQ(dst.items[2].y, 30);
}

TEST_F(ReflectionContainerTest, FixedCArrayRoundTripAndTruncation) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;
    src.carr[0] = 10.0f; src.carr[1] = 20.0f; src.carr[2] = 30.0f;

    const FieldInfo* carr = FindField(*ti, "carr");
    ASSERT_NE(carr, nullptr);
    EXPECT_EQ(carr->container_size(carr->cget(&src)), 3u);

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());
    ASSERT_TRUE(obj["carr"].IsArray());
    EXPECT_EQ(obj["carr"].Size(), 3u);

    TestContainers dst;
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_FLOAT_EQ(dst.carr[0], 10.0f);
    EXPECT_FLOAT_EQ(dst.carr[2], 30.0f);

    // 固定数组：输入元素过多时只写满容量，不越界。
    rapidjson::Document over;
    over.SetObject();
    rapidjson::Value arr(rapidjson::kArrayType);
    for (int i = 0; i < 8; ++i) arr.PushBack(static_cast<float>(i), over.GetAllocator());
    over.AddMember("carr", arr, over.GetAllocator());
    TestContainers dst2;
    DeserializeReflected(*ti, &dst2, over);
    EXPECT_FLOAT_EQ(dst2.carr[0], 0.0f);
    EXPECT_FLOAT_EQ(dst2.carr[2], 2.0f);  // 第 3 个之后被忽略
}

TEST_F(ReflectionContainerTest, StdArrayRoundTrip) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;
    src.sarr = {5, 6, 7, 8};

    const FieldInfo* sarr = FindField(*ti, "sarr");
    ASSERT_NE(sarr, nullptr);
    EXPECT_TRUE(sarr->container_fixed);
    EXPECT_EQ(sarr->container_size(sarr->cget(&src)), 4u);

    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());

    TestContainers dst;
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_EQ(dst.sarr[0], 5);
    EXPECT_EQ(dst.sarr[3], 8);
}

TEST_F(ReflectionContainerTest, EmptyVectorRoundTrips) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    TestContainers src;  // 所有 vector 为空
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Value obj(rapidjson::kObjectType);
    SerializeReflected(*ti, &src, obj, doc.GetAllocator());
    ASSERT_TRUE(obj["ints"].IsArray());
    EXPECT_EQ(obj["ints"].Size(), 0u);

    TestContainers dst;
    dst.ints = {99};  // 应被清空
    DeserializeReflected(*ti, &dst, obj);
    EXPECT_TRUE(dst.ints.empty());
}

// ─── 反序列化数值校验（按 range 钳制） ─────────────────────────────────────────

TEST_F(ReflectionContainerTest, OutOfRangeValueIsClamped) {
    const TypeInfo* ti = Reflection::Find<TestContainers>();
    ASSERT_NE(ti, nullptr);

    rapidjson::Document hi;
    hi.SetObject();
    hi.AddMember("clamped", 5.0, hi.GetAllocator());  // range [0,1]
    TestContainers dst_hi;
    DeserializeReflected(*ti, &dst_hi, hi);
    EXPECT_FLOAT_EQ(dst_hi.clamped, 1.0f);

    rapidjson::Document lo;
    lo.SetObject();
    lo.AddMember("clamped", -3.0, lo.GetAllocator());
    TestContainers dst_lo;
    DeserializeReflected(*ti, &dst_lo, lo);
    EXPECT_FLOAT_EQ(dst_lo.clamped, 0.0f);
}
