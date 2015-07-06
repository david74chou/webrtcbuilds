#include "umbo_debug.h"
#include "set_sdp_observer.h"

SetSDPObserver *SetSDPObserver::Create()
{
    return new rtc::RefCountedObject<SetSDPObserver>();
}

void SetSDPObserver::OnSuccess()
{
    UMBO_DBG("Success to set SDP");
}

void SetSDPObserver::OnFailure(const std::string &error)
{
    UMBO_DBG("Fail to set SDP: %s", error.c_str());
}