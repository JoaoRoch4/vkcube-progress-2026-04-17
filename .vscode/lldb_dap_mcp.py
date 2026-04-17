#!/usr/bin/env python3
"""
LLDB DAP MCP Server
===================
Exposes lldb-dap (Debug Adapter Protocol) as MCP tools for Copilot Chat.

Communicates with `/usr/bin/lldb-dap` over stdin/stdout using the standard
Debug Adapter Protocol (DAP) framing:  Content-Length: N\\r\\n\\r\\n<json>

Usage (VS Code mcp.json):
    "lldb-dap": {
        "type": "stdio",
        "command": "/home/joao/miniconda3/envs/imgui-cpp/bin/python",
        "args": ["/home/joao/vscode/vkcube/.vscode/lldb_dap_mcp.py"]
    }
"""

from __future__ import annotations

import json
import os
import subprocess
import threading
from typing import Any, Optional

from mcp.server.fastmcp import FastMCP

# ── Configuration ────────────────────────────────────────────────────────────

LLDB_DAP_BIN = "/usr/bin/lldb-dap"
DEFAULT_PROGRAM = "/home/joao/vscode/vkcube/cmake-build/vkcube"
DEFAULT_CWD = "/home/joao/vscode/vkcube/cmake-build"

# ── DAP Client ───────────────────────────────────────────────────────────────


