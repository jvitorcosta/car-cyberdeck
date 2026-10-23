import Toybox.Graphics;
import Toybox.Lang;
import Toybox.WatchUi;

// The partner's frames (tools/gen_pokeface.py builds them into resources/sprites/). Only the
// current Pokémon's frames are loaded: the watch face has 128 KB in all.
class Sprite {
    private var _species as Number = -1;
    private var _sleeping as Boolean = false;
    private var _bitmaps as Array<WatchUi.BitmapResource> = [];
    // Tails[species][frame]: the tail flame's [dx, dy] from the frame's centre (found by the tool)
    private var _tails as Array<Array<Array<Number>>>;

    function initialize() {
        _tails = WatchUi.loadResource(Rez.JsonData.Tails) as Array<Array<Array<Number>>>;
    }

    // Loads this Pokémon's frames (or its one sleep frame) when it changes.
    function show(species as Number, sleeping as Boolean) as Void {
        if (species == _species && sleeping == _sleeping) {
            return;
        }
        _bitmaps = [];   // let the old ones go before loading the new
        var ids = sleeping ? [sleepId(species)] : frameIds(species);
        var out = [] as Array<WatchUi.BitmapResource>;
        for (var i = 0; i < ids.size(); i++) {
            out.add(WatchUi.loadResource(ids[i]) as WatchUi.BitmapResource);
        }
        _bitmaps = out;
        _species = species;
        _sleeping = sleeping;
    }

    // Frame `frame` centred on (cx, cy), then the tail flame (radius from Partner.flameRadius).
    function draw(dc as Dc, cx as Number, cy as Number, frame as Number, flame as Number) as Void {
        if (_bitmaps.size() == 0) {
            return;
        }
        var i = frame % _bitmaps.size();
        var b = _bitmaps[i];
        dc.drawBitmap(cx - b.getWidth() / 2, cy - b.getHeight() / 2, b);
        var t = _tails[_species][i];   // the sleep frame is frame 0
        var blue = _species == Partner.MEGA_X;
        var outer = _sleeping ? (blue ? 0x0000AA : 0xAA5500) : (blue ? 0x0055FF : 0xFF5500);
        var inner = _sleeping ? (blue ? 0x0055AA : 0xAAAA00) : (blue ? 0x55FFFF : 0xFFFF00);
        // A teardrop: a round base with a tip rising from it, the hot core inside.
        var x = cx + t[0];
        var y = cy + t[1];
        var r = flame * 2 / 3 > 1 ? flame * 2 / 3 : 1;
        dc.setColor(outer, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x, y, r);
        dc.fillPolygon([[x - r, y], [x + r, y], [x, y - r * 5 / 2]]);
        dc.setColor(inner, Graphics.COLOR_TRANSPARENT);
        dc.fillCircle(x, y + r / 3, r / 2);
    }

    // Top-right corner of the current frame, for the "z Z".
    function corner() as [Number, Number] {
        if (_bitmaps.size() == 0) {
            return [0, 0];
        }
        var half = _bitmaps[0].getWidth() / 2;
        return [half < 14 ? half : 14, -_bitmaps[0].getHeight() / 2];   // clear of the Body Battery text
    }

    private function frameIds(species as Number) as Array<ResourceId> {
        switch (species) {
            case Partner.CHARMANDER:
                return [Rez.Drawables.Charmander0, Rez.Drawables.Charmander1, Rez.Drawables.Charmander2,
                        Rez.Drawables.Charmander3, Rez.Drawables.Charmander4, Rez.Drawables.Charmander5];
            case Partner.CHARMELEON:
                return [Rez.Drawables.Charmeleon0, Rez.Drawables.Charmeleon1, Rez.Drawables.Charmeleon2,
                        Rez.Drawables.Charmeleon3, Rez.Drawables.Charmeleon4, Rez.Drawables.Charmeleon5];
            case Partner.MEGA_X:
                return [Rez.Drawables.MegaX0, Rez.Drawables.MegaX1, Rez.Drawables.MegaX2,
                        Rez.Drawables.MegaX3, Rez.Drawables.MegaX4, Rez.Drawables.MegaX5];
            case Partner.MEGA_Y:
                // Mega Y's source is a still sprite: one frame is enough
                return [Rez.Drawables.MegaY0];
        }
        return [Rez.Drawables.Charizard0, Rez.Drawables.Charizard1, Rez.Drawables.Charizard2,
                Rez.Drawables.Charizard3, Rez.Drawables.Charizard4, Rez.Drawables.Charizard5];
    }

    private function sleepId(species as Number) as ResourceId {
        if (species == Partner.MEGA_X) {
            return Rez.Drawables.MegaXSleep;
        }
        return species == Partner.MEGA_Y ? Rez.Drawables.MegaYSleep : Rez.Drawables.CharizardSleep;
    }
}
