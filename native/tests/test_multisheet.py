"""Independent geometry checks of the native core's sheet assignments."""
from test_engine import run_case
rect=[(0,0),(60,0),(60,20),(0,20)]
run_case('overflow across three sheets',[rect]*3,width=80,height=40,rotations=1,expected=3,max_sheets=3)
run_case('overflow stops at two sheets',[rect]*3,width=80,height=40,rotations=1,expected=2,max_sheets=2)
triangle=[(0,0),(40,0),(20,35)]
run_case('irregular overflow across sheets',[triangle]*4,width=55,height=50,rotations=1,expected=4,max_sheets=4)
