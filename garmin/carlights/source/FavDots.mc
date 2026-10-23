import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// A preset's two lights as two dots side by side, inside then outside, each in its colour (dark
// grey when that light is off). The icon of a Presets list item; the page draws the same dots
// per row through paint().
class FavDots extends WatchUi.Drawable {
    private var _fav as Array;  // [inColor, inPct, inLit, outColor, outPct, outLit]

    function initialize(fav as Array) {
        Drawable.initialize({});
        _fav = fav;
    }

    function draw(dc as Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var r = (h / 2 < w / 4 ? h / 2 : w / 4) - 2;
        paint(dc, w / 4, w * 3 / 4, h / 2, r);
    }

    // The inside dot centred on (xIn, y), the outside one on (xOut, y).
    function paint(dc as Dc, xIn as Number, xOut as Number, y as Number, r as Number) as Void {
        dot(dc, xIn, y, r, _fav[2] as Boolean ? (COLORS[_fav[0] as Number] as Array)[2] as Number : Graphics.COLOR_DK_GRAY);
        dot(dc, xOut, y, r, _fav[5] as Boolean ? (COLORS[_fav[3] as Number] as Array)[2] as Number : Graphics.COLOR_DK_GRAY);
    }

    private function dot(dc as Dc, x as Number, y as Number, r as Number, rgb as Number) as Void {
        dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x, y, r + 1);  // a thin rim keeps white and yellow visible
        dc.setColor(rgb, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x, y, r);
    }
}
