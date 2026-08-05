#pragma once

/**
 * @file editor_blueprint_vm.h
 * @brief Blueprint VM（编辑器侧）— 引擎 VM 的别名转发头。
 *
 * 自 2026-08 起步进调试（StepState/BeginStep/StepOnce/CurrentNode）已下沉到
 * 引擎侧 dse::bp::BlueprintVM，编辑器与运行时共用同一解释器，避免双份实现
 * 漂移（此前编辑器 VM 与引擎 VM 各 ~420 行近重复代码，且 BpVariable 已发生
 * 字段漂移）。编辑器代码经本头以 dse::editor::bp 命名空间访问引擎 VM。
 */

#include "engine/scripting/blueprint/blueprint_vm.h"

namespace dse::editor::bp {

using OpCode = dse::bp::OpCode;
using Instruction = dse::bp::Instruction;
using BpValue = dse::bp::BpValue;
using CompiledFunction = dse::bp::CompiledFunction;
using CompiledBlueprint = dse::bp::CompiledBlueprint;
using BlueprintInstance = dse::bp::BlueprintInstance;
using ExternFn = dse::bp::ExternFn;
using VmContext = dse::bp::VmContext;
using StepState = dse::bp::StepState;
using BlueprintVM = dse::bp::BlueprintVM;

}  // namespace dse::editor::bp
