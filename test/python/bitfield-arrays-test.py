#!/usr/bin/env python3

import os
import struct
import sys
import unittest
from io import BytesIO

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                               '../../build/tests/test/types'))
from bitfield_arrays_t import bitfield_arrays_t
from bitfield_array_records_t import bitfield_array_records_t


def make_message(count):
    msg = bitfield_arrays_t()
    msg.count = count
    msg.aligned = [i % 8 for i in range(count)]
    msg.separator = 0x67
    msg.aligned_signed = [-16 + i % 32 for i in range(count)]
    msg.separator2 = 0x45
    msg.prefix = 5
    msg.unsigned_values = [(i + 7) % 16 for i in range(count)]
    msg.signed_values = [-4 + i % 8 for i in range(count)]
    msg.matrix = [[-4 + (i + row) % 8 for i in range(count)] for row in range(2)]
    msg.suffix = 19
    msg.neighbors = [2, 1]
    msg.tail = [i % 2 for i in range(count)]
    msg.marker = 0x1234
    msg.end_prefix = 6
    msg.end_values = [i % 4 for i in range(count)]
    return msg


def wire_body(msg):
    # Independent MSB-first wire oracle, including padding at each bitfield boundary.
    def pack_bits(fields):
        bits = ''.join(format(value & ((1 << width) - 1), '0%db' % width)
                       for width, values in fields for value in values)
        bits += '0' * (-len(bits) % 8)
        return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))

    body = struct.pack('b', msg.count)
    body += pack_bits([(3, msg.aligned)]) + struct.pack('b', msg.separator)
    body += pack_bits([(5, msg.aligned_signed)]) + struct.pack('b', msg.separator2)
    body += pack_bits([(3, [msg.prefix]), (4, msg.unsigned_values),
                      (3, msg.signed_values), (3, msg.matrix[0]), (3, msg.matrix[1]),
                      (5, [msg.suffix]), (2, msg.neighbors), (1, msg.tail)])
    body += struct.pack('>h', msg.marker)
    return body + pack_bits([(3, [msg.end_prefix]), (2, msg.end_values)])


class BitfieldArrayTests(unittest.TestCase):
    def assert_message(self, actual, expected):
        for field in expected.__slots__:
            self.assertEqual(getattr(actual, field), getattr(expected, field), field)

    def test_encode(self):
        for count in (0, 1, 2, 3, 8):
            with self.subTest(count=count):
                msg = make_message(count)
                self.assertEqual(msg.encode(), msg._get_packed_fingerprint() + wire_body(msg))

    def test_decode(self):
        for count in (0, 1, 2, 3, 8):
            with self.subTest(count=count):
                msg = make_message(count)
                wire = msg._get_packed_fingerprint() + wire_body(msg)
                stream = BytesIO(wire + b'next record')
                self.assert_message(msg.decode(stream), msg)
                self.assertEqual(stream.read(), b'next record')

    def test_nested_records_round_trip(self):
        for counts in ((0, 0), (0, 3), (3, 0)):
            with self.subTest(counts=counts):
                msg = bitfield_array_records_t()
                msg.records = [make_message(count) for count in counts]
                msg.marker = 0x5678
                wire = (msg._get_packed_fingerprint() +
                        b''.join(wire_body(record) for record in msg.records) + b'\x56\x78')
                self.assertEqual(msg.encode(), wire)
                decoded = msg.decode(wire)
                self.assertEqual(decoded.marker, msg.marker)
                for actual, expected in zip(decoded.records, msg.records):
                    self.assert_message(actual, expected)


if __name__ == '__main__':
    unittest.main()
