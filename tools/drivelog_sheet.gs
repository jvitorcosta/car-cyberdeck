// CarDeck drive log: the Apps Script web app behind the owner's Google Sheet. The OBD deck (src/obd/dlogup)
// POSTs a minute of rows at a time over the phone's hotspot; this appends them to the spreadsheet the
// script belongs to: one tab per power-on of the deck (named by its first record's UTC time,
// "20261119-225001"), and every link-up's raw supported-PID bitmaps and 01 A4 on the "linkup" tab.
// Setup: docs/drive-log.md. The shared key is the script property KEY (Project Settings > Script
// properties), the same string as DLOG_KEY on the deck; it is never written in this file.
//
// The POST body (text/plain):
//   <key>
//   rows<TAB><trip><TAB><part>        or   linkup<TAB><trip><TAB><n>
//   t_utc,ms,kmh,...                       0100 -> 41 00 BE 3F A8 13
//   1763592601.2,123456,80.0,...           ...
// The reply, the first line of the page Google redirects the POST to:
//   ok <rows> | ok dup (a part number this trip already has: a retry) | bad key | bad request

var COLUMNS = ['time', 't_utc', 'ms', 'kmh', 'rpm', 'coolant_c', 'iat_c', 'throttle_pct', 'pedal_pct',
               'load_pct', 'volt', 'nv'];
var LINKUP_COLUMNS = ['received', 'trip', 'linkup', 'request', 'reply'];

function reply_(text) {
  return ContentService.createTextOutput(text).setMimeType(ContentService.MimeType.TEXT);
}

function tripSheet_(ss, trip) {
  var sheet = ss.getSheetByName(trip);
  if (sheet) return sheet;
  sheet = ss.insertSheet(trip);
  sheet.getRange(1, 1, 1, COLUMNS.length).setValues([COLUMNS]).setFontWeight('bold');
  sheet.setFrozenRows(1);
  sheet.getRange('A:A').setNumberFormat('yyyy-mm-dd hh:mm:ss');   // the spreadsheet's time zone
  return sheet;
}

// One CSV line of the deck as a sheet row: the time as a date first, numbers as numbers, blanks blank.
function row_(line) {
  var cells = line.split(',').map(function (c) { return c === '' ? '' : Number(c); });
  while (cells.length < COLUMNS.length - 1) cells.push('');
  cells = cells.slice(0, COLUMNS.length - 1);
  var t = cells[0];
  return [t === '' ? '' : new Date(t * 1000)].concat(cells);
}

function doPost(e) {
  var text = (e && e.postData && e.postData.contents) || '';
  var lines = text.split('\n');
  var props = PropertiesService.getScriptProperties();
  var key = props.getProperty('KEY');
  if (!key || lines[0] !== key) return reply_('bad key');
  var head = (lines[1] || '').split('\t');
  var kind = head[0], trip = head[1] || '', num = Number(head[2]);
  if (!/^\d{8}-\d{6}$/.test(trip) || !(num >= 1) || (kind !== 'rows' && kind !== 'linkup')) {
    return reply_('bad request');
  }
  var lock = LockService.getScriptLock();
  lock.waitLock(20000);
  try {
    // Parts come in order; one the trip already has is a retry whose answer the deck didn't get.
    var seen = kind + ':' + trip;
    if (num <= Number(props.getProperty(seen) || 0)) return reply_('ok dup');
    var ss = SpreadsheetApp.getActiveSpreadsheet();
    var rows = [];
    if (kind === 'rows') {
      lines.slice(3).forEach(function (l) { if (l) rows.push(row_(l)); });   // lines[2]: the CSV header
      if (rows.length) {
        var sheet = tripSheet_(ss, trip);
        sheet.getRange(sheet.getLastRow() + 1, 1, rows.length, COLUMNS.length).setValues(rows);
      }
    } else {
      var now = new Date();
      lines.slice(2).forEach(function (l) {
        if (!l) return;
        var i = l.indexOf(' -> ');
        rows.push([now, trip, num, i < 0 ? l : l.slice(0, i), i < 0 ? '' : l.slice(i + 4)]);
      });
      var sheet = ss.getSheetByName('linkup') || ss.insertSheet('linkup');
      if (sheet.getLastRow() === 0) {
        sheet.getRange(1, 1, 1, LINKUP_COLUMNS.length).setValues([LINKUP_COLUMNS]).setFontWeight('bold');
      }
      if (rows.length) {
        var range = sheet.getRange(sheet.getLastRow() + 1, 1, rows.length, LINKUP_COLUMNS.length);
        range.offset(0, 3, rows.length, 2).setNumberFormat('@');   // "0100" stays text, not 100
        range.setValues(rows);
      }
    }
    props.setProperty(seen, String(num));
    return reply_('ok ' + rows.length);
  } finally {
    lock.releaseLock();
  }
}

// Opening the /exec URL in a browser: a sign of life, no data.
function doGet() {
  return reply_('cardeck drive log: up');
}
