import Toybox.Application;
import Toybox.Communications;
import Toybox.Lang;
import Toybox.WatchUi;

// The deck on the iPhone hotspot (net/wifi pins it there).
const DECK_URL = "http://172.20.10.11";

// [label, word sent to the deck, screen colour]: the deck's own screen colours, but purple, which
// is #8C00FF on the deck and the lights; the watch's 64 colours show it as AA00FF, so that is set here.
const COLORS = [
    ["Purple", "purple", 0xAA00FF], ["Red", "red", 0xFF0000], ["Blue", "blue", 0x0040FF],
    ["Pink", "pink", 0xFF0090], ["Green", "green", 0x00FF30], ["White", "white", 0xFFFFFF],
    ["Orange", "orange", 0xFF6A00], ["Yellow", "yellow", 0xFFD400],
];

// Zones, as the deck names them. BOTH changes both lights; its reply describes both.
enum { BOTH = 0, INSIDE = 1, OUTSIDE = 2 }

// The Presets page, and the marker on a job that isn't about one zone (/presets, /status, and the
// /say that applies a preset: no zone param, and its reply must not be read as a look).
const PRESETS = 3;

// One light's look as the watch knows it.
class Look {
    var color as Number = 0;
    var pct as Number = 100;
    var lit as Boolean = true;
}

// The lights as the deck reports them, and the requests that change them. Every reply ("Inside:
// Purple, 70 percent", "Lights off") is read back into the looks, so the screen shows what the
// deck says, not what the watch guessed. Requests go one at a time from a short queue.
class Deck {
    var inside as Look = new Look();
    var outside as Look = new Look();
    var status as String = "";  // "Sending...", "No answer", ...; "" when all is well
    // The deck's fixed presets, in its order: [name, inColor, inPct, inLit, outColor, outPct, outLit, number],
    // the colours as indices into COLORS (found by word: the deck's order differs). Empty until
    // /presets answers (and if it never does).
    var presets as Array = [];
    // Each controller's link, [inside, outside]: "on" | "off" | "nolink", "" until the deck answers.
    var links as Array<String> = ["", ""];

    private var _queue as Array<Array> = [];  // [path, text or null, zone]
    private var _busy as Boolean = false;
    private var _zone as Number = BOTH;       // the zone of the request that is out
    private var _path as String = "";         // and its path

    // A command for a zone: a colour word, "off", "brighter", "50%"...
    function send(text as String, zone as Number) as Void {
        push(["/say", text, zone]);
    }

    // Reads both lights, the presets and the links (when the app opens).
    function sync() as Void {
        syncLooks();
        push(["/presets", null, PRESETS]);
        push(["/status", null, PRESETS]);
    }

    // Reads both lights only (after a preset is applied: the presets and links haven't changed).
    function syncLooks() as Void {
        push(["/state", null, INSIDE]);
        push(["/state", null, OUTSIDE]);
    }

    // Applies preset n (its number on the deck) via /say. The reply isn't a look; onReply re-syncs instead.
    function applyPreset(n as Number) as Void {
        push(["/say", "preset " + n, PRESETS]);
    }

    function look(zone as Number) as Look {
        return zone == OUTSIDE ? outside : inside;
    }

    private function push(job as Array) as Void {
        if (_queue.size() >= 6) {
            return;  // something is badly stuck; don't pile up more
        }
        _queue.add(job);
        if (!_busy) {
            next();
        }
    }

    private function next() as Void {
        if (_busy || _queue.size() == 0) {  // one request out at a time
            return;
        }
        var job = _queue[0];
        _queue = _queue.slice(1, null);
        _busy = true;
        _zone = job[2] as Number;
        _path = job[0] as String;
        status = "Sending...";
        WatchUi.requestUpdate();
        var params = {"k" => DECK_KEY} as Dictionary<Object, Object>;
        if (job[1] != null) {
            params["text"] = job[1] as String;
        }
        if (_zone == INSIDE) {
            params["zone"] = "inside";
        } else if (_zone == OUTSIDE) {
            params["zone"] = "outside";
        }
        Communications.makeWebRequest(DECK_URL + (job[0] as String), params,
            {:method => Communications.HTTP_REQUEST_METHOD_GET,
             :responseType => Communications.HTTP_RESPONSE_CONTENT_TYPE_TEXT_PLAIN},
            method(:onReply));
    }

