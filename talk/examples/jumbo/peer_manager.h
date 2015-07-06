#ifndef __PEER_MANAGER_H__
#define __PEER_MANAGER_H__

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/json.h"

typedef void (*JumboMsgSender)(const std::string &type, const Json::Value &msg);

class PeerManager
{
public:
    PeerManager(const std::string &stunurl, JumboMsgSender jumbo_msg_sender);
    ~PeerManager();

    void setOffser(const std::string &peerid, const std::string &message);
    void addIceCandidate(const std::string &peerid, const std::string&message);
    void deletePeerConnection(const std::string &peerid);

private:
    std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, webrtc::PeerConnectionObserver*> CreatePeerConnection();
    bool AddStreams(webrtc::PeerConnectionInterface* peer_connection);
    cricket::VideoCapturer* OpenVideoCaptureDevice();

    std::string stunurl;
    JumboMsgSender jumbo_msg_sender;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
    std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> > peer_connection_map;
    std::map<std::string, webrtc::PeerConnectionObserver*> peer_connectionobs_map;
    rtc::Thread* signaling_thread;
    rtc::Thread* worker_thread;
    rtc::scoped_refptr<webrtc::MediaStreamInterface> media_stream;
};

#endif // __PEER_MANAGER_H__
