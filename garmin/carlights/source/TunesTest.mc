import Toybox.Lang;
import Toybox.Test;

// The black deck's /playlists page (util/black/playlists, test/test_playlists) as the watch reads it.
// Built only with monkeyc --unit-test; run in the simulator (monkeydo -t).
(:test)
function tunesReadsPages(logger as Logger) as Boolean {
    var t = new Tunes();
    t.readPage("playlists 10 8\n37i9dQZF1DXcBWIGoYBM5M Today's Top Hits\n0vvXsWCC9xrXsKd4FyS8kM Café noite\n");
    Test.assertEqual(t.total, 10);
    Test.assertEqual(t.list.size(), 2);
    Test.assertEqual(t.list[0][0], "37i9dQZF1DXcBWIGoYBM5M");
    Test.assertEqual(t.list[1][1], "Café noite");
    Test.assert(t.more());
    t.readPage("playlists 10 10\n37i9dQZF1DXcBWIGoYBM5N Last\n");
    Test.assertEqual(t.list.size(), 3);
    Test.assert(!t.more());
    return true;
}

(:test)
function tunesRefusesOtherAnswers(logger as Logger) as Boolean {
    var t = new Tunes();
    t.readPage("busy");
    Test.assertEqual(t.total, -1);
    Test.assertEqual(t.status, "Bad answer");
    t.readPage("playlists 0 0\n");
    Test.assertEqual(t.total, 0);
    Test.assertEqual(t.list.size(), 0);
    Test.assert(!t.more());
    return true;
}
