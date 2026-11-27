"""Check coordinate scale and explicit approximations without downloading map data."""

import importlib.util
from pathlib import Path

import pytest

spec = importlib.util.spec_from_file_location(
    "import_osm_city", Path(__file__).parents[1] / "scripts/import_osm_city.py"
)
city_import = importlib.util.module_from_spec(spec)
spec.loader.exec_module(city_import)


def test_map_coordinates_heights_and_incomplete_ways(tmp_path):
    source = tmp_path / "city.osm"
    source.write_text("""<osm>
      <node id="1" lat="0" lon="0"/>
      <node id="2" lat="0" lon="0.001"/>
      <node id="3" lat="0.001" lon="0.001"/>
      <way id="10"><nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="1"/>
        <tag k="building" v="yes"/><tag k="building:levels" v="4"/></way>
      <way id="11"><nd ref="1"/><nd ref="2"/><tag k="highway" v="residential"/></way>
      <way id="12"><nd ref="1"/><nd ref="99"/><tag k="highway" v="residential"/></way>
      <way id="13"><nd ref="1"/><nd ref="2"/><tag k="highway" v="primary"/>
        <tag k="bridge" v="yes"/></way>
    </osm>""")
    city = city_import.convert(source, 0, 0)
    assert len(city["buildings"]) == 1
    building = city["buildings"][0]
    assert building["width"] == pytest.approx(111.31949)
    assert building["height"] == 12
    assert building["height_inferred"]
    assert len(city["roads"]) == 1
    assert city["roads"][0]["width"] == 6.4
    assert "OpenStreetMap" in city["attribution"]


def test_invalid_origin_and_height_fallbacks(tmp_path):
    with pytest.raises(ValueError):
        city_import.convert(tmp_path / "unused.osm", float("nan"), 0)
    assert city_import.positive_number("nan", 9) == 9
    assert city_import.positive_number("-1", 9) == 9
    assert city_import.positive_number("12 m", 9) == 12
    assert city_import.positive_number("40 ft", 9) == 9
