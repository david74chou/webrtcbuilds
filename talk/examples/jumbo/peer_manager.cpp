#include <iostream>
#include <utility>

#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/media/devices/devicemanager.h"
#include "talk/media/base/videocapturer.h"
#include "webrtc/base/common.h"
#include "webrtc/base/json.h"
#include "webrtc/base/logging.h"

#include "peer_manager.h"
#include "umbo_debug.h"
#include "set_sdp_observer.h"
#include "create_sdp_observer.h"
#include "peer_connection_observer.h"

const std::string AUDIO_LABEL  = "audio_label";
const std::string VIDEO_LABEL  = "video_label";
const std::string STREAM_LABEL = "stream_label";

// Names used for a IceCandidate JSON object.
const std::string CANDIDATE_SDP_MID_NAME = "sdpMid";
const std::string CANDIDATE_SDP_MLINE_INDEX_NAME = "sdpMLineIndex";
const std::string CANDIDATE_SDP_NAME = "candidate";

// Names used for a SessionDescription JSON object.
const std::string SDP_TYPE_NAME = "type";
const std::string SDP_NAME = "sdp";

class VideoCapturerListener : public sigslot::has_slots<>
{
public:
    VideoCapturerListener(cricket::VideoCapturer* capturer) {
        capturer->SignalFrameCaptured.connect(this, &VideoCapturerListener::OnFrameCaptured);
    }

    void OnFrameCaptured(cricket::VideoCapturer* capturer, const cricket::CapturedFrame* frame) {
        UMBO_DBG("%s", __FUNCTION__);
    }
};

PeerManager::PeerManager(const std::string &stunurl, JumboMsgSender sender) :
        stunurl(stunurl),
        jumbo_msg_sender(sender)
{
    signaling_thread = new rtc::Thread();
    worker_thread = new rtc::Thread();
    signaling_thread->Start();
    worker_thread->Start();

    peer_connection_factory = webrtc::CreatePeerConnectionFactory(
            worker_thread,
            signaling_thread,
            NULL, NULL, NULL
    );
    if (!peer_connection_factory.get()) {
        UMBO_CRITICAL("Failed to initialize PeerConnectionFactory");
    }
}

PeerManager::~PeerManager()
{
    peer_connection_factory = NULL;
}

void PeerManager::setOffser(const std::string &peerid, const std::string &message)
{
    UMBO_DBG("setOffser -> peerid: %s", peerid.c_str());

    Json::Reader reader;
    Json::Value  jmessage;
    if (!reader.parse(message, jmessage)) {
        UMBO_WARN("setOffser <- Received unknown message: %s", message.c_str());
        return;
    }

    std::string type, sdp;
    if (!rtc::GetStringFromJsonObject(jmessage, SDP_TYPE_NAME, &type) ||
        !rtc::GetStringFromJsonObject(jmessage, SDP_NAME, &sdp)) {
        UMBO_WARN("setOffser <- Can't parse received message.");
        return;
    }

    webrtc::SdpParseError sdp_parse_error;
    webrtc::SessionDescriptionInterface* session_description = webrtc::CreateSessionDescription(type, sdp, &sdp_parse_error);
    if (!session_description) {
        UMBO_WARN("setOffser <- Can't parse received session description message: %s", sdp_parse_error.description.c_str());
        return;
    }
    UMBO_DBG("From peerid: %s, sdp type: %s", peerid.c_str(), session_description->type().c_str());

    std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, webrtc::PeerConnectionObserver*> peer_connection = CreatePeerConnection();
    if (!peer_connection.first) {
        UMBO_WARN("setOffser <- Fail to initialize peer connection");
        return;
    }

    // Set SDP offer to the PeerConnection
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc = peer_connection.first;
    pc->SetRemoteDescription(SetSDPObserver::Create(), session_description);

    // Register this peer
    peer_connection_map.insert(std::pair<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >(peerid, peer_connection.first));
    peer_connectionobs_map.insert(std::pair<std::string, webrtc::PeerConnectionObserver*>(peerid, peer_connection.second));

    // Create SDP answer
    webrtc::FakeConstraints constraints;
    constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, false);
    constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio, false);
    pc->CreateAnswer(CreateSDPObserver::Create(pc, jumbo_msg_sender), &constraints);

    UMBO_DBG("setOffser <- peerid: %s", peerid.c_str());
}

std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, webrtc::PeerConnectionObserver*> PeerManager::CreatePeerConnection()
{
    webrtc::PeerConnectionInterface::IceServers servers;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = "stun:" + stunurl;
    servers.push_back(server);

    PeerConnectionObserver *obs = PeerConnectionObserver::Create(jumbo_msg_sender);
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection =
        peer_connection_factory->CreatePeerConnection(servers, NULL, NULL, NULL, obs);
    if (!peer_connection.get()) {
        UMBO_WARN("CreatePeerConnection failed");
        delete obs;
        return std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, webrtc::PeerConnectionObserver*>(NULL, NULL);
    }

    AddStreams(peer_connection);

    return std::pair<rtc::scoped_refptr<webrtc::PeerConnectionInterface>, webrtc::PeerConnectionObserver*>(peer_connection, obs);
}

