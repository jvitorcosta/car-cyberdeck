import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// Emerald's battle screen in the deck's colours (src/ui/theme.cpp DAY / NIGHT, snapped to the 255's
// 64): the field image (tools/gen_pokeface.py dithers its sky and grass), the Pokémon's platform,
// the cream battle box with its drop shadow, the teal-framed dialogue box, and text with the
// game's drop shadow.
module Battle {
    // Text on the field, and its shadow (deck: 404040 on D8D0B0; night: cream on near-black)
    function ink(night as Boolean) as Number {
        return night ? 0xFFFFAA : 0x555555;
    }

    function shade(night as Boolean) as Number {
        return night ? 0x000000 : 0xAAAAAA;
    }

    function platform(dc as Dc, cx as Number, cy as Number, night as Boolean) as Void {
        dc.setColor(night ? 0x000000 : 0x55AA55, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(cx, cy, 58, 13);
        dc.setColor(night ? 0x005555 : 0xAAFFAA, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(cx, cy - 1, 55, 10);
    }

    // The deck's battle box: shadow 3 px down-right, dark border, cream fill (slate at night),
    // the right side slanted.
    function infoBox(dc as Dc, x as Number, y as Number, w as Number, h as Number, night as Boolean) as Void {
        slanted(dc, x + 3, y + 3, w, h, 10, night ? 0x000000 : 0x555555);
        slanted(dc, x, y, w, h, 10, night ? 0x000000 : 0x555555);
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

    // The deck's dialogue box: teal frame, a lighter inner line, white fill (dark at night).
    function dialogue(dc as Dc, x as Number, y as Number, w as Number, h as Number, night as Boolean) as Void {
        dc.setColor(night ? 0x000000 : 0x005555, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y, w, h);
        dc.setColor(night ? 0x005555 : 0x55AAAA, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y + 3, w, h - 3);
        dc.setColor(night ? 0x000055 : 0xFFFFFF, Graphics.COLOR_TRANSPARENT);
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
