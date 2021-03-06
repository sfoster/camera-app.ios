#include <opencv2/opencv.hpp>

#import "bridge.h"

#include "lighthouse.hpp"
#include "exceptions.hpp"
#include "image.hpp"
#include "filesystem.hpp"
#include "player.hpp"

NSObject *sViewController;

// FIXME: What's the ownership of this?
UIImage *
matrixToImage(const cv::Mat &matrix, UIImageOrientation *imageOrientation) {
  UIImageOrientation orientation = imageOrientation ? *imageOrientation : UIImageOrientationUp;
  NSData *data = [NSData dataWithBytes:matrix.data length:matrix.elemSize() * matrix.total()];

  CGColorSpaceRef colorSpace;
  if (matrix.elemSize() == 1) {
    colorSpace = CGColorSpaceCreateDeviceGray();
  } else {
    colorSpace = CGColorSpaceCreateDeviceRGB();
  }

  CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef) data);

  // Creating CGImage from cv::Mat
  CGImageRef imageRef = CGImageCreate(
    // Image width.
    matrix.cols,
    // Image height.
    matrix.rows,
    // Bits per component.
    8,
    // Bits per pixel.
    8 * matrix.elemSize(),
    // Bytes per row.
    matrix.step[0],
    // Color space.
    colorSpace,
    // Bitmap info.
   (matrix.channels() == 4 ? kCGImageAlphaLast : kCGImageAlphaNone) | kCGBitmapByteOrderDefault,
    // CGDataProvider reference.
    provider,
    // Decode.
    NULL,
    // Indicates whether we should interpolate.
    false,
    // Intent.
    kCGRenderingIntentDefault
  );

  // Getting UIImage from CGImage
  UIImage *image = [UIImage imageWithCGImage:imageRef scale:1 orientation:orientation];

  CGImageRelease(imageRef);
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(colorSpace);

  return image;
}


cv::Mat
imageToMatrix(UIImage *image) {
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

  CGFloat cols = image.size.width;
  CGFloat rows = image.size.height;

  cv::Mat matrix(rows, cols, CV_8UC4);
  CGContextRef context = CGBitmapContextCreate(matrix.data,
    cols,
    rows,
    8,
    matrix.step[0],
    colorSpace,
    kCGImageAlphaNoneSkipLast | kCGBitmapByteOrderDefault
  );

  CGContextDrawImage(context, CGRectMake(0, 0, cols, rows), image.CGImage);

  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);

  return matrix;
}


@implementation Bridge

lighthouse::ImageMatchingSettings matchingSettings = {
  .mNumberOfFeatures = 1000,
  .mMinNumberOfFeatures = 50,
  .mMatchingScoreThreshold = 10.0,
  .mRatioTestK = 0.8,
  .mHistogramWeight = 5.0,
};

lighthouse::Lighthouse lighthouseInstance(matchingSettings);

- (UIImage *)DrawKeypoints:(UIImage *)source {
  cv::Mat outputMatrix;
  lighthouseInstance.DrawKeypoints([self imageToMatrix:source], outputMatrix);

  return [self matrixToImage:outputMatrix andImageOrientation:[source imageOrientation]];
}

- (void)PlayVoiceLabel:(NSString *)id {
  lighthouseInstance.PlayVoiceLabel(lighthouseInstance.GetDescription([id UTF8String]));
}

- (void)PlaySound:(NSString *)soundResourceName {
  lighthouse::Player::Play(Filesystem::GetResourcePath([soundResourceName UTF8String], "wav", "sounds"));
}

// Converts UIImage instance into cv::Mat object that is known for OpenCV.
- (cv::Mat)imageToMatrix:(UIImage *)image {
  return imageToMatrix(image);
}


// Converts cv::Mat object into UIImage instance preserving original orientation.
- (UIImage *)matrixToImage:(const cv::Mat &)matrix andImageOrientation:(UIImageOrientation)orientation {
  return matrixToImage(matrix, &orientation);
}

- (void)onRecordObject {
  lighthouseInstance.OnRecordObject();
}

- (void)onIdentifyObject {
  lighthouseInstance.OnIdentifyObject();
}

- (void)onStopCapture {
  fprintf(stderr, "onStopCapture %s", "start");
  lighthouseInstance.StopRecord();
  fprintf(stderr, "onStopCapture %s", "stop");
}
@end


