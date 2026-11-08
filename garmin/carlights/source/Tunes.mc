import Toybox.Communications;
import Toybox.Cryptography;
import Toybox.Lang;
import Toybox.Time;
import Toybox.WatchUi;

// The black deck on the iPhone hotspot (net/wifi pins it there): it holds the Spotify login.
const BLACK_URL = "http://172.20.10.10";
const MAX_PLAYLISTS = 60;  // the list stops here, whatever Spotify has

// The owner's Spotify playlists, as the black deck lists them (GET /playlists, a page at a time), and
// the one picked (GET /playlists/play). Signed like the yellow deck's routes (Sign.mc). One request
// at a time; a second one while it is out is refused (false).
class Tunes {
    var list as Array<Array<String> > = [];  // [id, name], in Spotify's order
    var total as Number = -1;  // -1 until a page answers
    var playing as String = "";  // the playlist last started from here
    var status as String = "";   // "Loading...", "Sending...", an error; "" when all is well
    // Called when a page lands or fails (the open list adds the new rows or puts its More row back).
    var onPage as Method or Null = null;

    private var _next as Number = 0;  // the next page's offset
    private var _busy as Boolean = false;
    private var _path as String = "";
    private var _name as String = "";  // the playlist being started

    function busy() as Boolean {
        return _busy;
    }

    // Spotify has more than the list holds (and the list isn't full).
    function more() as Boolean {
        return total >= 0 && _next < total && list.size() < MAX_PLAYLISTS;
    }

    // The list from the start.
    function load() as Boolean {
        if (_busy) {
            return false;
        }
        list = [];
        total = -1;
        _next = 0;
        return page();
    }

    // The next page, added to the list.
    function page() as Boolean {
        if (_busy) {
            return false;
        }
        status = "Loading...";
        return request("/playlists", [["offset", _next.toString()]]);
    }

    // Plays list[i] on the phone's Spotify.
    function play(i as Number) as Boolean {
        if (_busy || i < 0 || i >= list.size()) {
            return false;
        }
        _name = list[i][1];
        status = "Sending...";
        return request("/playlists/play", [["id", list[i][0]]]);
    }

    private function request(path as String, pairs as Array<Array<String> >) as Boolean {
        _busy = true;
        _path = path;
        WatchUi.requestUpdate();
        var params = Sign.auth(DECK_KEY, path, pairs, Time.now().value(), Cryptography.randomBytes(8))
            as Dictionary<Object, Object>;
        params[pairs[0][0]] = pairs[0][1];
        Communications.makeWebRequest(BLACK_URL + path, params,
            {:method => Communications.HTTP_REQUEST_METHOD_GET,
             :responseType => Communications.HTTP_RESPONSE_CONTENT_TYPE_TEXT_PLAIN},
            method(:onReply));
        return true;
    }

    // code: the HTTP status, or a negative Communications error. 502: Spotify's answer in the deck's
    // words ("Open Spotify on phone", "Needs Spotify Premium"); 504: Spotify was too slow.
    function onReply(code as Number, data as Dictionary or String or Null) as Void {
        _busy = false;
        var page = _path.equals("/playlists");
        if (code == 200 && data != null) {
            status = "";
            if (page) {
                readPage(data.toString());
            } else {
                playing = _name;
            }
        } else if ((code == 502 || code == 504) && data != null) {
            status = data.toString();
        } else {
            status = errorText(code, data);
        }
        if (page && onPage != null) {
            (onPage as Method).invoke();
        }
        WatchUi.requestUpdate();
    }

    // "playlists <total> <next>" then "<22-char id> <name>" per line (util/black/playlists on the deck).
    function readPage(text as String) as Void {
        var lines = split(text, "\n");
        var head = split(lines[0], " ");
        if (head.size() != 3 || !head[0].equals("playlists") || head[1].toNumber() == null ||
            head[2].toNumber() == null) {
            status = "Bad answer";
            return;
        }
        total = head[1].toNumber() as Number;
        var next = head[2].toNumber() as Number;
        _next = next > _next ? next : total;  // never ask the same page twice
        for (var i = 1; i < lines.size() && list.size() < MAX_PLAYLISTS; i++) {
            var l = lines[i];
            if (l.length() > 23 && l.substring(22, 23).equals(" ")) {
                list.add([l.substring(0, 22) as String, l.substring(23, l.length()) as String]);
            }
        }
    }
}
