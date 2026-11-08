import Toybox.Attention;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Timer;
import Toybox.WatchUi;

// The zone pages' buttons:
//   UP / DOWN: previous / next page, looping BOTH -> INSIDE -> OUTSIDE -> PRESETS -> PLAYLISTS
//   START:     that zone's list (colours, Brightness >, Off; on BOTH also Music mode); on PRESETS the deck's presets as
//              bars, START on one applies it; on PLAYLISTS the owner's Spotify playlists, START on one plays it
//                                                                 BACK: leave
class ZonesDelegate extends WatchUi.BehaviorDelegate {
    private var _deck as Deck;
    private var _tunes as Tunes;
    private var _view as ZonesView;

    function initialize(deck as Deck, tunes as Tunes, view as ZonesView) {
        BehaviorDelegate.initialize();
        _deck = deck;
        _tunes = tunes;
        _view = view;
    }

    function onNextPage() as Boolean {
        return turn(_view.page + 1);
    }

    function onPreviousPage() as Boolean {
        return turn(_view.page + PAGES - 1);
    }

    // To page p (mod PAGES). The playlists are asked for the first time their page shows.
    private function turn(p as Number) as Boolean {
        _view.page = p % PAGES;
        if (_view.page == PLAYLISTS && _tunes.total < 0 && _tunes.status.length() == 0) {
            _tunes.load();
        }
        WatchUi.requestUpdate();
        return true;
    }

    function onSelect() as Boolean {
        var zone = _view.page;
        if (zone == PRESETS) {
            return presets();
        }
        if (zone == PLAYLISTS) {
            return playlists();
        }
        var menu = new WatchUi.Menu2({:title => ["Both", "Inside", "Outside"][zone] as String});
        for (var i = 0; i < N_PICK; i++) {
            var c = COLORS[i] as Array;
            menu.addItem(new WatchUi.IconMenuItem(c[0] as String, null, c[1] as String, new ColorDot(c[2] as Number), {}));
        }
        menu.addItem(new WatchUi.MenuItem("Brightness", null, :level, {}));
        menu.addItem(new WatchUi.MenuItem("Off", null, "off", {}));
        if (zone == BOTH) {  // music mode drives both lights: its toggle, with the deck's state
            var m = _deck.music;
            menu.addItem(new WatchUi.MenuItem("Music mode", m.equals("on") ? "On" : m.equals("off") ? "Off" : null,
                                              :music, {}));
        }
        // start on the zone's current colour (inside's for BOTH; blue for cyan, not in the list)
        var color = _deck.look(zone == BOTH ? INSIDE : zone).color;
        menu.setFocus(color < N_PICK ? color : BLUE_INDEX);
        WatchUi.pushView(menu, new ZoneMenuDelegate(_deck, zone), WatchUi.SLIDE_LEFT);
        return true;
    }

    // The Presets list: each preset's bar (PresetItem), START applies the focused one. Nothing
    // opens while the deck hasn't sent any.
    private function presets() as Boolean {
        if (_deck.presets.size() == 0) {
            return true;
        }
        var menu = new WatchUi.CustomMenu(30, Graphics.COLOR_BLACK,
            {:focusItemHeight => 44, :title => new PresetTitle()});
        for (var i = 0; i < _deck.presets.size(); i++) {
            menu.addItem(new PresetItem(_deck.presets[i] as Array));
        }
        WatchUi.pushView(menu, new PresetMenuDelegate(_deck), WatchUi.SLIDE_LEFT);
        return true;
    }

    // The Playlists list: each playlist by name, then More while Spotify has more. With none yet
    // (the black deck didn't answer), START asks again.
    private function playlists() as Boolean {
        if (_tunes.list.size() == 0) {
            _tunes.load();
            return true;
        }
        var menu = new WatchUi.Menu2({:title => "Playlists"});
        for (var i = 0; i < _tunes.list.size(); i++) {
            menu.addItem(new WatchUi.MenuItem(_tunes.list[i][1], null, i, {}));
        }
        if (_tunes.more()) {
            menu.addItem(new WatchUi.MenuItem("More", null, :more, {}));
        }
        var focus = _tunes.list.size() - 1;  // back on the one last played, else the top
        while (focus > 0 && !_tunes.list[focus][1].equals(_tunes.playing)) {
            focus--;
        }
        menu.setFocus(focus);
        WatchUi.pushView(menu, new PlaylistMenuDelegate(_tunes, menu), WatchUi.SLIDE_LEFT);
        return true;
    }
}

