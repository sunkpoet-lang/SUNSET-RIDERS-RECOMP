import hashlib
from pathlib import Path
import tempfile
import unittest
import warnings
import zipfile
import zlib
from rom_loader import SetError, assemble_lanes, read_verified


class LoaderTests(unittest.TestCase):
    def test_even_odd_golden(self):
        self.assertEqual(assemble_lanes([(b'ACE',0,3),(b'BDF',1,3)],6,1,2),b'ABCDEF')

    def test_word_lanes_preserve_word_order(self):
        self.assertEqual(assemble_lanes([(b'ABEF',0,4),(b'CDGH',2,4)],8,2,4),b'ABCDEFGH')

    def test_second_program_bank_boundary(self):
        self.assertEqual(assemble_lanes([(b'AC',0,2),(b'BD',1,2),(b'E',4,1),(b'F',5,1)],6,1,2),b'ABCDEF')

    def test_holes_rejected(self):
        with self.assertRaisesRegex(SetError,'uninitialised'):
            assemble_lanes([(b'AC',0,2)],4,1,2)

    def test_overlapping_lanes_rejected(self):
        with self.assertRaisesRegex(SetError,'overlapping'):
            assemble_lanes([(b'AC',0,2),(b'BD',0,2)],4,1,2)

    def test_out_of_bounds_rejected(self):
        with self.assertRaisesRegex(SetError,'exceeds'):
            assemble_lanes([(b'ABC',0,3)],4,1,2)

    def test_partial_word_rejected(self):
        with self.assertRaisesRegex(SetError,'size'):
            assemble_lanes([(b'ABC',0,3)],4,2,4)

    def archive_case(self, entries, expected=b'AB', sha=None):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'fixture.zip'
            with warnings.catch_warnings():
                warnings.simplefilter('ignore',UserWarning)
                with zipfile.ZipFile(path,'w') as zipped:
                    for name,data in entries:zipped.writestr(name,data)
            before=path.read_bytes()
            try:
                return read_verified(path,[{'name':'test.rom','size':len(expected),
                    'crc32':f'{zlib.crc32(expected)&0xffffffff:08x}',
                    'sha1':sha or hashlib.sha1(expected).hexdigest()}])
            finally:self.assertEqual(path.read_bytes(),before)

    def test_nested_member_and_unrelated_clone(self):
        self.assertEqual(self.archive_case([('eac/test.rom',b'AB'),('clone.bin',b'XXX')]),{'test.rom':b'AB'})

    def test_missing_rejected(self):
        with self.assertRaisesRegex(SetError,'exactly one'):
            self.archive_case([('other.rom',b'AB')])

    def test_ambiguous_basename_rejected(self):
        with self.assertRaisesRegex(SetError,'found 2'):
            self.archive_case([('test.rom',b'AB'),('clone/test.rom',b'AB')])

    def test_wrong_size_rejected(self):
        with self.assertRaisesRegex(SetError,'wrong size'):
            self.archive_case([('test.rom',b'ABC')])

    def test_corrupt_content_rejected(self):
        with self.assertRaisesRegex(SetError,'mismatch'):
            self.archive_case([('test.rom',b'AC')])

    def test_sha1_is_checked_independently(self):
        with self.assertRaisesRegex(SetError,'mismatch'):
            self.archive_case([('test.rom',b'AB')],sha='0'*40)


if __name__=='__main__':unittest.main(verbosity=2)
