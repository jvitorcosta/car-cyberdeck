"""Host tests for tools/stt_bench.py (no network). Run: python tools/test_stt_bench.py"""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stt_bench as sb  # noqa: E402


class Scoring(unittest.TestCase):
    def test_accents_case_and_punctuation_dont_matter(self):
        self.assertTrue(sb.actionable("Põe... TOCA Djavan!", ["toca", "djavan"]))
        self.assertTrue(sb.actionable("Pula essa música.", ["pula"]))

    def test_prefix_and_number_words(self):
        self.assertTrue(sb.actionable("liga as luzes", ["liga", "luz"]))
        self.assertTrue(sb.actionable("Roxo, cinquenta por cento.", ["roxo", "50"]))
        self.assertTrue(sb.actionable("roxo 50%", ["roxo", "50"]))

    def test_missing_keyword_fails(self):
        self.assertFalse(sb.actionable("toca já vá", ["toca", "djavan"]))
        self.assertFalse(sb.actionable("", ["pula"]))

    def test_a_keyword_must_start_a_word(self):
        self.assertFalse(sb.actionable("apaga a luz", ["paga"]))

    def test_keywords_cover_the_ten_phrases(self):
        self.assertEqual(len(sb.KEYWORDS), 10)
        self.assertEqual(sb.KEYWORDS[4], ["toca", "djavan"])

    def test_pass_bar_is_nine_of_ten_per_condition(self):
        rows = [{"condition": "A", "phrase": p, "transcript": "x", "ms": 100 + p} for p in range(10)]
        for r in rows[:9]:
            r["transcript"] = " ".join(sb.KEYWORDS[r["phrase"]])
        s = sb.score(rows)
        self.assertEqual((s["A"]["ok"], s["A"]["n"], s["A"]["pass"]), (9, 10, True))
        self.assertEqual(s["A"]["median_ms"], 104.5)
        rows[8]["transcript"] = "x"
        self.assertFalse(sb.score(rows)["A"]["pass"])

    def test_a_condition_with_missing_clips_does_not_pass(self):
        rows = [{"condition": "B", "phrase": p, "transcript": " ".join(sb.KEYWORDS[p]), "ms": 1}
                for p in range(9)]
        s = sb.score(rows)
        self.assertEqual((s["B"]["ok"], s["B"]["n"], s["B"]["pass"]), (9, 9, False))


class Report(unittest.TestCase):
    def test_providers_without_a_key_are_skipped(self):
        names = [p.name for p in sb.providers({"OPENAI_API_KEY": "x"})]
        self.assertEqual(names, ["openai"])

    def test_run_writes_a_report_from_a_pulled_folder(self):
        class Echo:
            name = "echo"

            def transcribe(self, wav):
                return wav.decode()

        with tempfile.TemporaryDirectory() as d:
            rows = []
            for p in range(10):
                name = "A%02d" % (p + 1)
                with open(os.path.join(d, name + ".wav"), "wb") as f:
                    f.write(" ".join(sb.KEYWORDS[p]).encode())
                rows.append({"slot": p, "name": name, "condition": "A", "phrase": p, "file": name + ".wav"})
            with open(os.path.join(d, "manifest.json"), "w") as f:
                json.dump(rows, f)
            text = sb.run(d, [Echo()])
            self.assertIn("echo", text)
            self.assertIn("| A | 10/10 | yes |", text)
            with open(os.path.join(d, "report.md"), encoding="utf-8") as f:
                self.assertEqual(f.read(), text)


if __name__ == "__main__":
    unittest.main()
