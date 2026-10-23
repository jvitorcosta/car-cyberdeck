import Toybox.Graphics;
import Toybox.Lang;

// Emerald's battle screen, drawn with shapes in the 255's 64 colours: the tall-grass field (dark
// at night), the Pokémon's platform, the player-side info box with its HP / EXP bars, the dark
// menu box, and text with the game's drop shadow.
module Battle {
    // Day: a pale sky over the grass field; night: a dark sky over a dim field.
    function field(dc as Dc, night as Boolean) as Void {
        var h = dc.getHeight();
        dc.setColor(night ? 0x000055 : 0xFFFFAA, night ? 0x000055 : 0xFFFFAA);
        dc.clear();
        dc.setColor(night ? 0x0000AA : 0xAAFFAA, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(0, 52, dc.getWidth(), 20);   // the horizon's lighter band
        dc.setColor(night ? 0x005500 : 0x55AA55, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(0, 72, dc.getWidth(), h - 72);
    }

    function platform(dc as Dc, cx as Number, cy as Number, night as Boolean) as Void {
        dc.setColor(night ? 0x005500 : 0x55AA00, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(cx, cy, 58, 13);
        dc.setColor(night ? 0x00AA00 : 0xAAFF55, Graphics.COLOR_TRANSPARENT);
        dc.fillEllipse(cx, cy - 1, 54, 10);
    }

    // Cream box, dark teal border, the bottom-right corner cut like the game's.
    function infoBox(dc as Dc, x as Number, y as Number, w as Number, h as Number) as Void {
        var c = 8;
        var pts = [[x, y], [x + w, y], [x + w, y + h - c], [x + w - c, y + h], [x, y + h]];
        dc.setColor(0x005555, Graphics.COLOR_TRANSPARENT);
        dc.fillPolygon(pts);
        dc.setColor(0xFFFFAA, Graphics.COLOR_TRANSPARENT);
        dc.fillPolygon([[x + 2, y + 2], [x + w - 2, y + 2], [x + w - 2, y + h - c - 1],
                        [x + w - c - 1, y + h - 2], [x + 2, y + h - 2]]);
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

    // The battle menu: dark blue with a gold rim and a white inner line.
    function menuBox(dc as Dc, x as Number, y as Number, w as Number, h as Number) as Void {
        dc.setColor(0xAAAA55, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y, w, h);
        dc.setColor(0x000055, Graphics.COLOR_TRANSPARENT);
        dc.fillRectangle(x, y + 3, w, h - 3);
        dc.setColor(0xFFFFFF, Graphics.COLOR_TRANSPARENT);
        dc.drawRoundedRectangle(x + 4, y + 6, w - 8, h - 9, 4);
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
