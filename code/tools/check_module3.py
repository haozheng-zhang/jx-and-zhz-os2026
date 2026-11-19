#!/usr/bin/env python3
"""Run Lab4 module 3 boundary checks in a disposable project copy.

    python3 tools/check_module3.py --qemu qemu-system-riscv64

Requires QEMU 4.1.1. Original sources and official grading tests are not edited.
Selection fixtures capture proc_run; the second phase performs real switches.
A timeout or missing check fails the run. Raw logs and JSON are retained.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time


MARKERS = [
    "empty/all-blocked lists fall back to idle",
    "sole runnable current is selected without switching",
    "circular selection skips sleeping/zombie entries",
    "proc_run(current) is a no-op with SIE off/on",
    "18 real switches pass context round-trip and register sentinel checks",
    "SIE off/on restored after real round trips",
    "distinct Sv39 root PPNs selected and restored",
    "switch entry has consistent current/satp with SIE off",
]


def run_logged(command, cwd, path, timeout, append=False):
    with path.open("a" if append else "w", encoding="utf-8") as log:
        log.write("$ " + shlex.join(command) + "\n")
        log.flush()
        process = subprocess.Popen(
            command, cwd=cwd, stdin=subprocess.DEVNULL,
            stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
        )
        try:
            return process.wait(timeout=timeout), False
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            log.write("\n[runner] TIMEOUT: this run failed.\n")
            return process.returncode, True


def prepare_copy(source, destination):
    shutil.copytree(source, destination,
                    ignore=shutil.ignore_patterns(".git", "obj", "bin", "__pycache__"))
    init = destination / "kern/init/init.c"
    text = init.read_text(encoding="utf-8")
    if text.count("proc_init();") != 1 or "test_lab4_module3();" in text:
        raise RuntimeError("Expected one original proc_init() call in init.c")
    declaration = "int kern_init(void)"
    if declaration not in text:
        raise RuntimeError("Cannot locate kern_init declaration")
    text = text.replace(declaration, "void test_lab4_module3(void);\n\n" + declaration, 1)
    text = text.replace("proc_init();", "proc_init();\n    test_lab4_module3();", 1)
    init.write_text(text, encoding="utf-8")
    for name in ["test_lab4_module3.c", "test_lab4_module3_context.S"]:
        shutil.copyfile(source / "tools" / name, destination / "kern/init" / name)
    with (destination / "Makefile").open("a", encoding="utf-8") as makefile:
        makefile.write("\n# Supplementary test build only.\n")
        makefile.write("LDFLAGS += --wrap=proc_run --wrap=switch_to\n")


def main():
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-riscv64"))
    parser.add_argument("--timeout", type=float, default=20,
                        help="QEMU timeout in seconds (default: 20)")
    parser.add_argument("--log-dir", type=Path,
                        default=source.parent.parent / "lab4-evidence/module3-extra")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    logs = args.log_dir.expanduser().resolve()
    logs.mkdir(parents=True, exist_ok=True)
    result = {"passed": False, "source": str(source), "checks": {}}
    try:
        executable = shutil.which(args.qemu)
        if executable is None:
            raise RuntimeError("QEMU executable not found: " + args.qemu)
        executable = str(Path(executable).resolve())
        result["qemu"] = executable
        code, timed_out = run_logged([executable, "--version"], source, logs / "version.log", 10)
        output = (logs / "version.log").read_text(encoding="utf-8", errors="replace")
        version = re.search(r"^QEMU emulator version ([^\s]+)", output, re.MULTILINE)
        result["qemu_version"] = version.group(1) if version else None
        if code != 0 or timed_out or result["qemu_version"] != "4.1.1":
            raise RuntimeError("QEMU 4.1.1 is required; see version.log")
        with tempfile.TemporaryDirectory(prefix="lab4-module3-") as temporary:
            work = Path(temporary) / "code"
            prepare_copy(source, work)
            for index, command in enumerate([
                ["make", "--no-print-directory", "clean"],
                ["make", "--no-print-directory", "-j2"],
            ]):
                code, timed_out = run_logged(command, work, logs / "build.log", 120,
                                             append=index != 0)
                if code != 0 or timed_out:
                    raise RuntimeError("Temporary build failed; see build.log")
            command = ["make", "--no-print-directory", "QEMU=" + executable, "qemu"]
            result["run_command"] = command
            started = time.monotonic()
            code, timed_out = run_logged(command, work, logs / "qemu.log", args.timeout)
            result.update(returncode=code, timed_out=timed_out,
                          elapsed_seconds=round(time.monotonic() - started, 3))
        output = (logs / "qemu.log").read_text(encoding="utf-8", errors="replace")
        lines = output.splitlines()
        checks = {name: lines.count("[Module 3] PASS: " + name) == 1 for name in MARKERS}
        checks["all checks reached real SBI shutdown"] = lines.count(
            "[Module 3] ALL CHECKS PASSED; requesting SBI shutdown") == 1
        checks["natural successful QEMU exit"] = code == 0 and not timed_out
        checks["no kernel panic"] = "kernel panic" not in output.lower()
        result["checks"] = checks
        result["passed"] = all(checks.values())
        for name, passed in checks.items():
            print(("PASS " if passed else "FAIL ") + name)
    except (OSError, RuntimeError) as error:
        result["error"] = str(error)
        print("FAIL " + str(error), file=sys.stderr)
    finally:
        (logs / "result.json").write_text(json.dumps(result, indent=2, ensure_ascii=False)
                                          + "\n", encoding="utf-8")
    print("Evidence: " + str(logs))
    print("Module 3 supplementary check: " + ("PASS" if result["passed"] else "FAIL"))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
