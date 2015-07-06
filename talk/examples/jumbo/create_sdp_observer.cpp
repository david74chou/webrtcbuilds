#include "webrtc/base/json.h"
#include "umbo_debug.h"
#include "create_sdp_observer.h"
#include "set_sdp_observer.h"
#include "peer_manager.h"

const std::string SDP_TYPE_NAME = "type";
const std::string SDP_NAME = "sdp";

CreateSDPObserver *CreateSDPObserver::Create(webrtc::PeerConnectionInterface* pc, JumboMsgSender jumbo_msg_sender)
{
    return  new rtc::RefCountedObject<CreateSDPObserver>(pc, jumbo_msg_sender);
}

void CreateSDPObserver::OnSuccess(webrtc::SessionDescriptionInterface* desc)
{
    m_pc->SetLocalDescription(SetSDPObserver::Create(), desc);

    Json::Value jmessage;
    jmessage[SDP_TYPE_NAME] = desc->type();
    std::string sdp;
    if (!desc->ToString(&sdp)) {
        UMBO_WARN("Failed to serialize sdp");
        return;
    }
    jmessage[SDP_NAME] = sdp;

    if (jumbo_msg_sender)
        jumbo_msg_sender(SDP_NAME, jmessage);
}

void CreateSDPObserver::OnFailure(const std::string& error)
{
    UMBO_WARN("Fail to create SDP: %s", error.c_str());
}