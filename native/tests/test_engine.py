"""Independent geometry checks; requires shapely in build/test-deps or installed."""
import pathlib, sys, subprocess, tempfile, math, os
ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE = ROOT/'native/build/tests/Release/NestNativeChecks.exe'
sys.path.insert(0, str(ROOT / 'build/test-deps'))
from shapely.geometry import Polygon, box
from shapely.affinity import rotate, translate

def run_case(name, parts, width=100, height=100, gap=3, margin=5, rotations=4, expected=None,max_sheets=1,packing_style=None,center=False):
    with tempfile.TemporaryDirectory() as temp:
        job=pathlib.Path(temp)/'job.nest'; result=pathlib.Path(temp)/'result.nest'
        lines=['FREENEST_JOB_1','test_123',f'{width} {height} {gap} {margin} {rotations} 1 {len(parts)}']
        for polygon in parts:
            lines.append(str(len(polygon)))
            lines.extend(f'{x} {y}' for x,y in polygon)
        job.write_text('\n'.join(lines))
        environment=os.environ.copy()
        if packing_style is not None: environment['FREENEST_PACKING_STYLE']=str(packing_style)
        if center: environment['FREENEST_CENTER']='1'
        if max_sheets>1: environment['NEST_MAX_SHEETS']=str(max_sheets)
        process=subprocess.run([str(EXE),str(job),str(result)],capture_output=True,text=True,timeout=20,env=environment)
        assert process.returncode==0,process.stderr
        tokens=result.read_text().split()
        assert tokens[:2]==['FREENEST_RESULT_1','test_123']
        n,placed=map(int,tokens[2:4]); assert n==len(parts)
        shapes=[]; seen=set()
        for i in range(n):
            part,sheet,tx,ty,angle=map(float,tokens[4+5*i:9+5*i])
            if sheet==-1: continue
            assert sheet==int(sheet) and 0<=sheet<max_sheets and int(part) not in seen
            seen.add(int(part))
            assert abs(angle/(360/rotations)-round(angle/(360/rotations)))<1e-6
            shape=translate(rotate(Polygon(parts[int(part)]),angle,origin=(0,0)),tx,ty)
            assert box(margin-1e-4,margin-1e-4,width-margin+1e-4,height-margin+1e-4).covers(shape), (name,shape.bounds)
            for other_sheet,other in shapes:
                if sheet!=other_sheet: continue
                assert shape.intersection(other).area<1e-6, name+' overlap'
                assert shape.distance(other)>=gap-1e-4, (name,shape.distance(other),gap)
            shapes.append((sheet,shape))
        assert len(shapes)==placed
        if expected is not None: assert placed==expected,(name,placed,expected)
        print(f'PASS {name}: {placed}/{n} placed; containment, spacing, rotations verified')

for style in range(3):
    run_case(f'packing style {style}, centered concave parts',
             [[(0,0),(30,0),(30,10),(10,10),(10,30),(0,30)]]*4,
             width=150,height=150,expected=4,packing_style=style,center=True)
run_case('centered overflow', [[(0,0),(60,0),(60,20),(0,20)]]*3,
         width=80,height=40,rotations=1,max_sheets=3,expected=3,center=True)
rect=[(0,0),(30,0),(30,20),(0,20)]
run_case('rectangles',[rect]*6,expected=6)
run_case('concave', [[(0,0),(35,0),(35,12),(12,12),(12,35),(0,35)]]*4,expected=4)
run_case('rotation required',[[(0,0),(60,0),(60,20),(0,20)]],width=40,height=80,expected=1)
run_case('fixed orientation cannot fit',[[(0,0),(60,0),(60,20),(0,20)]],width=40,height=80,rotations=1,expected=0)
run_case('oversized',[[(0,0),(200,0),(200,200),(0,200)]],expected=0)
run_case('partial placement',[rect]*20,width=70,height=50,expected=2)
with tempfile.TemporaryDirectory() as temp:
    job=pathlib.Path(temp)/'bad.nest'; result=pathlib.Path(temp)/'result.nest'
    for bad in ['FREENEST_JOB_1\nbad\n100 100 3 5 4 1 1\n4\n0 0\n30 0',
                'FREENEST_JOB_1\nbad\n100 100 3 5 4 1 1\n4\n0 0\n40 35\n0 30\n30 0']:
        job.write_text(bad); result.write_text('stale success')
        p=subprocess.run([str(EXE),str(job),str(result)],capture_output=True,text=True)
        assert p.returncode!=0 and not result.exists(),p.stderr
    print('PASS malformed and intersecting outlines rejected; stale result removed')
