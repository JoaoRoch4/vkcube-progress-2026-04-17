#!/usr/bin/env python3
"""
Combined C++ / ImGui / Vulkan / SDL3 / ImRAD / LLDB MCP server.

Merges the existing imgui-cpp-expert and lldb-dap MCP capabilities while adding:
  - workspace file search/read helpers
  - build/run helpers for known targets
  - compile_commands inspection helpers
  - LLVM/Clang docs lookup helpers
  - Vulkan docs lookup + diagnostic interpretation helpers
  - SDL3 docs lookup + diagnostic interpretation helpers
  - SDL3 project implementation scanning helpers
  - ImRAD install / prototype / launch helpers
  - session history and debug checkpoint helpers
"""

from __future__ import annotations

import asyncio
import glob as _glob
import html as _html
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Any
from urllib.parse import urljoin
from urllib.request import Request, urlopen

from mcp.server.fastmcp import FastMCP


THIS_DIR = Path(__file__).resolve().parent
# Repo root is two levels up from tools/cpp-imgui-mcp/
VKCUBE_PROGRESS_ROOT = THIS_DIR.parent.parent.resolve()
# External workspace roots — used when those repos are checked out locally.
# They may not exist in CI / cloud-agent environments; their workspace entries
# will simply produce empty results in that case.
IMGUI_ROOT = Path(os.environ.get("IMGUI_ROOT", "/home/joao/vscode/imgui-1")).resolve()
VKCUBE_ROOT = Path(os.environ.get("VKCUBE_ROOT", "/home/joao/vscode/vkcube")).resolve()
# imgui-cpp-expert server lives in agents/ inside this repo; fall back to the
# external imgui-1 repo path for local development setups that keep it there.
_EXPERT_IN_REPO = VKCUBE_PROGRESS_ROOT / "agents" / "imgui-cpp-expert" / "mcp_server.py"
_EXPERT_EXTERNAL = IMGUI_ROOT / "agents" / "imgui-cpp-expert" / "mcp_server.py"
EXPERT_SERVER_PATH = _EXPERT_IN_REPO if _EXPERT_IN_REPO.exists() else _EXPERT_EXTERNAL
# lldb DAP server lives in .vscode/ of this repo; fall back to the vkcube repo.
_LLDB_IN_REPO = VKCUBE_PROGRESS_ROOT / ".vscode" / "lldb_dap_mcp.py"
_LLDB_EXTERNAL = VKCUBE_ROOT / ".vscode" / "lldb_dap_mcp.py"
LLDB_SERVER_PATH = _LLDB_IN_REPO if _LLDB_IN_REPO.exists() else _LLDB_EXTERNAL
RG_BIN = subprocess.run(["bash", "-lc", "command -v rg || true"], capture_output=True, text=True).stdout.strip()
BUILD_TIMEOUT = 600
RUN_TIMEOUT = 15.0


def _load_module(module_name: str, path: Path):
    spec = importlib.util.spec_from_file_location(module_name, str(path))
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load module {module_name} from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


expert = _load_module("cpp_imgui_expert_server", EXPERT_SERVER_PATH)
lldb = _load_module("cpp_imgui_lldb_server", LLDB_SERVER_PATH)


COMBINED_INSTRUCTIONS = (
    expert.SYSTEM_PROMPT
    + "\n\n## Combined MCP Extensions\n"
      "- You also expose live LLDB DAP debugger tools for vkcube and arbitrary binaries.\n"
      "- You provide workspace-scoped file search/read helpers, compile_commands introspection,\n"
      "  build/run helpers for known binaries, LLVM/Clang, Vulkan, SDL3, and ImRAD helpers,\n"
      "  Vulkan/SDL3 diagnostic interpretation, SDL3 and ImRAD project scanning,\n"
      "  and debug checkpoints.\n"
      "- Prefer safe workspace-scoped operations over arbitrary shell execution.\n"
)

mcp = FastMCP(
    name="cpp-imgui-mcp",
    instructions=COMBINED_INSTRUCTIONS,
)


WORKSPACES: dict[str, Path] = {
    "imgui": IMGUI_ROOT,
    "vkcube": VKCUBE_ROOT,
    "vkcube-progress": VKCUBE_PROGRESS_ROOT,
}

LLVM_DOC_INDEXES = {
    "llvm": "https://llvm.org/docs/",
    "clang": "https://clang.llvm.org/docs/",
    "mlir": "https://mlir.llvm.org/docs/",
}

IMRAD_GITHUB_URL = "https://github.com/tpecholt/imrad"
IMRAD_VERSION = "0.10-WIP"
IMRAD_INSTALL_ROOT = Path("/opt/imrad/latest")
IMRAD_BINARY_CANDIDATES = [
    Path("/usr/local/bin/imrad"),
    IMRAD_INSTALL_ROOT / "imrad",
]
IMRAD_INCLUDE_CANDIDATES = [
    IMRAD_INSTALL_ROOT / "include" / "imrad.h",
]
IMRAD_TEMPLATE_CANDIDATES = [
    IMRAD_INSTALL_ROOT / "template" / "glfw" / "main.cpp",
]
IMRAD_LAUNCH_ROOT = Path("/tmp/cpp-imgui-mcp-imrad")

SDL_DOC_COMPONENTS: dict[str, dict[str, str]] = {
    "frontpage": {
        "base": "https://wiki.libsdl.org/SDL3/",
        "index": "https://wiki.libsdl.org/SDL3/FrontPage",
    },
    "api": {
        "base": "https://wiki.libsdl.org/SDL3/",
        "index": "https://wiki.libsdl.org/SDL3/CategoryAPI",
    },
    "categories": {
        "base": "https://wiki.libsdl.org/SDL3/",
        "index": "https://wiki.libsdl.org/SDL3/APIByCategory",
    },
    "tutorials": {
        "base": "https://wiki.libsdl.org/SDL3/",
        "index": "https://wiki.libsdl.org/SDL3/Tutorials",
    },
}

VULKAN_DOC_COMPONENTS: dict[str, dict[str, str]] = {
    "spec": {
        "base": "https://docs.vulkan.org/spec/latest/",
        "index": "https://docs.vulkan.org/spec/latest/index.html",
    },
    "guide": {
        "base": "https://docs.vulkan.org/guide/latest/",
        "index": "https://docs.vulkan.org/guide/latest/index.html",
    },
    "refpages": {
        "base": "https://docs.vulkan.org/refpages/latest/refpages/",
        "index": "https://docs.vulkan.org/refpages/latest/refpages/index.html",
    },
    "tutorial": {
        "base": "https://docs.vulkan.org/tutorial/latest/",
        "index": "https://docs.vulkan.org/tutorial/latest/index.html",
    },
}

KNOWN_TARGETS: dict[str, dict[str, Any]] = {
    "example_sdl3_vulkan": {
        "workspace": "imgui",
        "binary": str(IMGUI_ROOT / "examples" / "example_sdl3_vulkan" / "example_sdl3_vulkan"),
        "cwd": str(IMGUI_ROOT / "examples" / "example_sdl3_vulkan"),
        "build": ["cmake", "--build", str(IMGUI_ROOT / "examples" / "example_sdl3_vulkan" / "_cmake_build")],
        "notes": "ImGui SDL3 + Vulkan example binary",
    },
    "vkcube": {
        "workspace": "vkcube",
        "binary": lldb.DEFAULT_PROGRAM,
        "cwd": lldb.DEFAULT_CWD,
        "build": ["cmake", "--build", str(VKCUBE_ROOT / "cmake-build")],
        "notes": "Vulkan cube sample used by the lldb-dap server",
    },
    "vkcube-progress": {
        "workspace": "vkcube-progress",
        "binary": str(VKCUBE_PROGRESS_ROOT / "output" / "vkcube"),
        "cwd": str(VKCUBE_PROGRESS_ROOT),
        "build": ["cmake", "--build", str(VKCUBE_PROGRESS_ROOT / "cmake-build")],
        "notes": "Vulkan cube sample with progress tracking (2026-04-17)",
    },
}

_doc_index_cache: dict[str, list[dict[str, str]]] = {}
_doc_page_cache: dict[str, str] = {}
_doc_raw_cache: dict[str, str] = {}
_sdl_doc_index_cache: dict[str, list[dict[str, str]]] = {}
_vulkan_doc_index_cache: dict[str, list[dict[str, str]]] = {}
_debug_checkpoints: list[dict[str, Any]] = []

