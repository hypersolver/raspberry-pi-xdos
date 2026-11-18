/* XD OS ARM32 V0.3 - cooperative Circle HTTP task.
 * HTTP only, no TLS; URLs are deliberately bounded.
 * Page fetch is not executed in mouse/keyboard handlers.
 */
#include "webtask.h"
#include "desktop.h"
#include <circle/net/dnsclient.h>
#include <circle/net/httpclient.h>
#include <circle/net/ipaddress.h>
#include <circle/net/http.h>
#include <circle/sched/scheduler.h>
#include <string.h>

namespace {
static unsigned char page[48*1024+1];
static bool ParseURL(const char *url, char *host, unsigned hostSize,
                     char *path, unsigned pathSize)
{
    const char *p=url;
    if (!strncmp(p,"https://",8)) return false;
    if (!strncmp(p,"http://",7)) p+=7;
    if (!*p) return false;
    const char *start=p;
    while (*p && *p!='/' && *p!=':' && *p!='?' && *p!='#') p++;
    unsigned h=(unsigned)(p-start);
    if (!h || h>=hostSize || *p==':') return false; // port 80 only
    memcpy(host,start,h);host[h]=0;
    if (*p=='#') p="/";
    if (*p=='/') {
        unsigned n=0;
        while(p[n] && p[n]!='#') n++;
        if(n>=pathSize)return false;
        memcpy(path,p,n);path[n]=0;
    } else if (*p=='?') {
        unsigned n=0;
        while(p[n] && p[n]!='#')n++;
        if(n+1>=pathSize)return false;
        path[0]='/';memcpy(path+1,p,n);path[n+1]=0;
    } else {path[0]='/';path[1]=0;}
    return true;
}
}

CWebTask::CWebTask(CNetSubSystem *net, Desktop *desktop)
: net_(net), desktop_(desktop)
{
    SetName("xdos-web");
}

bool CWebTask::Fetch(const char *url)
{
    if (!net_->IsRunning()) {
        desktop_->BrowserError("NO NETWORK: Connect Ethernet and wait for DHCP.");
        return false;
    }
    char host[160], path[256];
    if (!ParseURL(url,host,sizeof(host),path,sizeof(path))) {
        desktop_->BrowserError("Use http://host/path (HTTP port 80 only). HTTPS unsupported.");
        return false;
    }
    desktop_->BrowserStatus("Resolving DNS...");
    CIPAddress ip;
    CDNSClient dns(net_);
    if (!dns.Resolve(host,&ip)) {
        desktop_->BrowserError("DNS resolution failed.");
        return false;
    }
    desktop_->BrowserStatus("HTTP GET in progress...");
    unsigned length=sizeof(page)-1;
    CHTTPClient http(net_,ip,HTTP_PORT,host);
    THTTPStatus status=http.Get(path,page,&length);
    if (status!=HTTPOK) {
        switch (status) {
        case HTTPRequestTimeout: desktop_->BrowserError("HTTP timeout."); break;
        case HTTPContentBufferTooSmall: desktop_->BrowserError("Page too large (48 KiB limit)."); break;
        case HTTPConnectionReset: desktop_->BrowserError("HTTP connection reset."); break;
        default: desktop_->BrowserError("HTTP request returned error/redirect (not 200)."); break;
        }
        return false;
    }
    page[length]=0;
    desktop_->BrowserLoaded((const char*)page,length);
    return true;
}

void CWebTask::Run(void)
{
    char url[192];
    while(1) {
        if (desktop_->TakeBrowserRequest(url,sizeof(url))) {
            Fetch(url);
        } else {
            CScheduler::Get()->MsSleep(30);
        }
    }
}
