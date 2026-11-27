"""Portable project commands for Windows, Linux and macOS.

Use the virtual environment's Python to run this file. Commands execute from the
repository root even when launched elsewhere. Engine discovery never installs or
upgrades Unreal; select an existing UE 5.2 installation with --ue-root or UE_ROOT.
"""

import argparse
import importlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROJECT_NAME = "AerialWorldLab"
COMMANDS = [
    "doctor",
    "build",
    "editor",
    "play",
    "play-windowed",
    "play-low",
    "headless",
    "profile",
    "test",
    "python-test",
    "cpp-test",
    "readability-check",
    "unreal-test",
    "prepare-new-york",
    "import-new-york",
    "bootstrap-city",
]
ASSET_STEPS = [
    "download_new_york.py",
    "prepare_new_york.py",
    "prepare_detailed_new_york.py",
    "batch_new_york.py",
    "optimize_new_york.py",
]


def engine_candidates(system, environment, home):
    """Recognize conventional installs without embedding a developer's username."""
    if system == "Windows":
        program_files = environment.get("ProgramFiles", "C:/Program Files")
        return [Path(program_files) / "Epic Games/UE_5.2"]
    if system == "Darwin":
        return [Path("/Users/Shared/Epic Games/UE_5.2"), Path("/Users/Shared/UnrealEngine/UE_5.2")]
    if system == "Linux":
        return [home / "UnrealEngine/UE_5.2", home / "UnrealEngine", home / "UnrealEngine-5.2"]
    raise ValueError("Native commands support Windows, Linux and macOS desktop hosts.")


def find_engine(explicit, system, environment, home):
    """An explicit invalid path must fail instead of silently selecting another engine."""
    requested = explicit
    if not requested:
        requested = environment.get("UE_ROOT")
    if requested:
        candidate = Path(requested).expanduser().resolve()
        if not (candidate / "Engine/Build/Build.version").is_file():
            raise ValueError("UE_ROOT must contain Engine/Build/Build.version: " + str(candidate))
        return candidate
    for candidate in engine_candidates(system, environment, home):
        if (candidate / "Engine/Build/Build.version").is_file():
            return candidate.resolve()
    raise ValueError("UE 5.2 was not found. Set UE_ROOT or pass --ue-root /path/to/UE_5.2.")


def validate_engine(root):
    """Reject an accidental engine upgrade before UBT rewrites/builds project outputs."""
    version = json.loads((root / "Engine/Build/Build.version").read_text(encoding="utf-8"))
    if version.get("MajorVersion") != 5 or version.get("MinorVersion") != 2:
        raise ValueError(
            "This project targets UE 5.2; the selected installation is " + str(version)
        )


def engine_tools(root, system):
    """Return the platform target, build entry point and editor executable."""
    if system == "Windows":
        return (
            "Win64",
            root / "Engine/Build/BatchFiles/Build.bat",
            root / "Engine/Binaries/Win64/UnrealEditor.exe",
        )
    if system == "Darwin":
        return (
            "Mac",
            root / "Engine/Build/BatchFiles/Mac/Build.sh",
            root / "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor",
        )
    if system == "Linux":
        return (
            "Linux",
            root / "Engine/Build/BatchFiles/Linux/Build.sh",
            root / "Engine/Binaries/Linux/UnrealEditor",
        )
    raise ValueError("Unsupported desktop platform: " + system)


def execute(arguments, project_root, dry_run=False):
    """Pass separate arguments to subprocess; paths with spaces stay intact."""
    if isinstance(arguments, str):
        # A Windows cmd.exe command line must reach CreateProcess unchanged;
        # list2cmdline around it a second time would corrupt cmd's outer quotes.
        command = arguments
        display = arguments
    else:
        command = [str(argument) for argument in arguments]
        display = shlex.join(command)
    print("Running:", display, flush=True)
    if not dry_run:
        subprocess.run(command, cwd=project_root, check=True)


def run_script(name, project_root, dry_run):
    execute([sys.executable, project_root / "scripts" / name], project_root, dry_run)


