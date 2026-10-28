import Toybox.Graphics;
import Toybox.Lang;

// The "filled" look: a light's colour rises from the bottom of the round face to its brightness
// (30 % fills the lower third), black above. Used whole-face by the INSIDE / OUTSIDE pages and the
// Brightness screen, and as two columns side by side by the BOTH page. Text crossing the fill line
// is drawn twice, clipped to each side, so every part of a word reads: white over black, dark or
// light over the colour as it needs.
module Fill {
    // Fills the column [x, x + w) up to pct (nothing when off); returns where the colour starts.
    function column(dc as Dc, x as Number, w as Number, rgb as Number, lit as Boolean, pct as Number) as Number {
        var h = dc.getHeight();
        var top = lit ? h - h * pct / 100 : h;
        if (top < h) {
            dc.setColor(rgb, Graphics.COLOR_TRANSPARENT);
            dc.fillRectangle(x, top, w, h - top);
        }
        return top;
    }

    // The ink that reads on a colour: black on the light ones (white, yellow, green, cyan),
    // white on the rest.
    function ink(rgb as Number) as Graphics.ColorType {
        return rgb == 0xFFFFFF || rgb == 0xFFD400 || rgb == 0x00FF30 || rgb == 0x00AAFF
            ? Graphics.COLOR_BLACK : Graphics.COLOR_WHITE;
    }

    // A whole-face page: title (small, top), main (big, centre), sub (under it), foot (bottom).
    function page(dc as Dc, rgb as Number, lit as Boolean, pct as Number, title as String,
                  main as String, sub as String, foot as String, warn as Boolean) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var cx = w / 2;
        var cy = h / 2;
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();
        var top = column(dc, 0, w, rgb, lit, pct);
        var on = ink(rgb);
        line(dc, 0, w, top, cx, 40, Graphics.FONT_XTINY, title, Graphics.COLOR_LT_GRAY, on);
        line(dc, 0, w, top, cx, cy - 12, Graphics.FONT_MEDIUM, main, Graphics.COLOR_WHITE, on);
        line(dc, 0, w, top, cx, cy + 22, Graphics.FONT_SMALL, sub, Graphics.COLOR_LT_GRAY, on);
        line(dc, 0, w, top, cx, h - 32, Graphics.FONT_XTINY, foot,
             warn ? Graphics.COLOR_ORANGE : Graphics.COLOR_DK_GRAY, on);
    }

    // One line of text centred on x, kept inside the column [cx0, cx0 + cw) and split at the fill
    // line `top`.
    function line(dc as Dc, cx0 as Number, cw as Number, top as Number, x as Number, y as Number,
                  font as Graphics.FontType, text as String, onDark as Graphics.ColorType,
                  onFill as Graphics.ColorType) as Void {
        if (text.length() == 0) {
            return;
        }
        var h = dc.getHeight();
        var j = Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER;
        if (top > 0) {
            dc.setClip(cx0, 0, cw, top);
            dc.setColor(onDark, Graphics.COLOR_TRANSPARENT);
            dc.drawText(x, y, font, text, j);
        }
        if (top < h) {
            dc.setClip(cx0, top, cw, h - top);
            dc.setColor(onFill, Graphics.COLOR_TRANSPARENT);
            dc.drawText(x, y, font, text, j);
        }
        dc.clearClip();
    }
}