// The Playlists list: START on a playlist plays it on the phone's Spotify (through the black deck),
// buzzes, says "Starting..." under it for STARTING_MS and returns to the pages, where the reply shows
// ("Playing" and its name, or what went wrong). START on More adds the next page in place.
class PlaylistMenuDelegate extends WatchUi.Menu2InputDelegate {
    private const STARTING_MS = 1000;
    private var _tunes as Tunes;
    private var _menu as WatchUi.Menu2;
    private var _timer as Timer.Timer = new Timer.Timer();
    private var _starting as Boolean = false;

    function initialize(tunes as Tunes, menu as WatchUi.Menu2) {
        Menu2InputDelegate.initialize();
        _tunes = tunes;
        _menu = menu;
    }

    function onSelect(item as WatchUi.MenuItem) as Void {
        if (_starting) {
            return;  // one playlist per visit: the list is on its way out
        }
        if (item.getId() == :more) {
            if (_tunes.page()) {
                _tunes.onPage = method(:onPage);
                item.setLabel("Loading...");
                WatchUi.requestUpdate();
            }
            return;
        }
        if (!_tunes.play(item.getId() as Number)) {
            return;  // a page is still loading
        }
        _starting = true;
        if (Attention has :vibrate) {
            Attention.vibrate([new Attention.VibeProfile(50, 120)]);
        }
        item.setSubLabel("Starting...");
        WatchUi.requestUpdate();
        _timer.start(method(:leave), STARTING_MS, false);
    }

    // A page landed (or failed): its playlists replace the More row, which comes back last while
    // Spotify has more (with the error under it, if the page failed).
    function onPage() as Void {
        _tunes.onPage = null;
        var at = _menu.findItemById(:more);
        if (at >= 0) {
            _menu.deleteItem(at);
        }
        var from = 0;
        while (_menu.getItem(from) != null) {
            from++;
        }
        for (var i = from; i < _tunes.list.size(); i++) {
            _menu.addItem(new WatchUi.MenuItem(_tunes.list[i][1], null, i, {}));
        }
        if (_tunes.more()) {
            _menu.addItem(new WatchUi.MenuItem("More", _tunes.status.length() > 0 ? _tunes.status : null, :more, {}));
        }
        WatchUi.requestUpdate();
    }

    function onBack() as Void {
        _timer.stop();  // or it would pop the pages too
        _tunes.onPage = null;
        leave();
    }

    function leave() as Void {
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }
}

// The Presets list: START applies the preset on the deck, buzzes, and the bar says "APPLIED" for
// APPLIED_MS before the list returns to the pages. BACK leaves without applying.
class PresetMenuDelegate extends WatchUi.Menu2InputDelegate {
    private const APPLIED_MS = 1000;
    private var _deck as Deck;
    private var _timer as Timer.Timer = new Timer.Timer();
    private var _applying as Boolean = false;

    function initialize(deck as Deck) {
        Menu2InputDelegate.initialize();
        _deck = deck;
    }

    function onSelect(item as WatchUi.MenuItem) as Void {
        if (_applying || !(item instanceof PresetItem)) {
            return;  // one preset per visit: the list is on its way out
        }
        _applying = true;
        _deck.applyPreset(item.getId() as Number);  // the preset's number on the deck
        if (Attention has :vibrate) {
            Attention.vibrate([new Attention.VibeProfile(50, 120)]);
        }
        item.applied = true;
        WatchUi.requestUpdate();
        _timer.start(method(:leave), APPLIED_MS, false);
    }

    function onBack() as Void {
        _timer.stop();  // or it would pop the pages too
        leave();
    }

    function leave() as Void {
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }
}

// A zone's list: a colour or Off is sent for that zone and returns to the pages; Brightness swaps
// the list for the level screen (BACK there returns to the pages); Music mode (BOTH's) toggles it.
class ZoneMenuDelegate extends WatchUi.Menu2InputDelegate {
    private var _deck as Deck;
    private var _zone as Number;

    function initialize(deck as Deck, zone as Number) {
        Menu2InputDelegate.initialize();
        _deck = deck;
        _zone = zone;
    }

    function onSelect(item as WatchUi.MenuItem) as Void {
        if (item.getId() == :level) {
            var view = new LevelView(_deck, _zone);
            WatchUi.switchToView(view, new LevelDelegate(_deck, _zone, view), WatchUi.SLIDE_UP);
            return;
        }
        if (item.getId() == :music) {
            _deck.toggleMusic();
        } else {
            _deck.send(item.getId() as String, _zone);
        }
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }
}
