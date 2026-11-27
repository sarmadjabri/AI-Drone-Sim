"""Check portable setup behavior without requiring other operating systems or Unreal."""

import importlib.util
import json
from pathlib import Path

import pytest

MODULE_PATH = Path(__file__).parents[1] / "scripts/project.py"
SPEC = importlib.util.spec_from_file_location("project_commands", MODULE_PATH)
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def fake_engine(tmp_path, major=5, minor=2):
    root = tmp_path / "Engine Install With Spaces"
    version = root / "Engine/Build/Build.version"
    version.parent.mkdir(parents=True)
    version.write_text(json.dumps({"MajorVersion": major, "MinorVersion": minor}))
    return root


def test_explicit_missing_engine_never_falls_back_to_another_install(tmp_path):
    valid = fake_engine(tmp_path)
    with pytest.raises(ValueError, match="Build.version"):
        runner.find_engine(str(tmp_path / "missing"), "Linux", {"UE_ROOT": str(valid)}, tmp_path)


def test_explicit_engine_takes_priority_over_environment(tmp_path):
    valid = fake_engine(tmp_path)
    assert runner.find_engine(str(valid), "Linux", {"UE_ROOT": "missing"}, tmp_path) == valid


def test_wrong_engine_minor_version_is_rejected(tmp_path):
    root = fake_engine(tmp_path, minor=3)
    with pytest.raises(ValueError, match="targets UE 5.2"):
        runner.validate_engine(root)


@pytest.mark.parametrize(
    "system,target,editor_suffix",
    [
        ("Windows", "Win64", "Engine/Binaries/Win64/UnrealEditor.exe"),
        ("Darwin", "Mac", "Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"),
        ("Linux", "Linux", "Engine/Binaries/Linux/UnrealEditor"),
    ],
)
def test_native_launch_keeps_project_paths_with_spaces_as_one_argument(
    tmp_path, monkeypatch, system, target, editor_suffix
):
    engine = fake_engine(tmp_path)
    project = tmp_path / "Project With Spaces"
    calls = []
    monkeypatch.setattr(
        runner.subprocess, "run", lambda args, **kwargs: calls.append((args, kwargs))
    )
    runner.launch_editor(engine, system, project, ["-game"], False)
    arguments, options = calls[0]
    assert arguments == [
        str(engine / editor_suffix),
        str(project / "AerialWorldLab.uproject"),
        "-game",
    ]
    assert options["cwd"] == project
    assert options["check"] is True
    assert runner.engine_tools(engine, system)[0] == target


def test_windows_batch_quoting_is_not_reescaped_by_subprocess(tmp_path, monkeypatch):
    engine = fake_engine(tmp_path)
    calls = []
    monkeypatch.setenv("COMSPEC", "cmd.exe")
    monkeypatch.setattr(runner.subprocess, "run", lambda args, **kwargs: calls.append(args))
    runner.build_engine(engine, "Windows", tmp_path / "Project With Spaces", False)
    assert isinstance(calls[0], str)
    assert calls[0].startswith('cmd.exe /d /s /c ""')
    assert 'Build.bat" AerialWorldLabEditor Win64 Development ' in calls[0]
    assert '"-Project=' in calls[0]
    assert calls[0].endswith('-WaitMutex"')


def test_bootstrap_builds_before_download_and_imports_vehicle_before_map(tmp_path, monkeypatch):
    engine = fake_engine(tmp_path)
    calls = []
    monkeypatch.setattr(runner.platform, "system", lambda: "Linux")
    monkeypatch.setattr(runner, "require_asset_modules", lambda: None)
    monkeypatch.setattr(runner, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(runner, "build_engine", lambda *args: calls.append("build"))
    monkeypatch.setattr(runner, "run_script", lambda name, *args: calls.append(name))
    monkeypatch.setattr(runner, "import_script", lambda name, *args, **kwargs: calls.append(name))
    assert runner.main(["bootstrap-city", "--ue-root", str(engine)]) == 0
    assert calls == [
        "build",
        *runner.ASSET_STEPS,
        "prepare_vehicle_asset.py",
        "import_vehicle_asset.py",
        "import_new_york.py",
    ]


def test_dry_run_executes_no_processes_or_build_outputs(tmp_path, monkeypatch):
    engine = fake_engine(tmp_path)
    monkeypatch.setattr(runner.platform, "system", lambda: "Linux")
    monkeypatch.setattr(runner, "PROJECT_ROOT", tmp_path)

    def unexpected_process(*args, **kwargs):
        pytest.fail("Dry run executed a process")

    monkeypatch.setattr(runner.subprocess, "run", unexpected_process)
    assert runner.main(["bootstrap-city", "--ue-root", str(engine), "--dry-run"]) == 0
    assert not (tmp_path / "Content").exists()
    assert not (tmp_path / "Saved").exists()


def test_automation_arguments_are_preserved(tmp_path, monkeypatch):
    engine = fake_engine(tmp_path)
    calls = []
    monkeypatch.setattr(runner.platform, "system", lambda: "Linux")
    monkeypatch.setattr(runner, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(runner, "build_engine", lambda *args: None)
    monkeypatch.setattr(runner, "launch_editor", lambda *args: calls.append(args[3]))
    assert (
        runner.main(
            [
                "unreal-test",
                "--ue-root",
                str(engine),
                "--test-filter",
                "Aether.Flight.CityEnvironment",
                "--test-arg=-ExportCityRoadGrid",
            ]
        )
        == 0
    )
    assert "-ExecCmds=Automation RunTests Aether.Flight.CityEnvironment; Quit" in calls[0]
    assert "-ExportCityRoadGrid" in calls[0]
