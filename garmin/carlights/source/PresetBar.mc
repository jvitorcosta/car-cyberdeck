import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// A preset as one filled bar: the left half is the inside light, the right half the outside one,
// each filled from its left edge to that light's brightness in its colour (ROSE's inside at 40 %
// fills 40 % of the left half), the rest dark grey; a light that's off stays grey with "OFF" in
// it. The name sits centred over the bar and, like Fill.line, every part of it is drawn in the
// ink that reads on what's under it. The PRESETS page draws a column of these; the Presets list
// is made of them (PresetItem).
module PresetBar {
    const GREY = 0x555555;  // the unfilled part
    const GAP = 3;          // black between the halves
    const RADIUS = 6;       // the bar's rounded ends
    // The list's bar width. The CustomMenu gives its rows ~210 of the 255's 260 px (a scroll bar
    // takes the left side), centred; a bar this wide clears the round face on the focused row.
    // A narrower row (a smaller watch) gets a bar 4 px in from each side. barW is what the rows
    // last used, so the title's IN / OUT sit over the halves.
    const LIST_W = 202;
    var barW as Number = LIST_W;

    // The bar in [x, x + w) by [y, y + h). p: a Deck preset; text: its name, or "APPLIED", in
    // font (FONT_SMALL; the PRESETS page's shorter bars take FONT_TINY). levels: each light's level
    // ("40%", or "OFF") near the half's outer end (the list's focused row).
    function draw(dc as Dc, x as Number, y as Number, w as Number, h as Number, p as Array,
                  text as String, levels as Boolean, font as Graphics.FontType) as Void {
        var hw = (w - GAP) / 2;
        var xOut = x + w - hw;
        var fills = [half(dc, x, y, w, h, x, hw, p[1] as Number, p[2] as Number, p[3] as Boolean),
                     half(dc, x, y, w, h, xOut, hw, p[4] as Number, p[5] as Number, p[6] as Boolean)];
        var halves = [x, xOut];
        var rgbs = [rgbOf(p[1] as Number), rgbOf(p[4] as Number)];
        var cy = y + h / 2;
        var cx = x + w / 2;
        var j = Graphics.TEXT_JUSTIFY_CENTER;
        label(dc, x, y, w, h, halves, fills, rgbs, cx, cy, font, text, j, Graphics.COLOR_WHITE);

        // "OFF" in a light that's off: in the slot near its outer end on the focused row, else in
        // the middle of the grey left free by the name (the half's own middle would touch a long one)
        var nameW = dc.getTextWidthInPixels(text, font);
        for (var i = 0; i < 2; i++) {
            var lit = p[i * 3 + 3] as Boolean;
            var tx;
            if (levels) {
                tx = i == 0 ? x + RADIUS + 2 : x + w - RADIUS - 2;
                j = i == 0 ? Graphics.TEXT_JUSTIFY_LEFT : Graphics.TEXT_JUSTIFY_RIGHT;
            } else if (!lit) {
                var mid = halves[i] + hw / 2;
                var free = i == 0 ? (x + RADIUS + cx - nameW / 2) / 2 : (cx + nameW / 2 + x + w - RADIUS) / 2;
                tx = i == 0 ? (free < mid ? free : mid) : (free > mid ? free : mid);
                j = Graphics.TEXT_JUSTIFY_CENTER;
            } else {
                continue;
            }
            label(dc, x, y, w, h, halves, fills, rgbs, tx, cy, Graphics.FONT_XTINY,
                  lit ? (p[i * 3 + 2] as Number) + "%" : "OFF", j, Graphics.COLOR_LT_GRAY);
        }
    }

    // One half [hx, hx + hw): grey, then its colour from its left edge to pct (the bar's rounded
    // outline clipped to it, so only the bar's own ends are round). Returns the filled width.
    function half(dc as Dc, x as Number, y as Number, w as Number, h as Number, hx as Number,
                  hw as Number, color as Number, pct as Number, lit as Boolean) as Number {
        dc.setClip(hx, y, hw, h);
        dc.setColor(GREY, Graphics.COLOR_TRANSPARENT);
        dc.fillRoundedRectangle(x, y, w, h, RADIUS);
        var fw = lit ? hw * pct / 100 : 0;
        if (fw > 0) {
            dc.setClip(hx, y, fw, h);
            dc.setColor(rgbOf(color), Graphics.COLOR_TRANSPARENT);
            dc.fillRoundedRectangle(x, y, w, h, RADIUS);
        }
        dc.clearClip();
        return fw;
    }

    // Text over the bar: onGrey over the grey (and the gap), then again over each filled part in
    // the ink that reads on its colour (Fill.ink: black on the light ones).
    function label(dc as Dc, x as Number, y as Number, w as Number, h as Number, halves as Array,
                   fills as Array, rgbs as Array, tx as Number, ty as Number, font as Graphics.FontType,
                   text as String, j as Number, onGrey as Graphics.ColorType) as Void {
        j = j | Graphics.TEXT_JUSTIFY_VCENTER;
        dc.setClip(x, y, w, h);
        dc.setColor(onGrey, Graphics.COLOR_TRANSPARENT);
        dc.drawText(tx, ty, font, text, j);
        for (var i = 0; i < 2; i++) {
            var fw = fills[i] as Number;
            if (fw > 0) {
                dc.setClip(halves[i] as Number, y, fw, h);
                dc.setColor(Fill.ink(rgbs[i] as Number), Graphics.COLOR_TRANSPARENT);
                dc.drawText(tx, ty, font, text, j);
            }
        }
        dc.clearClip();
    }

    function rgbOf(color as Number) as Number {
        return (COLORS[color] as Array)[2] as Number;
    }
}

// A row of the Presets list: the preset's bar. The focused row is taller (the menu's focus
// height), outlined in white and shows the levels; after START it says "APPLIED" for a moment.
class PresetItem extends WatchUi.CustomMenuItem {
    var applied as Boolean = false;
    private var _p as Array;

    function initialize(p as Array) {
        CustomMenuItem.initialize(p[7], {});  // the id is the preset's number on the deck
        _p = p;
    }

    function draw(dc as Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var focused = isFocused();
        var m = focused ? 5 : 3;  // room above and below (the focused row's outline sits in it)
        var bw = w - 8 < PresetBar.LIST_W ? w - 8 : PresetBar.LIST_W;
        var x = (w - bw) / 2;
        PresetBar.barW = bw;
        if (focused) {
            dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
            dc.drawRoundedRectangle(x - 1, m - 1, bw + 2, h - 2 * m + 2, PresetBar.RADIUS + 1);
            dc.drawRoundedRectangle(x - 2, m - 2, bw + 4, h - 2 * m + 4, PresetBar.RADIUS + 2);
        }
        PresetBar.draw(dc, x, m, bw, h - 2 * m, _p, applied ? "APPLIED" : _p[0] as String, focused,
                       Graphics.FONT_SMALL);
    }
}

// The Presets list's title: PRESETS, and IN / OUT over the bars' halves.
class PresetTitle extends WatchUi.Drawable {
    function initialize() {
        Drawable.initialize({});
    }

    function draw(dc as Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var hw = (PresetBar.barW - PresetBar.GAP) / 2;
        var x = (w - PresetBar.barW) / 2;  // the title spans the screen, the rows are centred on it
        var j = Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER;
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(w / 2, h - 34, Graphics.FONT_XTINY, "PRESETS", j);
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x + hw / 2, h - 12, Graphics.FONT_XTINY, "IN", j);
        dc.drawText(w - x - hw / 2, h - 12, Graphics.FONT_XTINY, "OUT", j);
    }
}