    // code: the HTTP status, or a negative Communications error.
    function onReply(code as Number, data as Dictionary or String or Null) as Void {
        _busy = false;
        if (code == 200 && data != null) {
            status = "";
            var text = data.toString();
            if (_path.equals("/presets")) {
                readPresets(text);
            } else if (_path.equals("/status")) {
                links = [linkOf(text, "inside="), linkOf(text, "outside=")];
            } else if (_zone == PRESETS) {
                syncLooks();  // a preset was applied: read the lights it set
            } else if (_zone == BOTH) {
                absorb(text, inside);
                absorb(text, outside);
            } else {
                absorb(text, look(_zone));
            }
            // for the glance: the last state the deck reported, [colour, pct, lit] per light
            if (!_path.equals("/presets") && !_path.equals("/status")) {
                Application.Storage.setValue("in", [inside.color, inside.pct, inside.lit]);
                Application.Storage.setValue("out", [outside.color, outside.pct, outside.lit]);
            }
        } else {
            _queue = [];  // the deck isn't answering: drop the rest rather than wait on each
            if (code == -300) {
                status = "No answer";            // timed out: the deck isn't on the hotspot
            } else if (code == -104 || code == -2) {
                status = "Phone not connected";  // no Bluetooth link to Garmin Connect
            } else if (code == 403) {
                status = "Wrong key";            // Secrets.mc doesn't match REMOTE_TOKEN
            } else if (code == 503) {
                status = "Deck busy";
            } else {
                status = "Error " + code;
            }
        }
        WatchUi.requestUpdate();
        next();
    }

    // "1 ROSE pink 40 pink 100" / "4 PURPURA off 0 purple 100" per line, to presets in the deck's
    // order, with the deck's number last (what /say needs). An answer with no valid line keeps the old list.
    private function readPresets(text as String) as Void {
        var out = [] as Array;
        var lines = split(text, "\n");
        for (var i = 0; i < lines.size(); i++) {
            var t = split(lines[i], " ");
            var n = t.size() > 0 ? t[0].toNumber() : null;
            var inPct = t.size() >= 6 ? t[3].toNumber() : null;
            var outPct = t.size() >= 6 ? t[5].toNumber() : null;
            if (n == null || inPct == null || outPct == null) {
                continue;  // noise or a trailing blank line
            }
            out.add([t[1], colorOf(t[2]), inPct, !t[2].equals("off"),
                     colorOf(t[4]), outPct, !t[4].equals("off"), n]);
        }
        if (out.size() > 0) {
            presets = out;
        }
    }

    // The COLORS index for a colour word (0 for "off" or anything unknown: the light isn't lit then).
    private function colorOf(word as String) as Number {
        for (var i = 0; i < COLORS.size(); i++) {
            if (word.equals((COLORS[i] as Array)[1] as String)) {
                return i;
            }
        }
        return 0;
    }

    // The word after `key` in "inside=on outside=nolink" ("" if absent).
    private function linkOf(text as String, key as String) as String {
        var at = text.find(key);
        if (at == null) {
            return "";
        }
        var rest = text.substring(at + key.length(), text.length()) as String;
        var sp = rest.find(" ");
        return sp != null ? rest.substring(0, sp) as String : rest;
    }

    private function split(text as String, sep as String) as Array<String> {
        var parts = [] as Array<String>;
        var rest = text;
        var at = rest.find(sep);
        while (at != null) {
            parts.add(rest.substring(0, at) as String);
            rest = rest.substring(at + sep.length(), rest.length()) as String;
            at = rest.find(sep);
        }
        parts.add(rest);
        return parts;
    }

    // Reads a deck line into a look: "[Inside: |Outside: ]<Colour>[, <n> percent]" or "... Lights off".
    private function absorb(text as String, l as Look) as Void {
        var colon = text.find(": ");
        if (colon != null) {
            text = text.substring(colon + 2, text.length()) as String;
        }
        if (text.find("Lights off") != null) {
            l.lit = false;
            return;
        }
        if (text.find("Sorry") != null || text.find("didn't") != null) {
            status = text;  // the deck didn't understand: show its words
            return;
        }
        for (var i = 0; i < COLORS.size(); i++) {
            if (text.find((COLORS[i] as Array)[0] as String) == 0) {
                l.color = i;
                l.lit = true;
            }
        }
        var comma = text.find(", ");
        var word = text.find(" percent");
        if (comma != null && word != null && word > comma) {
            var digits = text.substring(comma + 2, word);
            var n = digits != null ? digits.toNumber() : null;
            if (n != null) {
                l.pct = n;
            }
        }
    }
}
