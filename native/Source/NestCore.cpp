#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "NestCore.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <chrono>
#include <map>
#include <tuple>
namespace nestai {
static double Distance(Point p,Point a,Point b) {
    double dx=b.x-a.x,dy=b.y-a.y,d=dx*dx+dy*dy,t=d?((p.x-a.x)*dx+(p.y-a.y)*dy)/d:0;
    t=std::max(0.0,std::min(1.0,t));return std::hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);
}
static Point Mid(Point a,Point b) {return {(a.x+b.x)/2,(a.y+b.y)/2};}
static void Flatten(Point a,Point b,Point c,Point d,int depth,std::vector<Point>& out) {
    if(out.size()>2000) throw std::runtime_error("A path exceeds 2,000 flattened vertices.");
    if(std::max(Distance(b,a,d),Distance(c,a,d))<=0.1) {out.push_back(d);return;}
    if(depth>=20) throw std::runtime_error("A curve exceeds the subdivision limit.");
    Point ab=Mid(a,b),bc=Mid(b,c),cd=Mid(c,d),abc=Mid(ab,bc),bcd=Mid(bc,cd),m=Mid(abc,bcd);
    Flatten(a,ab,abc,m,depth+1,out);Flatten(m,bcd,cd,d,depth+1,out);
}
static double Cross(Point a,Point b,Point c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
static bool Intersects(Point a,Point b,Point c,Point d) {
    if(std::max(a.x,b.x)<std::min(c.x,d.x)-1e-8||std::max(c.x,d.x)<std::min(a.x,b.x)-1e-8||
       std::max(a.y,b.y)<std::min(c.y,d.y)-1e-8||std::max(c.y,d.y)<std::min(a.y,b.y)-1e-8) return false;
    auto sign=[](double v){return v>1e-8?1:v< -1e-8?-1:0;};
    return sign(Cross(a,b,c))*sign(Cross(a,b,d))<=0&&sign(Cross(c,d,a))*sign(Cross(c,d,b))<=0;
}
static bool Contains(const std::vector<Point>& polygon,Point p) {
    bool inside=false;size_t j=polygon.size()-1;
    for(size_t i=0;i<polygon.size();j=i++) {
        auto a=polygon[i],b=polygon[j];
        if((a.y>p.y)!=(b.y>p.y)&&p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
    }
    return inside;
}
Part Prepare(const std::vector<Segment>& segments) {
    if(segments.size()<2||segments.size()>2000) throw std::runtime_error("Unsupported path point count.");
    for(auto s:segments) for(auto p:{s.anchor,s.left,s.right})
        if(!std::isfinite(p.x)||!std::isfinite(p.y)) throw std::runtime_error("Invalid curve coordinate.");
    Part part;part.segments=segments;std::vector<Point> points{segments[0].anchor};
    for(size_t i=0;i<segments.size();++i) {auto a=segments[i],b=segments[(i+1)%segments.size()];Flatten(a.anchor,a.right,b.left,b.anchor,0,points);}
    points.pop_back();
    for(auto p:points) if(part.outline.empty()||Distance(p,part.outline.back(),part.outline.back())>1e-7) part.outline.push_back(p);
    auto& outline=part.outline;
    if(outline.size()>2&&Distance(outline.front(),outline.back(),outline.back())<1e-7) outline.pop_back();
    int n=int(outline.size());if(n<3||n>2000) throw std::runtime_error("Invalid path outline.");
    double area=0;part.origin=outline[0];
    for(int i=0;i<n;++i) {
        Point a=outline[i],b=outline[(i+1)%n];area+=a.x*b.y-b.x*a.y;
        part.origin.x=std::min(part.origin.x,a.x);part.origin.y=std::min(part.origin.y,a.y);
        for(int j=i+1;j<n;++j) if(j!=i+1&&!(i==0&&j==n-1)&&Intersects(a,b,outline[j],outline[(j+1)%n]))
            throw std::runtime_error("A selected path intersects itself.");
    }
    if(std::abs(area)<1e-6) throw std::runtime_error("A selected path has no usable area.");
    for(auto& p:outline) {p.x-=part.origin.x;p.y-=part.origin.y;if(std::abs(p.x)>100000||std::abs(p.y)>100000) throw std::runtime_error("Part is too large.");}
    return part;
}
Part PrepareSymbolBounds(double left,double bottom,double right,double top) {
    if(!(right>left&&top>bottom)) throw std::runtime_error("A symbol instance has no usable visible bounds.");
    std::vector<Segment> segments;
    for(Point p:std::initializer_list<Point>{{left,bottom},{right,bottom},{right,top},{left,top}})
        segments.push_back({p,p,p});
    return Prepare(segments);
}
Point Place(Point p,Point origin,Point destination,double angle) {
    double rad=angle*3.14159265358979323846/180,c=std::cos(rad),s=std::sin(rad),x=p.x-origin.x,y=p.y-origin.y;
    return {c*x-s*y+destination.x,s*x+c*y+destination.y};
}
bool SymbolFitsPlacedBounds(const Part& footprint,Point destination,double angle,Point lower,Point upper) {
    if(footprint.outline.empty()||!(lower.x<upper.x&&lower.y<upper.y)) return false;
    Point min=destination,max=destination;
    for(auto vertex:footprint.outline) {
        auto p=Place(vertex,{0,0},destination,angle);
        min.x=std::min(min.x,p.x);min.y=std::min(min.y,p.y);
        max.x=std::max(max.x,p.x);max.y=std::max(max.y,p.y);
    }
    constexpr double tolerance=0.025;
    return lower.x>=min.x-tolerance&&upper.x<=max.x+tolerance&&
           lower.y>=min.y-tolerance&&upper.y<=max.y+tolerance;
}
bool SymbolContourMatchesPlacement(const Part& footprint,const std::vector<Segment>& actual,Point destination,double angle) {
    if(actual.empty()||actual.size()!=footprint.segments.size()) return false;
    auto matches=[&](Point source,Point observed) {
        auto expected=Place(source,footprint.origin,destination,angle);
        return std::hypot(expected.x-observed.x,expected.y-observed.y)<=0.025;
    };
    for(size_t i=0;i<actual.size();++i) {
        const auto& before=footprint.segments[i];
        const auto& after=actual[i];
        if(!matches(before.anchor,after.anchor)||!matches(before.left,after.left)||
            !matches(before.right,after.right)) return false;
    }
    return true;
}
Point SheetSize(const Part& sheet) {
    Point size;
    for(auto p:sheet.outline) {size.x=std::max(size.x,p.x);size.y=std::max(size.y,p.y);}
    return size;
}
bool FitsSheet(const std::vector<Point>& part,const std::vector<Point>& sheet,double margin) {
    if(part.size()<3||sheet.size()<3||!std::isfinite(margin)||margin<0) return false;
    for(auto p:part) if(!Contains(sheet,p)) return false;
    // Vertices alone cannot detect an edge bridging across a concave notch.
    for(size_t i=0;i<part.size();++i) for(size_t j=0;j<sheet.size();++j) {
        auto a=part[i],b=part[(i+1)%part.size()],c=sheet[j],d=sheet[(j+1)%sheet.size()];
        if(Intersects(a,b,c,d)||std::min({Distance(a,c,d),Distance(b,c,d),Distance(c,a,b),Distance(d,a,b)})<margin) return false;
    }
    return true;
}
static double Area(const std::vector<Point>& ring) {
    double area=0;for(size_t i=0;i<ring.size();++i) {auto a=ring[i],b=ring[(i+1)%ring.size()];area+=a.x*b.y-b.x*a.y;}
    return area/2;
}
static bool MaterialContains(const std::vector<Point>& outer,const std::vector<std::vector<Point>>& holes,Point p) {
    if(!Contains(outer,p)) return false;
    for(const auto& hole:holes) if(Contains(hole,p)) return false;
    return true;
}
static bool RingsClear(const std::vector<Point>& first,const std::vector<Point>& second,double gap) {
    for(size_t i=0;i<first.size();++i) for(size_t j=0;j<second.size();++j) {
        auto a=first[i],b=first[(i+1)%first.size()],c=second[j],d=second[(j+1)%second.size()];
        if(Intersects(a,b,c,d)||std::min({Distance(a,c,d),Distance(b,c,d),Distance(c,a,b),Distance(d,a,b)})<gap) return false;
    }
    return true;
}
Part PrepareCompound(const std::vector<std::vector<Segment>>& rings,bool evenOdd) {
    if(rings.empty()||rings.size()>500) throw std::runtime_error("Unsupported compound path contour count.");
    std::vector<Part> contours;size_t outer=0;
    for(const auto& ring:rings) {contours.push_back(Prepare(ring));if(std::abs(Area(contours.back().outline))>std::abs(Area(contours[outer].outline))) outer=contours.size()-1;}
    Part part=contours[outer];std::vector<std::vector<Point>> interiors;
    for(size_t i=0;i<contours.size();++i) if(i!=outer) {
        auto ring=contours[i].outline;
        for(auto& p:ring) {p.x+=contours[i].origin.x-part.origin.x;p.y+=contours[i].origin.y-part.origin.y;}
        if(!FitsSheet(ring,part.outline,1e-6)) throw std::runtime_error("Compound paths need one outer contour with fully enclosed, non-touching holes. Separate islands are not supported.");
        for(const auto& other:interiors)
            if(Contains(other,ring[0])||Contains(ring,other[0])||!RingsClear(ring,other,1e-6))
                throw std::runtime_error("Overlapping holes or nested islands in compound paths are not supported.");
        interiors.push_back(ring);
        if(evenOdd||Area(ring)*Area(part.outline)<0) part.holes.push_back(std::move(ring));
    }
    return part;
}
Engine::Engine(const WCHAR* path) {
    library=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library) throw std::runtime_error("Cannot load nfp_nest.dll. Keep it beside FreeNest.aip.");
    solve=reinterpret_cast<decltype(solve)>(GetProcAddress(library,"nfp_nest"));
    cancel=reinterpret_cast<decltype(cancel)>(GetProcAddress(library,"nfp_cancel"));
    progress=reinterpret_cast<decltype(progress)>(GetProcAddress(library,"nfp_progress"));
    text=reinterpret_cast<decltype(text)>(GetProcAddress(library,"nfp_text_to_polylines"));
    if(!solve||!cancel||!progress||!text) {FreeLibrary(library);library=nullptr;throw std::runtime_error("Incompatible OpenNest engine.");}
}
Engine::~Engine() {if(library) FreeLibrary(library);}
Strokes Engine::Text(const std::string& value,double height,int font) {
    if(value.empty()||value.size()>128||!std::isfinite(height)||height<=0||height>10000||(font!=0&&font!=1)) throw std::runtime_error("Invalid VDA label settings.");
    for(unsigned char c:value) if(c<32||c>126) throw std::runtime_error("VDA labels currently support printable ASCII object names only (up to 128 characters).");
    int points=0;int strokes=text(value.c_str(),height,font,-1,0,nullptr,0,nullptr,&points);
    if(strokes<=0||points<=0) return {};
    if(strokes>10000||points>200000) throw std::runtime_error("Label geometry exceeds the supported limit.");
    std::vector<int> counts(strokes);std::vector<double> xy(2*points);int actualPoints=0;
    int actualStrokes=text(value.c_str(),height,font,-1,strokes,counts.data(),points,xy.data(),&actualPoints);
    if(actualStrokes!=strokes||actualPoints!=points) throw std::runtime_error("VDA label buffer size changed.");
    Strokes output;size_t offset=0;
    for(int n:counts) {
        if(n<1||offset+size_t(n)>size_t(points)) throw std::runtime_error("Invalid VDA stroke data.");
        std::vector<Point> stroke;
        for(int i=0;i<n;++i) {double x=xy[2*(offset+i)],y=xy[2*(offset+i)+1];if(!std::isfinite(x)||!std::isfinite(y)) throw std::runtime_error("Invalid VDA stroke coordinate.");stroke.push_back({x,y});}
        if(stroke.size()>1) output.push_back(std::move(stroke));offset+=n;
    }
    return output;
}
Label Engine::LabelInside(const Part& part,const std::string& value,double height,int font,double strokeWidth,bool preferEdge) {
    if(!std::isfinite(strokeWidth)||strokeWidth<=0||strokeWidth>100) throw std::runtime_error("Invalid label stroke weight.");
    auto strokes=Text(value,height,font);if(strokes.empty()) return {};
    double minx=1e100,miny=1e100,maxx=-1e100,maxy=-1e100,pw=0,ph=0;
    for(const auto& stroke:strokes) for(auto p:stroke) {minx=std::min(minx,p.x);miny=std::min(miny,p.y);maxx=std::max(maxx,p.x);maxy=std::max(maxy,p.y);}
    for(auto p:part.outline) {pw=std::max(pw,p.x);ph=std::max(ph,p.y);}
    const double pad=0.125+strokeWidth/2;double width=maxx-minx,h=maxy-miny;
    if(pw<=2*pad||ph<=2*pad) return {};
    double scale=std::min({1.0,(pw-2*pad)/std::max(width,1e-8),(ph-2*pad)/std::max(h,1e-8)});
    auto fits=[&](double left,double bottom,double w,double h) {
        Point box[]={{left-pad,bottom-pad},{left+w+pad,bottom-pad},{left+w+pad,bottom+h+pad},{left-pad,bottom+h+pad}};
        for(auto p:box) if(!Contains(part.outline,p)) return false;
        for(size_t i=0;i<part.outline.size();++i) for(int j=0;j<4;++j)
            if(Intersects(part.outline[i],part.outline[(i+1)%part.outline.size()],box[j],box[(j+1)%4])) return false;
        std::vector<Point> labelBox(std::begin(box),std::end(box));
        for(const auto& hole:part.holes) {
            if(Contains(labelBox,hole[0])||Contains(hole,box[0])||!RingsClear(labelBox,hole,1e-6)) return false;
        }
        return true;
    };
    double minimumHeight=std::min(height,0.5*72/25.4);
    for(int attempt=0;attempt<24&&height*scale>=minimumHeight-1e-8;++attempt,scale*=0.85) {
        double w=width*scale,hscaled=h*scale,left=0,bottom=0;bool found=false;
        for(int candidate=preferEdge?0:-1;candidate<17*17;++candidate) {
            double edge=preferEdge?2*pad+0.1:pad;
            left=candidate<0?(pw-w)/2:edge+(pw-w-2*edge)*(candidate%17)/16;
            bottom=candidate<0?(ph-hscaled)/2:edge+(ph-hscaled-2*edge)*(candidate/17)/16;
            if(fits(left,bottom,w,hscaled)) {found=true;break;}
        }
        if(!found) continue;
        for(auto& stroke:strokes) for(auto& p:stroke) {p.x=(p.x-minx)*scale+left;p.y=(p.y-miny)*scale+bottom;}
        return {std::move(strokes),height*scale};
    }
    return {};
}
void Engine::ReserveSheetLabel(Settings& s) {
    s.sheetLabelRegion.clear();
    if(!s.includeSheetBoundary||!s.labelSheetBoundary) return;
    auto strokes=Text("FreeNest Sheet "+std::to_string(s.multipleSheets?s.maxSheets:1),s.labelHeight,s.labelFont);
    if(strokes.empty()) throw std::runtime_error("Cannot generate the sheet label.");
    double left=1e100,bottom=1e100,right=-1e100,top=-1e100;
    for(const auto& stroke:strokes) for(auto p:stroke) {left=std::min(left,p.x);right=std::max(right,p.x);bottom=std::min(bottom,p.y);top=std::max(top,p.y);}
    double pad=0.125+s.labelStrokeWidth/2;
    double scale=std::min(1.0,std::max(1e-8,s.width-2*pad)/std::max(1e-8,right-left));
    double width=(right-left)*scale+2*pad,height=(top-bottom)*scale+2*pad;
    double gap=3*72/25.4;
    // Relative to the sheet's bounding-box origin: below the sheet, never inside it.
    s.sheetLabelRegion={{0,-gap-height},{width,-gap-height},{width,-gap},{0,-gap}};
}

Label Engine::SheetLabel(const std::string& value,const Settings& s) {
    if(s.sheetLabelRegion.size()!=4) throw std::runtime_error("Sheet label space has not been reserved.");
    auto strokes=Text(value,s.labelHeight,s.labelFont);if(strokes.empty()) return {};
    double left=1e100,bottom=1e100,right=-1e100,top=-1e100;
    for(const auto& stroke:strokes) for(auto p:stroke) {left=std::min(left,p.x);right=std::max(right,p.x);bottom=std::min(bottom,p.y);top=std::max(top,p.y);}
    double pad=0.125+s.labelStrokeWidth/2;auto box=s.sheetLabelRegion;
    double scale=std::min({1.0,(box[1].x-box[0].x-2*pad)/std::max(1e-8,right-left),(box[2].y-box[0].y-2*pad)/std::max(1e-8,top-bottom)});
    for(auto& stroke:strokes) for(auto& p:stroke) {p.x=box[0].x+pad+(p.x-left)*scale;p.y=box[0].y+pad+(p.y-bottom)*scale;}
    return {std::move(strokes),s.labelHeight*scale};
}
static bool MayNestInHoles(const std::vector<Part>& parts,double gap,int rotations) {
    // If no rotated outer bounding box fits a hole's bounding box with clearance,
    // hole-aware search cannot create any additional valid placements.
    std::vector<Point> extents;
    for(const auto& part:parts) for(int rotation=0;rotation<rotations;++rotation) {
        double left=1e100,bottom=1e100,right=-1e100,top=-1e100;
        for(auto p:part.outline) {
            auto q=Place(p,{0,0},{0,0},rotation*360.0/rotations);
            left=std::min(left,q.x);right=std::max(right,q.x);bottom=std::min(bottom,q.y);top=std::max(top,q.y);
        }
        extents.push_back({right-left,top-bottom});
    }
    for(const auto& part:parts) for(const auto& hole:part.holes) {
        double left=1e100,bottom=1e100,right=-1e100,top=-1e100;
        for(auto p:hole) {left=std::min(left,p.x);right=std::max(right,p.x);bottom=std::min(bottom,p.y);top=std::max(top,p.y);}
        for(auto size:extents) if(size.x+2*gap<=right-left+1e-6&&size.y+2*gap<=top-bottom+1e-6) return true;
    }
    return false;
}
int CenterLayouts(std::vector<Placement>& placements,const std::vector<Part>& parts,const Settings& settings) {
    if(!settings.centerLayouts) return 0;
    std::map<int,std::vector<size_t>> sheets;
    for(size_t i=0;i<placements.size();++i) sheets[placements[i].sheet].push_back(i);
    int skipped=0;
    for(const auto& sheet:sheets) {
        double left=1e100,bottom=1e100,right=-1e100,top=-1e100;
        for(auto i:sheet.second) {
            const auto& placement=placements[i];
            for(auto point:parts.at(placement.part).outline) {
                auto p=Place(point,{0,0},{placement.x,placement.y},placement.angle);
                left=std::min(left,p.x);right=std::max(right,p.x);bottom=std::min(bottom,p.y);top=std::max(top,p.y);
            }
        }
        Point shift{(settings.width-left-right)/2,(settings.height-bottom-top)/2};
        bool safe=true;
        if(settings.useCustomSheet) for(auto i:sheet.second) {
            const auto& placement=placements[i];std::vector<Point> translated;
            for(auto point:parts.at(placement.part).outline)
                translated.push_back(Place(point,{0,0},{placement.x+shift.x,placement.y+shift.y},placement.angle));
            if(!FitsSheet(translated,settings.customSheet.outline,settings.margin+0.2-1e-4)) {safe=false;break;}
        }
        if(!safe) {++skipped;continue;}
        // Translate each sheet as a rigid layout: rotations, gaps and hole placements stay intact.
        for(auto i:sheet.second) {placements[i].x+=shift.x;placements[i].y+=shift.y;}
    }
    return skipped;
}
static auto PackingScore(const std::vector<Placement>& placements,const std::vector<Part>& parts,PackingStyle style) {
    struct Bounds {double left=1e100,bottom=1e100,right=-1e100,top=-1e100;};
    std::map<int,Bounds> bounds;std::map<int,std::vector<Point>> points;double material=0,footprint=0;
    for(auto placement:placements) {
        const auto& part=parts[placement.part];material+=std::abs(Area(part.outline));
        for(const auto& hole:part.holes) material-=std::abs(Area(hole));
        auto& box=bounds[placement.sheet];
        for(auto point:part.outline) {
            auto p=Place(point,{0,0},{placement.x,placement.y},placement.angle);
            if(style==PackingStyle::TightFit) points[placement.sheet].push_back(p);
            box.left=std::min(box.left,p.x);box.right=std::max(box.right,p.x);box.bottom=std::min(box.bottom,p.y);box.top=std::max(box.top,p.y);
        }
    }
    for(auto item:bounds) {
        double width=item.second.right-item.second.left,height=item.second.top-item.second.bottom;
        if(style==PackingStyle::NarrowStrip) footprint+=2*width+height;
        else if(style==PackingStyle::CompactArea) footprint+=width*height;
        else {
            auto vertices=points[item.first];
            std::sort(vertices.begin(),vertices.end(),[](Point a,Point b){return a.x<b.x||(a.x==b.x&&a.y<b.y);});
            std::vector<Point> hull;
            for(auto point:vertices) {while(hull.size()>=2&&Cross(hull[hull.size()-2],hull.back(),point)<=0) hull.pop_back();hull.push_back(point);}
            size_t lower=hull.size();
            for(auto it=vertices.rbegin()+1;it!=vertices.rend();++it) {
                while(hull.size()>lower&&Cross(hull[hull.size()-2],hull.back(),*it)<=0) hull.pop_back();hull.push_back(*it);
            }
            if(hull.size()>1) hull.pop_back();
            footprint+=std::abs(Area(hull));
        }
    }
    return std::make_tuple(-int(placements.size()),int(bounds.size()),-material,footprint);
}
std::vector<Placement> Engine::Solve(const std::vector<Part>& parts,const Settings& s) {
    cancellationRequested=false;
    int n=int(parts.size());
    if(!std::isfinite(s.width)||!std::isfinite(s.height)||!std::isfinite(s.gap)||!std::isfinite(s.margin)||!std::isfinite(s.seconds)||
       n<1||n>500||s.width<=0||s.height<=0||s.gap<0||s.margin<0||s.seconds<0.1||s.seconds>300||
       s.width>100000||s.height>100000||2*(s.margin+0.1)>=std::min(s.width,s.height)||
       std::none_of(std::begin(OrientationChoices),std::end(OrientationChoices),
                    [&](auto choice){return choice.count==s.rotations;})||
       s.maxSheets<1||s.maxSheets>500||int(s.packingStyle)<0||int(s.packingStyle)>2) throw std::runtime_error("Invalid nesting settings.");
    Part custom;
    if(s.useCustomSheet) {
        custom=Prepare(s.customSheet.segments);
        auto size=SheetSize(custom);
        if(std::abs(size.x-s.width)>1e-6||std::abs(size.y-s.height)>1e-6) throw std::runtime_error("Captured sheet dimensions do not match its boundary.");
    }
    std::vector<int> counts,qty(n,1),holes(n,0),holeCounts,rotation(n,s.rotations);std::vector<double> xy,holeXY;
    int partIndex=0;
    for(const auto& part:parts) {
        holes[partIndex++]=int(part.holes.size());
        for(const auto& hole:part.holes) {holeCounts.push_back(int(hole.size()));for(auto p:hole) {holeXY.push_back(p.x);holeXY.push_back(p.y);}}
        auto outline=part.outline;
        // The rectangle fast path only tries quarter turns. Use the general solver
        // for half-turn-only jobs and when 30-degree steps add non-quarter-turn options.
        if((s.rotations==2||s.rotations==12)&&outline.size()==4)
            outline.insert(outline.begin()+1,Mid(outline[0],outline[1]));
        counts.push_back(int(outline.size()));for(auto p:outline) {xy.push_back(p.x);xy.push_back(p.y);}
    }
    if(xy.size()+holeXY.size()>200000) throw std::runtime_error("Selection exceeds 100,000 flattened vertices.");
    NfpParams p{};p.placementType=int(s.packingStyle);p.rotations=s.rotations;p.mutationRate=10;p.populationSize=10;p.seed=1;
    p.curveTolerance=0.01;p.clipperScale=1e6;p.spacing=s.gap+0.2;p.sheetSpacing=s.margin+(s.useCustomSheet?0.2:0.1);
    int sheetCount=s.multipleSheets?std::min(s.maxSheets,n):1;
    p.mode=1;p.generations=100;p.timeBudgetSecs=s.seconds;p.maxSheets=sheetCount;p.tryAllRotations=1;p.exactNfp=1;p.useHoles=1;p.exactVoids=1;
    std::vector<double> sheets;
    auto sheetOutline=s.useCustomSheet?custom.outline:std::vector<Point>{{0,0},{s.width,0},{s.width,s.height},{0,s.height}};
    for(int i=0;i<sheetCount;++i) for(auto point:sheetOutline) {sheets.push_back(point.x);sheets.push_back(point.y);}
    std::vector<int> vertices(sheetCount,int(sheetOutline.size())),noPartHoles(n,0);int used=0;double fitness=0;
    std::vector<int> sheetHoleCounts(sheetCount,0);
    auto run=[&](bool withHoles,double budget) {
    auto parameters=p;parameters.timeBudgetSecs=budget;
    std::vector<double> x(n),y(n),angle(n);std::vector<int> sid(n,-1),idx(n,-1);
    int placed=solve(n,counts.data(),xy.data(),qty.data(),rotation.data(),withHoles?holes.data():noPartHoles.data(),
        withHoles&&!holeCounts.empty()?holeCounts.data():nullptr,withHoles&&!holeXY.empty()?holeXY.data():nullptr,
        sheetCount,vertices.data(),sheets.data(),sheetHoleCounts.data(),nullptr,nullptr,&parameters,x.data(),y.data(),angle.data(),sid.data(),idx.data(),&used,&fitness);
    if(placed<0) throw std::runtime_error("The nesting engine could not solve this selection.");
    std::vector<Placement> output;std::set<int> seen;
    for(int i=0;i<n;++i) {
        if(sid[i]<0) continue;
        if(sid[i]>=sheetCount||idx[i]<0||idx[i]>=n||!seen.insert(idx[i]).second||!std::isfinite(x[i])||!std::isfinite(y[i])||!std::isfinite(angle[i]))
            throw std::runtime_error("The engine returned an invalid placement.");
        double step=360.0/s.rotations;
        if(std::abs(angle[i]/step-std::round(angle[i]/step))>1e-4) throw std::runtime_error("The engine returned a forbidden rotation.");
        for(auto point:parts[idx[i]].outline) {
            auto q=Place(point,{0,0},{x[i],y[i]},angle[i]);
            if(q.x<s.margin-0.001||q.y<s.margin-0.001||q.x>s.width-s.margin+0.001||q.y>s.height-s.margin+0.001)
                throw std::runtime_error("The engine returned a placement outside the sheet.");
        }
        output.push_back({idx[i],sid[i],x[i],y[i],angle[i]});
    }
    // Validate the final contours independently of the solver before editing the document.
    struct PlacedOutline {int sheet=-1;std::vector<Point> points;std::vector<std::vector<Point>> holes;double left=1e100,bottom=1e100,right=-1e100,top=-1e100;};
    std::vector<PlacedOutline> validated;
    for(auto placement:output) {
        PlacedOutline shape;shape.sheet=placement.sheet;
        for(auto point:parts[placement.part].outline) {
            auto q=Place(point,{0,0},{placement.x,placement.y},placement.angle);shape.points.push_back(q);
            shape.left=std::min(shape.left,q.x);shape.right=std::max(shape.right,q.x);
            shape.bottom=std::min(shape.bottom,q.y);shape.top=std::max(shape.top,q.y);
        }
        for(const auto& hole:parts[placement.part].holes) {
            std::vector<Point> transformed;for(auto point:hole) transformed.push_back(Place(point,{0,0},{placement.x,placement.y},placement.angle));
            shape.holes.push_back(std::move(transformed));
        }
        if(s.useCustomSheet&&!FitsSheet(shape.points,sheetOutline,s.margin+0.2-1e-4))
            throw std::runtime_error("Layout validation found a part outside the captured boundary or too close to its edge.");
        for(const auto& other:validated) {
            if(shape.sheet!=other.sheet) continue;
            double gap=s.gap+0.2-1e-4;
            if(shape.right+gap<other.left||other.right+gap<shape.left||shape.top+gap<other.bottom||other.top+gap<shape.bottom) continue;
            if(MaterialContains(shape.points,shape.holes,other.points[0])||MaterialContains(other.points,other.holes,shape.points[0]))
                throw std::runtime_error("Layout validation found overlapping parts.");
            auto first=shape.holes,second=other.holes;first.push_back(shape.points);second.push_back(other.points);
            for(const auto& a:first) for(const auto& b:second) if(!RingsClear(a,b,gap))
                throw std::runtime_error("Layout validation found overlapping parts or insufficient spacing around a contour or hole.");
        }
        validated.push_back(std::move(shape));
    }
    return output;
    };
    if(holeXY.empty()||!MayNestInHoles(parts,s.gap+0.2,s.rotations)) return run(false,s.seconds);
    auto started=std::chrono::steady_clock::now();
    auto baseline=run(false,std::min(1.0,s.seconds*0.2));
    if(cancellationRequested) return baseline;
    double remaining=s.seconds-std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    if(remaining<=0.01) return baseline;
    auto detailed=run(true,remaining);
    return PackingScore(detailed,parts,s.packingStyle)<PackingScore(baseline,parts,s.packingStyle)?detailed:baseline;
}
}
