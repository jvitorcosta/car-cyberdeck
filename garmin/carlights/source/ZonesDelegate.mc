import Toybox.Lang;
import Toybox.WatchUi;

// The zone pages' buttons:
//   UP / DOWN: previous / next page, looping BOTH -> INSIDE -> OUTSIDE (each one press from BOTH)
//   START:     that zone's list (colours, Brightness >, Off)        BACK: leave
class ZonesDelegate extends WatchUi.BehaviorDelegate {
    private var _deck as Deck;
    private var _view as ZonesView;

    function initialize(deck as Deck, view as ZonesView) {
        BehaviorDelegate.initialize();
        _deck = deck;
        _view = view;
    }

    function onNextPage() as Boolean {
        _view.page = (_view.page + 1) % 3;
        WatchUi.requestUpdate();
        return true;
    }

    function onPreviousPage() as Boolean {
        _view.page = (_view.page + 2) % 3;
        WatchUi.requestUpdate();
        return true;
    }

    function onSelect() as Boolean {
        var zone = _view.page;
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
