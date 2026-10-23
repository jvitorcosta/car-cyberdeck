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

    private var _species as Number = -1;
    private var _sheet as WatchUi.BitmapResource or Null;
    private var _scale as Graphics.AffineTransform;

    function initialize() {
        _scale = new Graphics.AffineTransform();
        _scale.scale(2.0, 2.0);
    }

    function show(species as Number) as Void {
        if (species == _species) {
            return;
        }
        _sheet = null;   // let the old one go before loading the new
        _sheet = WatchUi.loadResource(MONS[species]) as WatchUi.BitmapResource;
        _species = species;
    }

    // Frame `frame` (of 2) with its top-left at (x, y), 128x128 on screen.
    function draw(dc as Dc, x as Number, y as Number, frame as Number) as Void {
        if (_sheet == null) {
            return;
        }
        dc.drawBitmap2(x, y, _sheet, {
            :bitmapX => (frame % 2) * SIZE, :bitmapY => 0, :bitmapWidth => SIZE, :bitmapHeight => SIZE,
            :transform => _scale, :filterMode => Graphics.FILTER_MODE_POINT
        });
    }
}
