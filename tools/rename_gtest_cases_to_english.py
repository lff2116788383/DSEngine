#!/usr/bin/env python3
"""Rename Google Test Chinese case names to English (offline, fast)."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
CACHE_FILE = ROOT / "tools" / "gtest_case_name_en_cache.json"

TEST_MACRO = re.compile(r"(TEST(?:_F|P)?\s*\(\s*([^,]+)\s*,\s*)([^)]+?)(\s*\))")
HAS_CJK = re.compile(r"[\u4e00-\u9fff]")

EXACT: dict[str, str] = {}

PHRASES: list[tuple[str, str]] = [
    ("未知方法返回MethodNotFound", "UnknownMethodReturnsMethodNotFound"),
    ("未知", "Unknown"),
    ("缺code参数返回error", "MissingCodeParameterReturnsError"),
    ("缺path参数返回error", "MissingPathParameterReturnsError"),
    ("缺content参数返回error", "MissingContentParameterReturnsError"),
    ("路径含dotdot被拒绝", "PathWithDotDotIsRejected"),
    ("正常写入返回written", "NormalWriteReturnsWritten"),
    ("空栈返回success_false", "EmptyStackReturnsSuccessFalse"),
    ("执行后可撤销再重做", "UndoThenRedoAfterExecute"),
    ("缺name参数返回error", "MissingNameParameterReturnsError"),
    ("最小参数创建实体", "MinimalParametersCreateEntity"),
    ("带position生成正确Transform", "WithPositionGeneratesCorrectTransform"),
    ("带mesh自动添加MeshRenderer", "WithMeshAutoAddsMeshRenderer"),
    ("带components数组批量添加", "WithComponentsArrayBatchAdd"),
    ("无AssetManager返回error", "WithoutAssetManagerReturnsError"),
    ("未注册Manager不崩溃", "UnregisteredManagerDoesNotCrash"),
    ("Lua加载并查询翻译", "LuaLoadAndQueryTranslation"),
    ("无启动脚本时引导失败", "BootstrapFailsWithoutStartupScript"),
    ("有效脚本时引导成功", "BootstrapSucceedsWithValidScript"),
    ("无World时引导失败", "BootstrapFailsWithoutWorld"),
    ("引导后关闭不崩溃", "ShutdownAfterBootstrapDoesNotCrash"),
    ("帧更新执行Update函数", "FrameUpdateExecutesUpdateFunction"),
    ("未引导时帧更新不崩溃", "FrameUpdateWithoutBootstrapDoesNotCrash"),
    ("禁用的ScriptComponent不执行", "DisabledScriptComponentDoesNotExecute"),
    ("实体销毁后清理脚本实例", "ScriptInstanceCleanedUpAfterEntityDestroy"),
    ("GetLuaMemoryUsage返回非零值", "GetLuaMemoryUsageReturnsNonZero"),
    ("语法错误导致引导失败", "SyntaxErrorCausesBootstrapFailure"),
    ("Awake中运行时错误导致引导失败", "RuntimeErrorInAwakeCausesBootstrapFailure"),
    ("空World调用Update不崩溃", "EmptyWorldUpdateDoesNotCrash"),
    ("无状态时Update不崩溃", "UpdateWithoutStateDoesNotCrash"),
    ("单状态自动初始化current_state", "SingleStateAutoInitializesCurrentState"),
    ("帧推进current_frame递增", "FrameAdvanceIncrementsCurrentFrame"),
    ("循环播放到末帧后回到首帧", "LoopPlaybackWrapsToFirstFrameAtEnd"),
    ("非循环播放到末帧后停止", "NonLoopPlaybackStopsAtLastFrame"),
    ("SetBool触发状态转换", "SetBoolTriggersStateTransition"),
    ("PlaySegment分段播放", "PlaySegmentPlaysSegment"),
    ("帧率控制帧推进速度", "FrameRateControlsAdvanceSpeed"),
    ("停止播放后时间不推进", "TimeDoesNotAdvanceAfterStopPlayback"),
    ("无天气组件不创建发射器", "NoWeatherComponentDoesNotCreateEmitter"),
    ("类型None不创建发射器", "TypeNoneDoesNotCreateEmitter"),
    ("雪天气创建发射器并设置参数", "SnowWeatherCreatesEmitterAndSetsParameters"),
    ("雨天气参数与雪不同", "RainWeatherParametersDifferFromSnow"),
    ("禁用天气后发射率衰减", "EmissionRateDecaysAfterWeatherDisabled"),
    ("1000任务全部完成", "All1000TasksComplete"),
    ("100任务全部完成", "All100TasksComplete"),
    ("10帧连续提交稳定", "TenFramesContinuousSubmitStable"),
    ("50帧连续Tick不泄漏", "FiftyFramesContinuousTickNoLeak"),
    ("Ping_返回pong", "Ping_ReturnsPong"),
    ("AABB在视锥外_左侧", "AABBOutsideFrustum_LeftSide"),
    ("AABB完全在视锥内", "AABBFullyInsideFrustum"),
    ("AABB完全在视锥外_后方", "AABBFullyOutsideFrustum_Back"),
    ("AABB完全在视锥外_远处", "AABBFullyOutsideFrustum_FarAway"),
    ("不崩溃", "DoesNotCrash"),
    ("不泄漏", "NoLeak"),
    ("不执行", "DoesNotExecute"),
    ("不创建发射器", "DoesNotCreateEmitter"),
    ("不创建", "DoesNotCreate"),
    ("不推进", "DoesNotAdvance"),
    ("不加载", "DoesNotLoad"),
    ("不调用", "DoesNotCall"),
    ("不返回", "DoesNotReturn"),
    ("不触发", "DoesNotTrigger"),
    ("不更新", "DoesNotUpdate"),
    ("不渲染", "DoesNotRender"),
    ("不剔除", "DoesNotCull"),
    ("返回error", "ReturnsError"),
    ("返回false", "ReturnsFalse"),
    ("返回true", "ReturnsTrue"),
    ("返回nullptr", "ReturnsNullptr"),
    ("返回pong", "ReturnsPong"),
    ("返回written", "ReturnsWritten"),
    ("返回非零值", "ReturnsNonZeroValue"),
    ("返回非零", "ReturnsNonZero"),
    ("返回success_false", "ReturnsSuccessFalse"),
    ("引导失败", "BootstrapFails"),
    ("引导成功", "BootstrapSucceeds"),
    ("初始化失败", "InitFails"),
    ("初始化成功", "InitSucceeds"),
    ("加载成功", "LoadSucceeds"),
    ("加载失败", "LoadFails"),
    ("创建成功", "CreateSucceeds"),
    ("创建失败", "CreateFails"),
    ("执行成功", "ExecuteSucceeds"),
    ("编译成功", "CompileSucceeds"),
    ("编译失败", "CompileFails"),
    ("默认值", "DefaultValues"),
    ("默认状态", "DefaultState"),
    ("默认参数", "DefaultParameters"),
    ("默认禁用", "DisabledByDefault"),
    ("默认启用", "EnabledByDefault"),
    ("自定义值", "CustomValues"),
    ("字段修改", "FieldModification"),
    ("字段默认值", "FieldDefaultValues"),
    ("状态转换", "StateTransition"),
    ("任务全部完成", "AllTasksComplete"),
    ("全部完成", "AllComplete"),
    ("简单平面", "SimplePlane"),
    ("有效场景", "ValidScene"),
    ("空数据", "EmptyData"),
    ("空World", "EmptyWorld"),
    ("空栈", "EmptyStack"),
    ("空场景", "EmptyScene"),
    ("无效路径", "InvalidPath"),
    ("无效句柄", "InvalidHandle"),
    ("无效参数", "InvalidParameter"),
    ("不存在总线返回false", "AddEffectToMissingBusReturnsFalse"),
    ("未初始化不崩溃", "UninitializedDoesNotCrash"),
    ("施加冲量", "ApplyImpulse"),
    ("创建组件", "CreateComponent"),
    ("创建实体", "CreateEntity"),
    ("创建发射器", "CreateEmitter"),
    ("并设置参数", "AndSetParameters"),
    ("设置参数", "SetParameters"),
    ("发射率衰减", "EmissionRateDecays"),
    ("效果数正确", "EffectCountIsCorrect"),
    ("默认值为一", "DefaultValueIsOne"),
    ("默认值在合法范围", "DefaultValueInValidRange"),
    ("完全在视锥内", "FullyInsideFrustum"),
    ("完全在视锥外", "FullyOutsideFrustum"),
    ("在视锥外", "OutsideFrustum"),
    ("左侧", "LeftSide"),
    ("右侧", "RightSide"),
    ("后方", "Back"),
    ("远处", "FarAway"),
    ("预分配池", "PreallocatedPool"),
    ("从预分配池获取", "AcquireFromPreallocatedPool"),
    ("独立生命周期", "IndependentLifecycle"),
    ("弱引用", "WeakReference"),
    ("语法错误", "SyntaxError"),
    ("运行时错误", "RuntimeError"),
    ("独立", "Independent"),
    ("生命周期", "Lifecycle"),
    ("管理器", "Manager"),
    ("资源", "Asset"),
    ("缓存", "Cache"),
    ("加载并缓存", "LoadAndCache"),
    ("紧凑布局", "TightLayout"),
    ("边界距离", "BoundaryDistance"),
    ("近距离", "NearDistance"),
    ("中距离", "MidDistance"),
    ("远距离", "FarDistance"),
    ("全精度", "FullPrecision"),
    ("半精度", "HalfPrecision"),
    ("低精度", "LowPrecision"),
    ("程序化生成", "ProceduralGeneration"),
    ("数据完整性", "DataIntegrity"),
    ("帧连续提交稳定", "ContinuousFrameSubmitStable"),
    ("帧连续", "ContinuousFrames"),
    ("多帧持续按下状态正确", "MultiFrameHoldStateIsCorrect"),
    ("链接两个Float", "LinksTwoFloatNodes"),
    ("防重复", "PreventsDuplicate"),
    ("叠加不卸载旧场景", "AdditiveDoesNotUnloadOldScene"),
    ("不存在", "DoesNotExist"),
    ("总线返回false", "BusReturnsFalse"),
    ("缺name", "MissingName"),
    ("缺path", "MissingPath"),
    ("缺code", "MissingCode"),
    ("缺content", "MissingContent"),
    ("无效", "Invalid"),
    ("有效", "Valid"),
    ("成功", "Succeeds"),
    ("失败", "Fails"),
    ("拒绝", "Rejects"),
    ("触发", "Triggers"),
    ("创建", "Create"),
    ("删除", "Delete"),
    ("更新", "Update"),
    ("初始化", "Initialize"),
    ("未初始化", "Uninitialized"),
    ("已初始化", "Initialized"),
    ("引导", "Bootstrap"),
    ("关闭", "Shutdown"),
    ("启用", "Enabled"),
    ("禁用", "Disabled"),
    ("销毁", "Destroy"),
    ("写入", "Write"),
    ("读取", "Read"),
    ("查询", "Query"),
    ("加载", "Load"),
    ("保存", "Save"),
    ("撤销", "Undo"),
    ("重做", "Redo"),
    ("执行", "Execute"),
    ("调用", "Calls"),
    ("注入", "Injects"),
    ("发布", "Publishes"),
    ("广播", "Broadcasts"),
    ("响应", "Responds"),
    ("通知", "Notifies"),
    ("清理", "Cleanup"),
    ("参数", "Parameters"),
    ("缺", "Missing"),
    ("无", "Without"),
    ("有", "With"),
    ("未", "Not"),
    ("已", "Already"),
    ("后", "After"),
    ("前", "Before"),
    ("时", "When"),
    ("中", "In"),
    ("内", "Inside"),
    ("外", "Outside"),
    ("为", "Is"),
    ("与", "And"),
    ("和", "And"),
    ("或", "Or"),
    ("的", ""),
    ("了", ""),
    ("将", "Will"),
    ("按", "By"),
    ("从", "From"),
    ("到", "To"),
    ("向", "Toward"),
    ("被", "By"),
    ("可", "Can"),
    ("仍", "Still"),
    ("再", "Again"),
    ("不", "Not"),
    ("非", "Non"),
    ("全", "All"),
    ("半", "Half"),
    ("空", "Empty"),
    ("返回", "Returns"),
    ("正常", "Normal"),
    ("正确", "Correct"),
    ("错误", "Error"),
    ("类型", "Type"),
    ("组件", "Component"),
    ("实体", "Entity"),
    ("场景", "Scene"),
    ("系统", "System"),
    ("任务", "Tasks"),
    ("帧", "Frame"),
    ("连续", "Continuous"),
    ("稳定", "Stable"),
    ("泄漏", "Leak"),
    ("天气", "Weather"),
    ("发射器", "Emitter"),
    ("发射率", "EmissionRate"),
    ("衰减", "Decays"),
    ("雪", "Snow"),
    ("雨", "Rain"),
    ("节点", "Node"),
    ("数组", "Array"),
    ("批量", "Batch"),
    ("自动", "Auto"),
    ("手动", "Manual"),
    ("最小", "Minimal"),
    ("最大", "Max"),
    ("多", "Multi"),
    ("单", "Single"),
    ("两", "Two"),
    ("一", "One"),
    ("零", "Zero"),
    ("次", "Times"),
    ("视锥", "Frustum"),
    ("获取", "Acquire"),
    ("施加", "Apply"),
    ("冲量", "Impulse"),
    ("效果数", "EffectCount"),
    ("叠加", "Additive"),
    ("卸载旧场景", "UnloadOldScene"),
    ("总线", "Bus"),
    ("链接", "Link"),
    ("两个", "Two"),
    ("分段播放", "SegmentPlayback"),
    ("帧推进", "FrameAdvance"),
    ("帧率", "FrameRate"),
    ("停止播放", "StopPlayback"),
    ("播放", "Playback"),
    ("循环播放", "LoopPlayback"),
    ("非循环播放", "NonLoopPlayback"),
    ("末帧", "LastFrame"),
    ("首帧", "FirstFrame"),
    ("状态", "State"),
    ("转换", "Transition"),
    ("递增", "Increments"),
    ("时间", "Time"),
    ("控制", "Control"),
    ("速度", "Speed"),
    ("推进", "Advance"),
]


def load_phrase_table() -> list[tuple[str, str]]:
    table = list(PHRASES)
    if CACHE_FILE.exists():
        cache = json.loads(CACHE_FILE.read_text(encoding="utf-8"))
        for cn, en in cache.items():
            if cn and en and cn not in {x for x, _ in table}:
                table.append((cn, en))
    table.sort(key=lambda item: len(item[0]), reverse=True)
    return table


def split_segment(segment: str) -> list[str]:
    parts: list[str] = []
    buf: list[str] = []
    for ch in segment:
        if "\u4e00" <= ch <= "\u9fff":
            if buf and not HAS_CJK.search("".join(buf)):
                parts.append("".join(buf))
                buf = []
            buf.append(ch)
        else:
            if buf and HAS_CJK.search("".join(buf)):
                parts.append("".join(buf))
                buf = []
            buf.append(ch)
    if buf:
        parts.append("".join(buf))
    return [p for p in parts if p]


def apply_phrases(text: str, table: list[tuple[str, str]]) -> str:
    result = text
    for cn, en in table:
        if cn and cn in result:
            result = result.replace(cn, en)
    return result


def words_to_pascal(text: str) -> str:
    text = re.sub(r"[^A-Za-z0-9]+", " ", text.strip())
    tokens = text.split()
    if not tokens:
        return "Case"
    out: list[str] = []
    for token in tokens:
        if token.isupper() and len(token) <= 6:
            out.append(token)
        elif token[0].isdigit():
            out.append(token)
        else:
            out.append(token[0].upper() + token[1:])
    result = "".join(out)
    if result[0].isdigit():
        result = "Case" + result
    return result


def translate_segment(segment: str, table: list[tuple[str, str]]) -> str:
    if not HAS_CJK.search(segment):
        return segment
    pieces = split_segment(segment)
    translated: list[str] = []
    for piece in pieces:
        if HAS_CJK.search(piece):
            translated.append(apply_phrases(piece, table))
        else:
            translated.append(piece)
    merged = "".join(translated)
    if HAS_CJK.search(merged):
        # Last resort: strip remaining CJK instead of emitting placeholder noise.
        merged = HAS_CJK.sub("", merged)
    return words_to_pascal(merged)


def translate_case_name(original: str, table: list[tuple[str, str]]) -> str:
    if original in EXACT:
        return EXACT[original]
    if not HAS_CJK.search(original):
        return original
    if "_" in original:
        parts = [translate_segment(part, table) for part in original.split("_")]
        parts = [p for p in parts if p]
        return "_".join(parts)
    return translate_segment(original, table)


def process_files(table: list[tuple[str, str]]) -> tuple[int, int, list[tuple[str, str]]]:
    changed_cases = 0
    changed_files = 0
    remaining: list[tuple[str, str]] = []

    for path in sorted(TESTS_DIR.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        file_changed = 0

        def repl(match: re.Match[str]) -> str:
            nonlocal file_changed
            prefix, case, suffix = match.group(1), match.group(3).strip(), match.group(4)
            if not HAS_CJK.search(case):
                return match.group(0)
            new_name = translate_case_name(case, table)
            if HAS_CJK.search(new_name):
                remaining.append((str(path.relative_to(ROOT)), case))
                return match.group(0)
            if new_name != case:
                file_changed += 1
                return f"{prefix}{new_name}{suffix}"
            return match.group(0)

        new_text = TEST_MACRO.sub(repl, text)
        if file_changed:
            path.write_text(new_text, encoding="utf-8", newline="\n")
            changed_cases += file_changed
            changed_files += 1

    return changed_cases, changed_files, remaining


def main() -> int:
    table = load_phrase_table()
    changed_cases, changed_files, remaining = process_files(table)
    print(f"Renamed {changed_cases} cases in {changed_files} files.")
    print(f"Remaining Chinese cases: {len(remaining)}")
    if remaining:
        for path, case in remaining[:30]:
            print(f"  {path}: {case}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