_VULKAN_RESULT_HINTS = {
    "VK_SUCCESS": "The command completed successfully.",
    "VK_NOT_READY": "The operation has not completed yet. Wait or poll again when the API expects it.",
    "VK_TIMEOUT": "A wait operation timed out before the requested condition became true.",
    "VK_INCOMPLETE": "The output array was too small; query the required count and retry.",
    "VK_SUBOPTIMAL_KHR": "Presentation can continue, but the swapchain no longer matches the surface ideally. Recreate it soon.",
    "VK_ERROR_OUT_OF_DATE_KHR": "The swapchain is no longer compatible with the window surface. Recreate the swapchain and dependent framebuffers.",
    "VK_ERROR_DEVICE_LOST": "The GPU device became unusable. Inspect synchronization, resource lifetime, and recent validation output.",
    "VK_ERROR_INITIALIZATION_FAILED": "Initialization failed. Re-check instance/device creation, surface support, and required extensions.",
    "VK_ERROR_LAYER_NOT_PRESENT": "A requested validation or API layer is not installed on this system.",
    "VK_ERROR_EXTENSION_NOT_PRESENT": "A requested instance or device extension is unavailable.",
    "VK_ERROR_FEATURE_NOT_PRESENT": "The selected physical device does not support a requested feature.",
    "VK_ERROR_SURFACE_LOST_KHR": "The surface is no longer valid and must be recreated.",
    "VK_ERROR_VALIDATION_FAILED": "Validation caught incorrect API usage. Fix the reported misuse before continuing.",
}


def _workspace_root(name: str) -> Path:
    if name == "all":
        raise ValueError("workspace='all' is not valid for this operation")
    try:
        return WORKSPACES[name]
    except KeyError as exc:
        raise ValueError(f"Unknown workspace '{name}'. Expected one of: {', '.join(sorted(WORKSPACES))}") from exc


def _resolve_workspace_path(path: str) -> Path:
    p = Path(path)
    if not p.is_absolute():
        p = IMGUI_ROOT / p
    resolved = p.resolve()
    for root in WORKSPACES.values():
        try:
            resolved.relative_to(root)
            return resolved
        except ValueError:
            continue
    raise ValueError(f"Path is outside the configured workspaces: {path}")


def _resolve_workspace_output_path(workspace: str, relative_path: str) -> Path:
    root = _workspace_root(workspace)
    candidate = (root / relative_path).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"Output path escapes workspace '{workspace}': {relative_path}") from exc
    return candidate


def _workspace_paths(selection: str) -> list[Path]:
    if selection == "all":
        return list(WORKSPACES.values())
    return [_workspace_root(selection)]


def _read_text(path: str) -> str:
    req = Request(path, headers={"User-Agent": "Mozilla/5.0 (X11; Linux x86_64)"})
    with urlopen(req, timeout=15) as resp:
        return resp.read().decode("utf-8", errors="replace")


def _read_cached_text(path: str) -> str:
    cached = _doc_raw_cache.get(path)
    if cached is not None:
        return cached
    text = _read_text(path)
    _doc_raw_cache[path] = text
    return text


def _strip_html(raw: str, max_chars: int = 8000) -> str:
    raw = re.sub(r"<(script|style|nav|footer|header)[^>]*>.*?</\1>", "", raw, flags=re.DOTALL | re.IGNORECASE)
    raw = re.sub(r"<!--.*?-->", "", raw, flags=re.DOTALL)
    raw = re.sub(r"<[^>]+>", " ", raw)
    raw = _html.unescape(raw)
    raw = re.sub(r"[ \t]+", " ", raw)
    raw = re.sub(r"\n{3,}", "\n\n", raw)
    return raw.strip()[:max_chars]


def _format_json(value: Any) -> str:
    return json.dumps(value, indent=2, sort_keys=False)


def _compile_database_paths() -> list[Path]:
    found: list[Path] = []
    for root in WORKSPACES.values():
        for path in root.rglob("compile_commands.json"):
            if path.is_file():
                found.append(path.resolve())
    return sorted(set(found))


def _load_compile_database(path: Path) -> list[dict[str, Any]]:
    with path.open(encoding="utf-8") as fh:
        return json.load(fh)


def _search_with_python(query: str, roots: list[Path], glob_pattern: str, limit: int, case_sensitive: bool) -> list[str]:
    flags = 0 if case_sensitive else re.IGNORECASE
    matcher = re.compile(query, flags)
    results: list[str] = []
    for root in roots:
        iterator = root.rglob(glob_pattern or "*")
        for path in iterator:
            if not path.is_file():
                continue
            try:
                with path.open(encoding="utf-8", errors="replace") as fh:
                    for idx, line in enumerate(fh, start=1):
                        if matcher.search(line):
                            results.append(f"{path}:{idx}:{line.rstrip()}")
                            if len(results) >= limit:
                                return results
            except OSError:
                continue
    return results


def _list_workspace_processes_impl(name_filter: str = "") -> list[dict[str, str]]:
    filt = name_filter.lower().strip()
    processes: list[dict[str, str]] = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        pid = entry.name
        try:
            comm = (entry / "comm").read_text().strip()
            cmdline = (entry / "cmdline").read_text().replace("\x00", " ").strip()
        except OSError:
            continue
        text = f"{comm} {cmdline}".lower()
        if filt and filt not in text:
            continue
        if str(IMGUI_ROOT) not in cmdline and str(VKCUBE_ROOT) not in cmdline and not filt:
            continue
        processes.append({
            "pid": pid,
            "name": comm,
            "cmdline": cmdline[:200],
        })
    return processes


def _fetch_doc_index(name: str) -> list[dict[str, str]]:
    cached = _doc_index_cache.get(name)
    if cached is not None:
        return cached

    base = LLVM_DOC_INDEXES[name]
    raw = _read_cached_text(base)
    matches = re.findall(r'<a[^>]+href=["\']([^"\']+)["\'][^>]*>(.*?)</a>', raw, flags=re.DOTALL | re.IGNORECASE)
    docs: list[dict[str, str]] = []
    for href, text in matches:
        title = _strip_html(text, max_chars=300).strip()
        if not title:
            continue
        url = urljoin(base, href)
        if url.endswith((".png", ".svg", ".css", ".js")):
            continue
        docs.append({"source": name, "title": title, "url": url})
    _doc_index_cache[name] = docs
    return docs


def _search_docs(query: str, limit: int) -> list[dict[str, str]]:
    q = query.lower().strip()
    scored: list[tuple[int, dict[str, str]]] = []
    for source in LLVM_DOC_INDEXES:
        for entry in _fetch_doc_index(source):
            hay = f"{entry['title']} {entry['url']}".lower()
            if q not in hay:
                continue
            score = 0
            if entry["title"].lower().startswith(q):
                score -= 20
            if q in entry["title"].lower():
                score -= 10
            score += len(entry["title"])
            scored.append((score, entry))
    scored.sort(key=lambda item: item[0])
    return [entry for _, entry in scored[:limit]]


def _fetch_doc_page(url: str) -> str:
    cached = _doc_page_cache.get(url)
    if cached is not None:
        return cached
    text = _strip_html(_read_cached_text(url))
    result = f"URL: {url}\n\n{text}"
    _doc_page_cache[url] = result
    return result


def _safe_title_from_url(url: str) -> str:
    name = Path(url).name
    if name.endswith(".html"):
        name = name[:-5]
    return name or url


def _extract_text_snippet(raw: str, needle: str, before: int = 900, after: int = 1500) -> str:
    pos = raw.lower().find(needle.lower())
    if pos < 0:
        return ""
    excerpt = raw[max(0, pos - before):pos + len(needle) + after]
    return _strip_html(excerpt, max_chars=2200)


def _imrad_binary_path() -> Path | None:
    env = os.environ.get("IMRAD_BIN", "").strip()
    if env:
        path = Path(env)
        if path.exists():
            return path
    which = shutil.which("imrad")
    if which:
        return Path(which)
    for candidate in IMRAD_BINARY_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def _imrad_include_path() -> Path | None:
    for candidate in IMRAD_INCLUDE_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def _imrad_template_path() -> Path | None:
    for candidate in IMRAD_TEMPLATE_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def _require_imrad_binary() -> Path:
    path = _imrad_binary_path()
    if path is None:
        raise RuntimeError("ImRAD is not installed. Install it first so the GUI launcher can run.")
    return path


