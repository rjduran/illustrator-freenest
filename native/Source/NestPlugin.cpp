#include "IllustratorSDK.h"
#include "NestSuites.h"
#include "NestCore.h"
#include "NestSelection.h"
#include "NestOutput.h"
#include "NestPanel.h"
#include "resource.h"
#include <thread>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <map>
#include <chrono>

namespace {
HMODULE Module() {
    HMODULE module=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&Module),&module);return module;
}
void Check(AIErr error) {if(error) throw std::runtime_error("Illustrator could not complete the artwork operation ("+std::to_string(error)+").");}
void CenterDialog(HWND dialog) {
    HWND owner=GetWindow(dialog,GW_OWNER);
    HMONITOR monitor=MonitorFromWindow(owner?owner:dialog,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};info.cbSize=sizeof(info);RECT work{},bounds{};
    if(GetMonitorInfoW(monitor,&info)) work=info.rcWork;
    else if(!SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0)) return;
    if(!GetWindowRect(dialog,&bounds)) return;
    LONG width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    LONG x=work.left+std::max<LONG>(0,(work.right-work.left-width)/2);
    LONG y=work.top+std::max<LONG>(0,(work.bottom-work.top-height)/2);
    SetWindowPos(dialog,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}
INT_PTR CALLBACK AboutDialog(HWND h,UINT message,WPARAM w,LPARAM) {
    if(message==WM_INITDIALOG) {CenterDialog(h);return TRUE;}
    if(message==WM_CLOSE||(message==WM_COMMAND&&(LOWORD(w)==IDOK||LOWORD(w)==IDCANCEL))) {
        EndDialog(h,IDOK);return TRUE;
    }
    return FALSE;
}
void PutNumber(HWND h,int id,double value) {std::ostringstream text;text<<std::setprecision(12)<<value;SetDlgItemTextA(h,id,text.str().c_str());}
double Number(HWND h,int id,double min,double max) {
    char text[128];GetDlgItemTextA(h,id,text,128);char* end=nullptr;double value=strtod(text,&end);
    while(end&&*end==' ') ++end;
    if(end==text||!end||*end||!std::isfinite(value)||value<min||value>max) throw std::runtime_error("Enter valid positive dimensions, nonnegative spacing and a time limit from 0.1 to 300 seconds.");
    return value;
}
nestai::Units DocumentUnits() {
    ai::int16 unit=kUnknownUnits;Check(sAIDocument->GetDocumentRulerUnits(&unit));
    switch(unit) {
        case kInchesUnits:return nestai::Units::Inches;
        case kMillimetersUnits:return nestai::Units::Millimeters;
        case kCentimetersUnits:return nestai::Units::Centimeters;
        case kPicasUnits:return nestai::Units::Points;
        case kPixelsUnits:return nestai::Units::Pixels;
        case kQUnits:return nestai::Units::Millimeters;
        case kFeetInchesUnits:return nestai::Units::Inches;
        case kMetersUnits:return nestai::Units::Meters;
        case kYardsUnits:return nestai::Units::Feet;
        case kFeetsUnits:return nestai::Units::Feet;
        default:return nestai::Units::Points;
    }
}
struct SettingsState {
    nestai::Settings* saved;nestai::Units units;double labelHeight;int labelFont;
    nestai::LabelColor labelColor;double labelStrokeWidth;bool separateLabelLayer;
    bool executeLabels=false;
    double rectangleWidth=0,rectangleHeight=0;
    SettingsState(nestai::Settings* s,nestai::Units u,double h,int f):saved(s),units(u),labelHeight(h),labelFont(f),
        labelColor(s->labelColor),labelStrokeWidth(s->labelStrokeWidth),separateLabelLayer(s->separateLabelLayer),rectangleWidth(s->width),rectangleHeight(s->height){}
};
INT_PTR CALLBACK LabelDialog(HWND h,UINT message,WPARAM w,LPARAM l) {
    auto state=reinterpret_cast<SettingsState*>(GetWindowLongPtr(h,DWLP_USER));
    if(message==WM_INITDIALOG) {
        state=reinterpret_cast<SettingsState*>(l);SetWindowLongPtr(h,DWLP_USER,l);
        SetDlgItemTextA(h,IDC_LABEL_HEIGHT_LABEL,(std::string("Maximum label height (")+nestai::UnitSymbol(state->units)+")").c_str());
        if(state->executeLabels) {
            SetWindowTextA(h,"FreeNest - Label Selected Paths");
            SetDlgItemTextA(h,IDOK,"Label");
        }
        PutNumber(h,IDC_LABEL_HEIGHT,state->labelHeight/nestai::PointsPerUnit(state->units));
        for(auto label:{"Regular","Bold"}) SendDlgItemMessageA(h,IDC_LABEL_FONT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendDlgItemMessage(h,IDC_LABEL_FONT,CB_SETCURSEL,state->labelFont,0);
        for(auto label:{"Black (0,0,0)","Blue (0,0,255)","Red (255,0,0)","Magenta (255,0,255)"}) SendDlgItemMessageA(h,IDC_LABEL_COLOR,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendDlgItemMessage(h,IDC_LABEL_COLOR,CB_SETCURSEL,int(state->labelColor),0);
        PutNumber(h,IDC_LABEL_WEIGHT,state->labelStrokeWidth);
        for(auto label:{"Group with each polygon","Separate labels layer"}) SendDlgItemMessageA(h,IDC_LABEL_PLACEMENT,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendDlgItemMessage(h,IDC_LABEL_PLACEMENT,CB_SETCURSEL,state->separateLabelLayer?1:0,0);
        CenterDialog(h);return TRUE;
    }
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(w)==IDCANCEL)) {EndDialog(h,IDCANCEL);return TRUE;}
    if(message==WM_COMMAND&&LOWORD(w)==IDOK&&state) try {
        double scale=nestai::PointsPerUnit(state->units);
        state->labelHeight=Number(h,IDC_LABEL_HEIGHT,0.1*72/25.4/scale,10000/scale)*scale;
        int font=int(SendDlgItemMessage(h,IDC_LABEL_FONT,CB_GETCURSEL,0,0));
        if(font<0||font>1) throw std::runtime_error("Choose the regular or bold VDA font.");
        int color=int(SendDlgItemMessage(h,IDC_LABEL_COLOR,CB_GETCURSEL,0,0));
        int placement=int(SendDlgItemMessage(h,IDC_LABEL_PLACEMENT,CB_GETCURSEL,0,0));
        if(color<0||color>3||placement<0||placement>1) throw std::runtime_error("Choose a label color and placement.");
        state->labelStrokeWidth=Number(h,IDC_LABEL_WEIGHT,0.01,100);
        state->labelColor=nestai::LabelColor(color);state->separateLabelLayer=placement==1;
        state->labelFont=font;EndDialog(h,IDOK);return TRUE;
    }catch(const std::exception& e) {MessageBoxA(h,e.what(),"Label settings",MB_OK|MB_ICONEXCLAMATION);return TRUE;}
    return FALSE;
}
void ShowUnits(HWND h,nestai::Units units) {
    for(int i=0;i<int(std::size(nestai::UnitChoices));++i) if(nestai::UnitChoices[i]==units) SendDlgItemMessage(h,IDC_UNITS,CB_SETCURSEL,i,0);
    std::string suffix=std::string(" (")+nestai::UnitSymbol(units)+")";
    SetDlgItemTextA(h,IDC_WIDTH_LABEL,(std::string("Sheet width")+suffix).c_str());
    SetDlgItemTextA(h,IDC_HEIGHT_LABEL,(std::string("Sheet height")+suffix).c_str());
    SetDlgItemTextA(h,IDC_GAP_LABEL,(std::string("Contour spacing")+suffix).c_str());
    SetDlgItemTextA(h,IDC_MARGIN_LABEL,(std::string("Sheet edge margin")+suffix).c_str());
}
void SheetFields(HWND h,SettingsState* state) {
    bool custom=IsDlgButtonChecked(h,IDC_CUSTOM_SHEET)==BST_CHECKED;
    auto size=custom?nestai::SheetSize(state->saved->customSheet):nestai::Point{state->rectangleWidth,state->rectangleHeight};
    double scale=nestai::PointsPerUnit(state->units);
    PutNumber(h,IDC_WIDTH,size.x/scale);PutNumber(h,IDC_HEIGHT,size.y/scale);
    EnableWindow(GetDlgItem(h,IDC_WIDTH),!custom);EnableWindow(GetDlgItem(h,IDC_HEIGHT),!custom);
}
INT_PTR CALLBACK SettingsDialog(HWND h,UINT message,WPARAM w,LPARAM l) {
    auto state=reinterpret_cast<SettingsState*>(GetWindowLongPtr(h,DWLP_USER));
    if(message==WM_INITDIALOG) {
        state=reinterpret_cast<SettingsState*>(l);SetWindowLongPtr(h,DWLP_USER,l);auto s=state->saved;
        for(auto unit:nestai::UnitChoices) SendDlgItemMessageA(h,IDC_UNITS,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(nestai::UnitName(unit)));
        double scale=nestai::PointsPerUnit(state->units);ShowUnits(h,state->units);
        PutNumber(h,IDC_WIDTH,s->width/scale);PutNumber(h,IDC_HEIGHT,s->height/scale);
        PutNumber(h,IDC_GAP,s->gap/scale);PutNumber(h,IDC_MARGIN,s->margin/scale);PutNumber(h,IDC_SECONDS,s->seconds);
        for(auto label:{"Narrow strip","Compact area","Tight fit"}) SendDlgItemMessageA(h,IDC_PACKING_STYLE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        int style=s->packingStyle==nestai::PackingStyle::NarrowStrip?0:s->packingStyle==nestai::PackingStyle::CompactArea?1:2;
        SendDlgItemMessage(h,IDC_PACKING_STYLE,CB_SETCURSEL,style,0);
        CheckDlgButton(h,IDC_CENTER_LAYOUTS,s->centerLayouts?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(h,IDC_MULTIPLE_SHEETS,s->multipleSheets?BST_CHECKED:BST_UNCHECKED);
        SetDlgItemInt(h,IDC_MAX_SHEETS,s->maxSheets,FALSE);
        EnableWindow(GetDlgItem(h,IDC_MAX_SHEETS),s->multipleSheets);
        EnableWindow(GetDlgItem(h,IDC_MAX_SHEETS_LABEL),s->multipleSheets);
        CheckDlgButton(h,IDC_ADD_LABELS,s->addLabels?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(h,IDC_INCLUDE_BOUNDARY,s->includeSheetBoundary?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(h,IDC_LABEL_BOUNDARY,s->labelSheetBoundary?BST_CHECKED:BST_UNCHECKED);
        EnableWindow(GetDlgItem(h,IDC_LABEL_BOUNDARY),s->includeSheetBoundary);
        CheckDlgButton(h,IDC_CUSTOM_SHEET,s->useCustomSheet?BST_CHECKED:BST_UNCHECKED);
        EnableWindow(GetDlgItem(h,IDC_CUSTOM_SHEET),!s->customSheet.segments.empty());
        SheetFields(h,state);
        int selected=0;
        for(int i=0;i<int(std::size(nestai::OrientationChoices));++i) {
            auto choice=nestai::OrientationChoices[i];
            SendDlgItemMessageA(h,IDC_ROTATIONS,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(choice.label));
            if(choice.count==s->rotations) selected=i;
        }
        SendDlgItemMessage(h,IDC_ROTATIONS,CB_SETCURSEL,selected,0);CenterDialog(h);return TRUE;
    }
    if(message==WM_COMMAND&&LOWORD(w)==IDC_INCLUDE_BOUNDARY) {
        EnableWindow(GetDlgItem(h,IDC_LABEL_BOUNDARY),IsDlgButtonChecked(h,IDC_INCLUDE_BOUNDARY)==BST_CHECKED);return TRUE;
    }
    if(message==WM_COMMAND&&LOWORD(w)==IDC_CUSTOM_SHEET&&state) {
        // Preserve edited rectangle dimensions while switching sheet sources.
        if(IsDlgButtonChecked(h,IDC_CUSTOM_SHEET)==BST_CHECKED) try {
            double scale=nestai::PointsPerUnit(state->units);
            state->rectangleWidth=Number(h,IDC_WIDTH,0,100000/scale)*scale;
            state->rectangleHeight=Number(h,IDC_HEIGHT,0,100000/scale)*scale;
        }catch(const std::exception& e) {
            CheckDlgButton(h,IDC_CUSTOM_SHEET,BST_UNCHECKED);
            MessageBoxA(h,e.what(),"Sheet dimensions",MB_OK|MB_ICONEXCLAMATION);return TRUE;
        }
        SheetFields(h,state);return TRUE;
    }
    if(message==WM_COMMAND&&LOWORD(w)==IDC_LABEL_OPTIONS&&state) {
        auto options=*state;
        if(DialogBoxParamW(Module(),MAKEINTRESOURCEW(IDD_LABELS),h,LabelDialog,reinterpret_cast<LPARAM>(&options))==IDOK) {
            state->labelHeight=options.labelHeight;state->labelFont=options.labelFont;
            state->labelColor=options.labelColor;state->labelStrokeWidth=options.labelStrokeWidth;state->separateLabelLayer=options.separateLabelLayer;
        }
        return TRUE;
    }
    if(message==WM_COMMAND&&LOWORD(w)==IDC_MULTIPLE_SHEETS) {
        bool enabled=IsDlgButtonChecked(h,IDC_MULTIPLE_SHEETS)==BST_CHECKED;
        EnableWindow(GetDlgItem(h,IDC_MAX_SHEETS),enabled);EnableWindow(GetDlgItem(h,IDC_MAX_SHEETS_LABEL),enabled);return TRUE;
    }
    if(message==WM_COMMAND&&state&&LOWORD(w)==IDC_UNITS&&HIWORD(w)==CBN_SELCHANGE) {
        int choice=int(SendDlgItemMessage(h,IDC_UNITS,CB_GETCURSEL,0,0));
        if(choice<0||choice>=int(std::size(nestai::UnitChoices))) return TRUE;
        auto next=nestai::UnitChoices[choice];
        if(next!=state->units) {
            try {
                int fields[]={IDC_WIDTH,IDC_HEIGHT,IDC_GAP,IDC_MARGIN};double values[4];
                for(int i=0;i<4;++i) values[i]=nestai::ConvertUnits(Number(h,fields[i],0,100000/nestai::PointsPerUnit(state->units)),state->units,next);
                for(int i=0;i<4;++i) PutNumber(h,fields[i],values[i]);
                state->units=next;ShowUnits(h,next);
            }catch(const std::exception& e) {ShowUnits(h,state->units);MessageBoxA(h,e.what(),"Nesting units",MB_OK|MB_ICONEXCLAMATION);}
        }
        return TRUE;
    }
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(w)==IDCANCEL)) {EndDialog(h,IDCANCEL);return TRUE;}
    if(message==WM_COMMAND&&LOWORD(w)==IDOK&&state) {
        try {
            nestai::Settings next;next.units=state->units;double scale=nestai::PointsPerUnit(next.units);
            double mm=72/25.4;
            next.width=Number(h,IDC_WIDTH,mm/scale,35000*mm/scale)*scale;next.height=Number(h,IDC_HEIGHT,mm/scale,35000*mm/scale)*scale;
            next.gap=Number(h,IDC_GAP,0,3000*mm/scale)*scale;next.margin=Number(h,IDC_MARGIN,0,3000*mm/scale)*scale;
            next.seconds=Number(h,IDC_SECONDS,0.1,300);
            int style=int(SendDlgItemMessage(h,IDC_PACKING_STYLE,CB_GETCURSEL,0,0));
            if(style<0||style>2) throw std::runtime_error("Choose a packing style.");
            nestai::PackingStyle styles[]={nestai::PackingStyle::NarrowStrip,nestai::PackingStyle::CompactArea,nestai::PackingStyle::TightFit};
            next.packingStyle=styles[style];
            next.centerLayouts=IsDlgButtonChecked(h,IDC_CENTER_LAYOUTS)==BST_CHECKED;

            next.multipleSheets=IsDlgButtonChecked(h,IDC_MULTIPLE_SHEETS)==BST_CHECKED;
            double maxSheets=next.multipleSheets?Number(h,IDC_MAX_SHEETS,1,500):state->saved->maxSheets;
            if(maxSheets!=std::floor(maxSheets)) throw std::runtime_error("Maximum output artboards must be a whole number from 1 to 500.");
            next.maxSheets=int(maxSheets);
            next.addLabels=IsDlgButtonChecked(h,IDC_ADD_LABELS)==BST_CHECKED;
            next.includeSheetBoundary=IsDlgButtonChecked(h,IDC_INCLUDE_BOUNDARY)==BST_CHECKED;
            next.labelSheetBoundary=IsDlgButtonChecked(h,IDC_LABEL_BOUNDARY)==BST_CHECKED;
            next.customSheet=state->saved->customSheet;
            next.useCustomSheet=IsDlgButtonChecked(h,IDC_CUSTOM_SHEET)==BST_CHECKED;
            if(next.useCustomSheet&&next.customSheet.segments.empty()) throw std::runtime_error("Capture a sheet boundary first.");
            next.labelHeight=state->labelHeight;next.labelFont=state->labelFont;
            next.labelColor=state->labelColor;next.labelStrokeWidth=state->labelStrokeWidth;next.separateLabelLayer=state->separateLabelLayer;
            int selected=int(SendDlgItemMessage(h,IDC_ROTATIONS,CB_GETCURSEL,0,0));
            if(selected<0||selected>=int(std::size(nestai::OrientationChoices))) throw std::runtime_error("Choose permitted orientations.");
            next.rotations=nestai::OrientationChoices[selected].count;
            if(2*(next.margin+0.1)>=std::min(next.width,next.height)) throw std::runtime_error("The edge margin leaves no usable sheet area.");
            *state->saved=next;EndDialog(h,IDOK);
        }catch(const std::exception& e) {MessageBoxA(h,e.what(),"Nesting settings",MB_OK|MB_ICONEXCLAMATION);}
        return TRUE;
    }
    return FALSE;
}
struct LabelBatch {
    std::vector<nestai::Label> labels;int unnamed=0,unsupported=0,noRoom=0,labeled=0;
    std::string Summary() const {return "Labels:\n- "+std::to_string(labeled)+" labels added\n- "+std::to_string(unnamed)+" unnamed\n- "+std::to_string(noRoom)+" too small\n- "+std::to_string(unsupported)+" unsupported names skipped";}
};
LabelBatch MakeLabels(nestai::Engine& engine,const std::vector<nestai::Part>& parts,const std::vector<std::string>& names,
                      const std::vector<int>& indices,const nestai::Settings& settings,std::atomic<bool>& stop,std::atomic<int>& finished) {
    LabelBatch batch;batch.labels.resize(parts.size());
    for(int index:indices) {
        if(stop) break;
        const auto& name=names[index];
        if(name.find_first_not_of(' ')==std::string::npos) ++batch.unnamed;
        else {
            bool supported=name.size()<=128;for(unsigned char c:name) if(c<32||c>126) supported=false;
            if(!supported) ++batch.unsupported;
            else {
                batch.labels[index]=engine.LabelInside(parts[index],name,settings.labelHeight,settings.labelFont,settings.labelStrokeWidth);
                if(batch.labels[index].strokes.empty()) ++batch.noRoom;else ++batch.labeled;
            }
        }
        ++finished;
    }
    return batch;
}
std::unique_ptr<nestai::Engine> MakeEngine() {
    std::vector<WCHAR> path(32768);DWORD length=GetModuleFileNameW(Module(),path.data(),32768);
    if(!length||length>=32768) throw std::runtime_error("Cannot resolve the plugin folder.");
    auto slash=wcsrchr(path.data(),L'\\');if(!slash) throw std::runtime_error("Cannot resolve the engine path.");
    wcscpy_s(slash+1,32768-(slash-path.data())-1,L"nfp_nest.dll");return std::make_unique<nestai::Engine>(path.data());
}
struct Task {
    nestai::Engine& engine;const std::vector<nestai::Part>& parts;nestai::Settings settings;
    const std::vector<std::string>& names;bool labelsOnly=false;LabelBatch labels;std::atomic<bool> stop{false};
    std::vector<nestai::Placement> result;std::map<int,nestai::Label> sheetLabels;std::string error;std::atomic<bool> done{false};bool canceled=false;int centeringSkipped=0;
    std::thread worker;
    std::chrono::steady_clock::time_point started;
    std::atomic<int> phase{0},labelsFinished{0},labelsTotal{0};std::atomic<double> labelStart{0};
    Task(nestai::Engine& e,const std::vector<nestai::Part>& p,nestai::Settings s,const std::vector<std::string>& n,bool only=false):engine(e),parts(p),settings(s),names(n),labelsOnly(only){}
    ~Task() {if(worker.joinable()) worker.join();}
};
std::string Duration(double seconds) {
    int value=int(std::ceil(std::max(0.0,seconds)));
    std::ostringstream text;text<<value/60<<":"<<std::setw(2)<<std::setfill('0')<<value%60;return text.str();
}
void ProgressText(HWND h,Task* task) {
    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-task->started).count();
    SetDlgItemTextA(h,IDC_ELAPSED,("Elapsed: "+Duration(std::floor(elapsed))).c_str());
    if(task->canceled) {SetDlgItemTextA(h,IDC_REMAINING,"Estimated remaining: canceling...");return;}
    std::string status,remaining;
    if(task->phase.load()==1) {
        int finished=task->labelsFinished.load(),total=task->labelsTotal.load();
        status="Generating labels: "+std::to_string(finished)+" of "+std::to_string(total);
        double seconds=elapsed-task->labelStart.load();
        remaining=finished>0?"Estimated remaining: ~"+Duration(seconds*std::max(0,total-finished)/finished):"Estimated remaining: estimating...";
    } else {
        status=elapsed<task->settings.seconds?"Calculating layout... generation "+std::to_string(task->engine.Progress()):"Finishing layout and validation...";
        remaining=elapsed<task->settings.seconds?"Estimated remaining: ~"+Duration(task->settings.seconds-elapsed)+" + finishing":"Estimated remaining: finishing...";
    }
    SetDlgItemTextA(h,IDC_STATUS,status.c_str());SetDlgItemTextA(h,IDC_REMAINING,remaining.c_str());
}
INT_PTR CALLBACK ProgressDialog(HWND h,UINT message,WPARAM w,LPARAM l) {
    auto task=reinterpret_cast<Task*>(GetWindowLongPtr(h,DWLP_USER));
    if(message==WM_INITDIALOG) {
        task=reinterpret_cast<Task*>(l);SetWindowLongPtr(h,DWLP_USER,l);
        task->started=std::chrono::steady_clock::now();
        if(task->labelsOnly) {
            SetWindowTextA(h,"FreeNest - Labeling");
            SetDlgItemTextA(h,IDC_ESTIMATE_NOTE,"Remaining time is estimated from label progress.");
            task->phase=1;
        }
        ProgressText(h,task);CenterDialog(h);
        try {task->worker=std::thread([task] {
            try {
                std::vector<int> indices;
                if(task->labelsOnly) {for(size_t i=0;i<task->parts.size();++i) indices.push_back(int(i));}
                else {
                    task->engine.ReserveSheetLabel(task->settings);
                    task->result=task->engine.Solve(task->parts,task->settings);
                    task->centeringSkipped=nestai::CenterLayouts(task->result,task->parts,task->settings);
                    for(auto p:task->result) indices.push_back(p.part);
                }
                bool sheetLabels=!task->labelsOnly&&task->settings.includeSheetBoundary&&task->settings.labelSheetBoundary;
                std::map<int,int> sheetNumbers;
                if(sheetLabels) for(auto placement:task->result) sheetNumbers.emplace(placement.sheet,0);
                if(task->labelsOnly||task->settings.addLabels||sheetLabels) {
                    task->labelsTotal=int((task->labelsOnly||task->settings.addLabels?indices.size():0)+sheetNumbers.size());
                    task->labelStart=std::chrono::duration<double>(std::chrono::steady_clock::now()-task->started).count();task->phase=1;
                    if(task->labelsOnly||task->settings.addLabels)
                        task->labels=MakeLabels(task->engine,task->parts,task->names,indices,task->settings,task->stop,task->labelsFinished);
                    int number=0;
                    for(auto sheet:sheetNumbers) {
                        if(task->stop) break;
                        task->sheetLabels.emplace(sheet.first,task->engine.SheetLabel("FreeNest Sheet "+std::to_string(++number),task->settings));
                        ++task->labelsFinished;
                    }
                }
            }
            catch(const std::exception& e) {task->error=e.what();}
            catch(...) {task->error="Unexpected nesting engine error.";}
            task->done=true;
        });}catch(const std::exception& e) {task->error=e.what();task->done=true;EndDialog(h,IDOK);return TRUE;}
        SetTimer(h,1,100,nullptr);return TRUE;
    }
    if(!task) return FALSE;
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(w)==IDCANCEL)) {
        task->canceled=true;task->stop=true;task->engine.Cancel();EnableWindow(GetDlgItem(h,IDCANCEL),FALSE);
        SetDlgItemTextA(h,IDC_STATUS,"Canceling calculation...");return TRUE;
    }
    if(message==WM_TIMER) {
        if(task->canceled&&!task->done) task->engine.Cancel(); // Covers cancellation during solver initialization.
        if(task->done) {KillTimer(h,1);task->worker.join();EndDialog(h,task->canceled?IDCANCEL:IDOK);}
        else ProgressText(h,task);
        return TRUE;
    }
    return FALSE;
}
struct ArtList {
    AIArtHandle** items=nullptr;
    ~ArtList() {if(items) sAIMdMemory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(items));}
};
nestai::Point Point(AIRealPoint p) {return {p.h,p.v};}
AIRealPoint Point(nestai::Point p) {AIRealPoint out;out.h=p.x;out.v=p.y;return out;}
std::vector<AIArtHandle> PathContours(AIArtHandle art) {
    short type=0;Check(sAIArt->GetArtType(art,&type));
    if(type==kPathArt) return {art};
    if(type!=kCompoundPathArt) throw std::runtime_error("Select closed paths or compound paths with holes.");
    std::vector<AIArtHandle> paths;AIArtHandle child=nullptr;Check(sAIArt->GetArtFirstChild(art,&child));
    while(child) {
        Check(sAIArt->GetArtType(child,&type));
        if(type!=kPathArt||paths.size()>=500) throw std::runtime_error("Unsupported compound path structure.");
        paths.push_back(child);Check(sAIArt->GetArtSibling(child,&child));
    }
    if(paths.empty()) throw std::runtime_error("The compound path has no contours.");
    return paths;
}
AIRealMatrix RigidTransform(nestai::Point origin,nestai::Point destination,double angle) {
    auto translation=nestai::Place({0,0},origin,destination,angle);
    auto xAxis=nestai::Place({1,0},{0,0},{0,0},angle);
    auto yAxis=nestai::Place({0,1},{0,0},{0,0},angle);
    return {AIReal(xAxis.x),AIReal(xAxis.y),AIReal(yAxis.x),AIReal(yAxis.y),
            AIReal(translation.x),AIReal(translation.y)};
}
// Read the actual instance contour without inserting temporary artwork or editing the definition.
bool ReadSymbolContour(AIArtHandle instance,AIPatternHandle pattern,std::vector<nestai::Segment>& contour) {
    AIArtHandle art=nullptr;
    if(sAISymbol->GetSymbolPatternArt(pattern,&art)||!art) return false;
    auto hasExtendedAppearance=[](AIArtHandle object) {
        AIArtStyleHandle style=nullptr;
        if(sAIArtStyle->GetArtStyle(object,&style)) return true;
        if(!style) return false;
        return bool(sAIArtStyle->ExamineStyle(style,kStyleHasEffects|kStyleHasBrushes)&
                    (kStyleHasEffects|kStyleHasBrushes));
    };
    if(hasExtendedAppearance(instance)) return false;
    for(int depth=0;depth<32;++depth) {
        short type=0;ai::int32 attributes=0;
        if(sAIArt->GetArtType(art,&type)||sAIArt->GetArtUserAttr(art,kArtHidden|kArtIsClipMask,&attributes)||
           attributes||hasExtendedAppearance(art)) return false;
        if(type==kGroupArt) {
            AIArtHandle child=nullptr,sibling=nullptr;
            if(sAIArt->GetArtFirstChild(art,&child)||!child||
               sAIArt->GetArtSibling(child,&sibling)||sibling) return false;
            art=child;continue;
        }
        if(type!=kPathArt) return false;
        AIBoolean closed=false,guide=false,advanced=false;
        AIPathStyle style{};ai::int16 count=0;
        if(sAIPath->GetPathClosed(art,&closed)||!closed||sAIPath->GetPathGuide(art,&guide)||guide||
           sAIPathStyle->GetPathStyle(art,&style,&advanced)||(!style.fillPaint&&!style.strokePaint)||advanced||style.clip||
           sAIPath->GetPathSegmentCount(art,&count)||count<2||count>2000) return false;
        std::vector<AIPathSegment> segments(count);
        AIRealMatrix transform{};
        if(sAIPath->GetPathSegments(art,0,count,segments.data())||
           sAISymbol->GetSoftTransformOfSymbolArt(instance,&transform)) return false;
        auto point=[&](AIRealPoint p)->nestai::Point {
            return {double(transform.a)*p.h+double(transform.c)*p.v+transform.tx,
                    double(transform.b)*p.h+double(transform.d)*p.v+transform.ty};
        };
        for(const auto& p:segments) contour.push_back({point(p.p),point(p.in),point(p.out)});
        return true;
    }
    return false;
}
nestai::Part PrepareSymbol(AIArtHandle instance,AIPatternHandle pattern,AIRealRect bounds,bool& usesContour) {
    std::vector<nestai::Segment> contour;
    if(ReadSymbolContour(instance,pattern,contour)) {
        try {
            auto part=nestai::Prepare(contour);
            // The transformed definition must agree with Illustrator's measurements.
            auto size=nestai::SheetSize(part);
            constexpr double tolerance=0.11; // matches the existing curve flattening tolerance
            AIRealRect geometry{};
            if(!sAIArt->GetArtTransformBounds(instance,nullptr,kNoStrokeBounds|kExcludeGuideBounds,&geometry)&&
               std::abs(geometry.left-part.origin.x)<=tolerance&&
               std::abs(geometry.bottom-part.origin.y)<=tolerance&&
               std::abs(geometry.right-part.origin.x-size.x)<=tolerance&&
               std::abs(geometry.top-part.origin.y-size.y)<=tolerance) {
                usesContour=true;
                return part;
            }
        }catch(const std::runtime_error&) {
            // An unsupported definition remains packable using its visible rectangle.
        }
    }
    usesContour=false;
    return nestai::PrepareSymbolBounds(bounds.left,bounds.bottom,bounds.right,bounds.top);
}
void PlaceSymbol(AIArtHandle source,AIArtHandle copy,const nestai::Part& part,nestai::Point destination,double angle,bool usesContour) {
    AIPatternHandle originalPattern=nullptr,copyPattern=nullptr;
    Check(sAISymbol->GetSymbolPatternOfSymbolArt(source,&originalPattern));
    auto transform=RigidTransform(part.origin,destination,angle);
    Check(sAITransformArt->TransformArt(copy,&transform,1,kTransformObjects|kTransformEntireArtStyle));
    Check(sAISymbol->GetSymbolPatternOfSymbolArt(copy,&copyPattern));
    if(!originalPattern||copyPattern!=originalPattern)
        throw std::runtime_error("The nested copy did not retain its symbol definition.");
    if(usesContour) {
        std::vector<nestai::Segment> contour;
        if(!ReadSymbolContour(copy,copyPattern,contour)||
           !nestai::SymbolContourMatchesPlacement(part,contour,destination,angle))
            throw std::runtime_error("The nested symbol did not match its planned contour.");
        return;
    }
    // Compare the host's enclosing bounds with the placed footprint in document axes.
    // Asking the host to inverse-rotate the bounds can re-enclose an already measured
    // bounding rectangle, overstating the size of a diagonally rotated instance.
    AIRealRect actual{};
    Check(sAIArt->GetArtBounds(copy,&actual));
    if(!nestai::SymbolFitsPlacedBounds(part,destination,angle,
                                    {actual.left,actual.bottom},{actual.right,actual.top}))
        throw std::runtime_error("A symbol's appearance extends outside its reserved bounds after placement.");
}
void DrawLabel(const nestai::Label& label,const std::string& name,AIArtHandle parent,nestai::Point destination,double angle,const nestai::Settings& settings) {
    if(label.strokes.empty()) return;
    AIArtHandle group=nullptr;Check(sAIArt->NewArt(kGroupArt,kPlaceInsideOnTop,parent,&group));
    Check(sAIArt->SetArtName(group,ai::UnicodeString(("Label: "+name).c_str())));
    for(const auto& stroke:label.strokes) {
        if(stroke.size()<2||stroke.size()>32767) throw std::runtime_error("Unsupported label stroke size.");
        AIArtHandle path=nullptr;Check(sAIArt->NewArt(kPathArt,kPlaceInsideOnTop,group,&path));
        std::vector<AIPathSegment> segments(stroke.size());
        for(size_t i=0;i<stroke.size();++i) {
            auto p=nestai::Place(stroke[i],{0,0},destination,angle);
            segments[i].p=segments[i].in=segments[i].out=Point(p);segments[i].corner=true;
        }
        Check(sAIPath->SetPathSegmentCount(path,ai::int16(segments.size())));
        Check(sAIPath->SetPathSegments(path,0,ai::int16(segments.size()),segments.data()));Check(sAIPath->SetPathClosed(path,false));
        AIPathStyle style;AIBoolean advanced=false;Check(sAIPathStyle->GetPathStyle(path,&style,&advanced));
        style.fillPaint=false;style.strokePaint=true;style.stroke.width=settings.labelStrokeWidth;style.stroke.dash.length=0;
        auto color=nestai::LabelColorRGB(settings.labelColor);
        style.stroke.color.kind=kThreeColor;style.stroke.color.c.rgb.red=color.red/255.0;style.stroke.color.c.rgb.green=color.green/255.0;style.stroke.color.c.rgb.blue=color.blue/255.0;
        Check(sAIPathStyle->SetPathStyle(path,&style));Check(sAIArt->SetArtUserAttr(path,kArtSelected,0));
    }
}
}

