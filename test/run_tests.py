"""Native Simon tests. Python 3 and g++ are required; no board is needed."""

from pathlib import Path
import shutil
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    out = root / ".pio" / "native-tests"
    out.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which("g++")
    if not compiler:
        raise SystemExit("g++ is required")

    common = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
              "-I", str(root / "include"), "-I", str(root / "test" / "host")]

    def build_and_run(name, sources, flags=()):
        exe = out / (name + (".exe" if sys.platform == "win32" else ""))
        subprocess.run([compiler, *common, *flags,
                        *[str(root / source) for source in sources],
                        "-o", str(exe)], check=True, timeout=120)
        subprocess.run([str(exe)], check=True, timeout=120)

    build_and_run("core", [
        "test/host/core_tests.cpp", "test/host/MockBoard.cpp", "src/Application.cpp",
        "src/game/Puzzle.cpp", "src/game/Rules.cpp", "src/game/Game.cpp",
        "src/game/Buttons.cpp", "src/game/Display.cpp", "src/communication/Node.cpp",
    ])
    build_and_run("hardware", [
        "test/host/hardware_tests.cpp", "src/drivers/Board.cpp",
        "src/drivers/Clock.cpp", "src/drivers/TwiSlave.cpp", "src/drivers/Tm1637.cpp",
    ], ["-DF_CPU=16000000UL", "-I", str(root / "test" / "host" / "stubs")])


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