def _snake_case_identifier(text: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", text.strip())
    value = re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_").lower()
    return value or "imrad_window"


def _camel_case_identifier(text: str) -> str:
    parts = [part for part in re.split(r"[^A-Za-z0-9]+", text.strip()) if part]
    if not parts:
        return "ImradWindow"
    return "".join(part[:1].upper() + part[1:] for part in parts)


def _instance_name(class_name: str) -> str:
    return class_name[:1].lower() + class_name[1:] if class_name else "imradWindow"


def _imrad_header_template(class_name: str, kind: str, has_modal: bool) -> str:
    lines = [
        f"// Generated with ImRAD {IMRAD_VERSION}",
        f"// visit {IMRAD_GITHUB_URL}",
        "",
        "#pragma once",
        "#include <imrad.h>",
        "",
        f"class {class_name}",
        "{",
        "public:",
        "    /// @begin interface",
    ]
    if kind == "window":
        lines.extend([
            "    void Open();",
            "    void Close();",
            "    void Draw();",
        ])
    else:
        close_sig = "    void ClosePopup(ImRad::ModalResult mr = ImRad::Cancel);" if has_modal else "    void ClosePopup();"
        open_sig = "    void OpenPopup(std::function<void(ImRad::ModalResult)> clb = [](ImRad::ModalResult){});" if has_modal else "    void OpenPopup();"
        lines.extend([
            open_sig,
            close_sig,
            "    void Draw();",
        ])
    lines.extend([
        "",
        "    /// @end interface",
        "",
        "private:",
        "    /// @begin impl",
        "    void DrawPopups();",
    ])
    if kind in {"popup", "modal_popup"}:
        lines.append("    void Init();")
    lines.extend([
        "",
        "    ImGuiID ID = 0;",
    ])
    if kind == "window":
        lines.extend([
            "    bool isOpen = true;",
            "    int counter = 0;",
        ])
    else:
        lines.append("    ImRad::ModalResult modalResult = ImRad::None;")
        if has_modal:
            lines.append("    std::function<void(ImRad::ModalResult)> callback;")
        lines.append("    int counter = 0;")
    lines.extend([
        "    /// @end impl",
        "};",
        "",
        f"extern {class_name} {_instance_name(class_name)};",
    ])
    return "\n".join(lines) + "\n"


def _imrad_cpp_template(class_name: str, title: str, kind: str, has_modal: bool, header_name: str) -> str:
    instance = _instance_name(class_name)
    lines = [
        f"// Generated with ImRAD {IMRAD_VERSION}",
        f"// visit {IMRAD_GITHUB_URL}",
        "",
        f'#include "{header_name}"',
        "",
        f"{class_name} {instance};",
        "",
    ]
    if kind == "window":
        lines.extend([
            f"void {class_name}::Open()",
            "{",
            "    isOpen = true;",
            "}",
            "",
            f"void {class_name}::Close()",
            "{",
            "    isOpen = false;",
            "}",
            "",
        ])
    else:
        if has_modal:
            lines.extend([
                f"void {class_name}::OpenPopup(std::function<void(ImRad::ModalResult)> clb)",
                "{",
                "    callback = clb;",
                "    modalResult = ImRad::None;",
                '    IM_ASSERT(ID && "Call Draw at least once to get ID assigned");',
                "    ImGui::OpenPopup(ID);",
                "    Init();",
                "}",
                "",
                f"void {class_name}::ClosePopup(ImRad::ModalResult mr)",
                "{",
                "    modalResult = mr;",
                "}",
                "",
            ])
        else:
            lines.extend([
                f"void {class_name}::OpenPopup()",
                "{",
                "    modalResult = ImRad::None;",
                '    IM_ASSERT(ID && "Call Draw at least once to get ID assigned");',
                "    ImGui::OpenPopup(ID);",
                "    Init();",
                "}",
                "",
                f"void {class_name}::ClosePopup()",
                "{",
                "    modalResult = ImRad::Cancel;",
                "}",
                "",
            ])
        lines.extend([
            f"void {class_name}::Init()",
            "{",
            "    // TODO: Add your code here",
            "}",
            "",
        ])

    if kind == "window":
        begin_line = f'        if (ImGui::Begin("{title}###{class_name}", &isOpen, ImGuiWindowFlags_None))'
    elif kind == "modal_popup":
        begin_line = f'    if (ImGui::BeginPopupModal("{title}###{class_name}", &tmpOpen, ImGuiWindowFlags_AlwaysAutoResize))'
    else:
        begin_line = f'    if (ImGui::BeginPopup("{title}###{class_name}", ImGuiWindowFlags_AlwaysAutoResize))'

    lines.extend([
        f"void {class_name}::Draw()",
        "{",
        "    /// @dpi-info 96,1",
        "    /// @style Dark",
        "    /// @unit px",
        "    /// @begin TopWindow",
    ])
    if kind in {"popup", "modal_popup"}:
        lines.extend([
            f'    ID = ImGui::GetID("###{class_name}");',
            "    bool tmpOpen = true;",
            begin_line,
            "    {",
        ])
    else:
        lines.extend([
            "    if (isOpen)",
            "    {",
            "        ImGui::SetNextWindowSize({ 420, 220 }, ImGuiCond_FirstUseEver); //{ 420, 220 }",
            "        ImGui::SetNextWindowSizeConstraints({ 0, 0 }, { FLT_MAX, FLT_MAX });",
            begin_line,
            "        {",
        ])
    body_indent = "        " if kind in {"popup", "modal_popup"} else "            "
    if kind == "modal_popup":
        lines.extend([
            f"{body_indent}if (modalResult != ImRad::None)",
            f"{body_indent}{{",
            f"{body_indent}    ImGui::CloseCurrentPopup();",
            f"{body_indent}    if (modalResult != ImRad::Cancel)",
            f"{body_indent}        callback(modalResult);",
            f"{body_indent}}}",
        ])
    elif kind == "popup":
        lines.extend([
            f"{body_indent}if (modalResult != ImRad::None)",
            f"{body_indent}{{",
            f"{body_indent}    ImGui::CloseCurrentPopup();",
            f"{body_indent}}}",
        ])
    lines.extend([
        f"{body_indent}DrawPopups();",
        "",
        f"{body_indent}/// @separator",
        "",
        f"{body_indent}/// @begin Text",
        f'{body_indent}ImGui::TextUnformatted("Starter ImRAD prototype for {class_name}.");',
        f"{body_indent}/// @end Text",
        "",
        f"{body_indent}/// @begin Button",
        f'{body_indent}if (ImGui::Button("Increment"))',
        f"{body_indent}{{",
        f"{body_indent}    counter++;",
        f"{body_indent}}}",
        f"{body_indent}/// @end Button",
        "",
        f"{body_indent}/// @begin Text",
        f'{body_indent}ImGui::Text("counter = %d", counter);',
        f"{body_indent}/// @end Text",
    ])
    if kind == "modal_popup":
        lines.extend([
            "",
            f"{body_indent}/// @begin Button",
            f'{body_indent}if (ImGui::Button("Close"))',
            f"{body_indent}{{",
            f"{body_indent}    ClosePopup(ImRad::Ok);",
            f"{body_indent}}}",
            f"{body_indent}/// @end Button",
        ])
    elif kind == "popup":
        lines.extend([
            "",
            f"{body_indent}/// @begin Button",
            f'{body_indent}if (ImGui::Button("Close"))',
            f"{body_indent}{{",
            f"{body_indent}    ClosePopup();",
            f"{body_indent}}}",
            f"{body_indent}/// @end Button",
        ])
    else:
        lines.extend([
            "",
            f"{body_indent}/// @begin Button",
            f'{body_indent}if (ImGui::Button("Hide"))',
            f"{body_indent}{{",
            f"{body_indent}    Close();",
            f"{body_indent}}}",
            f"{body_indent}/// @end Button",
        ])
    lines.extend([
        "",
        f"{body_indent}/// @separator",
    ])
    if kind in {"popup", "modal_popup"}:
        lines.extend([
            "        ImGui::EndPopup();",
            "    }",
        ])
    else:
        lines.extend([
            "        }",
            "        ImGui::End();",
            "    }",
        ])
    lines.extend([
        "    /// @end TopWindow",
        "}",
        "",
        f"void {class_name}::DrawPopups()",
        "{",
        "    // TODO: Draw dependent popups here",
        "}",
    ])
    return "\n".join(lines) + "\n"


def _scan_imrad_project_candidates_impl(query: str = "", workspace: str = "vkcube-progress", limit: int = 12) -> str:
    limit = max(1, min(limit, 50))
    roots = _workspace_paths(workspace)
    terms = [query.strip()] if query.strip() else [
        "ImGui::Begin(",
        "ImGui::BeginPopup(",
        "ImGui::BeginPopupModal(",
        "ImGui::ShowDemoWindow(",
    ]
    source_suffixes = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
    hits: list[dict[str, Any]] = []

    for root in roots:
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in source_suffixes:
                continue
            if any(part in {"cmake-build", ".git", "output", "__pycache__"} for part in path.parts):
                continue
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue
            snippets: list[str] = []
            score = 0
            for idx, line in enumerate(lines, start=1):
                if not any(term in line for term in terms):
                    continue
                score += 3
                snippets.append(f"{idx}. {line.strip()[:180]}")
                if len(snippets) >= 3:
                    break
            if score == 0:
                continue
            if "/app/ui/" in str(path):
                score += 4
            if "ShowDemoWindow" in "\n".join(snippets):
                score -= 1
            hits.append({
                "score": score,
                "path": str(path),
                "snippets": snippets,
            })

    if not hits:
        return f"No ImRAD candidates matched '{query or 'default ImGui window scan'}' in workspace '{workspace}'."

    hits.sort(key=lambda item: (-item["score"], item["path"]))
    title = f"ImRAD conversion candidates in workspace '{workspace}'"
    if query.strip():
        title += f" for '{query.strip()}'"
    lines = [title + ":"]
    for idx, hit in enumerate(hits[:limit], start=1):
        lines.append(f"{idx}. {hit['path']}")
        for snippet in hit["snippets"]:
            lines.append(f"   {snippet}")
    return "\n".join(lines)


def _write_imrad_launch_ini(work_dir: Path, prototype_header: Path) -> Path:
    work_dir.mkdir(parents=True, exist_ok=True)
    ini_path = work_dir / "imgui.ini"
    ini_path.write_text(
        "\n".join([
            "[ImRAD][Recent]",
            f"File1={prototype_header}",
            "ActiveConfig1=0",
            "ActiveTab=0",
            "",
            "[ImRAD][Explorer]",
            f"Path={prototype_header.parent}",
            "Filter=0",
            "SortColumn=0",
            "SortDir=1",
            "",
            "[ImRAD][UI]",
            "CheckedRelease=",
            "FontName=Roboto-Regular.ttf",
            "FontSize=15.000000",
            "PgFontName=Roboto-Regular.ttf",
            "PgbFontName=Roboto-Bold.ttf",
            "PgFontSize=16.000000",
            "DesignFontName=Roboto-Regular.ttf",
            "DesignFontSize=15.000000",
            "",
        ]),
        encoding="utf-8",
    )
    return ini_path


def _fetch_sdl_doc_index(component: str) -> list[dict[str, str]]:
    cached = _sdl_doc_index_cache.get(component)
    if cached is not None:
        return cached

    try:
        info = SDL_DOC_COMPONENTS[component]
    except KeyError as exc:
        raise ValueError(
            f"Unknown SDL3 component '{component}'. Expected one of: {', '.join(sorted(SDL_DOC_COMPONENTS))}"
        ) from exc

    raw = _read_cached_text(info["index"])
    matches = re.findall(r'<a[^>]+href=["\']([^"\']+)["\'][^>]*>(.*?)</a>', raw, flags=re.DOTALL | re.IGNORECASE)
    entries: list[dict[str, str]] = [{
        "component": component,
        "title": f"SDL3 {component} index",
        "url": info["index"],
    }]
    seen = {info["index"]}

    for href, text in matches:
        url = urljoin(info["index"], href)
        if not url.startswith(info["base"]):
            continue
        if url in seen:
            continue
        if url.endswith((".png", ".svg", ".css", ".js", ".json", ".xml", ".ico", ".txt", ".zip")):
            continue
        if url.endswith(("/edit", "/delete", "/history")):
            continue
        title = _strip_html(text, max_chars=300).strip() or _safe_title_from_url(url)
        if not title:
            continue
        entries.append({
            "component": component,
            "title": title,
            "url": url,
        })
        seen.add(url)

    _sdl_doc_index_cache[component] = entries
    return entries


def _search_sdl_docs_impl(query: str, limit: int, component: str = "all") -> list[dict[str, str]]:
    q = query.lower().strip()
    if not q:
        return []

    components = list(SDL_DOC_COMPONENTS) if component == "all" else [component]
    scored: list[tuple[int, dict[str, str]]] = []
    for comp in components:
        for entry in _fetch_sdl_doc_index(comp):
            title = entry["title"].lower()
            url = entry["url"].lower()
            if q not in title and q not in url:
                tokens = [token for token in re.split(r"[^a-z0-9_]+", q) if token]
                if not tokens or not all(token in title or token in url for token in tokens):
                    continue
            score = len(entry["title"])
            if title == q:
                score -= 200
            if title.startswith(q):
                score -= 80
            if q in title:
                score -= 40
            if url.endswith(f"/{q}"):
                score -= 120
            elif q in url:
                score -= 20
            if entry["component"] == "api":
                score -= 5
            scored.append((score, entry))
    scored.sort(key=lambda item: (item[0], item[1]["title"], item[1]["url"]))
    return [entry for _, entry in scored[:limit]]


def _sdl_symbol_doc_url(topic: str) -> str | None:
    topic = topic.strip()
    if re.fullmatch(r"SDL[A-Za-z0-9_]+", topic):
        return f"{SDL_DOC_COMPONENTS['api']['base']}{topic}"
    return None


def _lookup_sdl3_doc_impl(topic: str, component: str = "all") -> str:
    topic = topic.strip()
    if topic.startswith("http://") or topic.startswith("https://"):
        return _fetch_doc_page(topic)

    symbol_url = _sdl_symbol_doc_url(topic)
    if symbol_url is not None:
        try:
            return _fetch_doc_page(symbol_url)
        except Exception:
            pass

    matches = _search_sdl_docs_impl(topic, 1, component=component)
    if not matches:
        return f"No SDL3 docs matched '{topic}'."
    return _fetch_doc_page(matches[0]["url"])


def _relevant_sdl3_project_files(text: str) -> list[str]:
    lowered = text.lower()
    files: list[str] = []

    def add(path: str) -> None:
        if path not in files:
            files.append(path)

    if any(token in lowered for token in ("sdl_init", "sdl_createwindow", "video", "window", "sdl_vulkan_")):
        add("app/platform/sdl_window.cpp")
    if any(token in lowered for token in ("sdl_creategpudevice", "sdl_claimwindowforgpudevice", "sdl_gpu_", "gpu")):
        add("app/renderer/sdlgpu3/sdlgpu3_context.cpp")
        add("app/renderer/sdlgpu3/sdlgpu3_emoji_atlas.cpp")
    if any(token in lowered for token in ("imgui_implsdl3_", "event", "mouse", "keyboard", "gamepad")):
        add("app/ui/imgui_layer.cpp")
        add("app/ui/imgui_layer_sdlgpu3.cpp")
        add("app/app.cpp")
    return files


def _scan_sdl3_project_implementations_impl(
    query: str = "",
    workspace: str = "vkcube-progress",
    limit: int = 12,
) -> str:
    limit = max(1, min(limit, 50))
    roots = _workspace_paths(workspace)
    terms = [query.strip()] if query.strip() else [
        "#include <SDL3/",
        "SDL_",
        "ImGui_ImplSDL3_",
        "SDL_Vulkan_",
        "SDL_GPU_",
        "sdlgpu3",
    ]
    source_suffixes = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
    hits: list[dict[str, Any]] = []

    for root in roots:
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in source_suffixes:
                continue
            if any(part in {"cmake-build", ".git", "output", "__pycache__"} for part in path.parts):
                continue
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError:
                continue

            snippets: list[str] = []
            symbols: list[str] = []
            for idx, line in enumerate(lines, start=1):
                hay = line.lower()
                if not any(term.lower() in hay for term in terms):
                    continue
                if len(snippets) < 3:
                    snippets.append(f"{idx}. {line.strip()[:180]}")
                for token in re.findall(r"\b(?:SDL[A-Za-z0-9_]+|ImGui_ImplSDL3_[A-Za-z0-9_]+)\b", line):
                    if token not in symbols:
                        symbols.append(token)

            if snippets:
                hits.append({
                    "path": str(path),
                    "symbols": symbols,
                    "snippets": snippets,
                })

    if not hits:
        return f"No SDL3-related implementations matched '{query or 'default SDL3 scan'}' in workspace '{workspace}'."

    hits.sort(key=lambda entry: (str(entry["path"]).count("/"), str(entry["path"])))
    heading = f"SDL3-related implementations in workspace '{workspace}'"
    if query.strip():
        heading += f" for '{query.strip()}'"
    lines = [heading + ":"]
    for idx, hit in enumerate(hits[:limit], start=1):
        lines.append(f"{idx}. {hit['path']}")
        if hit["symbols"]:
            lines.append(f"   symbols: {', '.join(hit['symbols'][:10])}")
        for snippet in hit["snippets"]:
            lines.append(f"   {snippet}")
    return "\n".join(lines)


def _interpret_sdl3_diagnostic_impl(diagnostic: str) -> str:
    text = diagnostic.strip()
    if not text:
        return "Diagnostic is empty."

    lines = [f"Diagnostic: {text}"]

    symbol_tokens: list[str] = []
    for token in re.findall(r"\bSDL[A-Za-z0-9_]+\b", text):
        if token not in symbol_tokens:
            symbol_tokens.append(token)

    if symbol_tokens:
        for token in symbol_tokens[:3]:
            symbol_url = _sdl_symbol_doc_url(token)
            lines.extend(["", f"SDL3 symbol: {token}"])
            if symbol_url:
                lines.append(f"URL: {symbol_url}")
    else:
        lines.extend([
            "",
            "No SDL3 symbol token was detected.",
            "Use lookup_sdl3_doc() for a specific SDL3 API, type, enum, or wiki topic.",
        ])

    if "error" in text.lower() or "failed" in text.lower():
        lines.extend([
            "",
            "SDL_GetError note: check the failing SDL call's return value first.",
            "SDL error strings are thread-local and may remain set even after later successful calls.",
            f"Reference: {SDL_DOC_COMPONENTS['api']['base']}SDL_GetError",
        ])

    project_files = _relevant_sdl3_project_files(text)
    if project_files:
        lines.extend(["", "Relevant project implementations:"])
        for path in project_files:
            lines.append(f"  - {path}")

    return "\n".join(lines)


def _fetch_vulkan_doc_index(component: str) -> list[dict[str, str]]:
    cached = _vulkan_doc_index_cache.get(component)
    if cached is not None:
        return cached

    try:
        info = VULKAN_DOC_COMPONENTS[component]
    except KeyError as exc:
        raise ValueError(
            f"Unknown Vulkan component '{component}'. Expected one of: {', '.join(sorted(VULKAN_DOC_COMPONENTS))}"
        ) from exc

    raw = _read_cached_text(info["index"])
    matches = re.findall(r'<a[^>]+href=["\']([^"\']+)["\'][^>]*>(.*?)</a>', raw, flags=re.DOTALL | re.IGNORECASE)
    entries: list[dict[str, str]] = [{
        "component": component,
        "title": f"Vulkan {component} index",
        "url": info["index"],
    }]
    seen = {info["index"]}

    for href, text in matches:
        url = urljoin(info["index"], href)
        if not url.startswith(info["base"]):
            continue
        if "/_/" in url or url.endswith((".png", ".svg", ".css", ".js", ".json", ".xml", ".ico", ".txt")):
            continue
        title = _strip_html(text, max_chars=300).strip() or _safe_title_from_url(url)
        if not title or url in seen:
            continue
        entries.append({
            "component": component,
            "title": title,
            "url": url,
        })
        seen.add(url)

    _vulkan_doc_index_cache[component] = entries
    return entries


def _search_vulkan_docs_impl(query: str, limit: int, component: str = "all") -> list[dict[str, str]]:
    q = query.lower().strip()
    if not q:
        return []

    components = list(VULKAN_DOC_COMPONENTS) if component == "all" else [component]
    scored: list[tuple[int, dict[str, str]]] = []
    for comp in components:
        for entry in _fetch_vulkan_doc_index(comp):
            title = entry["title"].lower()
            url = entry["url"].lower()
            if q not in title and q not in url:
                tokens = [token for token in re.split(r"[^a-z0-9_]+", q) if token]
                if not tokens or not all(token in title or token in url for token in tokens):
                    continue
            score = len(entry["title"])
            if title == q:
                score -= 200
            if title.startswith(q):
                score -= 80
            if q in title:
                score -= 40
            if url.endswith(f"/{q}.html"):
                score -= 120
            elif q in url:
                score -= 20
            if entry["component"] == "refpages":
                score -= 5
            scored.append((score, entry))
    scored.sort(key=lambda item: (item[0], item[1]["title"], item[1]["url"]))
    return [entry for _, entry in scored[:limit]]


def _vulkan_symbol_doc_url(topic: str) -> str | None:
    topic = topic.strip()
    if re.fullmatch(r"(vk|Vk)[A-Za-z0-9_]+", topic):
        return f"{VULKAN_DOC_COMPONENTS['refpages']['base']}source/{topic}.html"
    return None


def _lookup_vulkan_vuid(vuid: str) -> str:
    for entry in _fetch_vulkan_doc_index("spec"):
        url = entry["url"]
        if not url.endswith(".html"):
            continue
        try:
            raw = _read_cached_text(url)
        except Exception:
            continue
        if vuid not in raw:
            continue
        snippet = _extract_text_snippet(raw, vuid)
        if not snippet:
            snippet = vuid
        return f"VUID: {vuid}\nURL: {url}\n\n{snippet}"
    return f"Could not locate {vuid} in the indexed Vulkan spec pages."


def _lookup_vkresult(token: str) -> str:
    url = f"{VULKAN_DOC_COMPONENTS['refpages']['base']}source/VkResult.html"
    try:
        raw = _read_cached_text(url)
    except Exception as exc:
        return f"VkResult lookup failed for {token}: {exc}"
    snippet = _extract_text_snippet(raw, token)
    hint = _VULKAN_RESULT_HINTS.get(token, "See the Vulkan VkResult reference page for the exact semantics.")
    lines = [
        f"VkResult: {token}",
        f"URL: {url}",
        f"Hint: {hint}",
    ]
    if snippet:
        lines.extend(["", snippet])
    return "\n".join(lines)


def _lookup_vulkan_doc_impl(topic: str, component: str = "all") -> str:
    topic = topic.strip()
    if topic.startswith("http://") or topic.startswith("https://"):
        return _fetch_doc_page(topic)

    if re.fullmatch(r"VUID-[A-Za-z0-9_-]+", topic):
        return _lookup_vulkan_vuid(topic)

    symbol_url = _vulkan_symbol_doc_url(topic)
    if symbol_url is not None:
        try:
            return _fetch_doc_page(symbol_url)
        except Exception:
            pass

    matches = _search_vulkan_docs_impl(topic, 1, component=component)
    if not matches:
        return f"No Vulkan docs matched '{topic}'."
    return _fetch_doc_page(matches[0]["url"])


def _interpret_vulkan_diagnostic_impl(diagnostic: str) -> str:
    text = diagnostic.strip()
    if not text:
        return "Diagnostic is empty."

    lines = [f"Diagnostic: {text}"]

    vuid_match = re.search(r"(VUID-[A-Za-z0-9_-]+)", text)
    if vuid_match:
        lines.extend(["", _lookup_vulkan_vuid(vuid_match.group(1))])

    result_tokens = []
    for token in re.findall(r"\bVK_[A-Z0-9_]+\b", text):
        if token not in result_tokens and (token.startswith("VK_ERROR_") or token in _VULKAN_RESULT_HINTS or token in {"VK_SUBOPTIMAL_KHR", "VK_TIMEOUT", "VK_NOT_READY", "VK_INCOMPLETE"}):
            result_tokens.append(token)
    if result_tokens:
        for token in result_tokens[:3]:
            lines.extend(["", _lookup_vkresult(token)])

    symbol_match = re.search(r"\b((?:vk|Vk)[A-Za-z0-9_]+)\b", text)
    if symbol_match:
        symbol = symbol_match.group(1)
        symbol_url = _vulkan_symbol_doc_url(symbol)
        if symbol_url:
            lines.extend(["", f"Reference symbol: {symbol}", f"URL: {symbol_url}"])

    if len(lines) == 1:
        lines.extend([
            "",
            "No VkResult, VUID, or Vulkan symbol token was detected.",
            "Use lookup_vulkan_doc() for a specific command, type, enum, or spec topic.",
        ])
    return "\n".join(lines)


def _history_entries(session_id: str) -> list[dict[str, Any]] | None:
    return expert._sessions.get(session_id)


def _capture_debug_checkpoint(label: str = "") -> dict[str, Any]:
    index = len(_debug_checkpoints)
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    checkpoint = {
        "index": index,
        "label": label or f"checkpoint-{index}",
        "timestamp": timestamp,
        "status": lldb.debug_status(),
    }
    if lldb._client.is_active and lldb._client.is_stopped:
        checkpoint["inspect_state"] = lldb.inspect_state()
    else:
        checkpoint["inspect_state"] = "(session not stopped)"
    _debug_checkpoints.append(checkpoint)
    return checkpoint


@mcp.tool(description="List the configured workspace roots available to this combined MCP server.")
def list_workspace_roots() -> str:
    lines = ["Configured workspaces:"]
    for name, root in WORKSPACES.items():
        lines.append(f"  - {name}: {root}")
    return "\n".join(lines)


@mcp.tool(description="List files in a workspace using a glob pattern. Workspace: imgui, vkcube, or all.")
def list_workspace_files(workspace: str = "all", pattern: str = "**/*", limit: int = 200) -> str:
    limit = max(1, min(limit, 1000))
    paths: list[str] = []
    for root in _workspace_paths(workspace):
        for path in root.glob(pattern):
            paths.append(str(path))
            if len(paths) >= limit:
                break
        if len(paths) >= limit:
            break
    if not paths:
        return f"No files matched pattern '{pattern}' in workspace '{workspace}'."
    return "\n".join(paths[:limit])


@mcp.tool(description="Search workspace text with a regex query. Workspace: imgui, vkcube, or all.")
def search_workspace_text(
    query: str,
    workspace: str = "all",
    glob_pattern: str = "",
    limit: int = 50,
    case_sensitive: bool = False,
) -> str:
    limit = max(1, min(limit, 200))
    roots = _workspace_paths(workspace)
    if RG_BIN:
        cmd = [RG_BIN, query]
        if not case_sensitive:
            cmd.append("-i")
        cmd.extend(["-n", "--max-count", str(limit)])
        if glob_pattern:
            cmd.extend(["-g", glob_pattern])
        cmd.extend(str(root) for root in roots)
        result = subprocess.run(cmd, capture_output=True, text=True)
        output = result.stdout.strip()
        if output:
            return output
        if result.stderr.strip():
            return result.stderr.strip()
    matches = _search_with_python(query, roots, glob_pattern or "**/*", limit, case_sensitive)
    return "\n".join(matches) if matches else f"No matches for '{query}' in workspace '{workspace}'."


@mcp.tool(description="Read a workspace file with line numbers. Relative paths are resolved from the imgui workspace.")
def read_workspace_file(path: str, start_line: int = 1, end_line: int = 120) -> str:
    resolved = _resolve_workspace_path(path)
    if start_line < 1:
        start_line = 1
    if end_line < start_line:
        end_line = start_line
    try:
        with resolved.open(encoding="utf-8", errors="replace") as fh:
            lines = fh.readlines()
    except OSError as exc:
        return f"Cannot read {resolved}: {exc}"
    start_idx = start_line - 1
    end_idx = min(len(lines), end_line)
    out = [f"{resolved}:{start_line}-{end_idx}"]
    for idx in range(start_idx, end_idx):
        out.append(f"{idx + 1}. {lines[idx].rstrip()}")
    return "\n".join(out)


@mcp.tool(description="List all compile_commands.json databases visible in the configured workspaces.")
def list_compile_databases() -> str:
    found = _compile_database_paths()
    if not found:
        return "No compile_commands.json files found."
    return "\n".join(str(path) for path in found)


@mcp.tool(description="Find the compile_commands entry for a source file path.")
def lookup_compile_command(file_path: str) -> str:
    resolved = _resolve_workspace_path(file_path)
    found_entries: list[dict[str, Any]] = []
    for db_path in _compile_database_paths():
        try:
            entries = _load_compile_database(db_path)
        except Exception as exc:
            found_entries.append({"database": str(db_path), "error": str(exc)})
            continue
        for entry in entries:
            candidate = Path(entry.get("file", ""))
            if not candidate.is_absolute():
                candidate = Path(entry.get("directory", ".")) / candidate
            try:
                if candidate.resolve() == resolved:
                    found_entries.append({
                        "database": str(db_path),
                        "directory": entry.get("directory", ""),
                        "file": str(candidate.resolve()),
                        "command": entry.get("command"),
                        "arguments": entry.get("arguments"),
                    })
            except OSError:
                continue
    if not found_entries:
        return f"No compile_commands entry found for {resolved}"
    return _format_json(found_entries)


@mcp.tool(description="List the known build/run targets exposed by this combined MCP server.")
def list_known_targets() -> str:
    lines = []
    for name, info in KNOWN_TARGETS.items():
        lines.append(f"- {name}")
        lines.append(f"  workspace: {info['workspace']}")
        lines.append(f"  binary:    {info['binary']}")
        lines.append(f"  cwd:       {info['cwd']}")
        lines.append(f"  build:     {' '.join(info['build'])}")
        lines.append(f"  notes:     {info['notes']}")
    return "\n".join(lines)


@mcp.tool(description="Build a known target. Use dry_run=true to preview the command without executing it.")
def build_target(target: str, clean: bool = False, dry_run: bool = False) -> str:
    info = KNOWN_TARGETS.get(target)
    if info is None:
        return f"Unknown target '{target}'. Try list_known_targets()."
    cmd = list(info["build"])
    if clean:
        build_dir = Path(cmd[-1])
        if build_dir.is_dir():
            for child in build_dir.iterdir():
                if child.is_file():
                    try:
                        child.unlink()
                    except OSError:
                        pass
    if dry_run:
        return f"Dry run:\n{' '.join(cmd)}"
    try:
        result = subprocess.run(cmd, cwd=info["cwd"], capture_output=True, text=True, timeout=BUILD_TIMEOUT)
    except subprocess.TimeoutExpired:
        return f"Build timed out after {BUILD_TIMEOUT}s for target '{target}'."
    output = (result.stdout + result.stderr).strip()
    status = "OK" if result.returncode == 0 else f"FAILED ({result.returncode})"
    return f"Build {target}: {status}\n\n{output or '(no output)'}"


@mcp.tool(description="Run a known target binary. Use dry_run=true to preview the command without executing it.")
def run_target(target: str, args: list[str] | None = None, timeout_sec: float = RUN_TIMEOUT, dry_run: bool = False) -> str:
    info = KNOWN_TARGETS.get(target)
    if info is None:
        return f"Unknown target '{target}'. Try list_known_targets()."
    cmd = [info["binary"], *(args or [])]
    if dry_run:
        return f"Dry run:\n{' '.join(cmd)}"
    try:
        result = subprocess.run(cmd, cwd=info["cwd"], capture_output=True, text=True, timeout=max(1.0, min(timeout_sec, 120.0)))
    except subprocess.TimeoutExpired as exc:
        output = ((exc.stdout or "") + (exc.stderr or "")).strip()
        return f"Run timed out after {timeout_sec}s for target '{target}'.\n\n{output or '(no output before timeout)'}"
    output = (result.stdout + result.stderr).strip()
    status = "OK" if result.returncode == 0 else f"FAILED ({result.returncode})"
    return f"Run {target}: {status}\n\n{output or '(no output)'}"


@mcp.tool(description="Search official LLVM, Clang, and MLIR documentation indexes for a query.")
def search_llvm_docs(query: str, limit: int = 8) -> str:
    limit = max(1, min(limit, 20))
    try:
        matches = _search_docs(query, limit)
    except Exception as exc:
        return f"Failed to search LLVM docs: {exc}"
    if not matches:
        return f"No LLVM/Clang/MLIR docs matched '{query}'."
    lines = [f"LLVM docs matches for '{query}':"]
    for idx, entry in enumerate(matches, start=1):
        lines.append(f"{idx}. [{entry['source']}] {entry['title']}")
        lines.append(f"   {entry['url']}")
    return "\n".join(lines)


@mcp.tool(description="Compatibility alias for online LLVM/Clang documentation search.")
def search_online_llvm_clang_docs(query: str, limit: int = 8) -> str:
    return search_llvm_docs(query=query, limit=limit)


@mcp.tool(description="Fetch an LLVM/Clang/MLIR documentation page by query term or direct URL.")
def lookup_llvm_doc(topic: str) -> str:
    try:
        if topic.startswith("http://") or topic.startswith("https://"):
            return _fetch_doc_page(topic)
        matches = _search_docs(topic, 1)
        if not matches:
            return f"No LLVM/Clang/MLIR docs matched '{topic}'."
        return _fetch_doc_page(matches[0]["url"])
    except Exception as exc:
        return f"Failed to fetch LLVM docs for '{topic}': {exc}"


@mcp.tool(description="Compatibility alias for online LLVM/Clang documentation lookup.")
def lookup_online_llvm_clang_doc(topic: str) -> str:
    return lookup_llvm_doc(topic=topic)


@mcp.tool(description="Report whether ImRAD is installed and where its binary, headers, and templates live.")
def get_imrad_status() -> str:
    binary = _imrad_binary_path()
    include = _imrad_include_path()
    template = _imrad_template_path()
    lines = [
        "ImRAD status:",
        f"  binary:   {binary if binary else '(not found)'}",
        f"  include:  {include if include else '(not found)'}",
        f"  template: {template if template else '(not found)'}",
        f"  version:  {IMRAD_VERSION}",
    ]
    return "\n".join(lines)


@mcp.tool(description="Scan the workspace for ImGui windows/popups that look like good ImRAD conversion candidates.")
def scan_imrad_project_candidates(query: str = "", workspace: str = "vkcube-progress", limit: int = 12) -> str:
    return _scan_imrad_project_candidates_impl(query=query, workspace=workspace, limit=limit)


@mcp.tool(description="Create an ImRAD-compatible starter prototype (.h/.cpp) for a floating window, popup, or modal popup.")
def create_imrad_window_prototype(
    name: str,
    workspace: str = "vkcube-progress",
    output_dir: str = "app/ui/imrad",
    kind: str = "window",
    overwrite: bool = False,
) -> str:
    normalized_kind = kind.strip().lower()
    if normalized_kind not in {"window", "popup", "modal_popup"}:
        return "Unsupported kind. Expected one of: window, popup, modal_popup."

    class_name = _camel_case_identifier(name)
    file_stem = _snake_case_identifier(name)
    target_dir = _resolve_workspace_output_path(workspace, output_dir)
    target_dir.mkdir(parents=True, exist_ok=True)
    header_path = target_dir / f"{file_stem}.h"
    cpp_path = target_dir / f"{file_stem}.cpp"
    if not overwrite and (header_path.exists() or cpp_path.exists()):
        return f"Refusing to overwrite existing prototype files: {header_path}, {cpp_path}"

    title = re.sub(r"[_-]+", " ", name).strip() or class_name
    title = title[:1].upper() + title[1:]
    header_path.write_text(
        _imrad_header_template(class_name, normalized_kind, has_modal=normalized_kind == "modal_popup"),
        encoding="utf-8",
    )
    cpp_path.write_text(
        _imrad_cpp_template(class_name, title, normalized_kind, has_modal=normalized_kind == "modal_popup", header_name=header_path.name),
        encoding="utf-8",
    )
    return "\n".join([
        f"Created ImRAD prototype '{class_name}' ({normalized_kind}).",
        f"Header: {header_path}",
        f"Source: {cpp_path}",
        "Use launch_imrad_preview() on the header path to open it directly in ImRAD.",
    ])


@mcp.tool(description="Launch the installed ImRAD GUI preloaded with a prototype header, or preview the launch command with dry_run=true.")
def launch_imrad_preview(prototype_path: str, dry_run: bool = False) -> str:
    binary = _require_imrad_binary()
    resolved = _resolve_workspace_path(prototype_path)
    if resolved.suffix.lower() in {".cpp", ".cxx", ".cc", ".c"}:
        header_candidate = resolved.with_suffix(".h")
        if not header_candidate.exists():
            header_candidate = resolved.with_suffix(".hpp")
        resolved = header_candidate
    if resolved.suffix.lower() not in {".h", ".hpp", ".hxx"}:
        return f"ImRAD preview expects a generated header path; got '{prototype_path}'."
    if not resolved.exists():
        return f"Prototype file does not exist: {resolved}"

    IMRAD_LAUNCH_ROOT.mkdir(parents=True, exist_ok=True)
    launch_dir = Path(tempfile.mkdtemp(prefix="imrad-preview-", dir=str(IMRAD_LAUNCH_ROOT)))
    ini_path = _write_imrad_launch_ini(launch_dir, resolved)
    cmd = [str(binary)]
    if dry_run:
        return "\n".join([
            "Dry run:",
            f"  cwd: {launch_dir}",
            f"  ini: {ini_path}",
            f"  cmd: {' '.join(cmd)}",
            f"  preload: {resolved}",
        ])

    proc = subprocess.Popen(
        cmd,
        cwd=str(launch_dir),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        stdin=subprocess.DEVNULL,
        start_new_session=True,
    )
    return "\n".join([
        f"Launched ImRAD (pid {proc.pid}).",
        f"Working directory: {launch_dir}",
        f"Preloaded prototype: {resolved}",
        f"INI file: {ini_path}",
    ])


@mcp.tool(description="Search official SDL3 wiki documentation indexes for a query.")
def search_sdl3_docs(query: str, limit: int = 8, component: str = "all") -> str:
    limit = max(1, min(limit, 20))
    try:
        matches = _search_sdl_docs_impl(query, limit, component=component)
    except Exception as exc:
        return f"Failed to search SDL3 docs: {exc}"
    if not matches:
        return f"No SDL3 docs matched '{query}'."
    lines = [f"SDL3 docs matches for '{query}':"]
    for idx, entry in enumerate(matches, start=1):
        lines.append(f"{idx}. [{entry['component']}] {entry['title']}")
        lines.append(f"   {entry['url']}")
    return "\n".join(lines)


@mcp.tool(description="Compatibility alias for online SDL3 documentation search.")
def search_online_sdl3_docs(query: str, limit: int = 8, component: str = "all") -> str:
    return search_sdl3_docs(query=query, limit=limit, component=component)


@mcp.tool(description="Fetch an SDL3 documentation page by query term, SDL3 symbol, or direct URL.")
def lookup_sdl3_doc(topic: str, component: str = "all") -> str:
    try:
        return _lookup_sdl3_doc_impl(topic=topic, component=component)
    except Exception as exc:
        return f"Failed to fetch SDL3 docs for '{topic}': {exc}"


@mcp.tool(description="Compatibility alias for online SDL3 documentation lookup.")
def lookup_online_sdl3_doc(topic: str, component: str = "all") -> str:
    return lookup_sdl3_doc(topic=topic, component=component)


@mcp.tool(description="Interpret an SDL3 diagnostic and point to likely API references and project implementations.")
def interpret_sdl3_diagnostic(diagnostic: str) -> str:
    return _interpret_sdl3_diagnostic_impl(diagnostic)


@mcp.tool(description="Scan the workspace for SDL3-related implementations and surface the most relevant files and symbols.")
def scan_sdl3_project_implementations(
    query: str = "",
    workspace: str = "vkcube-progress",
    limit: int = 12,
) -> str:
    return _scan_sdl3_project_implementations_impl(query=query, workspace=workspace, limit=limit)


@mcp.tool(description="Search official Vulkan specification, guide, tutorial, and refpage indexes for a query.")
def search_vulkan_docs(query: str, limit: int = 8, component: str = "all") -> str:
    limit = max(1, min(limit, 20))
    try:
        matches = _search_vulkan_docs_impl(query, limit, component=component)
    except Exception as exc:
        return f"Failed to search Vulkan docs: {exc}"
    if not matches:
        return f"No Vulkan docs matched '{query}'."
    lines = [f"Vulkan docs matches for '{query}':"]
    for idx, entry in enumerate(matches, start=1):
        lines.append(f"{idx}. [{entry['component']}] {entry['title']}")
        lines.append(f"   {entry['url']}")
    return "\n".join(lines)


@mcp.tool(description="Compatibility alias for online Vulkan documentation search.")
def search_online_vulkan_docs(query: str, limit: int = 8, component: str = "all") -> str:
    return search_vulkan_docs(query=query, limit=limit, component=component)


@mcp.tool(description="Fetch a Vulkan documentation page by query term, Vulkan symbol, VUID, or direct URL.")
def lookup_vulkan_doc(topic: str, component: str = "all") -> str:
    try:
        return _lookup_vulkan_doc_impl(topic=topic, component=component)
    except Exception as exc:
        return f"Failed to fetch Vulkan docs for '{topic}': {exc}"


@mcp.tool(description="Compatibility alias for online Vulkan documentation lookup.")
def lookup_online_vulkan_doc(topic: str, component: str = "all") -> str:
    return lookup_vulkan_doc(topic=topic, component=component)


@mcp.tool(description="Look up a Vulkan VkResult code and explain its likely meaning.")
def lookup_vkresult(result_code: str) -> str:
    token = result_code.strip()
    if not token.startswith("VK_"):
        return f"'{result_code}' is not a Vulkan status token."
    return _lookup_vkresult(token)


@mcp.tool(description="Interpret a Vulkan validation/debug/runtime diagnostic containing VkResult codes, VUIDs, or Vulkan symbols.")
def interpret_vulkan_diagnostic(diagnostic: str) -> str:
    return _interpret_vulkan_diagnostic_impl(diagnostic)


@mcp.tool(description="List workspace-related processes, optionally filtered by name substring.")
def find_workspace_processes(name_filter: str = "") -> str:
    entries = _list_workspace_processes_impl(name_filter)
    if not entries:
        return f"No matching workspace processes found for filter '{name_filter}'."
    return "\n".join(f"{e['pid']}: {e['name']} :: {e['cmdline']}" for e in entries)


@mcp.tool(description="Return the full message history for an expert chat session.")
def get_expert_session_history(session_id: str = "default", limit: int = 20) -> str:
    history = _history_entries(session_id)
    if not history:
        return f"No expert session history found for '{session_id}'."
    limit = max(1, min(limit, 200))
    lines = [f"Session '{session_id}' ({len(history)} message(s)):"]
    for entry in history[-limit:]:
        role = entry.get("role", "?")
        content = str(entry.get("content", "")).strip()
        lines.append(f"[{role}] {content}")
    return "\n\n".join(lines)


@mcp.tool(description="List expert chat sessions and stored debug checkpoints.")
def list_combined_sessions() -> str:
    expert_sessions = sorted(expert._sessions.keys())
    lines = [
        "Expert sessions:",
        *(f"  - {name}" for name in expert_sessions),
        "",
        f"Debug checkpoints: {len(_debug_checkpoints)}",
    ]
    return "\n".join(lines)


@mcp.tool(description="Capture the current debugger state as a named checkpoint.")
def save_debug_checkpoint(label: str = "") -> str:
    checkpoint = _capture_debug_checkpoint(label)
    return (
        f"Saved checkpoint #{checkpoint['index']} '{checkpoint['label']}' at {checkpoint['timestamp']}\n\n"
        f"{checkpoint['status']}"
    )


@mcp.tool(description="List saved debugger checkpoints.")
def list_debug_checkpoints() -> str:
    if not _debug_checkpoints:
        return "No debug checkpoints saved."
    return "\n".join(
        f"{entry['index']}: {entry['label']} @ {entry['timestamp']}"
        for entry in _debug_checkpoints
    )


@mcp.tool(description="Get the contents of a saved debugger checkpoint. Use index=-1 for the latest checkpoint.")
def get_debug_checkpoint(index: int = -1) -> str:
    if not _debug_checkpoints:
        return "No debug checkpoints saved."
    if index < 0:
        entry = _debug_checkpoints[-1]
    elif index >= len(_debug_checkpoints):
        return f"Checkpoint index {index} out of range."
    else:
        entry = _debug_checkpoints[index]
    return _format_json(entry)


@mcp.tool(description="Ask the ImGui C++ / LLVM expert a question. Maintains session context across calls.")
async def ask_imgui_expert(question: str, session_id: str = "default") -> str:
    return await expert.ask_imgui_expert(question, session_id=session_id)


@mcp.tool(description="Reset conversation history for an expert session (start fresh).")
def reset_session(session_id: str = "default") -> str:
    return expert.reset_session(session_id=session_id)


@mcp.tool(description="List active expert chat sessions.")
def list_sessions() -> list[str]:
    return expert.list_sessions()


@mcp.tool(description="Search the clang-tidy check catalogue by name, category, or keyword.")
async def search_clang_tidy(query: str, limit: int = 15) -> str:
    return await expert.search_clang_tidy(query=query, limit=limit)


@mcp.tool(description="Fetch full documentation for a specific clang-tidy check from clang.llvm.org.")
async def lookup_clang_tidy(check_name: str) -> str:
    return await expert.lookup_clang_tidy(check_name=check_name)


@mcp.tool(description="Interpret a clang-tidy diagnostic and explain the triggered check.")
async def interpret_clang_tidy(diagnostic: str) -> str:
    return await expert.interpret_clang_tidy(diagnostic=diagnostic)


@mcp.tool(description="Inspect the live state of the running example_sdl3_vulkan process via /proc.")
def get_cpp_process_state() -> str:
    return expert.get_cpp_process_state()


@mcp.tool(description="Get full thread backtraces of the running example_sdl3_vulkan process using lldb batch mode.")
async def get_cpp_thread_backtraces(pid: int = 0) -> str:
    return await expert.get_cpp_thread_backtraces(pid=pid)


@mcp.tool(description="Analyze a core dump file with lldb and return thread backtraces and crash details.")
async def analyze_core_dump(core_path: str = "") -> str:
    return await expert.analyze_core_dump(core_path=core_path)


@mcp.tool(description="Monitor the running example_sdl3_vulkan process via /proc and report thread state changes.")
async def monitor_cpp_process(interval_ms: int = 500, duration_ms: int = 5000) -> str:
    return await expert.monitor_cpp_process(interval_ms=interval_ms, duration_ms=duration_ms)


@mcp.tool(description="Launch a new LLDB debug session for the given program.")
def start_debug_session(
    program: str = lldb.DEFAULT_PROGRAM,
    args: list[str] | None = None,
    cwd: str = lldb.DEFAULT_CWD,
    stop_at_entry: bool = True,
) -> str:
    return lldb.start_debug_session(program=program, args=args, cwd=cwd, stop_at_entry=stop_at_entry)


@mcp.tool(description="Attach LLDB to a running process by PID.")
def attach_debug_session(pid: int) -> str:
    return lldb.attach_debug_session(pid=pid)


@mcp.tool(description="Terminate the current debug session and kill the debugged process.")
def stop_debug_session() -> str:
    return lldb.stop_debug_session()


@mcp.tool(description="Return the current debugger state and recent DAP events.")
def debug_status() -> str:
    return lldb.debug_status()


@mcp.tool(description="Set or update source breakpoints in a file.")
def set_breakpoints(source_path: str, lines: list[int], condition: str | None = None) -> str:
    return lldb.set_breakpoints(source_path=source_path, lines=lines, condition=condition)


@mcp.tool(description="Set a breakpoint on a named function.")
def set_function_breakpoint(name: str, condition: str | None = None) -> str:
    return lldb.set_function_breakpoint(name=name, condition=condition)


@mcp.tool(description="Clear all breakpoints in a source file.")
def clear_breakpoints(source_path: str) -> str:
    return lldb.clear_breakpoints(source_path=source_path)


@mcp.tool(description="Continue execution of a stopped thread, optionally waiting for the next stop event.")
def continue_execution(thread_id: int = 0, wait_for_stop: bool = True, timeout: float = 30.0) -> str:
    return lldb.continue_execution(thread_id=thread_id, wait_for_stop=wait_for_stop, timeout=timeout)


@mcp.tool(description="Pause a running program.")
def pause_execution(thread_id: int = 0) -> str:
    return lldb.pause_execution(thread_id=thread_id)


@mcp.tool(description="Step over the current line.")
def step_over(thread_id: int = 0, wait_timeout: float = 10.0) -> str:
    return lldb.step_over(thread_id=thread_id, wait_timeout=wait_timeout)


@mcp.tool(description="Step into the function call on the current line.")
def step_in(thread_id: int = 0, wait_timeout: float = 10.0) -> str:
    return lldb.step_in(thread_id=thread_id, wait_timeout=wait_timeout)


@mcp.tool(description="Step out of the current function.")
def step_out(thread_id: int = 0, wait_timeout: float = 10.0) -> str:
    return lldb.step_out(thread_id=thread_id, wait_timeout=wait_timeout)


@mcp.tool(description="List all threads in the debugged process.")
def get_threads() -> str:
    return lldb.get_threads()


@mcp.tool(description="Get the call stack for a thread.")
def get_stack_trace(thread_id: int = 0, levels: int = 20) -> str:
    return lldb.get_stack_trace(thread_id=thread_id, levels=levels)


@mcp.tool(description="Get local variables, globals, or registers for a stack frame.")
def get_variables(frame_id: int, scope: str = "locals", depth: int = 3) -> str:
    return lldb.get_variables(frame_id=frame_id, scope=scope, depth=depth)


@mcp.tool(description="Evaluate a C++ expression in the context of a stack frame.")
def evaluate(expression: str, frame_id: int = 0, context: str = "repl") -> str:
    return lldb.evaluate(expression=expression, frame_id=frame_id, context=context)


@mcp.tool(description="Map the current debugger call stack and visible variables into Markdown tables.")
def map_function_calls(
    thread_id: int = 0,
    frames: int = 10,
    include_variables: bool = True,
    max_variables_per_frame: int = 8,
) -> str:
    return lldb.map_function_calls(
        thread_id=thread_id,
        frames=frames,
        include_variables=include_variables,
        max_variables_per_frame=max_variables_per_frame,
    )


@mcp.tool(description="Invoke a function call expression in the current debugger context.")
def call_function(function_call: str, frame_id: int = 0) -> str:
    return lldb.call_function(function_call=function_call, frame_id=frame_id)


@mcp.tool(description="Send a raw LLDB command string to the debugger.")
def lldb_command(command: str) -> str:
    return lldb.lldb_command(command=command)


@mcp.tool(description="Read source code lines around a given file and line.")
def read_source(path: str, line: int, context_lines: int = 10) -> str:
    return lldb.read_source(path=path, line=line, context_lines=context_lines)


@mcp.tool(description="Get both the call stack and local variables for the top frames.")
def inspect_state(thread_id: int = 0, frames: int = 5, vars_depth: int = 2) -> str:
    return lldb.inspect_state(thread_id=thread_id, frames=frames, vars_depth=vars_depth)


if __name__ == "__main__":
    mcp.run(transport="stdio")