class NestPlugin:public Plugin {
    AIMenuItemHandle rootMenu=nullptr,menu=nullptr,labelMenu=nullptr,sheetMenu=nullptr,panelMenu=nullptr,aboutMenu=nullptr;nestai::Settings settings;
    NestPanel commandPanel;
    bool commandRunning=false;
    struct CommandGuard {
        NestPlugin& plugin;
        explicit CommandGuard(NestPlugin& p):plugin(p) {plugin.commandRunning=true;plugin.commandPanel.SetBusy(true);}
        ~CommandGuard() {plugin.commandRunning=false;plugin.commandPanel.SetBusy(false);}
    };
    void CreateCommandPanel() {
        commandPanel.Create(GetPluginRef(),[this](int id){RunCommand(id==1?menu:id==2?labelMenu:sheetMenu);});
    }
public:
    explicit NestPlugin(SPPluginRef ref):Plugin(ref) {strncpy(fPluginName,"FreeNest",kMaxStringLength);}
    FIXUP_VTABLE_EX(NestPlugin,Plugin);
    ASErr StartupPlugin(SPInterfaceMessage* message) override {
        auto error=Plugin::StartupPlugin(message);if(error) return error;
        AIPlatformAddMenuItemDataUS data;data.groupName=kObjectUtilsMenuGroup;
        data.itemText=ai::UnicodeString("FreeNest");
        error=sAIMenu->AddMenuItem(message->d.self,"FreeNestRoot",&data,kMenuItemAlwaysEnabled,&rootMenu);if(error) return error;
        AIMenuGroup submenu=nullptr;
        error=sAIMenu->AddMenuGroupAsSubMenu("FreeNestSubmenu",kMenuGroupNoOptions,rootMenu,&submenu);if(error) return error;
        data.groupName="FreeNestSubmenu";
        data.itemText=ai::UnicodeString("Nest Selected Paths");
        error=sAIMenu->AddMenuItem(message->d.self,"FreeNestCommand",&data,0,&menu);if(error) return error;
        data.itemText=ai::UnicodeString("Label Selected Paths");
        error=sAIMenu->AddMenuItem(message->d.self,"FreeNestLabelCommand",&data,0,&labelMenu);if(error) return error;
        data.itemText=ai::UnicodeString("Use Selected Path as Sheet");
        error=sAIMenu->AddMenuItem(message->d.self,"FreeNestSheetCommand",&data,0,&sheetMenu);if(error) return error;
        data.itemText=ai::UnicodeString("About FreeNest");
        error=sAIMenu->AddMenuItem(message->d.self,"FreeNestAboutCommand",&data,kMenuItemAlwaysEnabled,&aboutMenu);if(error) return error;
        data.groupName=kOtherPalettesMenuGroup;data.itemText=ai::UnicodeString("FreeNest");
        return sAIMenu->AddMenuItem(message->d.self,"FreeNestPanelCommand",&data,kMenuItemAlwaysEnabled|kMenuItemWantsUpdateOption,&panelMenu);
    }
    ASErr PostStartupPlugin() override {
        auto error=Plugin::PostStartupPlugin();if(error) return error;
        try {CreateCommandPanel();}catch(const std::exception& e) {MessageBoxA(GetActiveWindow(),e.what(),"FreeNest panel",MB_OK|MB_ICONEXCLAMATION);}
        return kNoErr;
    }
    ASErr PreShutdownPlugin() override {commandPanel.Destroy();return Plugin::PreShutdownPlugin();}
    ASErr ShutdownPlugin(SPInterfaceMessage* message) override {commandPanel.Destroy();return Plugin::ShutdownPlugin(message);}
    ASErr UpdateMenuItem(AIMenuMessage* message) override {
        if(message->menuItem==panelMenu) return sAIMenu->CheckItem(panelMenu,commandPanel.Visible());
        return Plugin::UpdateMenuItem(message);
    }
    ASErr GoMenuItem(AIMenuMessage* message) override {
        if(message->menuItem==panelMenu) {
            try {CreateCommandPanel();commandPanel.Toggle();}catch(const std::exception& e) {MessageBoxA(GetActiveWindow(),e.what(),"FreeNest panel",MB_OK|MB_ICONEXCLAMATION);}
            return kNoErr;
        }
        return RunCommand(message->menuItem);
    }
    ASErr RunCommand(AIMenuItemHandle command) {
        if(command!=menu&&command!=labelMenu&&command!=sheetMenu&&command!=aboutMenu) return kNoErr;
        if(commandRunning) return kNoErr;
        CommandGuard guard(*this);
        if(command==aboutMenu) {
            HWND owner=GetAncestor(GetActiveWindow(),GA_ROOTOWNER);if(!owner) owner=GetActiveWindow();
            if(DialogBoxParamW(Module(),MAKEINTRESOURCEW(IDD_ABOUT),owner,AboutDialog,0)==-1)
                MessageBoxW(owner,L"FreeNest could not open the About dialog.",L"FreeNest",MB_OK|MB_ICONEXCLAMATION);
            return kNoErr;
        }
        bool labelsOnly=command==labelMenu;
        AILayerHandle output=nullptr,previous=nullptr,labelLayer=nullptr;
        bool regroupingSources=false;
        std::unique_ptr<ai::ArtboardList> artboards;
        std::vector<ai::ArtboardID> addedArtboards;
        ai::ArtboardID previousArtboard=-1;
        HWND owner=GetAncestor(GetActiveWindow(),GA_ROOTOWNER);if(!owner) owner=GetActiveWindow();
        try {
            settings.units=DocumentUnits();
            ArtList selected;ai::int32 count=0;Check(sAIMatchingArt->GetSelectedArt(&selected.items,&count));
            std::vector<nestai::SelectedKind> selectedKinds;std::vector<AIArtHandle> sources;
            for(int i=0;i<count;++i) {
                AIArtHandle source=(*selected.items)[i],parent=nullptr;short type=0,parentType=0;
                Check(sAIArt->GetArtType(source,&type));Check(sAIArt->GetArtParent(source,&parent));
                if(parent) Check(sAIArt->GetArtType(parent,&parentType));
                // Illustrator may return both a compound root and its selected child contours.
                if(type==kPathArt&&parentType==kCompoundPathArt) {source=parent;type=kCompoundPathArt;}
                if(std::find(sources.begin(),sources.end(),source)!=sources.end()) continue;
                ASBoolean layerGroup=false;
                if(type==kGroupArt) Check(sAIArt->IsArtLayerGroup(source,&layerGroup));
                sources.push_back(source);
                selectedKinds.push_back(layerGroup?nestai::SelectedKind::LayerContainer:
                    type==kPathArt?nestai::SelectedKind::Path:type==kCompoundPathArt?nestai::SelectedKind::CompoundPath:
                    type==kSymbolArt?nestai::SelectedKind::Symbol:nestai::SelectedKind::Unsupported);
            }
            auto selectedPaths=nestai::SelectedPathIndices(selectedKinds);count=ai::int32(selectedPaths.size());
            if(command==sheetMenu&&(count!=1||selectedKinds[selectedPaths[0]]!=nestai::SelectedKind::Path))
                throw std::runtime_error("Select exactly one simple closed path as the sheet boundary. Sheet holes are not supported yet.");
            std::vector<AIArtHandle> art;std::vector<nestai::Part> parts;
            std::vector<std::vector<std::vector<AIPathSegment>>> originals;
            std::vector<nestai::OutputRect> sourceBounds;
            std::vector<std::string> names;std::vector<bool> symbols,symbolContours;double right=-1e100;size_t total=0;
            for(int i=0;i<count;++i) {
                AIArtHandle source=sources[selectedPaths[i]];
                const bool symbol=selectedKinds[selectedPaths[i]]==nestai::SelectedKind::Symbol;
                ai::int32 attributes=0;Check(sAIArt->GetArtUserAttr(source,kArtLocked|kArtHidden,&attributes));
                if(attributes) throw std::runtime_error("Locked or hidden objects cannot be processed.");
                AIRealRect bounds;Check(sAIArt->GetArtBounds(source,&bounds));right=std::max(right,double(bounds.right));
                sourceBounds.push_back({bounds.left,bounds.bottom,bounds.right,bounds.top});
                ai::UnicodeString name;ASBoolean defaultName=true;Check(sAIArt->GetArtName(source,name,&defaultName));
                symbols.push_back(symbol);
                bool usesContour=false;
                if(symbol) {
                    AIPatternHandle pattern=nullptr;
                    Check(sAISymbol->GetSymbolPatternOfSymbolArt(source,&pattern));
                    if(!pattern) throw std::runtime_error("A selected symbol instance has no definition.");
                    auto part=PrepareSymbol(source,pattern,bounds,usesContour);
                    total+=part.outline.size();
                    if(total>100000) throw std::runtime_error("Selection exceeds 100,000 flattened vertices.");
                    if(defaultName||name.empty())
                        Check(sAISymbol->GetSymbolPatternName(pattern,name));
                    names.push_back(name.as_UTF8());
                    parts.push_back(std::move(part));symbolContours.push_back(usesContour);
                    art.push_back(source);originals.emplace_back();continue;
                }
                symbolContours.push_back(false);
                AIPathStyle style;AIBoolean advanced=false;Check(sAIPathStyle->GetPathStyle(source,&style,&advanced));
                if(style.clip) throw std::runtime_error("Clipping paths cannot be processed.");
                std::vector<std::vector<AIPathSegment>> sourceRings;std::vector<std::vector<nestai::Segment>> geometry;
                for(auto path:PathContours(source)) {
                    AIBoolean closed=false,guide=false;Check(sAIPath->GetPathClosed(path,&closed));Check(sAIPath->GetPathGuide(path,&guide));
                    Check(sAIArt->GetArtUserAttr(path,kArtLocked|kArtHidden,&attributes));
                    if(!closed||guide||attributes) throw std::runtime_error("Every contour must be closed, unlocked and visible, and cannot be a guide.");
                    ai::int16 n=0;Check(sAIPath->GetPathSegmentCount(path,&n));
                    if(n<2||n>2000) throw std::runtime_error("Unsupported path point count.");
                    std::vector<AIPathSegment> segments(n);Check(sAIPath->GetPathSegments(path,0,n,segments.data()));
                    std::vector<nestai::Segment> ring;
                    for(auto segment:segments) ring.push_back({Point(segment.p),Point(segment.in),Point(segment.out)});
                    total+=nestai::Prepare(ring).outline.size();
                    if(total>100000) throw std::runtime_error("Selection exceeds 100,000 flattened vertices.");
                    geometry.push_back(std::move(ring));sourceRings.push_back(std::move(segments));
                }
                auto part=nestai::PrepareCompound(geometry,style.evenodd);
                names.push_back(defaultName?std::string{}:name.as_UTF8());
                parts.push_back(std::move(part));art.push_back(source);originals.push_back(std::move(sourceRings));
            }
            if(command==sheetMenu) {
                settings.customSheet=parts.front();settings.useCustomSheet=true;
                MessageBoxA(owner,"Sheet boundary captured. Deselect it, select the parts, then choose Object > FreeNest > Nest Selected Paths.\n\nThe captured shape is a snapshot. Capture it again after editing the source boundary.","FreeNest - Sheet Boundary",MB_OK);
                return kNoErr;
            }
            if(labelsOnly) {
                SettingsState options{&settings,settings.units,settings.labelHeight,settings.labelFont};
                options.executeLabels=true;
                auto dialog=DialogBoxParamW(Module(),MAKEINTRESOURCEW(IDD_LABELS),owner,LabelDialog,reinterpret_cast<LPARAM>(&options));
                if(dialog==-1) throw std::runtime_error("Cannot open label settings.");if(dialog!=IDOK) return kNoErr;
                settings.units=options.units;settings.labelHeight=options.labelHeight;settings.labelFont=options.labelFont;
                settings.labelColor=options.labelColor;settings.labelStrokeWidth=options.labelStrokeWidth;settings.separateLabelLayer=options.separateLabelLayer;
                auto engine=MakeEngine();Task task(*engine,parts,settings,names,true);
                dialog=DialogBoxParamW(Module(),MAKEINTRESOURCEW(IDD_PROGRESS),owner,ProgressDialog,reinterpret_cast<LPARAM>(&task));
                if(dialog==-1) throw std::runtime_error("Cannot open label progress.");if(dialog!=IDOK) return kNoErr;
                if(!task.error.empty()) throw std::runtime_error(task.error);
                if(!task.labels.labeled) throw std::runtime_error(task.labels.Summary()+"\nName the artwork in Illustrator's Layers panel before labeling.");
                Check(sAILayer->GetCurrentLayer(&previous));
                Check(sAIUndo->SetUndoTextUS(ai::UnicodeString("Undo Label Selected Paths"),ai::UnicodeString("Redo Label Selected Paths")));
                if(settings.separateLabelLayer) {
                    Check(sAILayer->InsertLayer(nullptr,kPlaceAboveAll,&output));Check(sAILayer->SetLayerTitle(output,ai::UnicodeString("FreeNest Labels")));
                    Check(sAILayer->SetCurrentLayer(output));AIArtHandle group=nullptr;Check(sAIArt->GetFirstArtOfLayer(output,&group));
                    for(size_t i=0;i<parts.size();++i) DrawLabel(task.labels.labels[i],names[i],group,parts[i].origin,0,settings);
                } else {
                    regroupingSources=true;
                    for(size_t i=0;i<parts.size();++i) if(!task.labels.labels[i].strokes.empty()) {
                        AIArtHandle group=nullptr;Check(sAIArt->NewArt(kGroupArt,kPlaceAbove,art[i],&group));
                        Check(sAIArt->SetArtName(group,ai::UnicodeString(names[i].c_str())));
                        Check(sAIArt->ReorderArt(art[i],kPlaceInsideOnTop,group));
                        DrawLabel(task.labels.labels[i],names[i],group,parts[i].origin,0,settings);
                    }
                }
                Check(sAILayer->SetCurrentLayer(previous));output=nullptr;regroupingSources=false;
                MessageBoxA(owner,task.labels.Summary().c_str(),"Labeling complete",MB_OK);return kNoErr;
            }
            artboards=std::make_unique<ai::ArtboardList>();
            if(!artboards->IsValid()) throw std::runtime_error("Cannot read the document's artboards.");
            Check(artboards->GetActive(previousArtboard));
            auto active=artboards->GetArtboardProperties(previousArtboard);AIRealRect activeBounds;
            Check(active.GetPosition(activeBounds));
            settings.width=activeBounds.right-activeBounds.left;settings.height=activeBounds.top-activeBounds.bottom;
            SettingsState dialogSettings{&settings,settings.units,settings.labelHeight,settings.labelFont};
            auto dialog=DialogBoxParamW(Module(),MAKEINTRESOURCEW(IDD_SETTINGS),owner,SettingsDialog,reinterpret_cast<LPARAM>(&dialogSettings));
            if(dialog==-1) throw std::runtime_error("Could not open nesting settings.");
            if(dialog!=IDOK) return kNoErr;
            auto engine=MakeEngine();Task task(*engine,parts,settings,names);
            dialog=DialogBoxParamW(Module(),MAKEINTRESOURCEW(IDD_PROGRESS),owner,ProgressDialog,reinterpret_cast<LPARAM>(&task));
            if(dialog==-1) throw std::runtime_error("Could not open nesting progress.");
            if(dialog!=IDOK) return kNoErr;
            if(!task.error.empty()) throw std::runtime_error(task.error);
            if(task.result.empty()) throw std::runtime_error("No selected parts fit. Increase the sheet size or reduce spacing.");
            ai::ArtboardID boardCount=0;Check(artboards->GetCount(boardCount));
            std::vector<nestai::OutputRect> occupied;
            for(ai::ArtboardID i=0;i<boardCount;++i) {
                auto board=artboards->GetArtboardProperties(i);AIRealRect bounds;Check(board.GetPosition(bounds));
                occupied.push_back({bounds.left,bounds.bottom,bounds.right,bounds.top});
            }
            occupied.insert(occupied.end(),sourceBounds.begin(),sourceBounds.end());
            AIRealRect canvasBounds;Check(sAIDocument->GetDocumentMaxArtboardBounds(&canvasBounds));
            nestai::OutputRect canvas{canvasBounds.left,canvasBounds.bottom,canvasBounds.right,canvasBounds.top};
            std::map<int,nestai::Point> sheetOrigins;
            for(auto placement:task.result) sheetOrigins.emplace(placement.sheet,nestai::Point{});
            double labelDepth=0;
            for(const auto& caption:task.sheetLabels) for(const auto& stroke:caption.second.strokes) for(auto point:stroke)
                labelDepth=std::max(labelDepth,-point.y+settings.labelStrokeWidth/2);
            std::vector<ai::ArtboardProperties> newBoards;size_t ordinal=0;
            for(auto& sheet:sheetOrigins) {
                auto target=nestai::OutputArtboard({activeBounds.left,activeBounds.bottom,activeBounds.right,activeBounds.top},
                    occupied,right,settings.width,settings.height+labelDepth,20*72/25.4,canvas);
                ai::ArtboardProperties newBoard;AIRealRect newBounds;
                newBounds.left=target.left;newBounds.bottom=target.bottom+labelDepth;newBounds.right=target.right;newBounds.top=target.top;
                auto positionError=newBoard.SetPosition(newBounds);
                if(positionError) throw std::runtime_error("Illustrator rejected an output artboard within the reported canvas bounds. No output was added. Check the sheet dimensions and document canvas limits.");
                std::string name="FreeNest Sheet "+std::to_string(ordinal+1);
                Check(newBoard.SetName(ai::UnicodeString(name.c_str())));
                sheet.second={target.left,target.bottom+labelDepth};occupied.push_back(target);newBoards.push_back(std::move(newBoard));++ordinal;
            }
            Check(sAILayer->GetCurrentLayer(&previous));
            Check(sAIUndo->SetUndoTextUS(ai::UnicodeString("Undo Nest Selected Paths"),ai::UnicodeString("Redo Nest Selected Paths")));
            addedArtboards.reserve(newBoards.size());
            for(auto& board:newBoards) {ai::ArtboardID id=-1;Check(artboards->AddNew(board,id));addedArtboards.push_back(id);}
            Check(sAILayer->InsertLayer(nullptr,kPlaceAboveAll,&output));
            Check(sAILayer->SetLayerTitle(output,ai::UnicodeString("FreeNest Results")));
            Check(sAILayer->SetCurrentLayer(output));
            AIArtHandle group=nullptr;Check(sAIArt->GetFirstArtOfLayer(output,&group));
            AIArtHandle labelParent=group;
            if(settings.separateLabelLayer&&((settings.addLabels&&task.labels.labeled)||!task.sheetLabels.empty())) {
                Check(sAILayer->InsertLayer(nullptr,kPlaceAboveAll,&labelLayer));
                Check(sAILayer->SetLayerTitle(labelLayer,ai::UnicodeString("FreeNest Labels")));
                Check(sAIArt->GetFirstArtOfLayer(labelLayer,&labelParent));
            }
            if(settings.includeSheetBoundary) {
            for(const auto& sheet:sheetOrigins) {
            nestai::Point sheetOrigin=sheet.second;
            AIArtHandle boundaryParent=group;
            auto sheetLabel=task.sheetLabels.find(sheet.first);
            if(sheetLabel!=task.sheetLabels.end()&&!settings.separateLabelLayer) {
                Check(sAIArt->NewArt(kGroupArt,kPlaceInsideOnTop,group,&boundaryParent));
                Check(sAIArt->SetArtName(boundaryParent,ai::UnicodeString("Sheet boundary and label")));
            }
            AIArtHandle boundary=nullptr;Check(sAIArt->NewArt(kPathArt,kPlaceInsideOnTop,boundaryParent,&boundary));
            std::vector<AIPathSegment> border;
            if(settings.useCustomSheet) {
                for(auto segment:settings.customSheet.segments) {
                    AIPathSegment target{};
                    target.p=Point(nestai::Place(segment.anchor,settings.customSheet.origin,sheetOrigin,0));
                    target.in=Point(nestai::Place(segment.left,settings.customSheet.origin,sheetOrigin,0));
                    target.out=Point(nestai::Place(segment.right,settings.customSheet.origin,sheetOrigin,0));
                    border.push_back(target);
                }
            } else {
                for(nestai::Point corner:std::vector<nestai::Point>{{sheetOrigin.x,sheetOrigin.y},{sheetOrigin.x+settings.width,sheetOrigin.y},
                    {sheetOrigin.x+settings.width,sheetOrigin.y+settings.height},{sheetOrigin.x,sheetOrigin.y+settings.height}}) {
                    AIPathSegment target{};target.p=target.in=target.out=Point(corner);border.push_back(target);
                }
            }
            Check(sAIPath->SetPathSegmentCount(boundary,ai::int16(border.size())));Check(sAIPath->SetPathSegments(boundary,0,ai::int16(border.size()),border.data()));Check(sAIPath->SetPathClosed(boundary,true));
            AIPathStyle borderStyle;AIBoolean advanced=false;Check(sAIPathStyle->GetPathStyle(boundary,&borderStyle,&advanced));
            borderStyle.fillPaint=false;borderStyle.strokePaint=true;borderStyle.stroke.width=0.5;borderStyle.stroke.color.kind=kThreeColor;
            borderStyle.stroke.color.c.rgb.red=0;borderStyle.stroke.color.c.rgb.green=0.55;borderStyle.stroke.color.c.rgb.blue=0.86;
            Check(sAIPathStyle->SetPathStyle(boundary,&borderStyle));Check(sAIArt->SetArtName(boundary,ai::UnicodeString("Sheet boundary")));
            if(sheetLabel!=task.sheetLabels.end())
                DrawLabel(sheetLabel->second,"FreeNest Sheet "+std::to_string(std::distance(sheetOrigins.begin(),sheetOrigins.find(sheet.first))+1),
                    settings.separateLabelLayer?labelParent:boundaryParent,sheetOrigin,0,settings);
            }
            }
            for(auto placement:task.result) {
                auto index=placement.part;AIArtHandle copy=nullptr,partGroup=group;
                bool hasLabel=settings.addLabels&&!task.labels.labels[index].strokes.empty();
                if(hasLabel&&!settings.separateLabelLayer) {Check(sAIArt->NewArt(kGroupArt,kPlaceInsideOnTop,group,&partGroup));Check(sAIArt->SetArtName(partGroup,ai::UnicodeString(names[index].c_str())));}
                Check(sAIArt->DuplicateArt(art[index],kPlaceInsideOnTop,partGroup,&copy));
                auto sheetOrigin=sheetOrigins.at(placement.sheet);
                nestai::Point destination{sheetOrigin.x+placement.x,sheetOrigin.y+placement.y};
                if(symbols[index]) PlaceSymbol(art[index],copy,parts[index],destination,placement.angle,symbolContours[index]);
                else {
                    auto contours=PathContours(copy);
                    if(contours.size()!=originals[index].size()) throw std::runtime_error("The compound copy changed its contour structure.");
                    for(size_t ring=0;ring<contours.size();++ring) {
                        auto segments=originals[index][ring];
                        for(auto& segment:segments) {
                            segment.p=Point(nestai::Place(Point(segment.p),parts[index].origin,destination,placement.angle));
                            segment.in=Point(nestai::Place(Point(segment.in),parts[index].origin,destination,placement.angle));
                            segment.out=Point(nestai::Place(Point(segment.out),parts[index].origin,destination,placement.angle));
                        }
                        ai::int16 copiedCount=0;Check(sAIPath->GetPathSegmentCount(contours[ring],&copiedCount));
                        if(copiedCount!=ai::int16(segments.size())) throw std::runtime_error("The copied contour changed its point count.");
                        Check(sAIPath->SetPathSegments(contours[ring],0,ai::int16(segments.size()),segments.data()));
                        Check(sAIArt->SetArtUserAttr(contours[ring],kArtSelected,0));
                    }
                }
                Check(sAIArt->SetArtUserAttr(copy,kArtSelected,0));
                if(hasLabel) DrawLabel(task.labels.labels[index],names[index],settings.separateLabelLayer?labelParent:partGroup,destination,placement.angle,settings);
            }
            Check(sAILayer->SetCurrentLayer(previous));Check(artboards->SetActive(addedArtboards.front()));output=nullptr;labelLayer=nullptr;addedArtboards.clear();
            bool includesSymbols=std::find(symbols.begin(),symbols.end(),true)!=symbols.end();
            std::string text=includesSymbols?"Parts:\n- ":"Paths:\n- ";
            text+=std::to_string(task.result.size())+" of "+std::to_string(count)+(includesSymbols?" parts":" paths")+" placed on a new layer\n- "+
                std::to_string(count-task.result.size())+" unplaced\n- Created "+std::to_string(sheetOrigins.size())+" new artboard(s)"+
                (includesSymbols?"\n- Originals are preserved; symbol copies remain linked":"\n- Original paths are preserved");
            if(includesSymbols) {
                size_t contourCount=0,boundsCount=0;
                for(auto placement:task.result) if(symbols[placement.part]) {
                    if(symbolContours[placement.part]) ++contourCount; else ++boundsCount;
                }
                text+="\n- Symbols packed by contour: "+std::to_string(contourCount)+
                      "\n- Symbols packed by bounds: "+std::to_string(boundsCount);
            }
            if(settings.centerLayouts) text+="\n- Centered "+std::to_string(sheetOrigins.size()-task.centeringSkipped)+" finished layout(s)";
            if(task.centeringSkipped) text+="\n- Kept "+std::to_string(task.centeringSkipped)+" irregular-sheet layout(s) in place to preserve boundary clearance";
            text+="\n\n"+(settings.addLabels?task.labels.Summary():"Labels:\n- Part labeling was not enabled");
            if(!task.sheetLabels.empty()) text+="\n- "+std::to_string(task.sheetLabels.size())+" sheet labels added";
            MessageBoxA(owner,text.c_str(),"Nesting complete",MB_OK);
        }catch(const std::exception& e) {
            if(regroupingSources) sAIUndo->UndoChanges();
            if(labelLayer) sAILayer->DeleteLayer(labelLayer);
            if(output) sAILayer->DeleteLayer(output);if(previous) sAILayer->SetCurrentLayer(previous);
            if(artboards&&!addedArtboards.empty()) {for(auto i=addedArtboards.rbegin();i!=addedArtboards.rend();++i) artboards->Delete(*i);if(previousArtboard>=0) artboards->SetActive(previousArtboard);}
            MessageBoxA(owner,e.what(),"FreeNest for Illustrator",MB_OK|MB_ICONEXCLAMATION);
        }catch(...) {
            if(regroupingSources) sAIUndo->UndoChanges();
            if(labelLayer) sAILayer->DeleteLayer(labelLayer);
            if(output) sAILayer->DeleteLayer(output);if(previous) sAILayer->SetCurrentLayer(previous);
            if(artboards&&!addedArtboards.empty()) {for(auto i=addedArtboards.rbegin();i!=addedArtboards.rend();++i) artboards->Delete(*i);if(previousArtboard>=0) artboards->SetActive(previousArtboard);}
            MessageBoxA(owner,"Unexpected error. The trial results were removed.","FreeNest for Illustrator",MB_OK|MB_ICONEXCLAMATION);
        }
        return kNoErr;
    }
};
Plugin* AllocatePlugin(SPPluginRef ref) {return new NestPlugin(ref);}
void FixupReload(Plugin* plugin) {NestPlugin::FixupVTable(static_cast<NestPlugin*>(plugin));}
