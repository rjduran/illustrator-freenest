#include "IllustratorSDK.h"
#include "NestPanel.h"
#include "NestPanelIcons.h"
#include "AppContext.hpp"
#include <algorithm>
#include <stdexcept>

namespace {
const wchar_t* PanelClass=L"FreeNest.CommandPanel";
void PanelCheck(AIErr error) {if(error) throw std::runtime_error("Illustrator could not create the FreeNest panel ("+std::to_string(error)+").");}
HMODULE PanelModule() {
    HMODULE module=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&PanelModule),&module);return module;
}
COLORREF ThemeColor(int component,COLORREF fallback) {
    AIUIThemeColor color;
    if(sAIUITheme->GetUIThemeColor(kAIUIThemeSelectorPanel,component,color)) return fallback;
    auto channel=[](AIReal v){return int(std::clamp(double(v),0.0,1.0)*255);};
    return RGB(channel(color.red),channel(color.green),channel(color.blue));
}
}

void NestPanel::Create(SPPluginRef ref,std::function<void(int)> handler) {
    if(panel) return;
    plugin=ref;command=std::move(handler);
    try {
        AISize minimum={228,128};
        PanelCheck(sAIPanel->Create(plugin,ai::UnicodeString("org.freenest.illustrator.commands"),ai::UnicodeString("FreeNest"),
            1,minimum,true,nullptr,this,panel));
        HWND parent=nullptr;PanelCheck(sAIPanel->GetPlatformWindow(panel,parent));
        if(!parent) throw std::runtime_error("Illustrator did not provide a FreeNest panel window.");
        using GetDpi=UINT(WINAPI*)(HWND);
        auto user32=GetModuleHandleW(L"user32.dll");
        auto getDpi=user32?reinterpret_cast<GetDpi>(GetProcAddress(user32,"GetDpiForWindow")):nullptr;
        dpi=getDpi?int(getDpi(parent)):96;
        minimum={float(MulDiv(228,dpi,96)),float(MulDiv(128,dpi,96))};
        // Keep the host frame fitted to three 32px buttons and four 8px gaps.
        // Matching height limits also constrain restored workspace sizes.
        const float contentHeight=float(3*MulDiv(32,dpi,96)+4*MulDiv(8,dpi,96));
        minimum.height=contentHeight;
        AISize preferred={float(MulDiv(250,dpi,96)),contentHeight},maximum={float(MulDiv(1200,dpi,96)),contentHeight};
        PanelCheck(sAIPanel->SetSizes(panel,minimum,preferred,preferred,maximum));
        WNDCLASSW windowClass{};windowClass.lpfnWndProc=WindowProc;windowClass.hInstance=PanelModule();
        windowClass.lpszClassName=PanelClass;windowClass.hCursor=LoadCursor(nullptr,IDC_ARROW);
        if(!RegisterClassW(&windowClass)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register the FreeNest panel controls.");
        content=CreateWindowExW(WS_EX_CONTROLPARENT,PanelClass,L"FreeNest commands",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
            0,0,1,1,parent,nullptr,PanelModule(),this);
        if(!content) throw std::runtime_error("Cannot create the FreeNest panel controls.");
        font=CreateFontW(-MulDiv(9,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(!font) throw std::runtime_error("Cannot create the FreeNest panel font.");
        const wchar_t* labels[]={L"Nest Selected Paths",L"Label Selected Paths",L"Use Selected Path as Sheet"};
        for(int i=0;i<3;++i) {
            buttons[i]=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
                0,0,1,1,content,reinterpret_cast<HMENU>(INT_PTR(i+1)),PanelModule(),nullptr);
            if(!buttons[i]) throw std::runtime_error("Cannot create a FreeNest command button.");
            SendMessageW(buttons[i],WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        }
        ai::AutoBuffer<const ai::uint8*> light(1),dark(1);ai::AutoBuffer<ai::uint32> lightSize(1),darkSize(1);
        light[0]=FreeNestPanelLight;dark[0]=FreeNestPanelDark;
        lightSize[0]=sizeof(FreeNestPanelLight);darkSize[0]=sizeof(FreeNestPanelDark);
        PanelCheck(sAIPanel->SetIcon(panel,light,lightSize,dark,darkSize));
        PanelCheck(sAIPanel->SetSizeChangedNotifyProc(panel,SizeChanged));
        Refresh();Resize();SetTimer(content,1,500,nullptr);
    }catch(...) {Destroy();throw;}
}

void NestPanel::Destroy() {
    if(content) {KillTimer(content,1);DestroyWindow(content);content=nullptr;}
    if(panel) {sAIPanel->SetSizeChangedNotifyProc(panel,nullptr);sAIPanel->Destroy(panel);panel=nullptr;}
    if(font) {DeleteObject(font);font=nullptr;}
    if(background) {DeleteObject(background);background=nullptr;}
    command={};plugin=nullptr;
    UnregisterClassW(PanelClass,PanelModule());
}
bool NestPanel::Visible() const {AIBoolean shown=false;return panel&&!sAIPanel->IsShown(panel,shown)&&shown;}
void NestPanel::Toggle() {if(panel) PanelCheck(sAIPanel->Show(panel,!Visible()));}
void NestPanel::SetBusy(bool value) {busy=value;if(content) Refresh();}
void AIAPI NestPanel::SizeChanged(AIPanelRef ref) {
    AIPanelUserData data=nullptr;
    if(!sAIPanel->GetUserData(ref,data)&&data) static_cast<NestPanel*>(data)->Resize();
}
void NestPanel::Resize() {
    if(!content) return;
    RECT bounds{};GetClientRect(GetParent(content),&bounds);
    MoveWindow(content,0,0,bounds.right,bounds.bottom,TRUE);Layout();
}
void NestPanel::Layout() {
    RECT bounds{};GetClientRect(content,&bounds);
    int pad=MulDiv(8,dpi,96),height=MulDiv(32,dpi,96);
    for(int i=0;i<3;++i) if(buttons[i]) MoveWindow(buttons[i],pad,pad+i*(height+pad),std::max(1,int(bounds.right)-2*pad),height,TRUE);
}
void NestPanel::Refresh() {
    auto color=ThemeColor(kAIUIComponentColorBackground,back);
    if(!background||color!=back) {if(background) DeleteObject(background);back=color;background=CreateSolidBrush(back);RedrawWindow(content,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
    auto newText=ThemeColor(kAIUIComponentColorText,text),newBorder=ThemeColor(kAIUIComponentColorBorder,border);
    if(newText!=text||newBorder!=border) {text=newText;border=newBorder;RedrawWindow(content,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);}
    AIDocumentHandle document=nullptr;
    bool enabled=!busy&&!sAIDocument->GetDocument(&document)&&document;
    for(auto button:buttons) if(button&&bool(IsWindowEnabled(button))!=enabled) EnableWindow(button,enabled);
}
void NestPanel::DrawButton(const DRAWITEMSTRUCT& item) {
    bool dark=(GetRValue(back)+GetGValue(back)+GetBValue(back))<384;
    bool pressed=(item.itemState&ODS_SELECTED)!=0,disabled=(item.itemState&ODS_DISABLED)!=0;
    auto fill=CreateSolidBrush(dark?(pressed?RGB(55,55,55):RGB(85,85,85)):(pressed?RGB(210,210,210):RGB(250,250,250)));
    FillRect(item.hDC,&item.rcItem,fill);DeleteObject(fill);
    auto outline=CreateSolidBrush(border);FrameRect(item.hDC,&item.rcItem,outline);DeleteObject(outline);
    SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,disabled?(dark?RGB(155,155,155):RGB(130,130,130)):text);
    auto old=SelectObject(item.hDC,font);wchar_t label[128]{};GetWindowTextW(item.hwndItem,label,128);
    RECT bounds=item.rcItem;InflateRect(&bounds,-4,0);DrawTextW(item.hDC,label,-1,&bounds,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(item.itemState&ODS_FOCUS) {InflateRect(&bounds,-3,-3);DrawFocusRect(item.hDC,&bounds);}
    SelectObject(item.hDC,old);
}
LRESULT CALLBACK NestPanel::WindowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto self=reinterpret_cast<NestPanel*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        self=static_cast<NestPanel*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    if(!self) return DefWindowProcW(window,message,w,l);
    try {
        switch(message) {
            case WM_SIZE:self->Layout();return 0;
            case WM_ERASEBKGND: {RECT rect{};GetClientRect(window,&rect);FillRect(reinterpret_cast<HDC>(w),&rect,self->background?self->background:GetSysColorBrush(COLOR_BTNFACE));return 1;}
            case WM_DRAWITEM:self->DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;
            case WM_TIMER:if(!self->busy) {AppContext context(self->plugin);self->Refresh();}return 0;
            case WM_COMMAND:
                if(HIWORD(w)==BN_CLICKED&&LOWORD(w)>=1&&LOWORD(w)<=3&&!self->busy) {
                    AppContext context(self->plugin);
                    self->busy=true;for(auto button:self->buttons) EnableWindow(button,FALSE);
                    try {self->command(LOWORD(w));}catch(...) {self->busy=false;self->Refresh();throw;}
                    self->busy=false;self->Refresh();return 0;
                }
                break;
            case WM_NCDESTROY:SetWindowLongPtrW(window,GWLP_USERDATA,0);break;
        }
    }catch(const std::exception& error) {MessageBoxA(GetAncestor(window,GA_ROOTOWNER),error.what(),"FreeNest",MB_OK|MB_ICONEXCLAMATION);}
    catch(...) {MessageBoxW(GetAncestor(window,GA_ROOTOWNER),L"FreeNest could not complete the panel command.",L"FreeNest",MB_OK|MB_ICONEXCLAMATION);}
    return DefWindowProcW(window,message,w,l);
}
