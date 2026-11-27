"""Keep headless vehicle import off AssetTools' UI path and reject partial saves."""

import json
import runpy
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

SCRIPT = Path(__file__).parents[1] / "scripts/import_vehicle_asset.py"


class Properties:
    def __init__(self):
        self.values = {}

    def get_editor_property(self, name):
        return self.values.setdefault(name, Properties())

    def set_editor_property(self, name, value):
        self.values[name] = value


class StaticMesh:
    pass


@pytest.mark.parametrize("save_success", [True, False])
def test_vehicle_completion_requires_all_packages_saved(tmp_path, monkeypatch, save_success):
    source = tmp_path / "Assets/External/CarConcept/SurveyTrafficCar.glb"
    source.parent.mkdir(parents=True)
    source.write_bytes(b"source model for cache regression")
    mesh = StaticMesh()
    imports = []

    def import_asset(root, source_data, parameters):
        assert root == "/Game/External/CarConcept"
        assert source_data == str(source)
        assert parameters.values["is_automated"] is True
        imports.append(parameters.values["override_pipelines"])
        return True

    # Deliberately provide no AssetToolsHelpers. Returning to that UI-producing
    # entry point breaks this regression before it can falsely mark success.
    unreal = SimpleNamespace(
        Paths=SimpleNamespace(project_dir=lambda: str(tmp_path)),
        StaticMesh=StaticMesh,
        load_asset=lambda path: mesh,
        InterchangeGenericAssetsPipeline=Properties,
        ImportAssetParameters=Properties,
        InterchangeManager=SimpleNamespace(
            get_interchange_manager_scripted=lambda: SimpleNamespace(import_asset=import_asset),
            create_source_data=lambda path: path,
        ),
        EditorAssetLibrary=SimpleNamespace(save_directory=lambda *args, **kwargs: save_success),
        log_warning=lambda message: None,
    )
    monkeypatch.setitem(sys.modules, "unreal", unreal)
    marker = tmp_path / "Saved/AssetImport/vehicle.json"
    if save_success:
        runpy.run_path(str(SCRIPT))
        assert json.loads(marker.read_text())["source_sha256"]
        runpy.run_path(str(SCRIPT))
        assert len(imports) == 1
        # A changed source invalidates the completion marker and is imported again.
        source.write_bytes(b"changed source model")
        runpy.run_path(str(SCRIPT))
        assert len(imports) == 2
    else:
        with pytest.raises(RuntimeError, match="could not all be saved"):
            runpy.run_path(str(SCRIPT))
        assert len(imports) == 1
        assert not marker.exists()
