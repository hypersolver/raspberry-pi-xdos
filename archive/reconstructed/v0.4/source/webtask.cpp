/* XD OS ARM32 V0.4 — bounded HTTP fetch task.
 * Network I/O is exclusively inside this Circle scheduler task.
 * No HTTPS/TLS; request target only accepts port 80.
 */
#include "webtask.h"
#include "desktop.h"
#include <circle/net/dnsclient.h>
#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/net/in.h>
#include <circle/netdevice.h>
#include <circle/timer.h>
#include <circle/string.h>
#include <circle/sched/scheduler.h>
#include <string.h>

namespace {
enum { MAX_BODY = 48*1024, MAX_RAW = 56*1024, HEADER_MAX = 4096,
       TOTAL_TIMEOUT_SEC = 20, READ_IDLE_TIMEOUT_SEC = 5 };
static u8 raw[MAX_RAW];
static u8 body[MAX_BODY+1];

static bool ParseURL(const char *url,char *host,unsigned hostSize,
                     char *path,unsigned pathSize)
{
    const char *p=url;
    if (!strncmp(p,"https://",8)) return false;
    if (!strncmp(p,"http://",7)) p+=7;
    if (!*p) return false;
    const char *start=p;
    while (*p && *p!='/' && *p!=':' && *p!='?' && *p!='#') p++;
    unsigned n=(unsigned)(p-start);
    if (!n||n>=hostSize||*p==':')return false;
    memcpy(host,start,n);host[n]=0;
    if (*p=='/') {
        unsigned x=0;while(p[x]&&p[x]!='#')x++;
        if(x>=pathSize)return false;
        memcpy(path,p,x);path[x]=0;
    }else if(*p=='?'){
        unsigned x=0;while(p[x]&&p[x]!='#')x++;
        if(x+1>=pathSize)return false;
        path[0]='/';memcpy(path+1,p,x);path[x+1]=0;
    }else{path[0]='/';path[1]=0;}
    return true;
}
static bool StartsCase(const char *p,const char *q)
{
    while(*q){char a=*p++,b=*q++;
        if(a>='A'&&a<='Z')a+=32;
        if(b>='A'&&b<='Z')b+=32;
        if(a!=b)return false;
    }
    return true;
}
static int HeaderEnd(const u8 *p,unsigned n)
{
    for(unsigned i=0;i+3<n;i++)
        if(p[i]=='\r'&&p[i+1]=='\n'&&p[i+2]=='\r'&&p[i+3]=='\n')return (int)(i+4);
    for(unsigned i=0;i+1<n;i++)
        if(p[i]=='\n'&&p[i+1]=='\n')return (int)(i+2);
    return -1;
}
struct HeaderInfo {unsigned length;bool hasLength;bool chunked;int code;};
static bool ParseHeader(const u8 *data,unsigned n,HeaderInfo &out)
{
    if(n>=HEADER_MAX)return false;
    char h[HEADER_MAX+1];memcpy(h,data,n);h[n]=0;
    const char *p=h;
    if(strncmp(p,"HTTP/",5))return false;
    const char *space=strchr(p,' ');
    if(!space)return false;
    out.code=0;
    for(unsigned i=1;i<=3;i++){
        char c=space[i];if(c<'0'||c>'9')return false;
        out.code=out.code*10+c-'0';
    }
    out.length=0;out.hasLength=false;out.chunked=false;
    while(*p){
        const char *e=strchr(p,'\n');if(!e)break;
        unsigned ln=(unsigned)(e-p);
        if(ln && p[ln-1]=='\r')ln--;
        if(ln>=15 && StartsCase(p,"content-length:")){
            const char *v=p+15;while(*v==' '||*v=='\t')v++;
            unsigned length=0;unsigned digits=0;
            while(*v>='0'&&*v<='9'){
                if(length>MAX_RAW){return false;}
                length=length*10+(unsigned)(*v++-'0');digits++;
            }
            if(digits){out.hasLength=true;out.length=length;}
        }
        if(ln>=18 && StartsCase(p,"transfer-encoding:")){
            const char *v=p+18;
            while(*v&&v<e){if(StartsCase(v,"chunked"))out.chunked=true;v++;}
        }
        p=e+1;
    }
    return true;
}
static bool DecodeChunked(const u8 *in,unsigned size,u8 *out,unsigned &outlen)
{
    unsigned ip=0,op=0;
    while(ip<size){
        unsigned chunk=0,digits=0;
        while(ip<size){
            u8 c=in[ip];
            if(c==';'||c=='\r'||c=='\n')break;
            unsigned digit=0;
            if(c>='0'&&c<='9')digit=c-'0';
            else if(c>='a'&&c<='f')digit=c-'a'+10;
            else if(c>='A'&&c<='F')digit=c-'A'+10;
            else return false;
            if(chunk>MAX_BODY/16)return false;
            chunk=chunk*16+digit;ip++;digits++;
        }
        if(!digits)return false;
        while(ip<size&&in[ip]!='\n')ip++;
        if(ip>=size)return false;
        ip++;
        if(!chunk){outlen=op;return true;}
        if(chunk>MAX_BODY-op || chunk>size-ip)return false;
        memcpy(out+op,in+ip,chunk);op+=chunk;ip+=chunk;
        if(ip<size&&in[ip]=='\r')ip++;
        if(ip>=size||in[ip]!='\n')return false;
        ip++;
    }
    return false;
}
static unsigned Elapsed(unsigned start){return CTimer::Get()->GetTicks()-start;}
}