class DAPClient:
    """
    Synchronous DAP client that spawns an lldb-dap subprocess and communicates
    over its stdin/stdout using Content-Length framing.

    A background reader thread routes:
      - response messages → a per-request threading.Event + result dict
      - event messages    → an event queue (latest N events kept)
    """

    MAX_EVENTS = 200  # rolling event buffer size

    def __init__(self) -> None:
        self._seq: int = 0
        self._lock = threading.Lock()
        self._pending: dict[int, threading.Event] = {}
        self._responses: dict[int, dict] = {}
        self._events: list[dict] = []
        self._events_lock = threading.Lock()
        self._proc: Optional[subprocess.Popen] = None
        self._reader: Optional[threading.Thread] = None
        self._running: bool = False

        # "stopped" state
        self._stopped_event = threading.Event()
        self._current_thread_id: Optional[int] = None
        self._stop_reason: Optional[str] = None

    # ── internal helpers ─────────────────────────────────────────────────────

    def _next_seq(self) -> int:
        with self._lock:
            self._seq += 1
            return self._seq

    def _send(self, msg: dict) -> None:
        data = json.dumps(msg, separators=(",", ":"))
        header = f"Content-Length: {len(data)}\r\n\r\n"
        assert self._proc and self._proc.stdin
        self._proc.stdin.write((header + data).encode())
        self._proc.stdin.flush()

    def _read_one(self) -> Optional[dict]:
        """Read a single DAP message from the subprocess stdout."""
        assert self._proc and self._proc.stdout
        try:
            headers: dict[str, str] = {}
            while True:
                raw = self._proc.stdout.readline()
                if not raw:
                    return None
                line = raw.decode(errors="replace").rstrip("\r\n")
                if not line:
                    break
                key, _, val = line.partition(":")
                headers[key.strip().lower()] = val.strip()

            length = int(headers.get("content-length", 0))
            if not length:
                return None
            body = self._proc.stdout.read(length)
            return json.loads(body.decode(errors="replace"))
        except Exception:
            return None

    def _reader_loop(self) -> None:
        """Background thread: route messages to waiters or event buffer."""
        while self._running:
            msg = self._read_one()
            if msg is None:
                break

            mtype = msg.get("type")
            if mtype == "response":
                seq = msg.get("request_seq", -1)
                with self._lock:
                    evt = self._pending.get(seq)
                    if evt is not None:
                        self._responses[seq] = msg
                        evt.set()

            elif mtype == "event":
                with self._events_lock:
                    self._events.append(msg)
                    if len(self._events) > self.MAX_EVENTS:
                        self._events = self._events[-self.MAX_EVENTS :]

                ename = msg.get("event", "")
                if ename == "stopped":
                    body = msg.get("body", {})
                    self._current_thread_id = body.get("threadId")
                    self._stop_reason = body.get("reason", "")
                    self._stopped_event.set()
                elif ename == "continued":
                    self._stopped_event.clear()
                    self._stop_reason = None

    # ── public API ───────────────────────────────────────────────────────────

    def request(
        self,
        command: str,
        arguments: Optional[dict] = None,
        timeout: float = 15.0,
    ) -> dict:
        """
        Send a DAP request and block until the response arrives.
        Returns the full response dict.  Raises TimeoutError on timeout.
        """
        seq = self._next_seq()
        msg: dict[str, Any] = {"seq": seq, "type": "request", "command": command}
        if arguments:
            msg["arguments"] = arguments

        waiter = threading.Event()
        with self._lock:
            self._pending[seq] = waiter

        self._send(msg)

        if not waiter.wait(timeout):
            with self._lock:
                self._pending.pop(seq, None)
            raise TimeoutError(f"DAP '{command}' timed out after {timeout}s")

        with self._lock:
            self._pending.pop(seq, None)
            return self._responses.pop(seq, {})

    def start(
        self,
        program: str,
        args: list[str] | None = None,
        cwd: str | None = None,
        stop_at_entry: bool = False,
        env: dict[str, str] | None = None,
    ) -> dict:
        """Spawn lldb-dap, initialize, then launch the program."""
        if self._proc is not None:
            raise RuntimeError("Debug session already active — call stop() first")

        self._proc = subprocess.Popen(
            [LLDB_DAP_BIN],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self._running = True
        self._reader = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader.start()

        # Step 1: initialize
        self.request(
            "initialize",
            {
                "clientID": "mcp-lldb-dap",
                "clientName": "MCP LLDB DAP",
                "adapterID": "lldb-dap",
                "pathFormat": "path",
                "linesStartAt1": True,
                "columnsStartAt1": True,
                "supportsVariableType": True,
                "supportsVariablePaging": False,
                "supportsRunInTerminalRequest": False,
                "locale": "en-US",
            },
        )

        # Step 2: launch
        launch_args: dict[str, Any] = {
            "program": program,
            "args": args or [],
            "cwd": cwd or os.path.dirname(program),
            "stopOnEntry": stop_at_entry,
            "noDebug": False,
        }
        if env:
            launch_args["env"] = env

        launch_resp = self.request("launch", launch_args, timeout=30.0)

        # Step 3: configurationDone
        self.request("configurationDone")

        return launch_resp

    def attach(self, pid: int) -> dict:
        """Attach to a running process by PID."""
        if self._proc is not None:
            raise RuntimeError("Debug session already active")

        self._proc = subprocess.Popen(
            [LLDB_DAP_BIN],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self._running = True
        self._reader = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader.start()

        self.request(
            "initialize",
            {
                "clientID": "mcp-lldb-dap",
                "clientName": "MCP LLDB DAP",
                "adapterID": "lldb-dap",
                "pathFormat": "path",
                "linesStartAt1": True,
                "columnsStartAt1": True,
                "supportsVariableType": True,
                "supportsVariablePaging": False,
                "supportsRunInTerminalRequest": False,
                "locale": "en-US",
            },
        )

        resp = self.request("attach", {"pid": pid}, timeout=20.0)
        self.request("configurationDone")
        return resp

    def stop(self) -> None:
        """Disconnect and terminate the debug session."""
        if self._proc is None:
            return
        try:
            self.request("disconnect", {"terminateDebuggee": True}, timeout=5.0)
        except Exception:
            pass
        self._running = False
        self._proc.terminate()
        self._proc = None
        self._current_thread_id = None
        self._stop_reason = None
        self._stopped_event.clear()

    @property
    def is_active(self) -> bool:
        return self._proc is not None and self._proc.poll() is None

    @property
    def is_stopped(self) -> bool:
        return self._stopped_event.is_set()

    def wait_for_stop(self, timeout: float = 60.0) -> Optional[str]:
        """Block until the program stops.  Returns stop reason or None on timeout."""
        self._stopped_event.wait(timeout)
        return self._stop_reason

    def recent_events(self, n: int = 20) -> list[dict]:
        with self._events_lock:
            return list(self._events[-n:])

    def clear_events(self) -> None:
        with self._events_lock:
            self._events.clear()


# ── Singleton session ─────────────────────────────────────────────────────────

_client = DAPClient()

# Breakpoint bookkeeping:  {source_path: {line: bp_id}}
_bp_map: dict[str, dict[int, int]] = {}


def _require_active() -> str | None:
    """Return an error string if no active session, else None."""
    if not _client.is_active:
        return "No active debug session.  Call start_debug_session() first."
    return None


def _fmt_resp(resp: dict, extra: str = "") -> str:
    """Format a DAP response for human consumption."""
    success = resp.get("success", False)
    msg = resp.get("message", "")
    body = resp.get("body") or {}
    out = "OK" if success else f"FAILED: {msg}"
    if extra:
        out += f"\n{extra}"
    if body:
        out += "\n" + json.dumps(body, indent=2)
    return out


# ── MCP Server ────────────────────────────────────────────────────────────────

mcp = FastMCP(
    name="lldb-dap",
    instructions=(
        "Interact with the LLDB debugger via the Debug Adapter Protocol. "
        "Start with start_debug_session(), then use breakpoints and stepping tools. "
        "Always check debug_status() after a step or continue to see where execution stopped."
    ),
)


# ── Session management ────────────────────────────────────────────────────────


@mcp.tool(
    description=(
        "Launch a new LLDB debug session for the given program. "
        "Optionally pass args, cwd, stop_at_entry. "
        "Default program is the vkcube binary."
    )
)
def start_debug_session(
    program: str = DEFAULT_PROGRAM,
    args: list[str] | None = None,
    cwd: str = DEFAULT_CWD,
    stop_at_entry: bool = True,
) -> str:
    """Spawn lldb-dap and launch the program under debug control."""
    global _client, _bp_map
    if _client.is_active:
        return "Session already active.  Call stop_debug_session() first."

    _client = DAPClient()
    _bp_map = {}
    try:
        resp = _client.start(program, args, cwd, stop_at_entry)
        status = "STOPPED AT ENTRY" if stop_at_entry else "RUNNING"
        return f"Debug session launched — {status}\n{_fmt_resp(resp)}"
    except Exception as exc:
        return f"Failed to start session: {exc}"


@mcp.tool(
    description="Attach LLDB to a running process by PID."
)
def attach_debug_session(pid: int) -> str:
    """Attach lldb-dap to an already-running process."""
    global _client, _bp_map
    if _client.is_active:
        return "Session already active.  Call stop_debug_session() first."
    _client = DAPClient()
    _bp_map = {}
    try:
        resp = _client.attach(pid)
        return f"Attached to PID {pid}\n{_fmt_resp(resp)}"
    except Exception as exc:
        return f"Failed to attach: {exc}"


@mcp.tool(description="Terminate the current debug session and kill the debugged process.")
def stop_debug_session() -> str:
    """Disconnect lldb-dap and terminate the debugged process."""
    if not _client.is_active:
        return "No active session."
    _client.stop()
    return "Debug session stopped."


@mcp.tool(
    description=(
        "Return the current debugger state: active/stopped, current thread ID, "
        "stop reason, and the last 10 DAP events."
    )
)
def debug_status() -> str:
    """Get a snapshot of the current debug session state."""
    if not _client.is_active:
        return "No active debug session."

    lines = [
        f"active:    {_client.is_active}",
        f"stopped:   {_client.is_stopped}",
        f"thread_id: {_client._current_thread_id}",
        f"reason:    {_client._stop_reason or '—'}",
        "",
        "Recent events (last 10):",
    ]
    for evt in _client.recent_events(10):
        ename = evt.get("event", evt.get("type", "?"))
        body = evt.get("body") or {}
        lines.append(f"  [{ename}] {json.dumps(body)}")

    return "\n".join(lines)


# ── Breakpoints ───────────────────────────────────────────────────────────────


@mcp.tool(
    description=(
        "Set (or update) source breakpoints in a file. "
        "Provide a list of line numbers; an optional condition can be a plain C++ expression string. "
        "Returns the list of verified breakpoints."
    )
)
def set_breakpoints(
    source_path: str,
    lines: list[int],
    condition: str | None = None,
) -> str:
    """Set source-line breakpoints in source_path at the given lines."""
    err = _require_active()
    if err:
        return err

    bp_list = []
    for ln in lines:
        bp: dict[str, Any] = {"line": ln}
        if condition:
            bp["condition"] = condition
        bp_list.append(bp)

    resp = _client.request(
        "setBreakpoints",
        {
            "source": {"path": source_path},
            "breakpoints": bp_list,
        },
    )

    # Update our bookkeeping
    _bp_map[source_path] = {}
    verified = []
    for bp_resp in (resp.get("body") or {}).get("breakpoints", []):
        bid = bp_resp.get("id")
        bline = bp_resp.get("line", 0)
        bverif = bp_resp.get("verified", False)
        if bid is not None and bline:
            _bp_map[source_path][bline] = bid
        verified.append(
            f"  line {bline}: {'✓ verified' if bverif else '✗ unverified'}"
            + (f" (id={bid})" if bid else "")
        )

    return f"Breakpoints in {source_path}:\n" + "\n".join(verified)


@mcp.tool(description="Set a breakpoint on a named function (function breakpoint).")
def set_function_breakpoint(name: str, condition: str | None = None) -> str:
    """Set a function breakpoint by name (e.g. 'main', 'VulkanRenderer::render')."""
    err = _require_active()
    if err:
        return err

    bp: dict[str, Any] = {"name": name}
    if condition:
        bp["condition"] = condition

    resp = _client.request("setFunctionBreakpoints", {"breakpoints": [bp]})
    bps = (resp.get("body") or {}).get("breakpoints", [])
    if not bps:
        return f"No breakpoints set for function '{name}'"
    b = bps[0]
    return (
        f"Function breakpoint '{name}': "
        f"{'verified' if b.get('verified') else 'unverified'} "
        f"(id={b.get('id', '?')})"
    )


@mcp.tool(description="Clear all breakpoints in a source file.")
def clear_breakpoints(source_path: str) -> str:
    """Remove all breakpoints from a source file."""
    err = _require_active()
    if err:
        return err

    resp = _client.request(
        "setBreakpoints",
        {"source": {"path": source_path}, "breakpoints": []},
    )
    _bp_map.pop(source_path, None)
    return f"Cleared all breakpoints in {source_path} — {_fmt_resp(resp)}"


# ── Execution control ─────────────────────────────────────────────────────────


@mcp.tool(
    description=(
        "Continue execution of a stopped thread (or all threads if thread_id is 0). "
        "Optionally wait for the next stop event (blocking)."
    )
)
def continue_execution(thread_id: int = 0, wait_for_stop: bool = True, timeout: float = 30.0) -> str:
    """Resume execution from a breakpoint or step."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1
    resp = _client.request("continue", {"threadId": tid})

    if not wait_for_stop:
        return _fmt_resp(resp)

    reason = _client.wait_for_stop(timeout)
    if reason is None:
        return f"Continued (still running after {timeout}s — use debug_status() to check)"
    return f"Stopped: reason={reason!r}, thread={_client._current_thread_id}"


@mcp.tool(description="Pause a running program (equivalent to Ctrl-C / 'interrupt').")
def pause_execution(thread_id: int = 0) -> str:
    """Pause the running process."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1
    resp = _client.request("pause", {"threadId": tid})
    return _fmt_resp(resp)


@mcp.tool(
    description=(
        "Step over the current line (execute one source line without entering calls). "
        "Blocks until the next stop."
    )
)
def step_over(thread_id: int = 0, wait_timeout: float = 10.0) -> str:
    """Step over (next line, same scope)."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1
    _client.request("next", {"threadId": tid})
    reason = _client.wait_for_stop(wait_timeout)
    return f"step_over → stopped: reason={reason!r}, thread={_client._current_thread_id}"


@mcp.tool(description="Step into the function call on the current line.")
def step_in(thread_id: int = 0, wait_timeout: float = 10.0) -> str:
    """Step in (enter the next function call)."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1
    _client.request("stepIn", {"threadId": tid})
    reason = _client.wait_for_stop(wait_timeout)
    return f"step_in → stopped: reason={reason!r}, thread={_client._current_thread_id}"


@mcp.tool(description="Step out of the current function (run until the caller resumes).")
def step_out(thread_id: int = 0, wait_timeout: float = 10.0) -> str:
    """Step out (finish current function and stop in caller)."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1
    _client.request("stepOut", {"threadId": tid})
    reason = _client.wait_for_stop(wait_timeout)
    return f"step_out → stopped: reason={reason!r}, thread={_client._current_thread_id}"


# ── Inspection ────────────────────────────────────────────────────────────────


@mcp.tool(description="List all threads in the debugged process with their IDs and names.")
def get_threads() -> str:
    """Return all threads with their IDs, names, and stop status."""
    err = _require_active()
    if err:
        return err

    resp = _client.request("threads")
    threads = (resp.get("body") or {}).get("threads", [])
    if not threads:
        return "No threads (program may not be running)."

    lines = []
    for t in threads:
        marker = " ← current" if t.get("id") == _client._current_thread_id else ""
        lines.append(f"  [{t['id']}] {t.get('name', '?')}{marker}")

    return f"Threads ({len(threads)}):\n" + "\n".join(lines)


@mcp.tool(
    description=(
        "Get the call stack (stack trace) for a thread. "
        "Returns frame IDs, function names, source paths, and line numbers. "
        "Use the frame_id values with get_variables() and evaluate()."
    )
)
def get_stack_trace(thread_id: int = 0, levels: int = 20) -> str:
    """Return the call stack for a thread."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1
    resp = _client.request(
        "stackTrace",
        {"threadId": tid, "startFrame": 0, "levels": levels},
    )
    frames = (resp.get("body") or {}).get("stackFrames", [])
    if not frames:
        return "Empty stack trace."

    lines = []
    for f in frames:
        src = f.get("source") or {}
        path = src.get("path", src.get("name", "?"))
        line = f.get("line", 0)
        name = f.get("name", "?")
        fid = f.get("id", "?")
        lines.append(f"  #{fid:>5}  {name}  [{path}:{line}]")

    return f"Stack (thread {tid}):\n" + "\n".join(lines)


@mcp.tool(
    description=(
        "Get local variables (and optionally globals/registers) for a stack frame. "
        "frame_id comes from get_stack_trace(). "
        "scope: 'locals' (default), 'globals', 'registers'."
    )
)
def get_variables(frame_id: int, scope: str = "locals", depth: int = 3) -> str:
    """
    Return variables for a frame. Recursively expands structured values up to `depth` levels.
    """
    err = _require_active()
    if err:
        return err

    # First get scopes for this frame
    scopes_resp = _client.request("scopes", {"frameId": frame_id})
    scopes = (scopes_resp.get("body") or {}).get("scopes", [])

    # Match requested scope name
    scope_lower = scope.lower()
    mapping = {"locals": "locals", "arguments": "locals", "globals": "globals", "registers": "registers"}
    want = mapping.get(scope_lower, "locals")

    vars_ref: Optional[int] = None
    for s in scopes:
        sname = s.get("name", "").lower()
        if want in sname or sname in want:
            vars_ref = s.get("variablesReference")
            break

    if vars_ref is None and scopes:
        # Fallback: first scope
        vars_ref = scopes[0].get("variablesReference")

    if not vars_ref:
        return f"No '{scope}' scope found for frame {frame_id}"

    def _expand(ref: int, indent: int, remaining: int) -> list[str]:
        if remaining <= 0:
            return []
        resp = _client.request("variables", {"variablesReference": ref})
        variables = (resp.get("body") or {}).get("variables", [])
        result = []
        pad = "  " * indent
        for v in variables:
            vtype = v.get("type", "")
            value = v.get("value", "")
            name = v.get("name", "?")
            child_ref = v.get("variablesReference", 0)
            result.append(f"{pad}{name}: {vtype} = {value}")
            if child_ref and remaining > 1:
                result.extend(_expand(child_ref, indent + 1, remaining - 1))
        return result

    lines = _expand(vars_ref, 0, depth)
    return f"Variables [{scope}, frame {frame_id}]:\n" + ("\n".join(lines) or "  (empty)")


@mcp.tool(
    description=(
        "Evaluate a C++ expression in the context of a stack frame. "
        "frame_id comes from get_stack_trace() — use the innermost frame. "
        "Examples: 'x + y', '*ptr', 'obj.member', 'sizeof(int)'."
    )
)
def evaluate(expression: str, frame_id: int = 0, context: str = "repl") -> str:
    """Evaluate a C/C++ expression (or LLDB command) in the debugger."""
    err = _require_active()
    if err:
        return err

    args: dict[str, Any] = {
        "expression": expression,
        "context": context,  # 'watch' | 'repl' | 'hover' | 'clipboard' | 'variables'
    }
    if frame_id:
        args["frameId"] = frame_id

    resp = _client.request("evaluate", args)
    if not resp.get("success"):
        return f"Evaluation failed: {resp.get('message', '?')}"

    body = resp.get("body") or {}
    result = body.get("result", "")
    rtype = body.get("type", "")
    vref = body.get("variablesReference", 0)

    out = f"= {result}"
    if rtype:
        out += f"  ({rtype})"
    if vref:
        out += f"\n  [expandable — use get_variables with variablesReference={vref}]"
    return out


@mcp.tool(
    description=(
        "Send a raw LLDB command string to the debugger (via the 'repl' evaluate context). "
        "Examples: 'bt', 'frame info', 'memory read 0x...', 'register read', 'p myvar'."
    )
)
def lldb_command(command: str) -> str:
    """Execute any LLDB debugger command and return its output."""
    err = _require_active()
    if err:
        return err

    # Use the LLDB custom 'command' context if available, else repl
    resp = _client.request(
        "evaluate",
        {"expression": command, "context": "repl"},
    )
    body = resp.get("body") or {}
    return body.get("result", resp.get("message", "no output"))


@mcp.tool(
    description=(
        "Read N lines of source code centred around a given line in a file. "
        "Useful to see the context around a breakpoint or crash location."
    )
)
def read_source(path: str, line: int, context_lines: int = 10) -> str:
    """Return source code lines around a location."""
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            all_lines = fh.readlines()
    except OSError as exc:
        return f"Cannot read {path}: {exc}"

    total = len(all_lines)
    start = max(0, line - 1 - context_lines)
    end = min(total, line - 1 + context_lines + 1)
    result_lines = []
    for i in range(start, end):
        marker = ">>>" if (i + 1) == line else "   "
        result_lines.append(f"{i + 1:5} {marker} {all_lines[i].rstrip()}")

    return f"{path}:{line}\n" + "\n".join(result_lines)


@mcp.tool(
    description=(
        "Get both the call stack and local variables for the top N frames in one shot. "
        "Convenient shortcut after a breakpoint or crash."
    )
)
def inspect_state(thread_id: int = 0, frames: int = 5, vars_depth: int = 2) -> str:
    """Return stack + locals in a single call — the most useful 'where am I?' tool."""
    err = _require_active()
    if err:
        return err

    tid = thread_id or _client._current_thread_id or 1

    # --- threads summary ---
    t_resp = _client.request("threads")
    all_threads = (t_resp.get("body") or {}).get("threads", [])
    thread_lines = []
    for t in all_threads:
        mark = " ← current" if t["id"] == tid else ""
        thread_lines.append(f"  [{t['id']}] {t.get('name','?')}{mark}")

    # --- stack ---
    st_resp = _client.request(
        "stackTrace", {"threadId": tid, "startFrame": 0, "levels": frames}
    )
    stack_frames = (st_resp.get("body") or {}).get("stackFrames", [])

    out = [
        f"=== Debug state  (thread {tid}, reason={_client._stop_reason!r}) ===",
        "",
        "Threads:",
        *thread_lines,
        "",
        "Call stack:",
    ]

    for f in stack_frames:
        src = f.get("source") or {}
        path = src.get("path", src.get("name", "?"))
        ln = f.get("line", 0)
        name = f.get("name", "?")
        fid = f.get("id")
        out.append(f"  #{fid}  {name}  [{path}:{ln}]")

        if fid is not None:
            # Scopes → locals
            sc_resp = _client.request("scopes", {"frameId": fid})
            scopes = (sc_resp.get("body") or {}).get("scopes", [])
            for sc in scopes:
                if "local" in sc.get("name", "").lower():
                    vref = sc.get("variablesReference")
                    if vref:
                        vr = _client.request("variables", {"variablesReference": vref})
                        vs = (vr.get("body") or {}).get("variables", [])
                        for v in vs[:12]:  # cap at 12 vars per frame
                            out.append(
                                f"       {v.get('name','?')}: "
                                f"{v.get('type','')} = {v.get('value','')}"
                            )
                    break

    return "\n".join(out)


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    mcp.run(transport="stdio")
