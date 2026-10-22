import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Timer;
import Toybox.WatchUi;

// Brightness for one zone: the level as a big number over the zone's colour filled up to it (see
// Fill). UP / DOWN change it by 10 % (10..100); the deck gets it SEND_AFTER_MS after the
// last press, so a quick run of presses sends only the final level. START or BACK returns.
class LevelView extends WatchUi.View {
    var pct as Number;
    private var _deck as Deck;
    private var _zone as Number;

    function initialize(deck as Deck, zone as Number) {
        View.initialize();
        _deck = deck;
        _zone = zone;
        pct = deck.look(zone == BOTH ? INSIDE : zone).pct;
    }

    // The zone's colour filled up to the level being set: it rises and falls as UP/DOWN are pressed.
    function onUpdate(dc as Dc) as Void {
        var rgb = (COLORS[_deck.look(_zone == BOTH ? INSIDE : _zone).color] as Array)[2] as Number;
        var busy = _deck.status.length() > 0;
        Fill.page(dc, rgb, true, pct, ["BOTH", "INSIDE", "OUTSIDE"][_zone] as String, pct + "%",
                  "brightness", busy ? _deck.status : "UP / DOWN", busy);
    }
}

class LevelDelegate extends WatchUi.BehaviorDelegate {
    private const SEND_AFTER_MS = 500;
    private var _deck as Deck;
    private var _zone as Number;
    private var _view as LevelView;
    private var _timer as Timer.Timer = new Timer.Timer();

    function initialize(deck as Deck, zone as Number, view as LevelView) {
        BehaviorDelegate.initialize();
        _deck = deck;
        _zone = zone;
        _view = view;
    }

    function onPreviousPage() as Boolean {
        change(10);
        return true;
    }

    function onNextPage() as Boolean {
        change(-10);
        return true;
    }

    private function change(d as Number) as Void {
        var p = _view.pct + d;
        _view.pct = p < 10 ? 10 : p > 100 ? 100 : p;
        WatchUi.requestUpdate();
        _timer.stop();
        _timer.start(method(:sendLevel), SEND_AFTER_MS, false);
    }

    function sendLevel() as Void {
        _deck.send(_view.pct + "%", _zone);
    }

    function onSelect() as Boolean {
        return onBack();
    }

    function onBack() as Boolean {
        WatchUi.popView(WatchUi.SLIDE_DOWN);
        return true;
    }
}
