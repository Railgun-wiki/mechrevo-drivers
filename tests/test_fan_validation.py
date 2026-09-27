"""Compile and exercise the release patch's fan-curve validator without EC access."""

import pathlib
import re
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
PATCH = ROOT / "kernel" / "patches" / "0014-uniwill-validate-ec-writes-and-fan-tables.patch"
SOURCE = PATCH if PATCH.exists() else ROOT / "src" / "uniwill_keyboard.h"


def validator_source():
    source = SOURCE.read_text()
    prefix = r"\+" if SOURCE == PATCH else ""
    match = re.search(
        rf"^{prefix}static int uniwill_validate_fan_curve\(const u8 \*curve\)\n"
        rf"(?:(?:{prefix}.*|{prefix})\n)*?{prefix}}}\n", source, re.MULTILINE
    )
    if not match:
        raise AssertionError("fan validator missing from release source")
    return "\n".join(line[1:] if SOURCE == PATCH else line for line in match.group().splitlines())


class FanValidatorTests(unittest.TestCase):
    def test_invalid_curves_cannot_be_activated(self):
        c = r"""
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
typedef uint8_t u8;
%s
static void make_curve(u8 *v) {
    int i;
    for (i = 0; i < 16; i++) {
        v[3*i] = i ? 5*i + 15 : 0;
        v[3*i+1] = i ? v[3*i]-4 : 0;
        v[3*i+2] = i >= 14 ? 100 : i*6;
    }
    v[46] = v[43]; /* physical slot 15 DownT aliases slot 14 */
}
int main(void) {
    u8 v[48];
    make_curve(v);
    if (uniwill_validate_fan_curve(v)) return 1;
    v[15] = v[12]; /* duplicate temperature */
    if (!uniwill_validate_fan_curve(v)) return 2;
    make_curve(v);
    v[47] = 0; /* no high-temperature cooling */
    v[44] = 0;
    if (!uniwill_validate_fan_curve(v)) return 3;
    make_curve(v);
    v[8] = 101; /* duty overflow */
    if (!uniwill_validate_fan_curve(v)) return 4;
    make_curve(v);
    v[46] = 30; /* impossible round trip for last DownT */
    if (!uniwill_validate_fan_curve(v)) return 5;
    make_curve(v);
    v[44] = 80;
    v[47] = 10; /* last hot point must still cool */
    if (!uniwill_validate_fan_curve(v)) return 6;
    return 0;
}
""" % validator_source()
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "validator.c"
            binary = pathlib.Path(directory) / "validator"
            source.write_text(c)
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
