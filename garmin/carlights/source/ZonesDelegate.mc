import Toybox.Lang;
import Toybox.WatchUi;

// The zone pages' buttons:
//   UP / DOWN: previous / next page, looping BOTH -> INSIDE -> OUTSIDE -> FAVOURITES
//   START:     that zone's list (colours, Brightness >, Off); on FAVOURITES the list of saved
//              favourites to apply                                  BACK: leave
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
        if (zone == FAVS) {
            return favourites();
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

    // The Favourites list: an item per saved slot (label F1, "Pink 50 / Pink 100", the two dots);
    // a single "None saved" when the deck has none.
    private function favourites() as Boolean {
        var menu = new WatchUi.Menu2({:title => "Favourites"});
        var any = false;
        for (var i = 0; i < 4; i++) {
            var f = _deck.favs[i];
            if (f != null) {
                var fav = f as Array;
                menu.addItem(new WatchUi.IconMenuItem("F" + (i + 1),
                    say(fav[0] as Number, fav[1] as Number, fav[2] as Boolean) + " / "
                    + say(fav[3] as Number, fav[4] as Number, fav[5] as Boolean),
                    i + 1, new FavDots(fav), {}));
                any = true;
            }
        }
        if (!any) {
            menu.addItem(new WatchUi.MenuItem("None saved", null, :none, {}));
        }
        WatchUi.pushView(menu, new FavMenuDelegate(_deck), WatchUi.SLIDE_LEFT);
        return true;
    }

    // "Pink 50" or "Off" for one light of a favourite.
    private function say(color as Number, pct as Number, lit as Boolean) as String {
        return lit ? ((COLORS[color] as Array)[0] as String) + " " + pct : "Off";
    }
}

// The Favourites list: a favourite is applied on the deck and the list returns to the pages.
class FavMenuDelegate extends WatchUi.Menu2InputDelegate {
    private var _deck as Deck;

    function initialize(deck as Deck) {
        Menu2InputDelegate.initialize();
        _deck = deck;
    }

    function onSelect(item as WatchUi.MenuItem) as Void {
        var id = item.getId();
        if (id instanceof Number) {  // "None saved" has a symbol id and does nothing
            _deck.applyFav(id);
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
