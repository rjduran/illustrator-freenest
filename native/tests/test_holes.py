"""Check the native compound-path adapter with an independent filled-polygon oracle."""
import os
import math
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'build/test-deps'))
from shapely.geometry import Polygon, box
from shapely.affinity import rotate, translate


def check(name, parts, width, height, expected, sheets=1, rotations=4):
    with tempfile.TemporaryDirectory() as temp:
        job = pathlib.Path(temp) / 'holes.nest'
        result = pathlib.Path(temp) / 'result.nest'
        lines = ['FREENEST_HOLES_JOB_1', 'holes', f'{width} {height} 3 2 {rotations} 3 {len(parts)}']
        for contours in parts:
            lines.append(str(len(contours)))
            for ring in contours:
                lines += [str(len(ring))] + [f'{x} {y}' for x, y in ring]
        job.write_text('\n'.join(lines), encoding='utf-8')
        env = os.environ.copy()
        env['NEST_MAX_SHEETS'] = str(sheets)
        run = subprocess.run([str(ROOT / 'native/build/tests/Release/NestNativeChecks.exe'), str(job), str(result)],
                             env=env, capture_output=True, text=True, timeout=30)
        assert run.returncode == 0, (name, run.stderr)
        tokens = result.read_text().split()
        count, placed = map(int, tokens[2:4])
        assert count == len(parts) and placed == expected, (name, placed, expected)
        shapes = []
        for i in range(count):
            index, sheet, x, y, angle = map(float, tokens[4+5*i:9+5*i])
            if sheet < 0:
                continue
            assert index == i and sheet == int(sheet) and 0 <= sheet < sheets
            assert abs(angle/(360/rotations)-round(angle/(360/rotations))) < 1e-6
            # Determine the outer contour independently of the native input order.
            rings = sorted(parts[i], key=lambda ring: Polygon(ring).area, reverse=True)
            material = Polygon(rings[0], rings[1:])
            left, bottom, _, _ = material.bounds
            material = translate(material, -left, -bottom)
            material = translate(rotate(material, angle, origin=(0, 0)), x, y)
            assert box(1.9999, 1.9999, width-1.9999, height-1.9999).covers(material), name
            for other_sheet, other in shapes:
                if sheet == other_sheet:
                    assert material.intersection(other).area < 1e-6, (name, 'filled regions overlap')
                    assert material.distance(other) >= 3.1998, (name, 'clearance around holes')
            shapes.append((sheet, material))
        assert len(shapes) == expected
        print(f'PASS {name}: {placed}/{count}; filled material, hole clearance and rotation')


outer = [(200,-100),(300,-100),(300,0),(200,0)]
hole = [(220,-80),(280,-80),(280,-20),(220,-20)]
insert = [[(0,0),(30,0),(30,30),(0,30)]]
check('hole first and shifted source', [[hole, outer], insert], 110, 110, 2, rotations=1)
check('reversed compound winding', [[list(reversed(outer)), list(reversed(hole))], insert], 110, 110, 2)
check('overflow of compound paths', [[outer, hole]]*3, 110, 110, 3, sheets=3)
wide = [(0,0),(160,0),(160,100),(0,100)]
h1 = [(15,20),(65,20),(65,80),(15,80)]
h2 = [(95,20),(145,20),(145,80),(95,80)]
check('two holes receive parts', [[h1, wide, h2], insert, insert], 170, 110, 3)
rotation_outer = [(0,0),(100,0),(100,60),(0,60)]
rotation_hole = [(20,15),(80,15),(80,45),(20,45)]
check('compound path requires quarter-turn', [[rotation_outer, rotation_hole]], 70, 110, 1)

curved_hole = [(250+30*math.cos(i*math.tau/96),-50+30*math.sin(i*math.tau/96)) for i in range(96)]
check('curved hole receives a part', [[outer, curved_hole], insert], 110, 110, 2)

dense_outer = [(0,0),(30,0),(30,20),(0,20)]
dense_hole = [(12,7),(18,7),(18,13),(12,13)]
check('200 small-cutout parts retain rectangle efficiency', [[dense_outer, dense_hole]]*200, 500, 350, 200)
large_cutout = [(3,3),(27,3),(27,17),(3,17)]
small_insert = [[(0,0),(4,0),(4,4),(0,4)]]
mixed = [[dense_outer, large_cutout]] + [[dense_outer, dense_hole]]*199 + [small_insert]
check('hole search retains a better solid-envelope result', mixed, 500, 350, 201)