CWebTask::CWebTask(CNetSubSystem *net,Desktop *desktop)
: net_(net),desktop_(desktop)
{SetName("xdos-web");}

bool CWebTask::Fetch(const char *url,unsigned requestID)
{
    const unsigned started=CTimer::Get()->GetTicks();
    if(!desktop_->BrowserRequestActive(requestID))return false;
    if(!net_->IsRunning()){
        desktop_->BrowserError("NO NETWORK: Wait for Wi-Fi/Ethernet DHCP.");return false;
    }
    char host[160],path[256];
    if(!ParseURL(url,host,sizeof host,path,sizeof path)){
        desktop_->BrowserError("Invalid URL: use http://host/path on port 80.");return false;
    }
    desktop_->BrowserStatus("DNS resolving...");
    CIPAddress ip;
    CDNSClient dns(net_);
    bool resolved=dns.Resolve(host,&ip); // Circle DNS internally retries; this call is blocking.
    if(!desktop_->BrowserRequestActive(requestID))return false;
    if(Elapsed(started)>=TOTAL_TIMEOUT_SEC*HZ){
        desktop_->BrowserError("TOTAL TIMEOUT after DNS (>20s)");return false;
    }
    if(!resolved){desktop_->BrowserError("DNS resolution failed or timed out.");return false;}

    desktop_->BrowserStatus("TCP connecting (max 5s)...");
    CSocket sock(net_,IPPROTO_TCP);
    // The companion build script patches Circle CTCPConnection::Connect()
    // to use WaitWithTimeout(5*1000000). A socket timeout setting alone
    // does NOT bound Circle's original synchronous Connect() call.
    if(sock.Connect(ip,80)<0){
        if(desktop_->BrowserRequestActive(requestID))desktop_->BrowserError("TCP connect failed / 5s timeout.");
        return false;
    }
    if(!desktop_->BrowserRequestActive(requestID))return false;
    if(Elapsed(started)>=TOTAL_TIMEOUT_SEC*HZ){
        desktop_->BrowserError("TOTAL TIMEOUT after TCP connect.");return false;
    }
    sock.SetOptionSendTimeout(3*1000000);
    // HTTP/1.0 + identity simplifies a resource-limited bare-metal browser.
    CString request;
    request.Format("GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: XDOS-ARM32/0.4\r\n"
                   "Accept-Encoding: identity\r\nConnection: close\r\n\r\n",path,host);
    const char *text=(const char *)request;unsigned left=request.GetLength();
    while(left){
        if(!desktop_->BrowserRequestActive(requestID))return false;
        if(Elapsed(started)>=TOTAL_TIMEOUT_SEC*HZ){desktop_->BrowserError("TOTAL TIMEOUT during HTTP send.");return false;}
        int sent=sock.Send(text,left,0);
        if(sent<=0){
            if(desktop_->BrowserRequestActive(requestID))
                desktop_->BrowserError("HTTP send failed / timeout.");
            return false;
        }
        text+=sent;left-=(unsigned)sent;
    }
    desktop_->BrowserStatus("HTTP receiving (idle 5s / total 20s)...");
    unsigned received=0,lastData=CTimer::Get()->GetTicks();
    unsigned shownSec=0;
    HeaderInfo info={0,false,false,0};int headerEnd=-1;
    bool ended=false;
    while(true){
        if(!desktop_->BrowserRequestActive(requestID))return false;
        if(Elapsed(started)>=TOTAL_TIMEOUT_SEC*HZ){desktop_->BrowserError("TOTAL TIMEOUT (20 seconds).");return false;}
        if(Elapsed(lastData)>=READ_IDLE_TIMEOUT_SEC*HZ){
            desktop_->BrowserError("RECEIVE TIMEOUT (5 seconds without data).");return false;
        }
        u8 buf[FRAME_BUFFER_SIZE];
        int n=sock.Receive(buf,sizeof buf,MSG_DONTWAIT);
        if(n>0){
            if(received+(unsigned)n>MAX_RAW){desktop_->BrowserError("HTTP response too large (56 KiB).");return false;}
            memcpy(raw+received,buf,(unsigned)n);received+=(unsigned)n;
            lastData=CTimer::Get()->GetTicks();
            if(headerEnd<0){
                headerEnd=HeaderEnd(raw,received);
                if(headerEnd<0 && received>=HEADER_MAX){
                    desktop_->BrowserError("HTTP header too large.");return false;
                }
                if(headerEnd>=0){
                    if(!ParseHeader(raw,(unsigned)headerEnd,info)){
                        desktop_->BrowserError("Invalid HTTP response headers.");return false;
                    }
                    if(info.code!=200){
                        desktop_->BrowserError("HTTP status is not 200 (redirect/error).");return false;
                    }
                    if(info.hasLength && info.length>MAX_BODY){
                        desktop_->BrowserError("HTTP body exceeds 48 KiB.");return false;
                    }
                }
            }
            if(headerEnd>=0 && !info.chunked && info.hasLength &&
               received-(unsigned)headerEnd>=info.length){ended=true;break;}
        }else if(n<0){
            // Circle may report a connection reset after the peer closes.
            // Accept only when valid headers have already been received.
            if(headerEnd>=0){ended=true;break;}
            desktop_->BrowserError("HTTP connection closed before headers.");return false;
        }else{
            unsigned sec=Elapsed(started)/HZ;
            if(sec!=shownSec){
                shownSec=sec;
                CString progress;
                progress.Format("Receiving %u bytes / %u of 20 seconds",received,sec);
                desktop_->BrowserStatus((const char*)progress);
            }
            CScheduler::Get()->MsSleep(10);
        }
    }
    if(!ended||headerEnd<0){desktop_->BrowserError("Incomplete HTTP response.");return false;}
    unsigned available=received-(unsigned)headerEnd;
    unsigned count=0;
    if(info.chunked){
        if(!DecodeChunked(raw+headerEnd,available,body,count)){
            desktop_->BrowserError("Incomplete/invalid chunked HTTP body.");return false;
        }
    }else{
        count=info.hasLength?info.length:available;
        if(count>available){desktop_->BrowserError("HTTP response truncated.");return false;}
        if(count>MAX_BODY){desktop_->BrowserError("HTTP body exceeds 48 KiB.");return false;}
        memcpy(body,raw+headerEnd,count);
    }
    body[count]=0;
    if(!desktop_->BrowserRequestActive(requestID))return false;
    desktop_->BrowserLoaded((const char *)body,count);
    return true;
}

void CWebTask::Run(void)
{
    char url[192];unsigned requestID=0;
    while(1){
        if(desktop_->TakeBrowserRequest(url,sizeof url,requestID))Fetch(url,requestID);
        else CScheduler::Get()->MsSleep(30);
    }
}
