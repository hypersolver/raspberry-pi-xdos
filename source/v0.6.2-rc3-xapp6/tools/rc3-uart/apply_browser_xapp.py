"""Move the Browser HTTP transaction code out of the kernel and into its XAPP."""

from pathlib import Path
import sys


source = Path(sys.argv[1])
files = {}


def edit(name, old, new):
    path = source / name
    if path not in files:
        files[path] = path.read_text()
    if files[path].count(old) != 1:
        raise RuntimeError(f"{name}: expected one occurrence of {old!r}")
    files[path] = files[path].replace(old,new)


edit("xapp_workspace.h", "typedef struct XdosWorkspaceApi {",
     """typedef void (*XdosBrowserFetch)(void *network, void *desktop,const char *url,unsigned id);
typedef struct XdosWorkspaceApi {""")
edit("xapp_workspace.h", "} XdosWorkspaceApi;",
     """    int32_t (*register_browser_fetch)(void *context,XdosBrowserFetch fetch);
} XdosWorkspaceApi;""")
edit("kernel.h", '#include "webtask.h"\n', "")
edit("kernel.h", "    CWebTask *webTask_;",
     """    XdosBrowserFetch browserFetch_ = nullptr;
    bool StartBrowserRequest(const char *url);""")
edit("kernel.cpp", "  webTask_(0), storageTask_(0), storageMounted_(false),",
     "  storageTask_(0), storageMounted_(false),")
edit("kernel.cpp", """    webTask_=new CWebTask(net_,&desktop_);
    if(!webTask_) {
        Debug("[NET] HTTP worker allocation failed\\r\\n");
        desktop_.SetNetwork(false,"HTTP WORKER FAILED");
        return;
    }""", '    Debug("[NET] Browser HTTP provided by XAPP on request\\r\\n");')
edit("kernel.cpp", "void CKernel::UpdateNetworkStatus()", """namespace {
class BrowserXappJob : public CTask {
public:
    BrowserXappJob(XdosBrowserFetch fetch,CNetSubSystem *network,Desktop *desktop,
                   const char *url,unsigned id)
    : fetch_(fetch),network_(network),desktop_(desktop),id_(id)
    {
        strcpy(url_,url);
        SetName("browser-xapp");
    }

    void Run() override
    {
        CLogger::Get()->Write("BROWSER",LogWarning,"XAPP HTTP request started");
        fetch_(network_,desktop_,url_,id_);
        XappLoaderV1::ReleaseSlot();
        CLogger::Get()->Write("BROWSER",LogWarning,"XAPP HTTP request finished");
    }

private:
    XdosBrowserFetch fetch_;
    CNetSubSystem *network_;
    Desktop *desktop_;
    unsigned id_;
    char url_[192];
};
}

bool CKernel::StartBrowserRequest(const char *url)
{
    if (!networkInit_ || !net_ || !net_->IsRunning()) {
        desktop_.BrowserError("NO NETWORK: Wait for Wi-Fi/Ethernet DHCP.");
        return false;
    }
    if (!browserFetch_ || !XappLoaderV1::HoldSlot()) return false;
    char pending[192];
    unsigned id;
    if (desktop_.RequestHttp(url) && desktop_.TakeBrowserRequest(pending,sizeof pending,id)) {
        if (new BrowserXappJob(browserFetch_,net_,&desktop_,pending,id)) return true;
        desktop_.BrowserError("Could not start Browser XAPP job.");
    }
    XappLoaderV1::ReleaseSlot();
    return false;
}

void CKernel::UpdateNetworkStatus()""")
edit("kernel.cpp", '#include "app_shortcut.h"', '#include "app_shortcut.h"\n#include "xapp_loader.h"')
edit("kernel.cpp", "case XDOS_REQUEST_HTTP: return k->desktop_.RequestHttp(text) ? 0 : -1;",
     "case XDOS_REQUEST_HTTP: return k->StartBrowserRequest(text) ? 0 : -1;")
