// Honda City: a Forerunner 255 remote for the old deck's LED panel. It talks to the panel's GET /say
// (the Siri shortcut's endpoint) and GET /state through Garmin Connect on the iPhone, which is on
// the same hotspot as the deck, with &zone=inside|outside unless the zone is Both. Plain http to
// the hotspot works through Garmin Connect on iOS (tested 2026-10-22).
//   ZonesView / ZonesDelegate: pages BOTH, INSIDE, OUTSIDE, PRESETS (UP/DOWN); START opens that
//                              zone's list (ZoneMenuDelegate): colours with a dot, Brightness, Off
//                              (and Music mode on BOTH);
//                              on PRESETS, the deck's named presets as bars, START applies one
//                              (PresetMenuDelegate)
//   PresetBar: a preset as a bar, inside and outside filled to their levels; the list's rows
//   LevelView / LevelDelegate: a zone's brightness, UP/DOWN in 10 % steps
//   Deck: both lights' state, the presets, the controllers' links, music mode + the request queue
//   HondaGlance: the card in the watch's glance loop (last state the app saw)
import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

(:glance)
class CarLightsApp extends Application.AppBase {
    function initialize() {
        AppBase.initialize();
    }

    // The card in the watch's glance loop (see HondaGlance).
    (:glance)
    function getGlanceView() as [WatchUi.GlanceView] or [WatchUi.GlanceView, WatchUi.GlanceViewDelegate] or Null {
        return [new HondaGlance()];
    }

    function getInitialView() as [WatchUi.Views] or [WatchUi.Views, WatchUi.InputDelegates] {
        var deck = new Deck();
        deck.sync();  // show the lights as they are, not a default
        var view = new ZonesView(deck);
        return [view, new ZonesDelegate(deck, view)];
    }
}
