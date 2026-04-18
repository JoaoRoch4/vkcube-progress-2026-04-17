#!/usr/bin/env python3
"""
ImGui / C++ / LLVM Expert MCP Server
=====================================
Provides AI-assisted C++/ImGui/Vulkan/LLVM/Clang Q&A and code assistance,
clang-tidy check lookup, C++ process inspection, and debug helpers.

Can be loaded as a module by the combined cpp-imgui-mcp server, or run
standalone as an MCP server.
"""

from __future__ import annotations

import asyncio
import html as _html
import importlib.util
import json
import os
import re
import subprocess
import time
from pathlib import Path
from typing import Any
from urllib.parse import urljoin
from urllib.request import Request, urlopen

from mcp.server.fastmcp import FastMCP

# ── Configuration ──────────────────────────────────────────────────────────────

_PROCESS_NAME = "example_sdl3_vulkan"
_CLANG_TIDY_BASE = "https://clang.llvm.org/extra/clang-tidy/checks/"
_CLANG_TIDY_LIST = "https://clang.llvm.org/extra/clang-tidy/checks/list.html"
_LLDB_BIN = "/usr/bin/lldb"

SYSTEM_PROMPT = """\
You are an expert C++20 / ImGui / Vulkan / SDL3 / LLVM assistant embedded in a
VS Code Copilot MCP server. You help with:

- Dear ImGui API usage, patterns, and best practices (docking, tables, custom rendering)
- Vulkan and SDL3 integration with ImGui (swapchain, render-pass, descriptor sets)
- Modern C++20 idioms and the LLVM / Clang toolchain (concepts, ranges, modules)
- clang-tidy checks, diagnostics, and suppressions
- LLDB debugging techniques (watchpoints, scripted breakpoints, formatters)
- CMake / Meson build system configuration
- ImRAD UI designer workflow

Always provide concise, correct C++ code examples. Prefer modern C++20 idioms.
When discussing Vulkan, reference VkResult codes and validation-layer messages
accurately. When asked to fix a diagnostic, quote the offending line first.
"""

# ── Session management ─────────────────────────────────────────────────────────

_sessions: dict[str, list[dict[str, str]]] = {}
_clang_tidy_cache: dict[str, str] = {}
_clang_tidy_list_cache: list[dict[str, str]] | None = None

mcp = FastMCP(
    name="imgui-cpp-expert",
    instructions=SYSTEM_PROMPT,
)

# ── HTTP helpers ───────────────────────────────────────────────────────────────


def _fetch(url: str, timeout: int = 15) -> str:
    req = Request(url, headers={"User-Agent": "Mozilla/5.0 (X11; Linux x86_64)"})
    with urlopen(req, timeout=timeout) as resp:
        return resp.read().decode("utf-8", errors="replace")


def _strip_html(raw: str, max_chars: int = 8000) -> str:
    raw = re.sub(
        r"<(script|style|nav|footer|header)[^>]*>.*?</\1>",
        "",
        raw,
        flags=re.DOTALL | re.IGNORECASE,
    )
    raw = re.sub(r"<!--.*?-->", "", raw, flags=re.DOTALL)
    raw = re.sub(r"<[^>]+>", " ", raw)
    raw = _html.unescape(raw)
    raw = re.sub(r"[ \t]+", " ", raw)
    raw = re.sub(r"\n{3,}", "\n\n", raw)
    return raw.strip()[:max_chars]


# ── AI Q&A ─────────────────────────────────────────────────────────────────────


def _try_ai_answer(messages: list[dict[str, str]]) -> str | None:
    """Try Anthropic then OpenAI. Returns None if no API key / package found."""
    anthropic_key = os.environ.get("ANTHROPIC_API_KEY", "").strip()
    if anthropic_key and importlib.util.find_spec("anthropic"):
        try:
            import anthropic  # type: ignore[import]

            client = anthropic.Anthropic(api_key=anthropic_key)
            system = next(
                (m["content"] for m in messages if m["role"] == "system"),
                SYSTEM_PROMPT,
            )
            chat_msgs = [m for m in messages if m["role"] != "system"]
            resp = client.messages.create(
                model="claude-3-5-haiku-20241022",
                max_tokens=2048,
                system=system,
                messages=chat_msgs,
            )
            return resp.content[0].text
        except Exception:
            pass

    openai_key = os.environ.get("OPENAI_API_KEY", "").strip()
    if openai_key and importlib.util.find_spec("openai"):
        try:
            import openai as _openai  # type: ignore[import]

            client = _openai.OpenAI(api_key=openai_key)
            resp = client.chat.completions.create(
                model="gpt-4o-mini",
                messages=messages,  # type: ignore[arg-type]
                max_tokens=2048,
            )
            return resp.choices[0].message.content
        except Exception:
            pass

    return None


def _history_entries(session_id: str) -> list[dict[str, str]]:
    return _sessions.get(session_id, [])


