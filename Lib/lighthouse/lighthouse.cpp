//
//  lighthouse.cpp
//  Lighthouse Camera
//
//  Created by Aleh Zasypkin on 19/12/2016.
//  Copyright © 2016 Lighthouse. All rights reserved.
//

#include "feedback.hpp"
#include "filesystem.hpp"
#include "lighthouse.hpp"
#include "matching/exceptions.hpp"
#include "player.hpp"
#include "recorder.hpp"

namespace lighthouse {

Lighthouse::Lighthouse(ImageMatchingSettings aImageMatchingSettings)
    : mImageMatcher(ImageMatcher(aImageMatchingSettings)),
      mCamera(),
      mDescriptions(),
      mDbFolderPath(),
      mVideoThread() {
  // Create Data directory if it doesn't exist.
  mDbFolderPath = Filesystem::GetRoot() + "/Data/";
  Filesystem::CreateDirectory(mDbFolderPath);

  fprintf(stderr, "Lighthouse::Lighthouse() data folder is at %s.\n", mDbFolderPath.c_str());

  // Iterate through all sub folders, every folder should contain the following files:
  // 1. description.bin - binary serialized image description (keypoints, descriptors, histogram etc.);
  // 2. frame.bin - binary serialized image matrix. Optional, can be disabled;
  // 3. short-audio.wav - short voice label;
  // 4. long-audio.wav - long voice label.
  std::vector<std::string> subFolders = Filesystem::GetSubFolders(mDbFolderPath);
  for (std::string descriptionFolderPath : subFolders) {
    try {
      mImageMatcher.AddToDB(ImageDescription::Load(descriptionFolderPath + "/description.bin"));
    } catch(const cereal::Exception& e) {
      fprintf(stderr, "Lighthouse::Lighthouse() couldn't deserialize description at %s (reason: %s). Skipping...\n",
          descriptionFolderPath.c_str(), e.what());
    }
  }

  fprintf(stderr, "Lighthouse::Lighthouse() loaded %lu image description(s).\n", subFolders.size());

  // Start event loop.
  std::thread thread(Lighthouse::AuxRunEventLoop, this);
  mVideoThread.swap(thread);
}

Lighthouse::~Lighthouse() {
  StopRecord();
  mVideoThread.join();
}

void Lighthouse::DrawKeypoints(const cv::Mat &aInputFrame, cv::Mat &aOutputFrame) {
  ImageDescription description = mImageMatcher.GetDescription(aInputFrame);

  // We can't draw keypoints on the BGRA image.
  cv::Mat bgrInputFrame;
  cvtColor(aInputFrame, bgrInputFrame, cv::COLOR_BGRA2BGR);

  drawKeypoints(bgrInputFrame, description.GetKeypoints(), aOutputFrame, cv::Scalar::all(-1),
      cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
}

ImageDescription Lighthouse::GetDescription(const cv::Mat &aInputFrame) const {
  return mImageMatcher.GetDescription(aInputFrame);
}

const ImageDescription &Lighthouse::GetDescription(const std::string &id) const {
  return mImageMatcher.GetDescription(id);
}

void Lighthouse::SaveDescription(const ImageDescription &aDescription) {
  const std::string descriptionFolderPath = mDbFolderPath + aDescription.GetId();
  Filesystem::CreateDirectory(descriptionFolderPath);

  Player::Play(Filesystem::GetResourcePath("after-the-tone", "wav", "sounds"));

  Player::Play(Filesystem::GetResourcePath("beep", "wav", "sounds"));
  Recorder::Record(descriptionFolderPath + "/voice-label.aiff");
  Player::Play(Filesystem::GetResourcePath("beep", "wav", "sounds"));

  ImageDescription::Save(aDescription, descriptionFolderPath + "/description.bin");
  mImageMatcher.AddToDB(aDescription);

  Player::Play(Filesystem::GetResourcePath("registered", "wav", "sounds"));

  PlayVoiceLabel(aDescription);
}

void Lighthouse::PlayVoiceLabel(const ImageDescription &aDescription) {
  Player::Play(mDbFolderPath + aDescription.GetId() + "/voice-label.aiff");
}

std::vector<std::tuple<float, ImageDescription>> Lighthouse::FindMatches(const cv::Mat &aInputFrame) const {
  return FindMatches(GetDescription(aInputFrame));
}

std::vector<std::tuple<float, ImageDescription>> Lighthouse::FindMatches(const ImageDescription &aDescription) const {
  return mImageMatcher.FindMatches(aDescription);
}

void Lighthouse::OnRecordObject() {
  SendMessage(Task::RECORD);
}

void Lighthouse::OnIdentifyObject() {
  SendMessage(Task::IDENTIFY);
}

void Lighthouse::StopRecord() {
  SendMessage(Task::WAIT);
}

void Lighthouse::SendMessage(lighthouse::Task aMessage) {
  int message = (int) aMessage;
  fprintf(stderr, "Lighthouse::SendMessage(%d) to loop\n", message);
  std::unique_lock<std::mutex> lock(mTaskMutex);
  mTask.store(message);
  mTaskStamp += 1;
  mTaskCondition.notify_one();
  fprintf(stderr, "Lighthouse::SendMessage(%d) to loop done\n", message);
}

/*static*/void
Lighthouse::AuxRunEventLoop(Lighthouse *self) {
  self->RunEventLoop();
}

void Lighthouse::RunIdentifyObject() {
  assert(std::this_thread::get_id() == mVideoThread.get_id());
  // Start recording. `mCamera` is in charge of stopping itself if `mTask` stops being `Task::IDENTIFY`.
  cv::Mat source;
  if (!mCamera.CaptureForIdentification(&mTask, source)) {
    // FIXME: Somehow report error.
    return;
  }

  // Extract comparison points.
  ImageDescription sourceDescription;
  try {
    sourceDescription = GetDescription(source);
  } catch (ImageQualityException e) {
    fprintf(stderr, "Lighthouse::RunIdentifyObject() encountered an error\n");
    return; // FIXME: Report actual error.
  }

  // Compare with existing images.
  std::vector<std::tuple<float, ImageDescription>> matches = FindMatches(sourceDescription);
  
  if (matches.empty()) {
    Feedback::PlaySound("no-item");
    // FIXME: Display something.
  } else {
    ImageDescription match = std::get<1>(matches[0]);
    // FIXME: Display something.
    Feedback::PlayVoiceLabel(match.GetId());
  }
}

void Lighthouse::RunRecordObject() {
  assert(std::this_thread::get_id() == mVideoThread.get_id());
  // Start recording. `mCamera` is in charge of stopping itself if `mTask` stops being `Task::RECORD`.
  cv::Mat source;
  if (!mCamera.CaptureForRecord(&mTask, source)) {
    // FIXME: Somehow report error.
    return;
  }

  // Extract comparison points.
  ImageDescription sourceDescription;
  try {
    sourceDescription = GetDescription(source);
  } catch (ImageQualityException e) {
    fprintf(stderr, "Lighthouse::RunRecordObject() encountered an error\n");
    return; // FIXME: Report actual error.
  }

  SaveDescription(sourceDescription);
}

void Lighthouse::RunEventLoop() {
  assert(std::this_thread::get_id() == mVideoThread.get_id());
  // Stamp of the latest message received.
  uint64_t stamp = 0;
  while (true) {
    fprintf(stderr, "Lighthouse::RunEventLoop() looping\n");
    int task = 0;
    do {
      // While mTaskCondition is atomic, we still need a lock for the sake of the condition.
      std::unique_lock<std::mutex> lock(mTaskMutex);
      fprintf(stderr, "Lighthouse::RunEventLoop() checking %llu == %llu\n", mTaskStamp, stamp);
      if (mTaskStamp == stamp) {
        // No message has arrived while we were waiting. Go to sleep.
        fprintf(stderr, "Lighthouse::RunEventLoop() going to sleep\n");
        mTaskCondition.wait(lock);
      }
      stamp = mTaskStamp;
      task = mTask;
    } while (false); // Just a scope.
    switch (task) {
      case (int) Task::WAIT:
        // Nothing to do.
        continue;
      case (int) Task::RECORD:
        RunRecordObject();
        Feedback::OperationComplete();
        continue;
      case (int) Task::IDENTIFY:
        RunIdentifyObject();
        Feedback::OperationComplete();
        continue;
      default:
        assert(false);
        continue;
    }
  }
}

} // namespace lighthouse
