/**
 * @file dse_pch.h
 * @brief DSEngine 预编译头文件 — 包含常用 STL / 第三方库头文件以加速编译。
 *
 * 仅供 CMake target_precompile_headers 使用，不要手动 #include 此文件。
 */

#ifndef DSE_PCH_H
#define DSE_PCH_H

// ---- C 标准库 ----
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- STL 容器 ----
#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// ---- STL IO / 字符串 ----
#include <fstream>
#include <sstream>
#include <iostream>

// ---- STL 其他 ----
#include <chrono>
#include <limits>
#include <mutex>
#include <numeric>
#include <thread>
#include <type_traits>
#include <variant>

// ---- GLM (几乎所有 TU 都用到) ----
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// ---- EnTT (ECS 无处不在) ----
#include <entt/entt.hpp>

#endif // DSE_PCH_H
