import Toybox.Lang;
import Toybox.Math;

// What fits on the 255's round screen (260 px, radius 130): a line's width against the circle at
// the top and bottom of its letters.
module Fit {
    const R = 130;

    // Half the visible width at height y (0 outside the circle).
    function halfChord(y as Number) as Number {
        var d = y - R;
        if (d <= -R || d >= R) {
            return 0;
        }
        return Math.sqrt(R * R - d * d).toNumber();
    }

    // Whether a line `w` wide, centred, with ink from yTop to yBottom, stays `margin` px inside the edge.
    function fits(w as Number, yTop as Number, yBottom as Number, margin as Number) as Boolean {
        var a = halfChord(yTop);
        var b = halfChord(yBottom);
        return w <= 2 * ((a < b ? a : b) - margin);
    }
}
