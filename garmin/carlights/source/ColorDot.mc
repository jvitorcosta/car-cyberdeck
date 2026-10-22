import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// A filled circle in one colour: the icon beside each colour in a zone's list.
class ColorDot extends WatchUi.Drawable {
    private var _color as Number;

    function initialize(color as Number) {
        Drawable.initialize({});
        _color = color;
    }

    function draw(dc as Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var r = (w < h ? w : h) / 2 - 2;
        dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(w / 2, h / 2, r + 1);  // a thin rim keeps white and yellow visible
        dc.setColor(_color, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(w / 2, h / 2, r);
    }
}
