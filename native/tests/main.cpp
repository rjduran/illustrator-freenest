#include "NestCore.h"
#include "NestSelection.h"
#include "NestOutput.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cmath>
#include <thread>
#include <chrono>
#include <exception>
#include <set>
#include <cstdlib>
void Require(bool v,const char* reason) {if(!v) throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv) {
    try {
        auto library=std::filesystem::path(argv[0]).parent_path()/L"nfp_nest.dll";
        nestai::Engine engine(library.c_str());
        if(argc==3&&std::wstring(argv[1])==L"--sheet-labels") {
            auto polygon=[](std::initializer_list<nestai::Point> points) {
                std::vector<nestai::Segment> segments;for(auto p:points) segments.push_back({p,p,p});return nestai::Prepare(segments);
            };
            std::ofstream out{std::filesystem::path(argv[2])};out<<std::setprecision(17)<<'[';
            auto points=[&](const std::vector<nestai::Point>& ring) {
                out<<'[';for(size_t i=0;i<ring.size();++i) {if(i) out<<',';out<<'['<<ring[i].x<<','<<ring[i].y<<']';}out<<']';
            };
            auto part=polygon({{0,0},{12,0},{12,12},{0,12}});
            for(int test=0;test<6;++test) {
                nestai::Settings settings;settings.labelSheetBoundary=true;settings.width=240;settings.height=140;settings.seconds=2;
                settings.margin=test%2?0:9;settings.labelFont=test%2;settings.labelStrokeWidth=test>=4?2:0.1;
                auto sheet=polygon({{0,0},{240,0},{240,140},{0,140}});
                if(test==2||test==3) sheet=polygon({{0,0},{120,0},{120,40},{40,40},{40,100},{0,100}});
                if(test>=4) sheet=nestai::Prepare({{{0,50},{0,22.385},{0,77.615}},{{80,100},{35.817,100},{124.183,100}},
                    {{160,50},{160,77.615},{160,22.385}},{{80,0},{124.183,0},{35.817,0}}});
                if(test>=2) {settings.useCustomSheet=true;settings.customSheet=sheet;auto size=nestai::SheetSize(sheet);settings.width=size.x;settings.height=size.y;}
                engine.ReserveSheetLabel(settings);auto caption=engine.SheetLabel("FreeNest Sheet 1",settings);
                auto placements=engine.Solve(std::vector<nestai::Part>(4,part),settings);
                Require(placements.size()==4,"Sheet label consumed more area than expected");
                if(test) out<<',';out<<"{\"sheet\":";points(sheet.outline);out<<",\"region\":";points(settings.sheetLabelRegion);
                out<<",\"strokeWeight\":"<<settings.labelStrokeWidth<<",\"label\":[";
                for(size_t i=0;i<caption.strokes.size();++i) {if(i) out<<',';points(caption.strokes[i]);}
                out<<"],\"parts\":[";
                for(size_t i=0;i<placements.size();++i) {
                    if(i) out<<',';std::vector<nestai::Point> ring;auto placement=placements[i];
                    for(auto p:part.outline) ring.push_back(nestai::Place(p,{0,0},{placement.x,placement.y},placement.angle));points(ring);
                }
                out<<"]}";
            }
            out<<']';return 0;
        }
        if(argc==3&&std::wstring(argv[1])==L"--labels") {
            std::vector<nestai::Segment> shape;
            for(auto p:std::vector<nestai::Point>{{0,0},{100,0},{100,25},{25,25},{25,100},{0,100}}) shape.push_back({p,p,p});
            auto part=nestai::Prepare(shape);std::ofstream out{std::filesystem::path(argv[2])};out<<std::setprecision(17)<<'[';
            for(int test=0;test<8;++test) {
                int font=test%2;double weight=test%4<2?0.1:2;
                if(test==4) {
                    std::vector<nestai::Segment> outer,hole;
                    for(auto p:std::vector<nestai::Point>{{0,0},{100,0},{100,100},{0,100}}) outer.push_back({p,p,p});
                    for(auto p:std::vector<nestai::Point>{{20,20},{80,20},{80,80},{20,80}}) hole.push_back({p,p,p});
                    part=nestai::PrepareCompound({hole,outer},true);
                }
                auto label=engine.LabelInside(part,"Panel-01",12,font,weight);Require(!label.strokes.empty(),"No label fitted inside the L shape");
                if(test) out<<',';out<<"{\"strokeWeight\":"<<weight<<",\"outline\":[";
                for(size_t i=0;i<part.outline.size();++i) {if(i) out<<',';out<<'['<<part.outline[i].x<<','<<part.outline[i].y<<']';}
                out<<"],\"holes\":[";
                for(size_t hole=0;hole<part.holes.size();++hole) {
                    if(hole) out<<',';out<<'[';
                    for(size_t i=0;i<part.holes[hole].size();++i) {if(i) out<<',';auto p=part.holes[hole][i];out<<'['<<p.x<<','<<p.y<<']';}
                    out<<']';
                }
                out<<"],\"strokes\":[";
                for(size_t s=0;s<label.strokes.size();++s) {if(s) out<<',';out<<'[';
                    for(size_t i=0;i<label.strokes[s].size();++i) {if(i) out<<',';auto p=label.strokes[s][i];out<<'['<<p.x<<','<<p.y<<']';}out<<']';}
                out<<"]}";
            }
            out<<']';return 0;
        }
        if(argc==3) {
            if(std::filesystem::exists(std::filesystem::path(argv[2]))) std::filesystem::remove(std::filesystem::path(argv[2]));
            std::ifstream in{std::filesystem::path(argv[1])};
            std::string header,token;nestai::Settings s;int count;
            in>>header>>token>>s.width>>s.height>>s.gap>>s.margin>>s.rotations>>s.seconds>>count;
            if(auto maxSheets=std::getenv("NEST_MAX_SHEETS")) {s.multipleSheets=true;s.maxSheets=std::stoi(maxSheets);}
            Require((header=="FREENEST_JOB_1"||header=="FREENEST_SHEET_JOB_1"||header=="FREENEST_HOLES_JOB_1")&&count>0&&count<=500,"Invalid job");
            if(header=="FREENEST_SHEET_JOB_1") {
                int vertexCount=0;in>>vertexCount;Require(vertexCount>=3&&vertexCount<=2000,"Invalid sheet vertex count");
                std::vector<nestai::Segment> boundary;
                for(int i=0;i<vertexCount;++i) {nestai::Point p;in>>p.x>>p.y;Require(bool(in),"Truncated sheet");boundary.push_back({p,p,p});}
                s.useCustomSheet=true;s.customSheet=nestai::Prepare(boundary);
            }
            std::vector<nestai::Part> parts;
            for(int i=0;i<count;++i) {
                int ringCount=1;if(header=="FREENEST_HOLES_JOB_1") in>>ringCount;
                Require(ringCount>=1&&ringCount<=500,"Invalid contour count");
                std::vector<std::vector<nestai::Segment>> rings;
                for(int ring=0;ring<ringCount;++ring) {
                    int n=0;in>>n;Require(n>=3&&n<=2000,"Invalid vertex count");std::vector<nestai::Segment> segments;
                    for(int j=0;j<n;++j) {nestai::Point p;in>>p.x>>p.y;Require(bool(in),"Truncated job");segments.push_back({p,p,p});}
                    rings.push_back(std::move(segments));
                }
                parts.push_back(nestai::PrepareCompound(rings,true));
            }
            if(auto style=std::getenv("FREENEST_PACKING_STYLE")) s.packingStyle=nestai::PackingStyle(std::stoi(style));
            s.centerLayouts=std::getenv("FREENEST_CENTER")!=nullptr;
            auto result=engine.Solve(parts,s);
            nestai::CenterLayouts(result,parts,s);
            std::ofstream out{std::filesystem::path(argv[2])};out<<std::setprecision(17)<<"FREENEST_RESULT_1\n"<<token<<'\n'<<count<<' '<<result.size()<<'\n';
            for(int i=0;i<count;++i) {
                auto found=std::find_if(result.begin(),result.end(),[i](auto p){return p.part==i;});
                if(found==result.end()) out<<i<<" -1 0 0 0\n";
                else out<<found->part<<' '<<found->sheet<<' '<<found->x<<' '<<found->y<<' '<<found->angle<<'\n';
            }
            return 0;
        }
        using Kind=nestai::SelectedKind;
        nestai::Settings defaults;
        Require(std::size(nestai::UnitChoices)==7,"Units dropdown must contain exactly seven choices");
        Require(!defaults.labelSheetBoundary,"Sheet boundary labeling must be optional and off by default");
        Require(defaults.gap==9&&defaults.margin==9,"Spacing and edge margin must default to 0.125 inches (9 pt)");
        Require(defaults.includeSheetBoundary,"Sheet boundary must be included by default");
        Require(defaults.labelColor==nestai::LabelColor::Blue&&defaults.labelStrokeWidth==0.1&&!defaults.separateLabelLayer,"Label appearance or placement defaults changed");
        Require(std::abs(defaults.labelHeight-0.125*72)<1e-10,"Default label height is not 0.125 inches");
        auto black=nestai::LabelColorRGB(nestai::LabelColor::Black),blue=nestai::LabelColorRGB(nestai::LabelColor::Blue);
        auto red=nestai::LabelColorRGB(nestai::LabelColor::Red),magenta=nestai::LabelColorRGB(nestai::LabelColor::Magenta);
        Require(black.red==0&&black.green==0&&black.blue==0,"Black RGB mapping");
        Require(blue.red==0&&blue.green==0&&blue.blue==255,"Blue RGB mapping");
        Require(red.red==255&&red.green==0&&red.blue==0,"Red RGB mapping");
        Require(magenta.red==255&&magenta.green==0&&magenta.blue==255,"Magenta RGB mapping");
        std::cout<<"PASS label color choices, blue 0.1 pt default, retained size and grouped default\n";
        auto letters=engine.Text("Panel-01",10,0),largeLetters=engine.Text("Panel-01",20,0),boldLetters=engine.Text("Panel-01",10,1);
        Require(!letters.empty()&&!boldLetters.empty()&&letters.size()==largeLetters.size(),"VDA regular or bold text failed");
        for(size_t i=0;i<letters.size();++i) for(size_t j=0;j<letters[i].size();++j)
            Require(std::abs(letters[i][j].x*2-largeLetters[i][j].x)<1e-8&&std::abs(letters[i][j].y*2-largeLetters[i][j].y)<1e-8,"VDA font height scaling failed");
        Require(engine.Text("   ",10,0).empty(),"Whitespace created an engraving");
        bool unicodeRejected=false;try {engine.Text("\xc3\xa9",10,0);}catch(const std::exception&) {unicodeRejected=true;}
        Require(unicodeRejected,"Unsupported UTF-8 name was silently rendered");
        std::vector<nestai::Segment> tinySegments;
        for(auto p:std::vector<nestai::Point>{{0,0},{0.3,0},{0.3,0.3},{0,0.3}}) tinySegments.push_back({p,p,p});
        Require(engine.LabelInside(nestai::Prepare(tinySegments),"Panel",10,0).strokes.empty(),"A label escaped a tiny part");
        std::cout<<"PASS bundled VDA regular/bold text, height scaling, whitespace, unsupported characters and too-small parts\n";
        Require(nestai::PointsPerUnit(nestai::Units::Points)==1&&nestai::PointsPerUnit(nestai::Units::Pixels)==1,"Point/pixel document scale");
        Require(nestai::PointsPerUnit(nestai::Units::Picas)==12&&nestai::PointsPerUnit(nestai::Units::Feet)==864,"Pica/foot document scale");
        Require(std::abs(nestai::PointsPerUnit(nestai::Units::Centimeters)-72/2.54)<1e-10,"Centimeter scale");
        Require(std::abs(nestai::PointsPerUnit(nestai::Units::Q)-72/101.6)<1e-10,"Quarter-millimeter scale");
        for(auto unit:nestai::UnitChoices) {
            double displayed=nestai::ConvertUnits(0.125,nestai::Units::Inches,unit);
            Require(std::abs(nestai::ConvertUnits(displayed,unit,nestai::Units::Inches)-0.125)<1e-10,"Document units changed physical spacing");
            double height=3*72/25.4;
            Require(std::abs(height/nestai::PointsPerUnit(unit)*nestai::PointsPerUnit(unit)-height)<1e-10,"Inherited label units changed text height");
        }
        Require(std::abs(nestai::ConvertUnits(1,nestai::Units::Inches,nestai::Units::Millimeters)-25.4)<1e-10,"Inch-to-mm conversion");
        Require(std::abs(nestai::ConvertUnits(25.4,nestai::Units::Millimeters,nestai::Units::Inches)-1)<1e-10,"Mm-to-inch conversion");
        double length=0.125;
        for(int i=0;i<100;++i) length=nestai::ConvertUnits(nestai::ConvertUnits(length,nestai::Units::Inches,nestai::Units::Millimeters),nestai::Units::Millimeters,nestai::Units::Inches);
        Require(std::abs(length-0.125)<1e-10,"Unit switches changed the physical dimension");
        std::cout<<"PASS document-unit scales, inherited label height and reversible physical conversions\n";
        auto selected=nestai::SelectedPathIndices({Kind::LayerContainer,Kind::Path,Kind::Path,Kind::Path,Kind::Path,Kind::Path});
        Require(selected.size()==5&&selected.front()==1&&selected.back()==5,"Layer container prevented five-path selection");
        bool rejectedGroup=false;
        try {nestai::SelectedPathIndices({Kind::Unsupported,Kind::Path});}catch(const std::exception&) {rejectedGroup=true;}
        Require(rejectedGroup,"An ordinary group was silently split into parts");
        nestai::OutputRect sourceBoard{0,0,850,567},canvas{-8000,-8000,8000,8000};
        auto target=nestai::OutputArtboard(sourceBoard,{sourceBoard},550,850,567,57,canvas);
        Require(target.left==907&&target.top==567&&target.bottom==0,"Output did not align with the active artboard");
        target=nestai::OutputArtboard(sourceBoard,{sourceBoard,{900,0,1750,567}},550,850,567,57,canvas);
        Require(target.left==1807&&target.top==567,"Output overlapped an existing artboard");
        target=nestai::OutputArtboard({-500,-400,-200,-100},{{-500,-400,-200,-100},{1000,800,1200,1000}},-250,500,200,50,canvas);
        Require(target.left== -150&&target.top== -100&&target.bottom== -300,"Output failed with an offset artboard or a different sheet size");
        std::cout<<"PASS output artboard alignment, neighboring artboard clearance and offset coordinates\n";
        nestai::OutputRect tightCanvas{0,0,1000,1000},rightSource{650,500,900,600};
        target=nestai::OutputArtboard(rightSource,{rightSource},900,200,100,10,tightCanvas);
        Require(target.left==650&&target.top==490,"Right-edge source did not wrap below itself");
        auto overlap=[](nestai::OutputRect a,nestai::OutputRect b,double gap) {
            return a.left<b.right+gap&&a.right+gap>b.left&&a.bottom<b.top+gap&&a.top+gap>b.bottom;
        };
        nestai::OutputRect gridSource{0,900,180,1000};std::vector<nestai::OutputRect> grid{gridSource};
        std::vector<nestai::OutputRect> offsetGrid{{-1200,3200,-1020,3300}};
        for(int i=0;i<35;++i) {
            auto board=nestai::OutputArtboard(gridSource,grid,180,180,90,10,tightCanvas);
            Require(board.left>=0&&board.right<=1000&&board.bottom>=0&&board.top<=1000,"Wrapped sheet exceeded canvas");
            for(auto other:grid) Require(!overlap(board,other,10-1e-8),"Wrapped sheet overlapped an existing sheet or gap");
            auto offset=nestai::OutputArtboard({-1200,3200,-1020,3300},offsetGrid,-1020,180,90,10,{-1200,2300,-200,3300});
            Require(std::abs(offset.left-(board.left-1200))<1e-8&&std::abs(offset.top-(board.top+2300))<1e-8,"Wrapping changed with shifted ruler origin");
            grid.push_back(board);offsetGrid.push_back(offset);
        }
        // A partial source-path bound must not create a row inside the initial artboard,
        // or cause a jump into the otherwise empty far-left part of the canvas.
        nestai::OutputRect initial{450,500,550,600};
        std::vector<nestai::OutputRect> rightRow{initial,{465,545,480,570},
            {560,500,660,600},{670,500,770,600},{780,500,880,600},{890,500,990,600}};
        target=nestai::OutputArtboard(initial,rightRow,550,100,100,10,tightCanvas);
        Require(target.left==initial.left&&target.top==initial.bottom-10,"Wrapped row did not start beneath the initial artboard");
        rightRow.push_back(target);
        target=nestai::OutputArtboard(initial,rightRow,550,100,100,10,tightCanvas);
        Require(target.left==560&&target.top==490,"Wrapped row did not continue horizontally from the initial artboard");
        bool noNearbySpace=false;
        try {nestai::OutputArtboard({800,0,1000,100},{{800,0,1000,100}},1000,200,100,10,tightCanvas);}
        catch(const std::exception&) {noNearbySpace=true;}
        Require(noNearbySpace,"Exhausted local rows caused a jump to the top or far-left canvas");
        // Reserve the caption together with its sheet instead of clipping it at the bottom edge.
        target=nestai::OutputArtboard(rightSource,{rightSource,{650,350,900,490}},900,200,140,10,tightCanvas);
        Require(target.bottom>=0&&!overlap(target,{650,350,900,490},10-1e-8),"Sheet caption footprint overlapped an obstacle");
        bool full=false,oversized=false;
        try {nestai::OutputArtboard(gridSource,{tightCanvas},180,180,90,10,tightCanvas);} catch(const std::exception&) {full=true;}
        try {nestai::OutputArtboard(gridSource,{},180,1001,90,10,tightCanvas);} catch(const std::exception&) {oversized=true;}
        Require(full&&oversized,"Full canvas or oversized sheet was accepted");
        std::cout<<"PASS canvas-aware row wrapping, shifted origins, 35 non-overlapping sheets, source-aligned rows without canvas-edge jumps, captions and exhausted canvas\n";

        auto placed=nestai::Place({101,202},{100,200},{1000,2000},90);
        Require(std::abs(placed.x-998)<1e-8&&std::abs(placed.y-2001)<1e-8,"Quarter-turn coordinates");
        std::vector<nestai::Segment> ellipse={{{100,200},{100,160},{100,240}},{{180,200},{180,240},{180,160}}};
        auto curve=nestai::Prepare(ellipse);Require(curve.outline.size()>10,"Cubic curve was not flattened");
        Require(ellipse[0].anchor.x==100&&ellipse[0].left.y==160,"Source geometry changed");
        bool rejected=false;
        try {nestai::Prepare({{{0,0},{0,0},{0,0}},{{40,35},{40,35},{40,35}},{{0,30},{0,30},{0,30}},{{30,0},{30,0},{30,0}}});}
        catch(const std::exception&) {rejected=true;}
        Require(rejected,"Self-intersection was accepted");
        std::vector<nestai::Segment> rectangle;
        for(auto p:std::vector<nestai::Point>{{0,0},{60,0},{60,20},{0,20}}) rectangle.push_back({p,p,p});
        auto rect=nestai::Prepare(rectangle);nestai::Settings s;s.width=40;s.height=80;s.margin=5;s.gap=3;s.rotations=2;s.seconds=1;
        Require(engine.Solve({rect},s).empty(),"Half-turn-only rectangle was rotated by 90 degrees");
        s.rotations=4;Require(engine.Solve({rect},s).size()==1,"Quarter-turn rectangle did not fit");
        s.width=80;s.height=40;s.rotations=1;s.multipleSheets=false;
        Require(engine.Solve({rect,rect,rect},s).size()==1,"Single-sheet option used extra sheets");
        s.multipleSheets=true;s.maxSheets=3;
        auto overflow=engine.Solve({rect,rect,rect},s);std::set<int> sheets;
        for(auto part:overflow) sheets.insert(part.sheet);
        Require(overflow.size()==3&&sheets.size()==3,"Overflow parts were not assigned to separate sheets");
        s.maxSheets=2;Require(engine.Solve({rect,rect,rect},s).size()==2,"Maximum sheet count was exceeded");
        std::cout<<"PASS optional overflow sheets, distinct sheet assignments and maximum sheet limit\n";
        auto polygon=[](std::initializer_list<nestai::Point> points) {
            std::vector<nestai::Segment> segments;
            for(auto p:points) segments.push_back({p,p,p});
            return nestai::Prepare(segments);
        };
        auto uSheet=polygon({{0,0},{100,0},{100,100},{70,100},{70,30},{30,30},{30,100},{0,100}});
        auto bridge=polygon({{20,60},{80,60},{80,80},{20,80}});
        Require(!nestai::FitsSheet(bridge.outline,uSheet.outline,1),"Concave notch accepted a bridging edge");
        auto inside=std::vector<nestai::Point>{{3,3},{23,3},{23,23},{3,23}};
        Require(nestai::FitsSheet(inside,uSheet.outline,2),"Interior contour rejected");
        Require(!nestai::FitsSheet(inside,uSheet.outline,4),"Insufficient sheet clearance accepted");
        nestai::Settings irregular;irregular.useCustomSheet=true;irregular.margin=2;irregular.gap=3;irregular.seconds=1;irregular.rotations=1;
        // Shifted source coordinates exercise boundary normalization and output transforms.
        irregular.customSheet=polygon({{300,-200},{380,-200},{380,-170},{330,-170},{330,-120},{300,-120}});
        auto sheetSize=nestai::SheetSize(irregular.customSheet);irregular.width=sheetSize.x;irregular.height=sheetSize.y;
        auto lResult=engine.Solve({rect,rect,rect},irregular);
        Require(lResult.size()==1,"L-shaped single sheet used its missing corner");
        irregular.multipleSheets=true;irregular.maxSheets=3;
        auto lOverflow=engine.Solve({rect,rect,rect},irregular);std::set<int> irregularSheets;
        for(auto placement:lOverflow) {
            irregularSheets.insert(placement.sheet);std::vector<nestai::Point> outline;
            for(auto point:rect.outline) outline.push_back(nestai::Place(point,{0,0},{placement.x,placement.y},placement.angle));
            Require(nestai::FitsSheet(outline,irregular.customSheet.outline,irregular.margin+0.1998),"Irregular placement failed containment oracle");
        }
        Require(lOverflow.size()==3&&irregularSheets.size()==3,"Irregular sheet overflow failed");
        irregular.multipleSheets=false;
        std::reverse(irregular.customSheet.segments.begin(),irregular.customSheet.segments.end());
        irregular.customSheet=nestai::Prepare(irregular.customSheet.segments);
        Require(engine.Solve({rect},irregular).size()==1,"Reversed sheet winding failed");
        irregular.customSheet=curve;sheetSize=nestai::SheetSize(curve);irregular.width=sheetSize.x;irregular.height=sheetSize.y;
        auto smallPart=polygon({{0,0},{10,0},{10,8},{0,8}});
        Require(engine.Solve({smallPart,smallPart},irregular).size()==2,"Curved sheet failed");
        std::cout<<"PASS concave sheet containment, edge margin, shifted origin, reversed winding, curved sheets and irregular overflow\n";
        auto outerRing=polygon({{200,-100},{300,-100},{300,0},{200,0}}).segments;
        auto holeRing=polygon({{220,-80},{280,-80},{280,-20},{220,-20}}).segments;
        auto donut=nestai::PrepareCompound({holeRing,outerRing},true);
        Require(donut.holes.size()==1&&donut.origin.x==200&&donut.origin.y==-100,"Compound outer detection or shared origin failed");
        Require(donut.holes[0][0].x==20&&donut.holes[0][0].y==20,"Hole origin differs from outer origin");
        Require(nestai::PrepareCompound({outerRing,holeRing},false).holes.empty(),"Nonzero same-winding contour became a hole");
        std::reverse(holeRing.begin(),holeRing.end());
        Require(nestai::PrepareCompound({outerRing,holeRing},false).holes.size()==1,"Nonzero opposite-winding hole lost");
        bool badCompound=false;
        try {nestai::PrepareCompound({outerRing,polygon({{400,0},{420,0},{420,20},{400,20}}).segments},true);}catch(const std::exception&) {badCompound=true;}
        Require(badCompound,"Separate islands accepted as holes");
        badCompound=false;
        try {nestai::PrepareCompound({outerRing,holeRing,polygon({{230,-70},{250,-70},{250,-50},{230,-50}}).segments},true);}catch(const std::exception&) {badCompound=true;}
        Require(badCompound,"Nested material island accepted");
        nestai::Settings holeSettings;holeSettings.width=110;holeSettings.height=110;holeSettings.margin=2;holeSettings.gap=3;holeSettings.seconds=2;holeSettings.rotations=1;
        auto insertPart=polygon({{0,0},{30,0},{30,30},{0,30}});
        auto holeResult=engine.Solve({donut,insertPart},holeSettings);
        Require(holeResult.size()==2,"Small part was not nested into available hole");
        auto curvedCompound=nestai::PrepareCompound({polygon({{90,160},{190,160},{190,240},{90,240}}).segments,ellipse},true);
        Require(curvedCompound.holes.size()==1&&curvedCompound.holes[0].size()>10,"Curved compound hole was not flattened");
        auto curvedSettings=holeSettings;curvedSettings.height=90;
        Require(engine.Solve({curvedCompound,smallPart},curvedSettings).size()==2,"Curved cutout did not receive the smaller part");
        auto donutLabel=engine.LabelInside(donut,"Panel-01",8,0);
        Require(!donutLabel.strokes.empty(),"No label fitted on compound material");
        for(auto stroke:donutLabel.strokes) for(auto point:stroke)
            Require(point.x<=20||point.x>=80||point.y<=20||point.y>=80,"Label placed inside a cutout");
        Require(nestai::SelectedPathIndices({Kind::CompoundPath,Kind::Path}).size()==2,"Compound selection rejected");
        std::cout<<"PASS compound holes, fill rules, shifted/shared origin, invalid islands, labels and part-in-hole nesting\n";
        auto smallCutout=nestai::PrepareCompound({polygon({{0,0},{30,0},{30,20},{0,20}}).segments,
            polygon({{12,7},{18,7},{18,13},{12,13}}).segments},true);
        nestai::Settings dense;dense.width=500;dense.height=350;dense.margin=2;dense.gap=3;dense.seconds=1;
        std::vector<nestai::Part> denseParts(200,smallCutout);
        Require(engine.Solve(denseParts,dense).size()==200,"Small holes disabled dense rectangle packing");
        denseParts[0]=nestai::PrepareCompound({smallCutout.segments,polygon({{3,3},{27,3},{27,17},{3,17}}).segments},true);
        denseParts.push_back(polygon({{0,0},{4,0},{4,4},{0,4}}));
        Require(engine.Solve(denseParts,dense).size()==201,"Hole-aware search lost a better conservative packing");
        std::cout<<"PASS dense compound packing and conservative candidate retention\n";
        Require(nestai::Settings{}.packingStyle==nestai::PackingStyle::NarrowStrip&&!nestai::Settings{}.centerLayouts,"Packing defaults changed");
        auto concavePart=polygon({{0,0},{30,0},{30,10},{10,10},{10,30},{0,30}});
        for(auto style:{nestai::PackingStyle::NarrowStrip,nestai::PackingStyle::CompactArea,nestai::PackingStyle::TightFit}) {
            nestai::Settings options;options.width=150;options.height=150;options.seconds=0.3;options.packingStyle=style;
            Require(engine.Solve(std::vector<nestai::Part>(4,concavePart),options).size()==4,"Packing style failed a concave job");
        }
        nestai::Settings centered;centered.width=200;centered.height=160;centered.centerLayouts=true;
        std::vector<nestai::Part> centerParts{donut,smallPart};
        std::vector<nestai::Placement> layout{{0,0,10,10,0},{1,0,50,50,90},{1,1,20,25,90}};
        auto original=layout;
        Require(nestai::CenterLayouts(layout,centerParts,centered)==0,"Rectangle centering skipped a sheet");
        Require(std::abs((layout[1].x-layout[0].x)-(original[1].x-original[0].x))<1e-8&&
                std::abs((layout[1].y-layout[0].y)-(original[1].y-original[0].y))<1e-8,"Centering changed part-in-hole offsets");
        for(int sheet=0;sheet<2;++sheet) {
            double minx=1e100,miny=1e100,maxx=-1e100,maxy=-1e100;
            for(auto p:layout) if(p.sheet==sheet) for(auto v:centerParts[p.part].outline) {
                auto q=nestai::Place(v,{0,0},{p.x,p.y},p.angle);
                minx=std::min(minx,q.x);maxx=std::max(maxx,q.x);miny=std::min(miny,q.y);maxy=std::max(maxy,q.y);
            }
            Require(std::abs(minx+maxx-centered.width)<1e-8&&std::abs(miny+maxy-centered.height)<1e-8,"Rotated multi-sheet layout was not centered");
        }
        centered.useCustomSheet=true;centered.customSheet=uSheet;centered.width=100;centered.height=100;centered.margin=2;
        std::vector<nestai::Placement> notch{{0,0,5,55,0}};auto before=notch;
        Require(nestai::CenterLayouts(notch,{smallPart},centered)==1&&notch[0].x==before[0].x&&notch[0].y==before[0].y,"Centering crossed an irregular sheet notch");
        centered.customSheet=polygon({{300,400},{400,400},{400,500},{300,500}});
        Require(nestai::CenterLayouts(notch,{smallPart},centered)==0,"Safe custom-sheet centering was rejected");
        centered.centerLayouts=false;before=notch;
        Require(nestai::CenterLayouts(notch,{smallPart},centered)==0&&notch[0].x==before[0].x&&notch[0].y==before[0].y,"Disabled centering moved a layout");
        std::cout<<"PASS three packing styles, centered rotated multi-sheet layouts, hole offsets and irregular-sheet safety\n";
        // Cancel a long solve on the same thread boundary used by the plugin dialog.
        std::vector<nestai::Part> many(35,curve);s.width=600;s.height=500;s.seconds=30;s.multipleSheets=false;
        std::atomic<bool> done{false};std::exception_ptr failure;
        auto start=std::chrono::steady_clock::now();
        std::thread worker([&] {try {engine.Solve(many,s);}catch(...) {failure=std::current_exception();}done=true;});
        while(!done) {engine.Cancel();std::this_thread::sleep_for(std::chrono::milliseconds(50));}
        worker.join();if(failure) std::rethrow_exception(failure);
        Require(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<10,"Cancellation took too long");
        std::cout<<"PASS selected layer containers filtered; ordinary groups rejected\n";
        std::cout<<"PASS native curve flattening, source preservation, quarter-turn placement, invalid paths, rotation limits and threaded cancellation\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
