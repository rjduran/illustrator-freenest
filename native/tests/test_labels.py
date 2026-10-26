"""Check actual VDA engraving paths against a concave part and its rotated placement."""
import pathlib,sys,subprocess,json,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'build/test-deps'))
from shapely.geometry import Polygon,LineString,box
from shapely.affinity import rotate,translate
with tempfile.TemporaryDirectory() as temp:
    result=pathlib.Path(temp)/'labels.json'
    subprocess.run([str(root/'native/build/tests/Release/NestNativeChecks.exe'),'--labels',str(result)],check=True)
    cases=json.loads(result.read_text())
    for case in cases:
        part=Polygon(case['outline'],case.get('holes',[]))
        for stroke in case['strokes']:
            geometry=LineString(stroke).buffer(case['strokeWeight']/2)
            assert part.covers(geometry),'Engraving escaped the concave part'
            placed_part=translate(rotate(part,90,origin=(0,0)),1000,2000)
            placed_stroke=translate(rotate(geometry,90,origin=(0,0)),1000,2000)
            assert placed_part.covers(placed_stroke),'Engraving escaped after part rotation'
print('PASS VDA regular/bold clearance at 0.1 and 2 pt stroke weights, including rotated label containment')

with tempfile.TemporaryDirectory() as temp:
    result=pathlib.Path(temp)/'sheet-labels.json'
    subprocess.run([str(root/'native/build/tests/Release/NestNativeChecks.exe'),'--sheet-labels',str(result)],check=True)
    for case in json.loads(result.read_text()):
        sheet=Polygon(case['sheet'])
        reserved=Polygon(case['region'])
        assert box(*sheet.bounds).disjoint(reserved),'Sheet label box must be outside the sheet bounds'
        parts=[Polygon(outline) for outline in case['parts']]
        for stroke in case['label']:
            ink=LineString(stroke).buffer(case['strokeWeight']/2)
            assert reserved.covers(ink) and box(*sheet.bounds).disjoint(ink),'Sheet label must remain outside the sheet bounds'
            for part in parts:
                assert not ink.intersects(part),'Sheet label overlaps a nested part'
        for part in parts:
            assert sheet.covers(part) and not reserved.intersects(part),'Part overlaps reserved sheet label'
print('PASS outside-sheet labels on rectangular, concave and curved sheets, including zero margin and 2 pt strokes')
