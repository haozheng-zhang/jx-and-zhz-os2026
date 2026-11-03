#!/usr/bin/env python3
"""Check module 2 in a disposable copy; leave source and grading tests intact.

Example:
    python3 tools/check_module2.py --qemu /opt/qemu-4.1.1/bin/qemu-system-riscv64
The QEMU run must exit naturally. A timeout is always a failure.
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


MARKERS = [
    "[Module 2] PASS: 16-bit breakpoint returns",
    "[Module 2] PASS: 32-bit breakpoint returns",
    "[Module 2] PASS: 16-bit illegal instruction returns",
    "[Module 2] PASS: 32-bit illegal instruction returns",
    "[Module 2] PASS: non-timer handlers preserve trap frames",
    "[Module 2] PASS: shutdown at ticks=1000",
]


def run_logged(command, cwd, log_path, timeout, append=False):
    """Record real output and terminate only our own subprocess group on timeout."""
    with log_path.open("a" if append else "w", encoding="utf-8") as log:
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
    shutil.copytree(
        source, destination,
        ignore=shutil.ignore_patterns(".git", "bin", "obj", "__pycache__"),
    )
    init = destination / "kern/init/init.c"
    text = init.read_text(encoding="utf-8")
    if text.count("clock_init();") != 1 or "test_lab3_module2();" in text:
        raise RuntimeError("Expected one unmodified clock_init() call in init.c")
    declaration = "int kern_init(void)"
    if declaration not in text:
        raise RuntimeError("Cannot locate the kern_init declaration")
    text = text.replace(
        declaration, "void test_lab3_module2(void);\n\n" + declaration, 1,
    )
    text = text.replace(
        "clock_init();", "test_lab3_module2();\n    clock_init();", 1,
    )
    init.write_text(text, encoding="utf-8")
    shutil.copyfile(
        source / "tools/test_lab3_module2.c",
        destination / "kern/init/test_lab3_module2.c",
    )
    with (destination / "Makefile").open("a", encoding="utf-8") as makefile:
        makefile.write("\n# Supplementary test build only.\n")
        makefile.write("LDFLAGS += --wrap=sbi_shutdown\n")


def main():
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-riscv64"),
                        help="QEMU executable; must report version 4.1.1")
    parser.add_argument("--timeout", type=float, default=40,
                        help="QEMU run timeout in seconds (default: 40)")
    parser.add_argument("--log-dir", type=Path,
                        default=source.parent.parent / "lab3-evidence/module2-extra",
                        help="Directory for build, QEMU, version and result logs")
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
        code, timed_out = run_logged(
            [executable, "--version"], source, logs / "version.log", 10,
        )
        version_log = (logs / "version.log").read_text(encoding="utf-8", errors="replace")
        version = re.search(r"^QEMU emulator version ([^\s]+)", version_log, re.MULTILINE)
        result["qemu_version"] = version.group(1) if version else None
        if code != 0 or timed_out or result["qemu_version"] != "4.1.1":
            raise RuntimeError("This check requires QEMU 4.1.1; see version.log")

        with tempfile.TemporaryDirectory(prefix="lab3-module2-") as temporary:
            work = Path(temporary) / "code"
            prepare_copy(source, work)
            for index, command in enumerate([
                ["make", "--no-print-directory", "clean"],
                ["make", "--no-print-directory", "-j2"],
            ]):
                code, timed_out = run_logged(
                    command, work, logs / "build.log", 120, append=index != 0,
                )
                if code != 0 or timed_out:
                    raise RuntimeError("Temporary build failed; see build.log")
            command = ["make", "--no-print-directory", "QEMU=" + executable, "qemu"]
            result["run_command"] = command
            code, timed_out = run_logged(command, work, logs / "qemu.log", args.timeout)
            result["returncode"] = code
            result["timed_out"] = timed_out

        output = (logs / "qemu.log").read_text(encoding="utf-8", errors="replace")
        lines = output.splitlines()
        counts = {
            "100_ticks": sum(line.strip() == "100 ticks" for line in lines),
            "breakpoints": sum(line.startswith("Breakpoint at ") for line in lines),
            "illegal_instructions": sum(line.startswith("Illegal instruction at ") for line in lines),
        }
        result["counts"] = counts
        checks = {marker: lines.count(marker) == 1 for marker in MARKERS}
        checks.update({
            "ten 100-tick reports": counts["100_ticks"] == 10,
            "exactly two breakpoint traps": counts["breakpoints"] == 2,
            "exactly two illegal-instruction traps": counts["illegal_instructions"] == 2,
            "natural successful QEMU exit": code == 0 and not timed_out,
        })
        result["checks"] = checks
        result["passed"] = all(checks.values())
        for name, passed in checks.items():
            print(("PASS " if passed else "FAIL ") + name.removeprefix("[Module 2] PASS: "))
    except (OSError, RuntimeError) as error:
        result["error"] = str(error)
        print("FAIL " + str(error), file=sys.stderr)
    finally:
        (logs / "result.json").write_text(
            json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
        )
    print("Evidence: " + str(logs))
    print("Module 2 supplementary check: " + ("PASS" if result["passed"] else "FAIL"))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