bool PeerManager::AddStreams(webrtc::PeerConnectionInterface* peer_connection)
{
    if (media_stream.get() == NULL) {

        cricket::VideoCapturer *capturer = OpenVideoCaptureDevice();
        if (!capturer) {
            UMBO_WARN("Cannot create capturer");
            return false;
        }

        // Register video capturer listener
        //VideoCapturerListener listener(capturer);

        // Create media stream
        media_stream = peer_connection_factory->CreateLocalMediaStream(STREAM_LABEL);
        if (!media_stream.get()) {
            UMBO_WARN("Fail to create stream");
            return false;
        }

        // Create video track
        rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
                peer_connection_factory->CreateVideoTrack(VIDEO_LABEL, peer_connection_factory->CreateVideoSource(capturer, NULL))
        );

        // Create audio track
        rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
                peer_connection_factory->CreateAudioTrack(AUDIO_LABEL, peer_connection_factory->CreateAudioSource(NULL))
        );

        if (!media_stream->AddTrack(video_track)) {
            UMBO_WARN("Fail to add video track");
            return false;
        }

        if (!media_stream->AddTrack(audio_track)) {
            UMBO_WARN("Fail to add audio track");
            return false;
        }
    }

    if (!peer_connection->AddStream(media_stream)) {
        UMBO_WARN("Fail to add media stream to PeerConnection");
        return false;
    }

    return true;

}

cricket::VideoCapturer*PeerManager::OpenVideoCaptureDevice()
{
    cricket::VideoCapturer* capturer = NULL;

    rtc::scoped_ptr<cricket::DeviceManagerInterface> dev_manager(cricket::DeviceManagerFactory::Create());
    if (!dev_manager.get() || !dev_manager->Init()) {
        UMBO_WARN("Fail to create device manager");
        return NULL;
    }

    std::vector<cricket::Device> devs;
    if (!dev_manager->GetVideoCaptureDevices(&devs)) {
        UMBO_WARN("Fail to enumerate video devices");
        return NULL;
    }

//#define FAKE_VIDEO
#ifdef  FAKE_VIDEO
    cricket::Device device;
    if (!dev_manager->GetVideoCaptureDevice("YuvFramesGenerator", &device)) {
        UMBO_WARN("Fail to get fake video devices");
        return NULL;
    }

    capturer = dev_manager->CreateVideoCapturer(device);
    if (!capturer) {
        UMBO_WARN("Fail to create fake video device");
        return NULL;
    }
#else
    std::vector<cricket::Device>::iterator dev_it = devs.begin();
    for (; dev_it != devs.end(); ++dev_it) {
        capturer = dev_manager->CreateVideoCapturer(*dev_it);
        if (capturer != NULL)
            break;
    }
#endif

    if (!capturer) {
        UMBO_WARN("Fail to create video capture device");
        return NULL;
    }

    return capturer;
}

void PeerManager::deletePeerConnection(const std::string &peerid)
{
    std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >::iterator it = peer_connection_map.find(peerid);
    if (it != peer_connection_map.end()) {
        it->second = NULL;
        peer_connection_map.erase(it);
    }

    std::map<std::string, webrtc::PeerConnectionObserver*>::iterator obs_it = peer_connectionobs_map.find(peerid);
    if (obs_it == peer_connectionobs_map.end()) {
        PeerConnectionObserver *obs = dynamic_cast<PeerConnectionObserver *>(obs_it->second);
        delete obs;
        peer_connectionobs_map.erase(obs_it);
    }
}

void PeerManager::addIceCandidate(const std::string &peerid, const std::string& message)
{
    UMBO_DBG("addIceCandidate -> peerid: %s, message: %s", peerid.c_str(), message.c_str());

    std::map<std::string, rtc::scoped_refptr<webrtc::PeerConnectionInterface> >::iterator it = peer_connection_map.find(peerid);
    if (it == peer_connection_map.end()) {
        UMBO_WARN("addIceCandidate <- Fail to find the existed peer connection.");
        return;
    }

    Json::Reader reader;
    Json::Value  jmessage;
    if (!reader.parse(message, jmessage)) {
        UMBO_WARN("addIceCandidate <- Received unknown message: %s", message.c_str());
        return;
    }

    std::string sdp_mid, sdp;
    int sdp_mlineindex = 0;
    if (!rtc::GetStringFromJsonObject(jmessage, CANDIDATE_SDP_MID_NAME, &sdp_mid) ||
        !rtc::GetIntFromJsonObject(jmessage, CANDIDATE_SDP_MLINE_INDEX_NAME, &sdp_mlineindex) ||
        !rtc::GetStringFromJsonObject(jmessage, CANDIDATE_SDP_NAME, &sdp)) {
        UMBO_WARN("addIceCandidate <- Fail to parse received message.");
        return;
    }

    rtc::scoped_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp));
    if (!candidate.get()) {
        UMBO_WARN("addIceCandidate <- Fail to parse received candidate message.");
        return;
    }

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc = it->second;
    if (!pc->AddIceCandidate(candidate.get())) {
        UMBO_WARN("addIceCandidate <- Failed to apply the received candidate");
        return;
    }

    UMBO_DBG("addIceCandidate <- peerid: %s", peerid.c_str());
}