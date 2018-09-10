/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peerconnectionwrapper.h"

#include <memory>
#include <string>
#include <utility>

#include "api/jsepsessiondescription.h"
#include "media/base/fakevideocapturer.h"
#include "pc/sdputils.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

namespace {
const uint32_t kWaitTimeout = 10000U;
}

PeerConnectionWrapper::PeerConnectionWrapper(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer)
    : pc_factory_(pc_factory), observer_(std::move(observer)), pc_(pc) {
  RTC_DCHECK(pc_factory_);
  RTC_DCHECK(pc_);
  RTC_DCHECK(observer_);
  observer_->SetPeerConnectionInterface(pc_.get());
}

PeerConnectionWrapper::~PeerConnectionWrapper() = default;

PeerConnectionFactoryInterface* PeerConnectionWrapper::pc_factory() {
  return pc_factory_.get();
}

PeerConnectionInterface* PeerConnectionWrapper::pc() {
  return pc_.get();
}

MockPeerConnectionObserver* PeerConnectionWrapper::observer() {
  return observer_.get();
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOffer() {
  return CreateOffer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    std::string* error_out) {
  return CreateSdp(
      [this, options](CreateSessionDescriptionObserver* observer) {
        pc()->CreateOffer(observer, options);
      },
      error_out);
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOfferAndSetAsLocal() {
  return CreateOfferAndSetAsLocal(
      PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOfferAndSetAsLocal(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  auto offer = CreateOffer(options);
  if (!offer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(CloneSessionDescription(offer.get())));
  return offer;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer() {
  return CreateAnswer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    std::string* error_out) {
  return CreateSdp(
      [this, options](CreateSessionDescriptionObserver* observer) {
        pc()->CreateAnswer(observer, options);
      },
      error_out);
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswerAndSetAsLocal() {
  return CreateAnswerAndSetAsLocal(
      PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswerAndSetAsLocal(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  auto answer = CreateAnswer(options);
  if (!answer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(CloneSessionDescription(answer.get())));
  return answer;
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateSdp(
    std::function<void(CreateSessionDescriptionObserver*)> fn,
    std::string* error_out) {
  rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
  fn(observer);
  EXPECT_EQ_WAIT(true, observer->called(), kWaitTimeout);
  if (error_out && !observer->result()) {
    *error_out = observer->error();
  }
  return observer->MoveDescription();
}

bool PeerConnectionWrapper::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* error_out) {
  return SetSdp(
      [this, &desc](SetSessionDescriptionObserver* observer) {
        pc()->SetLocalDescription(observer, desc.release());
      },
      error_out);
}

bool PeerConnectionWrapper::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* error_out) {
  return SetSdp(
      [this, &desc](SetSessionDescriptionObserver* observer) {
        pc()->SetRemoteDescription(observer, desc.release());
      },
      error_out);
}

bool PeerConnectionWrapper::SetSdp(
    std::function<void(SetSessionDescriptionObserver*)> fn,
    std::string* error_out) {
  rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  fn(observer);
  EXPECT_EQ_WAIT(true, observer->called(), kWaitTimeout);
  if (error_out && !observer->result()) {
    *error_out = observer->error();
  }
  return observer->result();
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnectionWrapper::AddAudioTrack(
    const std::string& track_label,
    std::vector<MediaStreamInterface*> streams) {
  auto media_stream_track =
      pc_factory()->CreateAudioTrack(track_label, nullptr);
  return pc()->AddTrack(media_stream_track, streams);
}

rtc::scoped_refptr<RtpSenderInterface> PeerConnectionWrapper::AddVideoTrack(
    const std::string& track_label,
    std::vector<MediaStreamInterface*> streams) {
  auto video_source = pc_factory()->CreateVideoSource(
      rtc::MakeUnique<cricket::FakeVideoCapturer>());
  auto media_stream_track =
      pc_factory()->CreateVideoTrack(track_label, video_source);
  return pc()->AddTrack(media_stream_track, streams);
}

PeerConnectionInterface::SignalingState
PeerConnectionWrapper::signaling_state() {
  return pc()->signaling_state();
}

bool PeerConnectionWrapper::IsIceGatheringDone() {
  return observer()->ice_complete_;
}

}  // namespace webrtc
