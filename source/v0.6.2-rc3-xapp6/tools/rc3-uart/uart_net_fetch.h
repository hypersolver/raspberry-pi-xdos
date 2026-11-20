#pragma once

#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/sched/scheduler.h>
#include <circle/string.h>
#include <circle/timer.h>
#include <string.h>

namespace XdosNetFetch {
constexpr unsigned MaxBytes = 16U * 1024 * 1024;

inline bool ParseDecimal(const char *&text, unsigned limit, unsigned &value)
{
    if (*text < '0' || *text > '9') return false;
    value = 0;
    do {
        value = value * 10 + unsigned(*text++ - '0');
        if (value > limit) return false;
    } while (*text >= '0' && *text <= '9');
    return true;
}

inline bool ParseEndpoint(char *&args, u8 address[4], unsigned &port)
{
    const char *text = args;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned octet;
        if (!ParseDecimal(text,255,octet) || *text++ != (i < 3 ? '.' : ' ')) return false;
        address[i] = static_cast<u8>(octet);
    }
    if (!ParseDecimal(text,65535,port) || !port || *text++ != ' ') return false;
    args += text-args;
    return true;
}

inline bool HeaderName(const char *row, const char *name)
{
    for (; *name; ++row, ++name) {
        char ch = *row;
        if (ch >= 'A' && ch <= 'Z') ch += 'a'-'A';
        if (ch != *name) return false;
    }
    return true;
}

inline bool ValidHeader(char *header, unsigned expected)
{
    if (strncmp(header,"HTTP/1.0 200 ",13) && strncmp(header,"HTTP/1.1 200 ",13))
        return false;
    bool hasLength = false;
    for (char *row = strstr(header,"\r\n"); row && row[2] != '\r'; ) {
        row += 2;
        char *end = strstr(row,"\r\n");
        if (!end) return false;
        *end = 0;
        if (HeaderName(row,"content-length:")) {
            const char *value = row+15;
            while (*value == ' ' || *value == '\t') ++value;
            unsigned size;
            if (hasLength || !ParseDecimal(value,MaxBytes,size) || *value || size != expected)
                return false;
            hasLength = true;
        }
        if (HeaderName(row,"transfer-encoding:") || HeaderName(row,"content-encoding:"))
            return false;
        *end = '\r';
        row = end;
    }
    return hasLength;
}

template<class Write>
bool Download(CNetSubSystem *net,const CIPAddress &host,unsigned port,
              const char *name,unsigned size,Write write)
{
    CSocket socket(net,IPPROTO_TCP);
    if (socket.Connect(host,port)<0) return false;
    socket.SetOptionSendTimeout(3000000);
    CString hostString, request;
    host.Format(&hostString);
    request.Format("GET /%s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                   name,static_cast<const char *>(hostString));
    const char *next = request;
    unsigned left = request.GetLength();
    while (left) {
        const int sent = socket.Send(next,left,0);
        if (sent<=0) return false;
        next += sent;
        left -= sent;
    }
    char header[4096];
    unsigned headerLength = 0;
    bool haveHeader = false;
    unsigned remaining = size;
    const unsigned started = CTimer::GetClockTicks();
    unsigned lastData = started;
    while (remaining) {
        const unsigned now = CTimer::GetClockTicks();
        if (now-started > 120000000U || now-lastData > 5000000U) return false;
        u8 buffer[8192];
        const int received = socket.Receive(buffer,sizeof buffer,MSG_DONTWAIT);
        if (received < 0) return false;
        if (received == 0) {
            CScheduler::Get()->MsSleep(1);
            continue;
        }
        lastData = now;
        unsigned i = 0;
        while (!haveHeader && i < static_cast<unsigned>(received)) {
            if (headerLength+1>=sizeof header || buffer[i]==0) return false;
            header[headerLength++] = buffer[i++];
            if (headerLength>=4 && !memcmp(header+headerLength-4,"\r\n\r\n",4)) {
                header[headerLength] = 0;
                if (!ValidHeader(header,size)) return false;
                haveHeader = true;
            }
        }
        const unsigned count = received-i;
        if (count > remaining || (count && !write(buffer+i,count))) return false;
        remaining -= count;
        // A cooperative scheduler still needs to service incoming TCP and UI work.
        CScheduler::Get()->Yield();
    }
    return haveHeader;
}
}
