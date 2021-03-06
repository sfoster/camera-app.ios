//
//  feedback.hpp
//  Lighthouse Camera
//
//  Created by David Teller on 06/01/17.
//  Copyright © 2017 Lighthouse. All rights reserved.
//
//
// C++ callbacks (implemented in Obj-C) to let C++ code provide visual/auditory feedback.


#ifndef feedback_hpp
#define feedback_hpp

#include <stdio.h>

#include <opencv2/opencv.hpp> // Must be imported before Cocoa


class Feedback {
public:
  static void ShowLabel(const char* info);
  static void ReceivedFrame(const char* info, cv::Mat& frame);
  static void OperationComplete();

  static void CameraSnap();

  static void CannotTakePicture();

  static void SetFlashLight(bool isOn); // FIXME: Implement

  static void PlaySoundNamed(const std::string &aSoundName);
  static void PlaySound(const std::string &aSoundPath);
  static void PlaySound(const std::string &aSoundPath, float aVolume);
};

#endif /* feedback_hpp */
