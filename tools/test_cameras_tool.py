"""Host tests for tools/cameras.py (no network). Run: python -m pytest tools/test_cameras_tool.py"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cameras as cam  # noqa: E402

HDR = "id,kind,lat,lon,heading,limit,label,source,checked\n"
HERE = os.path.dirname(os.path.abspath(__file__))


def errors(body):
    return cam.validate(HDR + body)[0]


def test_validate_accepts_a_good_list():
    body = ("mao-0001,speed,-3.100000,-60.000000,270,60,Bench speed,car,2026-11-03\n"
            "mao-0003,redlight,-3.090000,-60.020000,180,,Bench light,car,2026-11-03\n")
    assert errors(body) == []


def test_validate_rejects_missing_heading():
    errs = errors("mao-0001,speed,-3.1,-60.0,,60,X,car,2026-11-03\n")
    assert errs and "line 2" in errs[0] and "heading" in errs[0]


def test_validate_rejects_redlight_limit():
    errs = errors("mao-0001,redlight,-3.1,-60.0,90,60,X,car,2026-11-03\n")
    assert errs and "limit" in errs[0]


def test_validate_rejects_bad_header_columns_and_too_big():
    assert cam.validate("id,kind\n")[0]
    assert errors("mao-1,speed,-3.1,-60.0,90,60,A,B,car,2026-11-03\n")        # 10 columns
    assert errors("mao-1,boat,-3.1,-60.0,90,60,A,car,2026-11-03\n")           # kind
    assert errors("mao-1,speed,-93.1,-60.0,90,60,A,car,2026-11-03\n")         # lat
    assert errors("mao-1,speed,-3.1,-60.0,360,60,A,car,2026-11-03\n")         # heading
    assert errors("mao-1,speed,-3.1,-60.0,90,200,A,car,2026-11-03\n")         # limit
    row = "mao-%04d,speed,-3.1,-60.0,90,60,A,car,2026-11-03\n"
    assert errors("".join(row % i for i in range(97)))                         # > 96 rows
    long_label = "mao-%04d,speed,-3.1,-60.0,90,60," + "x" * 100 + ",car,2026-11-03\n"
    assert any("8192" in e for e in errors("".join(long_label % i for i in range(90))))


def test_validate_warns_on_long_label():
    errs, warns = cam.validate(HDR + "mao-1,speed,-3.1,-60.0,90,60," + "ã" * 20 + ",car,2026-11-03\n")
    assert errs == [] and warns and "27" in warns[0]


def test_parse_immu_lists():
    html = open(os.path.join(HERE, "testdata", "immu_inforadar.html"), encoding="utf-8").read()
    items = cam.parse_immu(html)
    by = {}
    for it in items:
        by.setdefault(it["kind"], []).append(it)
    assert len(by["speed"]) == 13 and len(by["bump"]) == 2 and len(by["redlight"]) == 5
    assert all(it["sentido"] for it in items)
    first = by["speed"][0]
    assert first["sentido"] == "Oeste" and first["heading"] == 270 and first["source"] == "immu"
    assert "Santos Dumont" in first["address"]
    assert "," not in first["label"] and "·" in first["label"]


def test_survey_rows_are_speed_60():
    s = cam.survey_items()
    assert len(s) == 7
    assert all(i["kind"] == "speed" and i["limit"] == 60 and i["source"] == "immu-survey" for i in s)


def test_parse_overpass():
    data = {"elements": [{"type": "node", "id": 7, "lat": -3.05, "lon": -60.01, "tags": {"maxspeed": "60", "name": "Radar X"}},
                         {"type": "node", "id": 8, "lat": -3.06, "lon": -60.02, "tags": {}}]}
    items = cam.parse_overpass(data)
    assert items[0]["limit"] == 60 and items[0]["lat"] == -3.05 and items[0]["source"] == "osm"
    assert items[1]["limit"] is None


def test_match_by_distance():
    row = {"kind": "speed", "lat": -3.100000, "lon": -60.000000, "label": "Totally different"}
    near = {"kind": "speed", "lat": -3.100300, "lon": -60.0, "label": "Other words"}   # ~33 m
    far = {"kind": "speed", "lat": -3.101000, "lon": -60.0, "label": "Other words"}    # ~111 m
    assert cam.matches(near, row)
    assert not cam.matches(far, row)


def test_match_by_label_words():
    row = {"kind": "speed", "lat": -3.1, "lon": -60.0, "label": "Av. Torquato Tapajós · Flores"}
    it = {"kind": "speed", "label": "Avenida Torquato Tapajós · Flores"}
    assert cam.matches(it, row)
    assert not cam.matches({"kind": "speed", "label": "Av. Autaz Mirim · Coroado"}, row)


def test_diff_new_and_gone():
    rows = [{"id": "mao-0001", "kind": "speed", "lat": -3.1, "lon": -60.0, "label": "Av. Torquato Tapajós · Flores", "source": "immu"},
            {"id": "mao-0002", "kind": "speed", "lat": -3.2, "lon": -60.0, "label": "Av. Autaz Mirim · Coroado", "source": "immu"},
            {"id": "mao-0003", "kind": "speed", "lat": -3.3, "lon": -60.0, "label": "Mine", "source": "car"}]
    items = [{"kind": "speed", "label": "Av. Torquato Tapajós · Flores"}, {"kind": "speed", "label": "Av. Codajás · Petrópolis"}]
    new, gone = cam.diff(items, rows)
    assert [i["label"] for i in new] == ["Av. Codajás · Petrópolis"]
    assert [r["id"] for r in gone] == ["mao-0002"]        # car-sourced rows are never "gone"


def test_apply_assigns_next_id(tmp_path):
    p = tmp_path / "cameras.csv"
    p.write_text(HDR + "mao-0007,speed,-3.1,-60.0,90,60,Old,car,2026-01-01\n", encoding="utf-8", newline="")
    placed = [{"kind": "bump", "lat": -3.2, "lon": -60.1, "heading": 45, "limit": 30, "label": "New one", "source": "immu"},
              {"kind": "redlight", "lat": -3.3, "lon": -60.2, "heading": 180, "limit": None, "label": "Light", "source": "osm"}]
    n = cam.apply_placed(str(p), placed, today="2026-11-03")
    assert n == 2
    text = p.read_text(encoding="utf-8")
    assert "mao-0008,bump,-3.200000,-60.100000,45,30,New one,immu,2026-11-03\n" in text
    assert "mao-0009,redlight,-3.300000,-60.200000,180,,Light,osm,2026-11-03\n" in text
    assert cam.validate(text)[0] == []


def test_apply_refuses_an_invalid_result(tmp_path):
    p = tmp_path / "cameras.csv"
    p.write_text(HDR, encoding="utf-8", newline="")
    bad = [{"kind": "speed", "lat": -3.2, "lon": -60.1, "heading": None, "limit": 60, "label": "No arrow", "source": "immu"}]
    try:
        cam.apply_placed(str(p), bad, today="2026-11-03")
        assert False, "should refuse"
    except ValueError:
        pass
    assert p.read_text(encoding="utf-8") == HDR


def test_parse_marks():
    rows = cam.parse_marks("123,-3.100000,-60.000000,271,55\n456,-3.2,-60.1,10,0\n")
    assert rows[0]["lat"] == -3.1 and rows[0]["heading"] == 271 and rows[0]["source"] == "car"
    assert len(rows) == 2


def test_place_page_embeds_items(tmp_path):
    out = tmp_path / "p.html"
    cam.write_place_page([{"kind": "speed", "label": "A </script> b", "lat": -3.1, "lon": -60.0, "heading": 90, "limit": 60, "source": "osm"}], str(out))
    html = out.read_text(encoding="utf-8")
    assert "__ITEMS__" not in html and "leaflet" in html.lower()
    assert "</script> b" not in html          # embedded JSON can't close the script tag
