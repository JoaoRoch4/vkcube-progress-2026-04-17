#!/usr/bin/env python3
"""Smoke tests for the combined cpp-imgui-mcp server."""

from __future__ import annotations

import importlib.util
import os
import shutil
import sys
import tempfile


MODULE_PATH = os.path.join(os.path.dirname(__file__), "mcp_server.py")
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

spec = importlib.util.spec_from_file_location("cpp_imgui_mcp_server", MODULE_PATH)
srv = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(srv)

passed = 0
failed = 0


def ok(label: str, detail: str = "") -> None:
    global passed
    passed += 1
    suffix = f"\n     {detail}" if detail else ""
    print(f"  PASS  {label}{suffix}")


def fail(label: str, reason: str) -> None:
    global failed
    failed += 1
    print(f"  FAIL  {label}  →  {reason}")


def run() -> int:
    roots = srv.list_workspace_roots()
    if "imgui" in roots and "vkcube" in roots:
        ok("list_workspace_roots() exposes both workspaces")
    else:
        fail("list_workspace_roots() exposes both workspaces", roots)

    files = srv.list_workspace_files("imgui", "**/imgui_demo.cpp", 5)
    if "imgui_demo.cpp" in files:
        ok("list_workspace_files() finds imgui_demo.cpp")
    else:
        fail("list_workspace_files() finds imgui_demo.cpp", files)

    compile_dbs = srv.list_compile_databases()
    if "compile_commands.json" in compile_dbs and "vkcube" in compile_dbs:
        ok("list_compile_databases() returns workspace compile databases")
    else:
        fail("list_compile_databases() returns workspace compile databases", compile_dbs)

    compile_cmd = srv.lookup_compile_command("/home/joao/vscode/imgui-1/imgui_demo.cpp")
    if "imgui_demo.cpp" in compile_cmd and ("command" in compile_cmd or "arguments" in compile_cmd):
        ok("lookup_compile_command() resolves imgui_demo.cpp")
    else:
        fail("lookup_compile_command() resolves imgui_demo.cpp", compile_cmd[:200])

    targets = srv.list_known_targets()
    if "example_sdl3_vulkan" in targets and "vkcube" in targets:
        ok("list_known_targets() lists the known binaries")
    else:
        fail("list_known_targets() lists the known binaries", targets)

    build_preview = srv.build_target("vkcube", dry_run=True)
    if "cmake --build" in build_preview:
        ok("build_target(..., dry_run=True) previews the build command")
    else:
        fail("build_target(..., dry_run=True) previews the build command", build_preview)

    run_preview = srv.run_target("vkcube", dry_run=True)
    if "vkcube" in run_preview:
        ok("run_target(..., dry_run=True) previews the binary command")
    else:
        fail("run_target(..., dry_run=True) previews the binary command", run_preview)

    docs = srv.lookup_online_llvm_clang_doc("https://llvm.org/docs/NewPassManager.html")
    if "URL: https://llvm.org/docs/NewPassManager.html" in docs:
        ok("lookup_online_llvm_clang_doc() fetches LLVM docs online")
    else:
        fail("lookup_online_llvm_clang_doc() fetches LLVM docs online", docs[:200])

    doc_search = srv.search_online_llvm_clang_docs("clang-tidy", 5)
    if "clang" in doc_search.lower() or "llvm" in doc_search.lower():
        ok("search_online_llvm_clang_docs() returns online doc matches")
    else:
        fail("search_online_llvm_clang_docs() returns online doc matches", doc_search[:200])

    imrad_status = srv.get_imrad_status()
    if "binary:" in imrad_status and "include:" in imrad_status and "template:" in imrad_status:
        ok("get_imrad_status() reports installed paths")
    else:
        fail("get_imrad_status() reports installed paths", imrad_status[:200])

    imrad_candidates = srv.scan_imrad_project_candidates("ImGui::Begin(")
    if "app/ui" in imrad_candidates or "main.cpp" in imrad_candidates:
        ok("scan_imrad_project_candidates() finds ImGui window candidates")
    else:
        fail("scan_imrad_project_candidates() finds ImGui window candidates", imrad_candidates[:200])

    temp_name = os.path.basename(tempfile.mkdtemp(prefix="imrad-smoke-", dir=os.path.join(REPO_ROOT, "output")))
    try:
        imrad_proto = srv.create_imrad_window_prototype("Smoke Preview", output_dir=f"output/{temp_name}", overwrite=True)
        if "Created ImRAD prototype" in imrad_proto and "smoke_preview.h" in imrad_proto:
            ok("create_imrad_window_prototype() scaffolds a starter pair")
        else:
            fail("create_imrad_window_prototype() scaffolds a starter pair", imrad_proto[:200])

        header_path = os.path.join(REPO_ROOT, "output", temp_name, "smoke_preview.h")
        source_path = os.path.join(REPO_ROOT, "output", temp_name, "smoke_preview.cpp")
        if os.path.exists(header_path) and os.path.exists(source_path):
            ok("create_imrad_window_prototype() writes both files")
        else:
            fail("create_imrad_window_prototype() writes both files", f"{header_path} | {source_path}")

        preview = srv.launch_imrad_preview(header_path, dry_run=True)
        if "Dry run:" in preview and "preload:" in preview and "smoke_preview.h" in preview:
            ok("launch_imrad_preview(..., dry_run=True) prepares a preloaded ImRAD session")
        else:
            fail("launch_imrad_preview(..., dry_run=True) prepares a preloaded ImRAD session", preview[:200])
    finally:
        shutil.rmtree(os.path.join(REPO_ROOT, "output", temp_name), ignore_errors=True)

    sdl_doc_search = srv.search_sdl3_docs("SDL_CreateWindow", 5)
    if "wiki.libsdl.org/SDL3/SDL_CreateWindow" in sdl_doc_search:
        ok("search_sdl3_docs() returns SDL3 doc matches")
    else:
        fail("search_sdl3_docs() returns SDL3 doc matches", sdl_doc_search[:200])

    sdl_doc = srv.lookup_sdl3_doc("SDL_GetError")
    if "URL: https://wiki.libsdl.org/SDL3/SDL_GetError" in sdl_doc:
        ok("lookup_sdl3_doc() resolves SDL3 symbol docs")
    else:
        fail("lookup_sdl3_doc() resolves SDL3 symbol docs", sdl_doc[:200])

    sdl_diag = srv.interpret_sdl3_diagnostic("Error: SDL_CreateWindow(): No available video device")
    if "SDL_CreateWindow" in sdl_diag and "SDL_GetError" in sdl_diag:
        ok("interpret_sdl3_diagnostic() explains SDL3 runtime diagnostics")
    else:
        fail("interpret_sdl3_diagnostic() explains SDL3 runtime diagnostics", sdl_diag[:200])

    sdl_scan = srv.scan_sdl3_project_implementations("SDL_CreateGPUDevice")
    if "sdlgpu3_context.cpp" in sdl_scan and "SDL_CreateGPUDevice" in sdl_scan:
        ok("scan_sdl3_project_implementations() finds SDL3 project implementations")
    else:
        fail("scan_sdl3_project_implementations() finds SDL3 project implementations", sdl_scan[:200])

    vk_doc_search = srv.search_vulkan_docs("swapchain", 5)
    if "docs.vulkan.org" in vk_doc_search and "swapchain" in vk_doc_search.lower():
        ok("search_vulkan_docs() returns Vulkan doc matches")
    else:
        fail("search_vulkan_docs() returns Vulkan doc matches", vk_doc_search[:200])

    vk_doc = srv.lookup_vulkan_doc("VkResult")
    if "URL: https://docs.vulkan.org/refpages/latest/refpages/source/VkResult.html" in vk_doc:
        ok("lookup_vulkan_doc() resolves Vulkan symbol refpages")
    else:
        fail("lookup_vulkan_doc() resolves Vulkan symbol refpages", vk_doc[:200])

    vk_result = srv.lookup_vkresult("VK_ERROR_OUT_OF_DATE_KHR")
    if "VkResult: VK_ERROR_OUT_OF_DATE_KHR" in vk_result:
        ok("lookup_vkresult() explains Vulkan status codes")
    else:
        fail("lookup_vkresult() explains Vulkan status codes", vk_result[:200])

    vk_diag = srv.interpret_vulkan_diagnostic("vkQueuePresentKHR returned VK_ERROR_OUT_OF_DATE_KHR")
    if "VK_ERROR_OUT_OF_DATE_KHR" in vk_diag and "vkQueuePresentKHR" in vk_diag:
        ok("interpret_vulkan_diagnostic() explains Vulkan runtime diagnostics")
    else:
        fail("interpret_vulkan_diagnostic() explains Vulkan runtime diagnostics", vk_diag[:200])

    sessions = srv.list_combined_sessions()
    if "Debug checkpoints" in sessions:
        ok("list_combined_sessions() reports checkpoint state")
    else:
        fail("list_combined_sessions() reports checkpoint state", sessions)

    launch = srv.start_debug_session(stop_at_entry=True)
    if "Debug session launched" in launch and "Failed" not in launch:
        ok("start_debug_session() works through the combined server", launch.splitlines()[0])
    else:
        fail("start_debug_session() works through the combined server", launch)
        return 1

    mapped = srv.map_function_calls(frames=5)
    if "| Level | Frame ID | Function | Source | Line |" in mapped:
        ok("map_function_calls() returns a Markdown function table")
    else:
        fail("map_function_calls() returns a Markdown function table", mapped[:200])

    invalid_call = srv.call_function("not_a_call")
    if "Expected a full function call expression" in invalid_call:
        ok("call_function() validates the expression shape")
    else:
        fail("call_function() validates the expression shape", invalid_call[:200])

    checkpoint = srv.save_debug_checkpoint("smoke")
    if "Saved checkpoint #0 'smoke'" in checkpoint:
        ok("save_debug_checkpoint() captures debugger state")
    else:
        fail("save_debug_checkpoint() captures debugger state", checkpoint[:200])

    checkpoints = srv.list_debug_checkpoints()
    if "smoke" in checkpoints:
        ok("list_debug_checkpoints() lists saved checkpoints")
    else:
        fail("list_debug_checkpoints() lists saved checkpoints", checkpoints)

    stop = srv.stop_debug_session()
    if "stopped" in stop.lower():
        ok("stop_debug_session() closes the combined debugger session")
    else:
        fail("stop_debug_session() closes the combined debugger session", stop)

    print("\n──────────────────────────────────────────────────")
    print(f"  {passed + failed} checks: {passed} passed, {failed} failed")
    return 1 if failed else 0


try:
    raise SystemExit(run())
finally:
    try:
        if getattr(srv.lldb, "_client", None) and srv.lldb._client.is_active:
            srv.stop_debug_session()
    except Exception:
        pass
