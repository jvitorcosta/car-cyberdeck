import Toybox.Application;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// The glance: Honda City's card in the watch's glance loop (UP/DOWN from the watch face). It shows
// the last state the app saw, saved in Application.Storage on every deck reply: a glance can't make
// its own web requests, so this is "as of the last time the app ran". START opens the app.
// Everything here is (:glance): the glance runs with only this code loaded.
// Purple is AA00FF: the deck's #8C00FF as the watch's 64 colours show it. COLORS' order (Deck.mc).
(:glance)
const GLANCE_COLORS = [
    ["Purple", 0xAA00FF], ["Red", 0xFF0000], ["Blue", 0x0040FF], ["Pink", 0xFF0090],
    ["Green", 0x00FF30], ["White", 0xFFFFFF], ["Orange", 0xFF6A00], ["Yellow", 0xFFD400],
    ["Cyan", 0x00AAFF],
];

(:glance)
class HondaGlance extends WatchUi.GlanceView {
    function initialize() {
        GlanceView.initialize();
    }

    function onUpdate(dc as Dc) as Void {
        var h = dc.getHeight();
        var inside = Application.Storage.getValue("in");
        var outside = Application.Storage.getValue("out");
        var j = Graphics.TEXT_JUSTIFY_LEFT | Graphics.TEXT_JUSTIFY_VCENTER;

        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(0, h / 4, Graphics.FONT_GLANCE, "HONDA CITY", j);
        if (!(inside instanceof Array) || !(outside instanceof Array)) {
            dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
            dc.drawText(0, h * 3 / 4, Graphics.FONT_GLANCE, "open to connect", j);
            return;
        }
        var x = dot(dc, 0, h * 3 / 4, inside as Array);
        x = text(dc, x, h * 3 / 4, inside as Array, "In");
        x = dot(dc, x + 10, h * 3 / 4, outside as Array);
        text(dc, x, h * 3 / 4, outside as Array, "Out");
    }

    // A small dot in the light's colour (grey when off); returns the x after it.
    private function dot(dc as Dc, x as Number, y as Number, l as Array) as Number {
        var lit = l[2] as Boolean;
        var rgb = (GLANCE_COLORS[(l[0] as Number) % GLANCE_COLORS.size()] as Array)[1] as Number;
        dc.setColor(Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x + 7, y, 7);
        dc.setColor(lit ? rgb : Graphics.COLOR_DK_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x + 7, y, 6);
        return x + 18;
    }

    // "In 80%" (or "In off"); returns the x after it.
    private function text(dc as Dc, x as Number, y as Number, l as Array, zone as String) as Number {
        var s = zone + " " + ((l[2] as Boolean) ? (l[1] as Number) + "%" : "off");
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y, Graphics.FONT_GLANCE, s, Graphics.TEXT_JUSTIFY_LEFT | Graphics.TEXT_JUSTIFY_VCENTER);
        return x + dc.getTextWidthInPixels(s, Graphics.FONT_GLANCE);
    }
}
