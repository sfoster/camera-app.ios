//
//  lighthouse.hpp
//  Lighthouse Camera
//
//  Created by Aleh Zasypkin on 19/12/2016.
//  Copyright © 2016 Lighthouse. All rights reserved.
//

#ifndef lighthouse_hpp
#define lighthouse_hpp

#include <stdio.h>
#include <string>
#include <thread>
#include <mutex>
#include <stdatomic.h>
#include <atomic>
#include <condition_variable>


#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>

#include "image_matcher.hpp"
#include "video.hpp"

namespace lighthouse {

enum class Task {
  // Nothing to do.
  WAIT = 0,

  // Record a new object.
  RECORD = 1,
  IDENTIFY = 2,
};

// Describes all possible assets that are related to the image description, but are managed separately.
enum ImageDescriptionAsset {
  // Main binary data for the image description (id, descriptors, keypoints, histogram).
  Data,
  // Image description voice label.
  VoiceLabel,
  // Source image from which image description has been extracted.
  SourceImage
};

class Lighthouse {
public:
  Lighthouse(ImageMatchingSettings aImageMatchingSettings);

  ~Lighthouse();

  void DrawKeypoints(const cv::Mat &aInputFrame, cv::Mat &aOutputFrame);

  ImageDescription GetDescription(const cv::Mat &aInputFrame) const;

  const ImageDescription &GetDescription(const std::string &id) const;

  void SaveDescription(const ImageDescription &aDescription, const cv::Mat &aSourceImage);

  void PlayVoiceLabel(const ImageDescription &aDescription);

  std::vector<std::tuple<float, ImageDescription>> FindMatches(const cv::Mat &aInputFrame) const;

  std::vector<std::tuple<float, ImageDescription>> FindMatches(const ImageDescription &aDescription) const;

  // Start recording a new object.
  void OnRecordObject();

  // Start identifying an existing object.
  void OnIdentifyObject();

  // Stop recording/identifying object.
  void StopRecord();

private:
  // Run the C++ event loop on thread `mVideoThread`.
  //
  // The event loop is NEVER taken down.
  static void AuxRunEventLoop(Lighthouse *);

  void SendMessage(Task aMessage);

  // Actual implementation of the event loop. Runs in `mVideoThread`.
  void RunEventLoop();

  // Actual implementation of recording an object. Runs in `mVideoThread`.
  void RunRecordObject();

  // Actual implementation of identifying an object. Runs in `mVideoThread`.
  void RunIdentifyObject();

  // Returns a file name of the description asset (data, voice label, source image).
  std::string GetDescriptionAssetName(const ImageDescriptionAsset aAsset);

  // Builds a full absolute path the image description's asset based on description id and asset type.
  std::string GetDescriptionAssetPath(const std::string &aDescriptionId, const ImageDescriptionAsset aAsset);

  // A thread designed to run all blocking camera/vision operations.
  std::thread mVideoThread;
  // Representation of the latest `Task` requested from the event loop.
  std::atomic_int mTask;
  // A stamp incremented each time we send a message to the event loop.
  // Protected by mTaskMutex.
  uint64_t mTaskStamp;
  std::mutex mTaskMutex;
  // Condition variable used to communicate with mCaptureThread.
  std::condition_variable mTaskCondition;
  // The camera. Access only on mVideoThread.
  Camera mCamera;

  ImageMatcher mImageMatcher;
  std::string mDbFolderPath;
};

} // namespace lighthouse

#endif /* lighthouse_hpp */
