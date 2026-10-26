#pragma once
#include <vector>
#include <string>
#include <atomic>
#include <windows.h>
#include "capi/nfp_nest_capi.h"
#include "NestUnits.h"
#include "NestLabelStyle.h"
namespace nestai {
struct Point { double x=0,y=0; };
struct Segment { Point anchor,left,right; };
struct Part { std::vector<Segment> segments; std::vector<Point> outline; Point origin; std::vector<std::vector<Point>> holes; };
enum class PackingStyle { CompactArea=0, NarrowStrip=1, TightFit=2 };
struct Settings { double width=300*72/25.4,height=200*72/25.4,gap=0.125*72,margin=0.125*72,seconds=5; int rotations=4; PackingStyle packingStyle=PackingStyle::NarrowStrip; bool centerLayouts=false; Units units=Units::Inches; bool multipleSheets=false; int maxSheets=10; bool includeSheetBoundary=true; bool labelSheetBoundary=false; std::vector<Point> sheetLabelRegion; bool useCustomSheet=false; Part customSheet; bool addLabels=false; double labelHeight=0.125*72; int labelFont=0; LabelColor labelColor=LabelColor::Blue; double labelStrokeWidth=0.1; bool separateLabelLayer=false; };
using Strokes=std::vector<std::vector<Point>>;
struct Label {Strokes strokes;double height=0;};
struct Placement { int part=-1,sheet=-1; double x=0,y=0,angle=0; };
// Returns the number of irregular sheets that could not be safely centered.
int CenterLayouts(std::vector<Placement>& placements,const std::vector<Part>& parts,const Settings& settings);
Part Prepare(const std::vector<Segment>& segments);
Part PrepareCompound(const std::vector<std::vector<Segment>>& rings,bool evenOdd);
Point Place(Point p,Point origin,Point destination,double angle);
Point SheetSize(const Part& sheet);
bool FitsSheet(const std::vector<Point>& part,const std::vector<Point>& sheet,double margin);
class Engine {
    HMODULE library=nullptr;
    std::atomic<bool> cancellationRequested{false};
    decltype(&nfp_nest) solve=nullptr;
    decltype(&nfp_cancel) cancel=nullptr;
    decltype(&nfp_progress) progress=nullptr;
    decltype(&nfp_text_to_polylines) text=nullptr;
public:
    explicit Engine(const WCHAR* path);
    ~Engine();
    Engine(const Engine&)=delete; Engine& operator=(const Engine&)=delete;
    std::vector<Placement> Solve(const std::vector<Part>& parts,const Settings& settings);
    Strokes Text(const std::string& value,double height,int font);
    Label LabelInside(const Part& part,const std::string& value,double height,int font,double strokeWidth=0.1,bool preferEdge=false);
    void ReserveSheetLabel(Settings& settings);
    Label SheetLabel(const std::string& value,const Settings& settings);
    void Cancel() {cancellationRequested=true;cancel();}
    long long Progress() {return progress();}
};
}
