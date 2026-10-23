import Toybox.Lang;
import Toybox.WatchUi;

// The zone pages' buttons:
//   UP / DOWN: previous / next page, looping BOTH -> INSIDE -> OUTSIDE -> PRESETS
//   START:     that zone's list (colours, Brightness >, Off); on PRESETS the list of the deck's
//              presets to apply                                  BACK: leave
class ZonesDelegate extends WatchUi.BehaviorDelegate {
    private var _deck as Deck;
    private var _view as ZonesView;

    function initialize(deck as Deck, view as ZonesView) {
        BehaviorDelegate.initialize();
        _deck = deck;
        _view = view;
    }

    function onNextPage() as Boolean {
        _view.page = (_view.page + 1) % 4;
        WatchUi.requestUpdate();
        return true;
    }

    function onPreviousPage() as Boolean {
        _view.page = (_view.page + 3) % 4;
        WatchUi.requestUpdate();
        return true;
    }

    function onSelect() as Boolean {
        var zone = _view.page;
        if (zone == PRESETS) {
            return presets();
        }
        var menu = new WatchUi.Menu2({:title => ["Both", "Inside", "Outside"][zone] as String});
        for (var i = 0; i < COLORS.size(); i++) {
            var c = COLORS[i] as Array;
            menu.addItem(new WatchUi.IconMenuItem(c[0] as String, null, c[1] as String, new ColorDot(c[2] as Number), {}));
        }
        menu.addItem(new WatchUi.MenuItem("Brightness", null, :level, {}));
        menu.addItem(new WatchUi.MenuItem("Off", null, "off", {}));
        // start on the zone's current colour (inside's for BOTH)
        menu.setFocus(_deck.look(zone == BOTH ? INSIDE : zone).color);
        WatchUi.pushView(menu, new ZoneMenuDelegate(_deck, zone), WatchUi.SLIDE_LEFT);
        return true;
    }

    // The Presets list: an item per preset (its name, "Pink 40 / Pink 100", the two dots). Nothing
    // opens while the deck hasn't sent any.
    private function presets() as Boolean {
        if (_deck.presets.size() == 0) {
            return true;
        }
        var menu = new WatchUi.Menu2({:title => "Presets"});
        for (var i = 0; i < _deck.presets.size(); i++) {
            var p = _deck.presets[i] as Array;
            menu.addItem(new WatchUi.IconMenuItem(p[0] as String,
                say(p[1] as Number, p[2] as Number, p[3] as Boolean) + " / "
                + say(p[4] as Number, p[5] as Number, p[6] as Boolean),
                p[7] as Number, new FavDots(p.slice(1, 7)), {}));
        }
        WatchUi.pushView(menu, new PresetMenuDelegate(_deck), WatchUi.SLIDE_LEFT);
        return true;
    }

    // "Pink 40" or "Off" for one light of a preset.
    private function say(color as Number, pct as Number, lit as Boolean) as String {
        return lit ? ((COLORS[color] as Array)[0] as String) + " " + pct : "Off";
    }
}

// The Presets list: a preset is applied on the deck and the list returns to the pages.
class PresetMenuDelegate extends WatchUi.Menu2InputDelegate {
    private var _deck as Deck;

    function initialize(deck as Deck) {
        Menu2InputDelegate.initialize();
        _deck = deck;
    }

    function onSelect(item as WatchUi.MenuItem) as Void {
        var id = item.getId();
        if (id instanceof Number) {  // the preset's number on the deck
            _deck.applyPreset(id);
            WatchUi.popView(WatchUi.SLIDE_RIGHT);
        }
    }
}

// A zone's list: a colour or Off is sent for that zone and returns to the pages; Brightness swaps
// the list for the level screen (BACK there returns to the pages).
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
        _deck.send(item.getId() as String, _zone);
        WatchUi.popView(WatchUi.SLIDE_RIGHT);
    }
}
