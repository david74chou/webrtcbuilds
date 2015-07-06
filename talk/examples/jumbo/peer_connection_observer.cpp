#include "webrtc/base/json.h"
#include "peer_connection_observer.h"
#include "umbo_debug.h"

const std::string CANDIDATE_SDP_MID_NAME = "sdpMid";
const std::string CANDIDATE_SDP_MLINE_INDEX_NAME = "sdpMLineIndex";
const std::string CANDIDATE_SDP_NAME = "candidate";

PeerConnectionObserver *PeerConnectionObserver::Create(JumboMsgSender jumbo_msg_sender) {
    return new PeerConnectionObserver(jumbo_msg_sender);
}

void PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
    UMBO_DBG("OnIceCandidate -> mline_index: %d", candidate->sdp_mline_index());

    Json::Value jmessage;
    std::string candidate_string;
    jmessage[CANDIDATE_SDP_MID_NAME] = candidate->sdp_mid();
    jmessage[CANDIDATE_SDP_MLINE_INDEX_NAME] = candidate->sdp_mline_index();
    if (!candidate->ToString(&candidate_string)) {
        UMBO_WARN("Failed to serialize candidate");
        return;
    }
    jmessage[CANDIDATE_SDP_NAME] = candidate_string;

    if (jumbo_msg_sender)
        jumbo_msg_sender(CANDIDATE_SDP_NAME, jmessage);

    UMBO_DBG("OnIceCandidate <-");
}