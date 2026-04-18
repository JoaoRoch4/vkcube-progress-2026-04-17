import json
import sys


def respond(ok: bool, exit_code: int, message: str) -> None:
    print(json.dumps({"ok": ok, "runtime": "python", "exitCode": exit_code, "message": message}))
    raise SystemExit(exit_code)


raw = sys.argv[1] if len(sys.argv) > 1 else ""
if not raw:
    respond(True, 0, "python worker ready")

try:
    req = json.loads(raw)
except Exception as exc:
    respond(False, 2, f"invalid json request: {exc}")

command = req.get("command", "execute") if isinstance(req, dict) else "execute" # type: ignore
prompt = req.get("prompt", "") if isinstance(req, dict) else "" # type: ignore

if command == "ping":
    respond(True, 0, "pong from python worker")
if command == "echo":
    respond(True, 0, str(prompt))
if command == "execute":
    respond(True, 0, f"python processed: {prompt}")

respond(False, 3, f"unknown command: {command}")