edit("kernel.cpp", """                        },ctx);
                }};""", """                        },ctx);
                },
                [](void *ctx,XdosBrowserFetch fetch) -> int32_t {
                    if (!fetch) return -1;
                    static_cast<CKernel *>(ctx)->browserFetch_=fetch;
                    return 0;
                }};
            if (workspace==Desktop::BrowserApp) browserFetch_=nullptr;""")
edit("app_shortcut.cpp", "    if (!VerifyManifest(entry.manifest,entry.module)) return",
     """    const bool browser = app == 3;
    if (browser) {
        const auto staged = XappLoaderV1::ExecuteFile("SD:/XDOS/INBOX/B2WEB.XAPP",api,true);
        if (staged != XappLoaderV1::OPEN_FAILED)
            return staged == XappLoaderV1::OK ? entry.started : "XAPP: staged Browser failed";
    }
    if (!VerifyManifest(entry.manifest,entry.module)) return""")
edit("app_shortcut.cpp", "return XappLoaderV1::ExecuteFile(entry.module,api) == XappLoaderV1::OK ?",
     "return XappLoaderV1::ExecuteFile(entry.module,api,browser) == XappLoaderV1::OK ?")
edit("desktop.cpp", "            Open(Browser);SubmitBrowserURL();",
     "            workspacePending_=BrowserApp;")
edit("desktop.h", "    bool TakeBrowserRequest(", """    const char *BrowserURL() const { return browserURL_; }
    bool TakeBrowserRequest(""")
edit("xdos_lvgl.cpp", "    lv_textarea_set_placeholder_text(workspaceInput_,hint);",
     """    lv_textarea_set_placeholder_text(workspaceInput_,hint);
    if (panel == XDOS_PANEL_HTTP) lv_textarea_set_text(workspaceInput_,desktop_->BrowserURL());""")
edit("xapp_loader.h", "    static Result ExecuteFile", """    static bool HoldSlot();
    static void ReleaseSlot();
    static Result ExecuteFile""")
edit("xapp_loader.cpp", "XappLoaderV1::Result XappLoaderV1::ExecuteFile",
     """namespace { bool slotHeld = false; }

bool XappLoaderV1::HoldSlot()
{
    if (slotHeld) return false;
    slotHeld=true;
    return true;
}

void XappLoaderV1::ReleaseSlot()
{
    slotHeld=false;
}

XappLoaderV1::Result XappLoaderV1::ExecuteFile""")
edit("xapp_loader.cpp", """{
    if (!path || !api || api->abi_version""", """{
    if (slotHeld) return ENTRY_ERROR;
    if (!path || !api || api->abi_version""")
edit("Makefile", "desktop.o xdos_gui.o webtask.o boot_splash.o",
     "desktop.o xdos_gui.o boot_splash.o")
edit("webtask.cpp", '#include "webtask.h"', '#include <circle/net/netsubsystem.h>')
edit("webtask.cpp", """CWebTask::CWebTask(CNetSubSystem *net,Desktop *desktop)
: net_(net),desktop_(desktop)
{SetName("xdos-web");}

bool CWebTask::Fetch(const char *url,unsigned requestID)""", """static bool Fetch(CNetSubSystem *net_,Desktop *desktop_,const char *url,unsigned requestID)""")
edit("webtask.cpp", """void CWebTask::Run(void)
{
    char url[192];unsigned requestID=0;
    while(1){
        if(desktop_->TakeBrowserRequest(url,sizeof url,requestID))Fetch(url,requestID);
        else CScheduler::Get()->MsSleep(30);
    }
}""", """extern "C" void xdos_browser_fetch(void *network,void *desktop,const char *url,unsigned requestID)
{
    Fetch(static_cast<CNetSubSystem *>(network),static_cast<Desktop *>(desktop),url,requestID);
}""")

for path,text in files.items():
    path.write_text(text,encoding="utf-8")