def build_engine(engine_root, system, project_root, dry_run):
    target, build, _editor = engine_tools(engine_root, system)
    arguments = [
        build,
        PROJECT_NAME + "Editor",
        target,
        "Development",
        "-Project=" + str(project_root / (PROJECT_NAME + ".uproject")),
        "-WaitMutex",
    ]
    # Windows batch files need cmd.exe. list2cmdline handles paths with spaces;
    # /s /c requires one outer pair of quotes around the complete command line.
    if system == "Windows":
        command_line = subprocess.list2cmdline([str(argument) for argument in arguments])
        command_processor = subprocess.list2cmdline([os.environ.get("COMSPEC", "cmd.exe")])
        arguments = command_processor + ' /d /s /c "' + command_line + '"'
    else:
        arguments.insert(0, "/bin/bash")
    execute(arguments, project_root, dry_run)


def launch_editor(engine_root, system, project_root, extra, dry_run):
    _target, _build, editor = engine_tools(engine_root, system)
    execute([editor, project_root / (PROJECT_NAME + ".uproject"), *extra], project_root, dry_run)


def import_script(name, engine_root, system, project_root, dry_run, vehicle=False):
    plugins = "PythonScriptPlugin"
    if vehicle:
        plugins += ",GLTFImporter"
    launch_editor(
        engine_root,
        system,
        project_root,
        [
            "-EnablePlugins=" + plugins,
            "-run=pythonscript",
            "-script=" + str(project_root / "scripts" / name),
            "-unattended",
            "-nop4",
            "-nosound",
            "-NullRHI",
        ],
        dry_run,
    )


def require_asset_modules():
    missing = []
    for name in ["numpy", "scipy", "requests", "pxr.Usd", "PIL.Image", "fast_simplification"]:
        try:
            # Loading native bindings catches a wrong-architecture wheel/DLL
            # before starting a multi-gigabyte city download.
            importlib.import_module(name)
        except (ImportError, OSError) as error:
            missing.append(name + " (" + str(error) + ")")
    if missing:
        raise ValueError(
            "Missing asset modules: "
            + ", ".join(missing)
            + ". Install with: python -m pip install -e '.[dev,assets]'"
        )


def doctor(arguments, system, project_root):
    """Check prerequisites without downloads, installs or source-control mutations."""
    print("Host:", system, platform.machine())
    print("Python:", sys.version.split()[0], sys.executable)
    print("Project:", project_root)
    if sys.version_info < (3, 11):  # noqa: UP036 - this launcher runs before package installation
        raise ValueError("Python 3.11 or newer is required; Python 3.12 is the setup baseline.")
    free_gib = shutil.disk_usage(project_root).free / (1024**3)
    print(f"Free project disk space: {free_gib:.1f} GiB")
    print("Full city preparation: plan for 32 GiB RAM minimum, 64 GiB recommended,")
    print("and at least 40 GiB free project storage, in addition to the engine/toolchain.")
    if free_gib < 40:
        print("WARNING: Free project storage is below the recommended preparation budget.")
    if arguments.assets:
        require_asset_modules()
        print("Asset preparation modules: imported successfully")
    if not arguments.python_only:
        root = find_engine(arguments.ue_root, system, os.environ, Path.home())
        validate_engine(root)
        target, build, editor = engine_tools(root, system)
        for path in [build, editor]:
            if not path.is_file():
                raise ValueError("Selected engine is missing a required tool: " + str(path))
        print("UE 5.2:", root)
        print("Native target:", target)
        print("Tool paths: found; compiler/SDK compatibility is verified by the build.")
    for path in [
        "Content/Maps/NewYorkDaytime.umap",
        "Content/External/CarConcept/SurveyTrafficCar.uasset",
    ]:
        status = "missing; run bootstrap-city"
        if (project_root / path).is_file():
            status = "present"
        print(path + ": " + status)


