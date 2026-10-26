"""Independent Shapely containment/clearance oracle for captured sheet outlines."""
import math
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'build/test-deps'))
from shapely.geometry import Polygon
from shapely.affinity import rotate, translate

EXE = ROOT / 'native/build/tests/Release/NestNativeChecks.exe'


def check(name, boundary, part, count, sheets=1, expected=None, rotations=4):
    stock = Polygon(boundary)
    left, bottom, right, top = stock.bounds
    stock = translate(stock, -left, -bottom)
    margin, gap = 2, 3
    with tempfile.TemporaryDirectory() as temp:
        job = pathlib.Path(temp) / 'job.nest'
        result = pathlib.Path(temp) / 'result.nest'
        lines = ['FREENEST_SHEET_JOB_1', 'irregular_sheet',
                 f'{right-left} {top-bottom} {gap} {margin} {rotations} 2 {count}',
                 str(len(boundary))]
        lines += [f'{x} {y}' for x, y in boundary]
        for _ in range(count):
            lines += [str(len(part))] + [f'{x} {y}' for x, y in part]
        job.write_text('\n'.join(lines), encoding='utf-8')
        environment = os.environ.copy()
        environment['NEST_MAX_SHEETS'] = str(sheets)
        run = subprocess.run([str(EXE), str(job), str(result)], env=environment,
                             capture_output=True, text=True, timeout=30)
        assert run.returncode == 0, (name, run.stderr)
        tokens = result.read_text().split()
        assert tokens[:2] == ['FREENEST_RESULT_1', 'irregular_sheet']
        total, placed = map(int, tokens[2:4])
        assert total == count
        outlines = []
        for i in range(total):
            index, sheet, x, y, angle = map(float, tokens[4 + 5*i:9 + 5*i])
            if sheet < 0:
                continue
            assert index == i and sheet == int(sheet) and sheet < sheets
            assert abs(angle/(360/rotations)-round(angle/(360/rotations))) < 1e-6
            outline = translate(rotate(Polygon(part), angle, origin=(0, 0)), x, y)
            assert stock.covers(outline), (name, 'outside real sheet')
            assert stock.boundary.distance(outline) >= margin + .1998, (name, 'edge clearance')
            for other_sheet, other in outlines:
                if sheet == other_sheet:
                    assert not outline.intersects(other), (name, 'overlap')
                    assert outline.distance(other) >= gap + .1998, (name, 'part clearance')
            outlines.append((sheet, outline))
        assert len(outlines) == placed
        if expected is not None:
            assert placed == expected, (name, placed, expected)
        assert placed > 0, (name, 'no usable placements')
        print(f'PASS {name}: {placed}/{count}; actual sheet containment, margins and spacing')


l_shape = [(300,-200),(380,-200),(380,-170),(330,-170),(330,-120),(300,-120)]
bar = [(0,0),(60,0),(60,20),(0,20)]
check('offset L-shaped sheet', l_shape, bar, 3, expected=1, rotations=1)
check('L-shaped overflow', l_shape, bar, 3, sheets=3, expected=3, rotations=1)
check('reversed sheet winding', list(reversed(l_shape)), bar, 1, expected=1, rotations=1)
u_shape = [(0,0),(100,0),(100,100),(70,100),(70,30),(30,30),(30,100),(0,100)]
square = [(0,0),(12,0),(12,12),(0,12)]
check('deep concave notch', u_shape, square, 8, expected=8)
ellipse = [(60*math.cos(i*math.tau/96),40*math.sin(i*math.tau/96)) for i in range(96)]
check('curved sheet polygon', ellipse, square, 6, expected=6)
