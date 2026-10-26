import Toybox.Graphics;
import Toybox.Lang;
import Toybox.Math;
import Toybox.WatchUi;

// The zone pages, all in the "filled" look (see Fill): BOTH shows inside (left) and outside
// (right) as two columns filled up to their brightness; INSIDE and OUTSIDE fill the whole face.
// A fourth page shows the deck's named presets as bars (see PresetBar). A controller that isn't
// linked says "Not connected".
// Page dots on the right; the status shows while something is being sent or failed.
class ZonesView extends WatchUi.View {
    var page as Number = BOTH;  // opens on BOTH: every zone is one press away from it
    private var _deck as Deck;

    function initialize(deck as Deck) {
        View.initialize();
        _deck = deck;
    }

    function onUpdate(dc as Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var cx = w / 2;
        var cy = h / 2;
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();

        if (page == BOTH) {
            // inside on the left, outside on the right: each a column filled up to its brightness,
            // like the single pages. BOTH on top and the hint or status at the bottom sit on black
            // pills over the middle line, so the line doesn't cut them.
            column(dc, 0, cx - 2, cx - 58, cy, "INSIDE", _deck.inside);
            column(dc, cx + 2, w - cx - 2, cx + 58, cy, "OUTSIDE", _deck.outside);
            dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
            dc.fillRectangle(cx - 2, 0, 4, h);
            var busy = _deck.status.length() > 0;
            var inGone = _deck.links[0].equals("nolink");
            var outGone = _deck.links[1].equals("nolink");
            var note = inGone && outGone ? "Not connected" : inGone ? "Inside not connected"
                     : outGone ? "Outside not connected" : "";
            pill(dc, cx, 40, "BOTH", Graphics.COLOR_WHITE);
            pill(dc, cx, h - 32, busy ? _deck.status : note.length() > 0 ? note : "START: change",
                 busy || note.length() > 0 ? Graphics.COLOR_ORANGE : Graphics.COLOR_WHITE);
        } else if (page == PRESETS) {
            presets(dc, cx, h);
        } else {
            // one zone: its colour filled up from the bottom to its brightness
            var l = _deck.look(page);
            var c = COLORS[l.color] as Array;
            var busy = _deck.status.length() > 0;
            var gone = _deck.links[page - 1].equals("nolink");
            Fill.page(dc, c[2] as Number, l.lit, l.pct, page == INSIDE ? "INSIDE" : "OUTSIDE",
                      l.lit ? c[0] as String : "Off", l.lit ? l.pct + "%" : "",
                      busy ? _deck.status : gone ? "Not connected" : "START: change", busy || gone);
        }

        // page dots on the right: BOTH, INSIDE, OUTSIDE, PRESETS from top to bottom
        // (black-rimmed, so they read on a coloured half too)
        for (var i = 0; i < 4; i++) {
            dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(w - 14, cy - 21 + i * 14, 6);
            dc.setColor(i == page ? Graphics.COLOR_WHITE : Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.fillCircle(w - 14, cy - 21 + i * 14, 4);
        }
    }

    // The Presets page: PRESETS on top, each preset's bar as the list will show it (PresetBar; up
    // to six, narrower where the round face is), and the hint or status at the bottom. "No presets
    // yet" when /presets hasn't answered.
    private function presets(dc as Dc, cx as Number, h as Number) as Void {
        pill(dc, cx, 40, "PRESETS", Graphics.COLOR_WHITE);
        var list = _deck.presets;
        if (list.size() == 0) {
            dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(cx, cx, Graphics.FONT_SMALL, "No presets yet",
                        Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        }
        // rows between the two pills, centred on the face, in slots for four: 32 px apart and 24
        // tall on the 255; five or six fill the same span, 26 px apart and 19 tall there, the name
        // a size smaller
        var rows = list.size() > 6 ? 6 : list.size();
        var slots = rows > 4 ? rows : 4;
        var pitch = (h - 104) / slots;
        pitch = pitch > 32 ? 32 : pitch;
        var bh = pitch * 3 / 4;
        var font = slots > 4 ? Graphics.FONT_TINY : Graphics.FONT_SMALL;
        var cy = h / 2;
        for (var i = 0; i < rows; i++) {
            var y = cy - pitch * (slots - 1) / 2 + i * pitch;
            var d = (y > cy ? y - cy : cy - y) + bh / 2;  // the row's edge farthest from the centre
            var half = Math.sqrt(cx * cx - d * d).toNumber() - 10;
            if (half > PresetBar.LIST_W / 2) {
                half = PresetBar.LIST_W / 2;  // as wide as the list's (and clear of the page dots)
            }
            var p = list[i] as Array;
            PresetBar.draw(dc, cx - half, y - bh / 2, half * 2, bh, p, p[0] as String, false, font);
        }
        var busy = _deck.status.length() > 0;
        if (busy || list.size() > 0) {
            pill(dc, cx, h - 32, busy ? _deck.status : "START: apply",
                 busy ? Graphics.COLOR_ORANGE : Graphics.COLOR_WHITE);
        }
    }

    // Text on a black rounded pill centred on (x, y).
    private function pill(dc as Dc, x as Number, y as Number, text as String, color as Graphics.ColorType) as Void {
        var pw = dc.getTextWidthInPixels(text, Graphics.FONT_XTINY) + 20;
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(x - pw / 2, y - 12, pw, 24, 12);
        dc.setColor(color, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y, Graphics.FONT_XTINY, text, Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // One column of the BOTH page [x, x + cw): the light's colour filled up to its brightness, with
    // its zone, colour name and level centred on tx. Returns where the fill starts.
    private function column(dc as Dc, x as Number, cw as Number, tx as Number, cy as Number,
                            label as String, l as Look) as Number {
        var c = COLORS[l.color] as Array;
        var rgb = c[2] as Number;
        var top = Fill.column(dc, x, cw, rgb, l.lit, l.pct);
        var on = Fill.ink(rgb);
        Fill.line(dc, x, cw, top, tx, cy - 42, Graphics.FONT_XTINY, label, Graphics.COLOR_LT_GRAY, on);
        Fill.line(dc, x, cw, top, tx, cy - 16, Graphics.FONT_SMALL, l.lit ? c[0] as String : "Off",
                  Graphics.COLOR_WHITE, on);
        Fill.line(dc, x, cw, top, tx, cy + 8, Graphics.FONT_XTINY, l.lit ? l.pct + "%" : "",
                  Graphics.COLOR_LT_GRAY, on);
        return top;
    }
}