def cpp_test(project_root, system, dry_run):
    """Run the portable dynamics regression with MSVC, GCC or Clang."""
    compiler = os.environ.get("CXX")
    if not compiler:
        candidates = ["c++", "g++", "clang++"]
        if system == "Windows":
            candidates = ["cl.exe", "clang++", "g++"]
        for candidate in candidates:
            compiler = shutil.which(candidate)
            if compiler:
                break
    if not compiler:
        raise ValueError("No C++ compiler found. Windows: use Developer PowerShell for VS 2022.")
    output = project_root / "Saved/PortableTests"
    if not dry_run:
        output.mkdir(parents=True, exist_ok=True)
    executable = output / "quadrotor_dynamics_test"
    if system == "Windows":
        executable = executable.with_suffix(".exe")
    sources = [
        project_root / "Source/AerialWorldLab/Private/Flight/QuadrotorDynamics.cpp",
        project_root / "tests/cpp/quadrotor_dynamics_test.cpp",
    ]
    include = project_root / "Source/AerialWorldLab/Public"
    if Path(compiler).name.lower() in ["cl", "cl.exe"]:
        arguments = [
            compiler,
            "/nologo",
            "/std:c++17",
            "/EHsc",
            "/W4",
            "/WX",
            "/I" + str(include),
            *sources,
            "/Fe" + str(executable),
            "/Fo" + str(output) + os.sep,
        ]
    else:
        arguments = [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-I" + str(include),
            *sources,
            "-o",
            executable,
        ]
    execute(arguments, project_root, dry_run)
    execute([executable], project_root, dry_run)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=COMMANDS)
    parser.add_argument("--ue-root", help="UE 5.2 installation directory; overrides UE_ROOT")
    parser.add_argument(
        "--dry-run", action="store_true", help="Print commands without running them"
    )
    parser.add_argument("--assets", action="store_true", help="Also check asset modules in doctor")
    parser.add_argument(
        "--python-only", action="store_true", help="Skip Unreal discovery in doctor"
    )
    parser.add_argument("--test-filter", default="Aether.Flight")
    parser.add_argument(
        "--test-arg",
        action="append",
        default=[],
        help="Extra Unreal test argument; use --test-arg=-Flag",
    )
    arguments = parser.parse_args(argv)
    system = platform.system()
    project_root = PROJECT_ROOT
    command = arguments.command
    try:
        if command == "doctor":
            doctor(arguments, system, project_root)
            return 0
        if command in ["test", "readability-check"]:
            run_script("check_readability.py", project_root, arguments.dry_run)
        if command in ["test", "python-test"]:
            execute([sys.executable, "-m", "pytest", "-q"], project_root, arguments.dry_run)
        if command in ["test", "cpp-test"]:
            cpp_test(project_root, system, arguments.dry_run)
        if command in ["test", "python-test", "cpp-test", "readability-check"]:
            return 0
        if command == "prepare-new-york":
            if not arguments.dry_run:
                require_asset_modules()
            for script in ASSET_STEPS:
                run_script(script, project_root, arguments.dry_run)
            return 0
        engine_root = find_engine(arguments.ue_root, system, os.environ, Path.home())
        validate_engine(engine_root)
        if command == "bootstrap-city":
            # Fail before spending time on the large download if dependencies/engine
            # are missing. A successful build validates the native SDK first.
            if not arguments.dry_run:
                require_asset_modules()
            build_engine(engine_root, system, project_root, arguments.dry_run)
            for script in ASSET_STEPS:
                run_script(script, project_root, arguments.dry_run)
            run_script("prepare_vehicle_asset.py", project_root, arguments.dry_run)
            import_script(
                "import_vehicle_asset.py",
                engine_root,
                system,
                project_root,
                arguments.dry_run,
                vehicle=True,
            )
            import_script(
                "import_new_york.py", engine_root, system, project_root, arguments.dry_run
            )
        elif command in ["build", "unreal-test", "import-new-york"]:
            build_engine(engine_root, system, project_root, arguments.dry_run)
            if command == "unreal-test":
                launch_editor(
                    engine_root,
                    system,
                    project_root,
                    [
                        "-unattended",
                        "-nop4",
                        "-nosound",
                        "-NullRHI",
                        "-ExecCmds=Automation RunTests " + arguments.test_filter + "; Quit",
                        "-abslog=" + str(project_root / "Saved/Logs/ControllerAutomation.log"),
                        *arguments.test_arg,
                    ],
                    arguments.dry_run,
                )
            elif command == "import-new-york":
                import_script(
                    "import_new_york.py", engine_root, system, project_root, arguments.dry_run
                )
        else:
            extra = []
            if command != "editor":
                extra.append("-game")
            if command == "play":
                extra.append("-fullscreen")
            elif command in ["play-windowed", "play-low", "profile"]:
                extra.extend(["-windowed", "-ResX=1280", "-ResY=720"])
            elif command == "headless":
                extra.extend(["-unattended", "-nosound", "-NullRHI"])
            if command == "play-low":
                extra.append(
                    "-ExecCmds=sg.ViewDistanceQuality 1,sg.ShadowQuality 1,sg.PostProcessQuality 1,sg.EffectsQuality 1"
                )
            elif command == "profile":
                extra.extend(["-nosound", "-csvCaptureFrames=360", "-csvGpuStats"])
            launch_editor(engine_root, system, project_root, extra, arguments.dry_run)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print("Setup/command failed:", error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
