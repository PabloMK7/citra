// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#import <AVFoundation/AVFoundation.h>

#include "citra_qt/usage_authorization.h"
#include "common/logging/log.h"

namespace AppleAuthorization {

static bool authorized = false;

enum class AuthMediaType {
    Camera,
    Microphone
};

// Based on https://developer.apple.com/documentation/avfoundation/cameras_and_media_capture/requesting_authorization_for_media_capture_on_macos
void CheckAuthorization(AuthMediaType type) {
    if (@available(macOS 10.14, *)) {
        NSString *media_type;
        if(type == AuthMediaType::Camera) {
            media_type = AVMediaTypeVideo;
        }
        else {
            media_type = AVMediaTypeAudio;
        }

        // Request permission to access the camera and microphone.
        switch ([AVCaptureDevice authorizationStatusForMediaType:media_type]) {
            case AVAuthorizationStatusAuthorized:
                // The user has previously granted access to the camera.
                authorized = true;
                break;
            case AVAuthorizationStatusNotDetermined:
            {
                // The app hasn't yet asked the user for camera access.
                [AVCaptureDevice requestAccessForMediaType:media_type completionHandler:^(BOOL) {
                    authorized = true;
                }];
                if(type == AuthMediaType::Camera) {
                    LOG_INFO(Frontend, "Camera access requested.");
                }
                break;
            }
            case AVAuthorizationStatusDenied:
            {
                // The user has previously denied access.
                authorized = false;
                if(type == AuthMediaType::Camera) {
                    LOG_WARNING(Frontend, "Camera access denied. To change this you may modify the macos system settings for Citra at 'System Preferences -> Security & Privacy -> Camera'");
                }
                return;
            }
            case AVAuthorizationStatusRestricted:
            {
                // The user can't grant access due to restrictions.
                authorized = false;
                return;
            }
        }
    }
    else {
        authorized = true;
    }
}

bool CheckAuthorizationForCamera() {
    CheckAuthorization(AuthMediaType::Camera);
    return authorized;
}

bool CheckAuthorizationForMicrophone() {
    CheckAuthorization(AuthMediaType::Microphone);
    return authorized;
}

} //AppleAuthorization
