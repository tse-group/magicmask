#!/bin/bash

cp frontend mask.app/Contents/MacOS/mask.app
cp '../bazel-bin/mediapipe/examples/desktop/OWN_facehand_tracking/OWN_facehand_tracking' 'mask.app/Contents/MacOS/MagicMask'
chmod 755 'mask.app/Contents/MacOS/MagicMask'
cp ../mediapipe/graphs/OWN_facehand_tracking/OWN_facehand_tracking_desktop_live.pbtxt mask.app/Contents/MacOS/OWN_facehand_tracking_desktop_live.pbtxt
cp ../mediapipe/models/face_detection_front.tflite mask.app/Contents/MacOS/mediapipe/models/face_detection_front.tflite
cp ../mediapipe/models/palm_detection.tflite mask.app/Contents/MacOS/mediapipe/models/palm_detection.tflite
cp ../mediapipe/models/face_detection_front_labelmap.txt mask.app/Contents/MacOS/mediapipe/models/face_detection_front_labelmap.txt
cp ../mediapipe/models/palm_detection_labelmap.txt mask.app/Contents/MacOS/mediapipe/models/palm_detection_labelmap.txt
cp ../mediapipe/models/hand_landmark.tflite mask.app/Contents/MacOS/mediapipe/models/hand_landmark.tflite

cd mask.app/Contents/MacOS
dylibbundler -x 'MagicMask' -b -cd
rm -rf ../libs
mv libs ..
