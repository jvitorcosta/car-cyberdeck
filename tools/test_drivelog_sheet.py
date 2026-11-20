"""Runs tools/drivelog_sheet.gs (the drive log's Apps Script) under node with stand-ins for
SpreadsheetApp, PropertiesService, LockService and ContentService, as the deck's POSTs reach it.
Skipped where node isn't installed."""
import json
import pathlib
import shutil
import subprocess
import unittest

GS = pathlib.Path(__file__).resolve().parent / "drivelog_sheet.gs"

HARNESS = r"""
const fs = require('fs'), vm = require('vm');
const sheets = {}, props = {KEY: 'k3y'};
function makeSheet(name) {
  const s = {name, rows: [], frozen: 0, formats: [],
    getLastRow() { return this.rows.length; },
    setFrozenRows(n) { this.frozen = n; },
    getRange(a, b, c, d) {
      const sheet = this;
      if (typeof a === 'string') return {setNumberFormat(f) { sheet.formats.push([a, f]); return this; }};
      const r = {row: a, col: b, nr: c, nc: d,
        setValues(v) {
          if (v.length !== this.nr || v.some(x => x.length !== this.nc)) throw new Error('shape');
          v.forEach((x, i) => { sheet.rows[this.row - 1 + i] = x; });
          return this;
        },
        setFontWeight() { return this; },
        setNumberFormat(f) { sheet.formats.push([this.row, this.col, f]); return this; },
        offset(dr, dc, nr, nc) { return Object.assign({}, r, {row: r.row + dr, col: r.col + dc, nr, nc}); }};
      return r;
    }};
  return s;
}
const ctx = {
  SpreadsheetApp: {getActiveSpreadsheet: () => ({
    getSheetByName: n => sheets[n] || null,
    insertSheet: n => (sheets[n] = makeSheet(n))})},
  PropertiesService: {getScriptProperties: () => ({
    getProperty: k => (k in props ? props[k] : null), setProperty: (k, v) => { props[k] = v; }})},
  LockService: {getScriptLock: () => ({waitLock() {}, releaseLock() {}})},
  ContentService: {MimeType: {TEXT: 'text'},
    createTextOutput: t => ({text: t, setMimeType() { return this; }})},
};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(process.argv[process.argv.length - 1], 'utf8'), ctx);
const post = body => ctx.doPost({postData: {contents: body}}).text;
const H = 't_utc,ms,kmh,rpm,coolant_c,iat_c,throttle_pct,pedal_pct,load_pct,volt,nv\n';
const out = {
  part1: post('k3y\nrows\t20261119-225001\t1\n' + H + '1763592601.2,123456,80.0,2100,90.0,34.0,18.4,22.0,41.2,14.18,26.2\n,5,80.0,2100,,34.0,18.4,,41.2,,\n'),
  dup: post('k3y\nrows\t20261119-225001\t1\n' + H + '1.0,1,1,1,1,1,1,1,1,1,1\n'),
  part2: post('k3y\nrows\t20261119-225001\t2\n' + H + '1763592662.0,183456,81.0,2150,90.0,34.0,18.0,21.0,40.0,14.20,26.5\n'),
  badKey: post('nope\nrows\t20261119-225001\t3\n' + H),
  badTrip: post('k3y\nrows\t../x\t3\n' + H),
  badKind: post('k3y\nclear\t20261119-225001\t3\n'),
  linkup: post('k3y\nlinkup\t20261119-225001\t1\n0100 -> 41 00 BE 3F A8 13\n01A4 -> NO DATA\n'),
  get: ctx.doGet().text,
};
const trip = sheets['20261119-225001'];
out.tripRows = trip.rows.map(r => r.map(c => (c instanceof Date ? 'D' + c.getTime() : c)));
out.frozen = trip.frozen;
out.linkupRows = sheets['linkup'].rows.map(r => r.map(c => (c instanceof Date ? 'D' : c)));
out.linkupFormats = sheets['linkup'].formats;
out.props = props;
console.log(JSON.stringify(out));
"""


@unittest.skipUnless(shutil.which("node"), "node not installed")
class SheetScriptTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        r = subprocess.run(["node", "-e", HARNESS, "--", str(GS)], capture_output=True, text=True, timeout=60)
        if r.returncode != 0:
            raise AssertionError(r.stderr)
        cls.out = json.loads(r.stdout)

    def test_replies(self):
        o = self.out
        self.assertEqual(o["part1"], "ok 2")
        self.assertEqual(o["dup"], "ok dup")
        self.assertEqual(o["part2"], "ok 1")
        self.assertEqual(o["badKey"], "bad key")
        self.assertEqual(o["badTrip"], "bad request")
        self.assertEqual(o["badKind"], "bad request")
        self.assertEqual(o["linkup"], "ok 2")
        self.assertTrue(o["get"].startswith("cardeck drive log"))

    def test_rows_land_in_the_trip_tab_once(self):
        rows = self.out["tripRows"]
        self.assertEqual(rows[0][:3], ["time", "t_utc", "ms"])
        self.assertEqual(self.out["frozen"], 1)
        self.assertEqual(len(rows), 4)   # header + 2 + 1: the duplicate added nothing
        self.assertEqual(rows[1], ["2025-11-19T22:50:01.200Z", 1763592601.2, 123456, 80, 2100, 90, 34, 18.4, 22, 41.2, 14.18, 26.2])
        self.assertEqual(rows[2][0], "")      # no clock: no time
        self.assertEqual(rows[2][5], "")      # a blank cell stays blank, not 0
        self.assertEqual(rows[2][11], "")
        self.assertEqual(len(rows[2]), 12)

    def test_linkup_tab_keeps_requests_as_text(self):
        rows = self.out["linkupRows"]
        self.assertEqual(rows[0], ["received", "trip", "linkup", "request", "reply"])
        self.assertEqual(rows[1][1:], ["20261119-225001", 1, "0100", "41 00 BE 3F A8 13"])
        self.assertEqual(rows[2][3:], ["01A4", "NO DATA"])
        self.assertIn([2, 4, "@"], self.out["linkupFormats"])

    def test_the_key_is_never_echoed_and_parts_are_tracked(self):
        self.assertEqual(self.out["props"]["rows:20261119-225001"], "2")
        for k in ("part1", "dup", "badKey", "linkup"):
            self.assertNotIn("k3y", self.out[k])


if __name__ == "__main__":
    unittest.main()