@mcp.tool(
    description="Ask the ImGui C++ / LLVM expert a question. Maintains session context across calls."
)
async def ask_imgui_expert(question: str, session_id: str = "default") -> str:
    history = _sessions.setdefault(session_id, [])
    history.append({"role": "user", "content": question})

    messages: list[dict[str, str]] = [
        {"role": "system", "content": SYSTEM_PROMPT},
        *history,
    ]

    answer = _try_ai_answer(messages)
    if answer is None:
        answer = (
            "ImGui/C++ expert AI is not available in this environment "
            "(set ANTHROPIC_API_KEY or OPENAI_API_KEY to enable AI Q&A).\n\n"
            "Useful references:\n"
            "- ImGui API: https://github.com/ocornut/imgui/blob/master/imgui.h\n"
            "- ImGui wiki: https://github.com/ocornut/imgui/wiki\n"
            "- Use search_clang_tidy() / lookup_clang_tidy() for clang-tidy checks."
        )

    history.append({"role": "assistant", "content": answer})
    return answer


@mcp.tool(description="Reset conversation history for an expert session (start fresh).")
def reset_session(session_id: str = "default") -> str:
    _sessions.pop(session_id, None)
    return f"Session '{session_id}' reset."


@mcp.tool(description="List active expert chat sessions.")
def list_sessions() -> list[str]:
    return list(_sessions.keys())


# ── clang-tidy helpers ─────────────────────────────────────────────────────────


def _fetch_clang_tidy_list() -> list[dict[str, str]]:
    global _clang_tidy_list_cache
    if _clang_tidy_list_cache is not None:
        return _clang_tidy_list_cache

    raw = _fetch(_CLANG_TIDY_LIST)
    matches = re.findall(
        r'<a[^>]+href=["\']([^"\']*\.html)["\'][^>]*>(.*?)</a>',
        raw,
        flags=re.DOTALL | re.IGNORECASE,
    )
    entries: list[dict[str, str]] = []
    for href, text in matches:
        title = _strip_html(text, 200).strip()
        if not title:
            continue
        url = urljoin(_CLANG_TIDY_LIST, href)
        entries.append({"name": title, "url": url})
    _clang_tidy_list_cache = entries
    return entries


@mcp.tool(
    description="Search the clang-tidy check catalogue by name, category, or keyword."
)
async def search_clang_tidy(query: str, limit: int = 15) -> str:
    limit = max(1, min(limit, 50))
    q = query.lower().strip()
    try:
        entries = _fetch_clang_tidy_list()
    except Exception as exc:
        return f"Failed to fetch clang-tidy list: {exc}"

    scored: list[tuple[int, dict[str, str]]] = []
    for entry in entries:
        hay = entry["name"].lower()
        if q not in hay:
            continue
        score = 0 if hay.startswith(q) else (5 if q in hay else 10)
        scored.append((score, entry))

    scored.sort(key=lambda x: (x[0], x[1]["name"]))
    results = [e for _, e in scored[:limit]]
    if not results:
        return f"No clang-tidy checks matched '{query}'."
    lines = [f"clang-tidy checks matching '{query}':"]
    for idx, e in enumerate(results, 1):
        lines.append(f"{idx}. {e['name']}")
        lines.append(f"   {e['url']}")
    return "\n".join(lines)


@mcp.tool(
    description="Fetch full documentation for a specific clang-tidy check from clang.llvm.org."
)
async def lookup_clang_tidy(check_name: str) -> str:
    name = check_name.strip().removesuffix(".html")
    if name in _clang_tidy_cache:
        return _clang_tidy_cache[name]

    candidates: list[str] = []
    if name.startswith("http"):
        candidates.append(name)
    else:
        parts = name.split("-", 1)
        if len(parts) == 2:
            candidates.append(f"{_CLANG_TIDY_BASE}{parts[0]}/{name}.html")
        candidates.append(f"{_CLANG_TIDY_BASE}{name}.html")

    for url in candidates:
        try:
            raw = _fetch(url)
            text = _strip_html(raw, max_chars=6000)
            result = f"URL: {url}\n\n{text}"
            _clang_tidy_cache[name] = result
            return result
        except Exception:
            continue

    return f"Could not fetch clang-tidy documentation for '{check_name}'."


@mcp.tool(
    description="Interpret a clang-tidy diagnostic and explain the triggered check."
)
async def interpret_clang_tidy(diagnostic: str) -> str:
    match = re.search(r"\[([a-z][a-z0-9-]+-[a-z][a-z0-9-]+)\]", diagnostic)
    if not match:
        return (
            "Could not identify a clang-tidy check name in the diagnostic.\n"
            "Expected format: 'warning: message [check-category-name]'\n\n"
            f"Diagnostic: {diagnostic}"
        )
    check_name = match.group(1)
    doc = await lookup_clang_tidy(check_name)
    return f"Triggered check: {check_name}\n\n{doc}"


# ── Process inspection ─────────────────────────────────────────────────────────


