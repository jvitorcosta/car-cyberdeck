"""Host tests for tools/cameras.py (no network). Run: python -m pytest tools/test_cameras_tool.py"""
import json
import os
import sys
import urllib.error

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cameras as cam  # noqa: E402

HDR = "id,kind,lat,lon,heading,limit,label,source,checked\n"


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
    assert any("6144" in e for e in errors("".join(long_label % i for i in range(60))))


def test_validate_warns_on_long_label():
    errs, warns = cam.validate(HDR + "mao-1,speed,-3.1,-60.0,90,60," + "ã" * 20 + ",car,2026-11-03\n")
    assert errs == [] and warns and "27" in warns[0]


def test_parse_marks():
    rows = cam.parse_marks("123,-3.100000,-60.000000,271,55\n456,-3.2,-60.1,10,0\n")
    assert rows[0]["lat"] == -3.1 and rows[0]["heading"] == 271 and rows[0]["source"] == "car"
    assert len(rows) == 2


def test_validate_rejects_unicode_digits():
    # str.isdigit() takes these, the deck's parser doesn't ("²" even made int() raise)
    assert errors("mao-1,speed,-3.1,-60.0,²,60,A,car,2026-11-03\n")            # heading "²"
    assert errors("mao-1,speed,-3.1,-60.0,٩٠,60,A,car,2026-11-03\n")      # heading "٩٠"
    assert errors("mao-1,speed,-3.1,-60.0,90,٦٠,A,car,2026-11-03\n")      # limit "٦٠"
    assert errors("mao-1,speed,-٣.1,-60.0,90,60,A,car,2026-11-03\n")           # lat "-٣.1"


def test_validate_rejects_quote_in_label():
    errs = errors('mao-1,speed,-3.1,-60.0,90,60,Av. "X",car,2026-11-03\n')
    assert errs and "line 2" in errs[0] and '"' in errs[0]


def test_validate_accepts_every_source_of_the_data_repo():
    body = "".join("mao-%d,speed,-3.1,-60.0,90,60,A,%s,2026-11-03\n" % (n, src)
                   for n, src in enumerate(("immu", "osm", "survey", "car", "community")))
    assert errors(body) == []


def _push(tmp_path, monkeypatch, body, *args):
    p = tmp_path / "radars.csv"
    p.write_text(HDR + body, encoding="utf-8", newline="")
    sent = []
    monkeypatch.setattr(cam, "deck_call", lambda host, method, path, data=None: (sent.append(path), (204, ""))[1])
    return cam.main(["--host", "deck.test", "push", "--file", str(p), *args]), sent


def test_push_refuses_an_empty_list_without_force(tmp_path, monkeypatch):
    code, sent = _push(tmp_path, monkeypatch, "")
    assert code == 1 and sent == []          # the deck would drop every point it has
    code, sent = _push(tmp_path, monkeypatch, "", "--force")
    assert code == 0 and sent == ["/cam/list"]
    code, sent = _push(tmp_path, monkeypatch, "mao-0001,speed,-3.1,-60.0,90,60,A,car,2026-11-03\n")
    assert code == 0 and sent == ["/cam/list"]


def test_release_url_latest_and_pinned():
    base = "https://github.com/jvitorcosta/open-radars-manaus/releases"
    assert cam.release_url(None) == base + "/latest/download/radars.csv"
    assert cam.release_url("v1.2.0") == base + "/download/v1.2.0/radars.csv"


class _Resp:
    def __init__(self, body):
        self.body = body

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False

    def read(self):
        return self.body

    status = 200


def test_push_from_release_downloads_and_posts(monkeypatch):
    text = HDR + "mao-0001,speed,-3.1,-60.0,90,60,A,community,2026-11-03\n"
    urls, posted = [], []
    monkeypatch.setattr(cam.urllib.request, "urlopen",
                        lambda req, timeout=None: (urls.append(req.full_url), _Resp(text.encode("utf-8")))[1])
    monkeypatch.setattr(cam, "deck_call",
                        lambda host, method, path, data=None: (posted.append((path, data)), (204, ""))[1])
    assert cam.main(["--host", "deck.test", "push", "--release", "v1.0.0"]) == 0
    assert urls == [cam.release_url("v1.0.0")]
    assert posted == [("/cam/list", text.encode("utf-8"))]


def test_push_download_failure_exits_2(monkeypatch, capsys):
    def boom(req, timeout=None):
        raise urllib.error.URLError("offline")
    monkeypatch.setattr(cam.urllib.request, "urlopen", boom)
    monkeypatch.setattr(cam, "deck_call", lambda *a, **k: (_ for _ in ()).throw(AssertionError("must not post")))
    assert cam.main(["push"]) == 2
    assert "release" in capsys.readouterr().err


def test_push_download_http_exception_and_bad_encoding_exit_2(monkeypatch, capsys):
    import http.client
    monkeypatch.setattr(cam, "deck_call", lambda *a, **k: (_ for _ in ()).throw(AssertionError("must not post")))

    def cut(req, timeout=None):
        raise http.client.IncompleteRead(b"")
    monkeypatch.setattr(cam.urllib.request, "urlopen", cut)
    assert cam.main(["push"]) == 2
    assert "no such release, or no network" in capsys.readouterr().err
    monkeypatch.setattr(cam.urllib.request, "urlopen", lambda req, timeout=None: _Resp(b"\xff\xfe\x00bad"))
    assert cam.main(["push"]) == 2
    assert "not UTF-8 text" in capsys.readouterr().err


def test_import_car_writes_marks_file(tmp_path, monkeypatch):
    calls = []
    marks = "123,-3.100000,-60.000000,271,55\n456,-3.2,-60.1,10,0\n"
    monkeypatch.setattr(cam, "deck_call", lambda host, method, path, data=None: (calls.append((method, path)), (200, marks))[1])
    out = tmp_path / "build" / "car-marks.json"
    assert cam.main(["--host", "deck.test", "import-car", "--out", str(out)]) == 0
    items = json.loads(out.read_text(encoding="utf-8"))
    assert len(items) == 2 and all(i["source"] == "car" for i in items)
    assert calls == [("GET", "/cam/marks")]


def test_import_car_clear_posts_clear(monkeypatch):
    calls = []
    monkeypatch.setattr(cam, "deck_call", lambda host, method, path, data=None: (calls.append((method, path)), (204, ""))[1])
    assert cam.main(["--host", "deck.test", "import-car", "--clear"]) == 0
    assert calls == [("POST", "/cam/marks/clear")]
