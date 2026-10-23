import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// Today's Pokémon (tools/gen_pokeface.py builds the sheets into resources/gen/): one 128x64 sheet
// of two 64x64 frames, drawn at 2x with point sampling so the pixels stay sharp. Only the current
// Pokémon's sheet is loaded.
class Sprite {
    const SIZE = 64;
    const MONS = [Rez.Drawables.Mon0, Rez.Drawables.Mon1, Rez.Drawables.Mon2, Rez.Drawables.Mon3,
                  Rez.Drawables.Mon4, Rez.Drawables.Mon5, Rez.Drawables.Mon6, Rez.Drawables.Mon7,
                  Rez.Drawables.Mon8, Rez.Drawables.Mon9, Rez.Drawables.Mon10, Rez.Drawables.Mon11,
                  Rez.Drawables.Mon12, Rez.Drawables.Mon13, Rez.Drawables.Mon14, Rez.Drawables.Mon15,
                  Rez.Drawables.Mon16, Rez.Drawables.Mon17, Rez.Drawables.Mon18, Rez.Drawables.Mon19,
                  Rez.Drawables.Mon20];
    // The weekday rewards (every third species) in their shiny colours
    const SHINIES = [Rez.Drawables.Shiny2, Rez.Drawables.Shiny5, Rez.Drawables.Shiny8, Rez.Drawables.Shiny11,
                     Rez.Drawables.Shiny14, Rez.Drawables.Shiny17, Rez.Drawables.Shiny20];

    private var _species as Number = -1;
    private var _shiny as Boolean = false;
    private var _sheet as WatchUi.BitmapResource or Null;
    private var _scale as Graphics.AffineTransform;

    function initialize() {
        _scale = new Graphics.AffineTransform();
        _scale.scale(2.0, 2.0);
    }

    // shiny only applies to a reward (species % 3 == 2), which has a shiny sheet.
    function show(species as Number, shiny as Boolean) as Void {
        shiny = shiny && species % 3 == 2;
        if (species == _species && shiny == _shiny) {
            return;
        }
        _sheet = null;   // let the old one go before loading the new
        _sheet = WatchUi.loadResource(shiny ? SHINIES[species / 3] : MONS[species]) as WatchUi.BitmapResource;
        _species = species;
        _shiny = shiny;
    }

    // Frame `frame` (of 2) with its top-left at (x, y), 128x128 on screen.
    function draw(dc as Dc, x as Number, y as Number, frame as Number) as Void {
        if (_sheet == null) {
            return;
        }
        // The whole sheet is drawn at 2x, shifted so frame `frame` sits at (x, y), and clipped to that
        // 128x128 square: no source-rectangle options, whose offset handling differs between devices.
        dc.setClip(x, y, SIZE * 2, SIZE * 2);
        dc.drawBitmap2(x - (frame % 2) * SIZE * 2, y, _sheet, {
            :transform => _scale, :filterMode => Graphics.FILTER_MODE_POINT
        });
        dc.clearClip();
    }
}
