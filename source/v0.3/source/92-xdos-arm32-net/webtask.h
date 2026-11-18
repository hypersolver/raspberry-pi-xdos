#ifndef XDOS_WEBTASK_H
#define XDOS_WEBTASK_H
#include <circle/sched/task.h>
#include <circle/net/netsubsystem.h>
class Desktop;
class CWebTask : public CTask
{
public:
    CWebTask(CNetSubSystem *net, Desktop *desktop);
    void Run(void) override;
private:
    bool Fetch(const char *url);
    CNetSubSystem *net_;
    Desktop *desktop_;
};
#endif
