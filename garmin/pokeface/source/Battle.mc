import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// Emerald's battle screen laid out like the deck's (src/ui/battle.cpp) in solid, vivid colours: the
// pale ones washed out on the 255's reflective screen. The field image (tools/gen_pokeface.py), the
// Pokémon's platform, the cream battle box with its drop shadow, the navy dialogue box, and text
// with the game's drop shadow.
module Battle {
    // Text on the sky, and its shadow: black on the day's blue, cream on the night's navy
    function ink(night as Boolean) as Number {
        return night ? 0xFFFFAA : 0x000000;
    }

    function shade(night as Boolean) as Number {
        return night ? 0x000000 : 0x0055AA;
    }

    function platform(dc as Dc, cx as Number, cy as Number, night as Boolean) as Void {
        dc.setColor(night ? 0x000000 : 0x005500, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(cx, cy, 58, 13);
        dc.setColor(night ? 0x00AA55 : 0x55AA00, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(cx, cy - 1, 55, 10);
    }

    // The deck's battle box: shadow 3 px down-right, black border, cream fill (navy at night),
    // the right side slanted.
    function infoBox(dc as Dc, x as Number, y as Number, w as Number, h as Number, night as Boolean) as Void {
        slanted(dc, x + 3, y + 3, w, h, 10, night ? 0x000000 : 0x005500);
        slanted(dc, x, y, w, h, 10, 0x000000);
        slanted(dc, x + 2, y + 2, w - 4, h - 4, 8, night ? 0x000055 : 0xFFFFAA);
    }

    function slanted(dc as Dc, x as Number, y as Number, w as Number, h as Number, c as Number, colour as Number) as Void {
        dc.setColor(colour, Graphics.COLOR_TRANSPARENT);
        dc.fillPolygon([[x, y], [x + w, y], [x + w - c, y + h], [x, y + h]]);
    }

    // A bar's dark track and its fill (frac 0..1); colour null leaves it empty.
    function bar(dc as Dc, x as Number, y as Number, w as Number, frac as Float, colour as Number or Null) as Void {
        dc.setColor(0x555555, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y, w, 5);
        dc.setColor(0x000000, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x + 1, y + 1, w - 2, 3);
        var fill = ((w - 2) * frac).toNumber();
        if (colour != null && fill > 0) {
            dc.setColor(colour, Graphics.COLOR_TRANSPARENT);
            dc.fillRectangle(x + 1, y + 1, fill, 3);
        }
    }

    // The dialogue box: black edge, a teal line, navy fill, for white text.
    function dialogue(dc as Dc, x as Number, y as Number, w as Number, h as Number, night as Boolean) as Void {
        dc.setColor(0x000000, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y, w, h);
        dc.setColor(night ? 0x005555 : 0x00AAAA, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y + 3, w, h - 3);
        dc.setColor(0x000055, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y + 5, w, h - 5);
    }

    // Emerald text: the shadow `px` down and right, then the letters.
    function text(dc as Dc, x as Number, y as Number, font as Graphics.FontType, s as String, colour as Number,
                  shadow as Number, px as Number, justify as Number) as Void {
        var j = justify | Graphics.TEXT_JUSTIFY_VCENTER;
        dc.setColor(shadow, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x + px, y + px, font, s, j);
        dc.setColor(colour, Graphics.COLOR_TRANSPARENT);
        dc.drawText(x, y, font, s, j);
    }
}
