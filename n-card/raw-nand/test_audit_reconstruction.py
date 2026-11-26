import unittest

from audit_reconstruction import make_claims, score


class Reconstruction(unittest.TestCase):
    def test_held_out_scoring_including_unclaimed(self):
        result = "\n".join(score([("fixture", 0, b"abc")], b"axc\0\xff", 8))
        self.assertIn("2/3 exact", result)
        self.assertIn("first difference at 0x1", result)
        self.assertIn("miss 1 non-FF bytes", result)
        self.assertIn("0x3 bytes not captured", result)

    def test_split_spare_does_not_claim_page_zero(self):
        stage = bytearray(0x4800)
        stage[0x0c:0x10] = b"DSGB"
        stage[0x1000:0x1048] = bytes(range(72))
        claims = make_claims(128, stage, None, None, 0x20000, None)
        self.assertEqual(claims["spare"], [
            ("split table page 2", 128, bytes(range(8, 72))),
            ("split table page 3", 192, bytes(range(64))),
        ])
        self.assertTrue(all(offset for _, offset, _ in claims["main"]))

    def test_inputs_and_overlap(self):
        with self.assertRaises(ValueError):
            make_claims(128, b"bad", None, None, 0x20000, None)
        with self.assertRaises(ValueError):
            score([("first", 0, b"123"), ("second", 2, b"34")], b"1234", 4)


if __name__ == "__main__":
    unittest.main()