@mcp.tool(
    description="Inspect the live state of the running example_sdl3_vulkan process via /proc."
)
def get_cpp_process_state() -> str:
    """Find the example_sdl3_vulkan process and dump its /proc state."""
    pids: list[str] = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            comm = (entry / "comm").read_text().strip()
            if _PROCESS_NAME in comm:
                pids.append(entry.name)
        except OSError:
            continue

    if not pids:
        return f"Process '{_PROCESS_NAME}' is not running."

    lines: list[str] = []
    for pid in pids:
        proc_dir = Path("/proc") / pid
        try:
            status = (proc_dir / "status").read_text()
            cmdline = (proc_dir / "cmdline").read_text().replace("\x00", " ").strip()
            fd_count = sum(1 for _ in (proc_dir / "fd").iterdir())
            lines.append(f"PID {pid}: {cmdline[:120]}")
            lines.append(f"  FDs: {fd_count}")
            for line in status.splitlines():
                if any(
                    line.startswith(k)
                    for k in ("State:", "VmRSS:", "Threads:", "voluntary_ctxt")
                ):
                    lines.append(f"  {line.strip()}")
        except OSError:
            lines.append(f"PID {pid}: (could not read /proc state)")

    return "\n".join(lines) if lines else f"Process '{_PROCESS_NAME}' not found."


@mcp.tool(
    description="Get full thread backtraces of the running example_sdl3_vulkan process using lldb batch mode."
)
async def get_cpp_thread_backtraces(pid: int = 0) -> str:
    """Run lldb in batch mode to get all thread backtraces."""
    if not pid:
        for entry in Path("/proc").iterdir():
            if not entry.name.isdigit():
                continue
            try:
                comm = (entry / "comm").read_text().strip()
                if _PROCESS_NAME in comm:
                    pid = int(entry.name)
                    break
            except OSError:
                continue

    if not pid:
        return f"Process '{_PROCESS_NAME}' is not running."

    lldb = _LLDB_BIN if Path(_LLDB_BIN).exists() else "lldb"
    cmd = [
        lldb, "-p", str(pid),
        "--batch",
        "-o", "thread backtrace all",
        "-o", "quit",
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        output = (result.stdout + result.stderr).strip()
        return output or f"No output from lldb for PID {pid}"
    except subprocess.TimeoutExpired:
        return f"lldb timed out while attaching to PID {pid}"
    except Exception as exc:
        return f"Failed to run lldb: {exc}"


@mcp.tool(
    description="Analyze a core dump file with lldb and return thread backtraces and crash details."
)
async def analyze_core_dump(core_path: str = "") -> str:
    """Use lldb to analyze a core dump file."""
    if not core_path:
        candidates = list(Path("/tmp").glob("core*")) + list(Path(".").glob("core*"))
        if not candidates:
            return (
                "No core dump path specified and no core files found in "
                "/tmp or current directory."
            )
        core_path = str(max(candidates, key=lambda p: p.stat().st_mtime))

    if not Path(core_path).exists():
        return f"Core dump not found: {core_path}"

    lldb = _LLDB_BIN if Path(_LLDB_BIN).exists() else "lldb"
    cmd = [
        lldb, "--core", core_path,
        "--batch",
        "-o", "thread backtrace all",
        "-o", "frame info",
        "-o", "quit",
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        output = (result.stdout + result.stderr).strip()
        return output or "No output from lldb core analysis."
    except subprocess.TimeoutExpired:
        return f"lldb timed out analyzing core dump: {core_path}"
    except Exception as exc:
        return f"Failed to analyze core dump: {exc}"


@mcp.tool(
    description="Monitor the running example_sdl3_vulkan process via /proc and report thread state changes."
)
async def monitor_cpp_process(interval_ms: int = 500, duration_ms: int = 5000) -> str:
    """Poll /proc for the process and report state changes over time."""
    interval = max(100, min(interval_ms, 5000)) / 1000.0
    duration = max(500, min(duration_ms, 30000)) / 1000.0

    pid: int | None = None
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            comm = (entry / "comm").read_text().strip()
            if _PROCESS_NAME in comm:
                pid = int(entry.name)
                break
        except OSError:
            continue

    if not pid:
        return f"Process '{_PROCESS_NAME}' is not running."

    snapshots: list[str] = []
    end_time = time.time() + duration
    last_state = ""
    while time.time() < end_time:
        proc_dir = Path("/proc") / str(pid)
        try:
            status = (proc_dir / "status").read_text()
            state_line = next(
                (l for l in status.splitlines() if l.startswith("State:")), ""
            )
            if state_line != last_state:
                snapshots.append(f"t={time.time():.2f}: {state_line.strip()}")
                last_state = state_line
        except OSError:
            snapshots.append(f"t={time.time():.2f}: process exited")
            break
        await asyncio.sleep(interval)

    if not snapshots:
        return f"No state changes observed for PID {pid} over {duration_ms}ms."
    return (
        f"Process {pid} ({_PROCESS_NAME}) state monitor over {duration_ms}ms:\n"
        + "\n".join(snapshots)
    )


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    mcp.run(transport="stdio")
