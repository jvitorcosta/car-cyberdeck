import Toybox.Attention;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Timer;
import Toybox.WatchUi;

// The zone pages' buttons:
//   UP / DOWN: previous / next page, looping BOTH -> INSIDE -> OUTSIDE -> PRESETS
//   START:     that zone's list (colours, Brightness >, Off); on PRESETS the deck's presets as
//              bars, START on one applies it                     BACK: leave
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
