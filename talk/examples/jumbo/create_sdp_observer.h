#ifndef __CREATE_SDP_OBSERVER_H__
#define __CREATE_SDP_OBSERVER_H__

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "peer_manager.h"

class CreateSDPObserver : public webrtc::CreateSessionDescriptionObserver {
public:
    static CreateSDPObserver * Create(webrtc::PeerConnectionInterface* pc, JumboMsgSender jumbo_msg_sender);

    virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
    virtual void OnFailure(const std::string& error);

protected:
    CreateSDPObserver(webrtc::PeerConnectionInterface* pc, JumboMsgSender jumbo_msg_sender) :
            m_pc(pc), jumbo_msg_sender(jumbo_msg_sender) {};

private:
    webrtc::PeerConnectionInterface* m_pc;
    JumboMsgSender jumbo_msg_sender;
};

#endif //__CREATE_SDP_OBSERVER_H__
