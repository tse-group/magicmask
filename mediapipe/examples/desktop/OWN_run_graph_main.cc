// Copyright 2019 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// An example of sending OpenCV webcam frames into a MediaPipe graph.


// sources:
// * how to extract features: https://github.com/google/mediapipe/issues/200
// * hand landmark indices: https://github.com/google/mediapipe/issues/119


#include <cstdlib>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string>


#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/detection.pb.h"
#include "mediapipe/framework/port/commandlineflags.h"
#include "mediapipe/framework/port/file_helpers.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"


constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "face___output_video";
constexpr char kOutputHandLandmarks[] = "multi_hand_landmarks";
constexpr char kOutputFaceDetections[] = "face___output_detections";
constexpr char kWindowName[] = "Magic Mask";


DEFINE_string(
    calculator_graph_config_file, "",
    "Name of file containing text format CalculatorGraphConfig proto.");

DEFINE_bool(
    notifications_enabled, true,
    "Enable sending notifications to the frontend.");

DEFINE_bool(
    show_video, true,
    "Enable showing the video.");

DEFINE_string(
    notifications_host, "127.0.0.1",
    "Host name to send notifications to the frontend.");

// DEFINE_integer(
//     notifications_port, 4221,
//     "Port to send notifications to the frontend.");
DEFINE_string(
    notifications_port_str, "4221",
    "Port to send notifications to the frontend.");

DEFINE_string(input_video_path, "",
              "Full path of video to load. "
              "If not provided, attempt to use a webcam.");

DEFINE_string(output_video_path, "",
              "Full path of where to save result (.mp4 only). "
              "If not provided, show result in a window.");


::mediapipe::Status RunMPPGraph() {
  std::string calculator_graph_config_contents;
  std::string socket_host_str, socket_port_str;


  MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
      FLAGS_calculator_graph_config_file, &calculator_graph_config_contents));
  LOG(INFO) << "Get calculator graph config contents: "
            << calculator_graph_config_contents;

  mediapipe::CalculatorGraphConfig config =
      mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
          calculator_graph_config_contents);

  LOG(INFO) << "Initialize the calculator graph.";
  mediapipe::CalculatorGraph graph;
  MP_RETURN_IF_ERROR(graph.Initialize(config));


  LOG(INFO) << "Initialize the camera or load the video.";
  cv::VideoCapture capture;
  const bool load_video = !FLAGS_input_video_path.empty();
  if (load_video) {
    capture.open(FLAGS_input_video_path);
  } else {
    capture.open(0);
  }
  RET_CHECK(capture.isOpened());

  cv::VideoWriter writer;
  //const bool save_video = !FLAGS_output_video_path.empty();
  if (FLAGS_show_video) {
    cv::namedWindow(kWindowName, /*flags=WINDOW_AUTOSIZE*/ 1);
#if (CV_MAJOR_VERSION >= 3) && (CV_MINOR_VERSION >= 2)
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 30);
#endif
  }


  LOG(INFO) << "Start running the calculator graph.";
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_output_video,
                   graph.AddOutputStreamPoller(kOutputStream));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_output_hand_landmarks,
                   graph.AddOutputStreamPoller(kOutputHandLandmarks));
  ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller_output_face_detections,
                   graph.AddOutputStreamPoller(kOutputFaceDetections));
  MP_RETURN_IF_ERROR(graph.StartRun({}));


  // connect to notification port
  int notifications_socket = 0;
  if(FLAGS_notifications_enabled) {

    // TODO: Once DEFINE_integer() works, this can be removed!
    int FLAGS_notifications_port = std::stoi(FLAGS_notifications_port_str);

    LOG(INFO) << "Notification connection: " << FLAGS_notifications_host << ":" << FLAGS_notifications_port;
    const char *socket_host = FLAGS_notifications_host.c_str();

    if ((notifications_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    // if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      LOG(ERROR) << "Socket creation error!";
      RET_CHECK_FAIL();
    }
   
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(FLAGS_notifications_port);
       
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, socket_host, &serv_addr.sin_addr) <= 0) {
      LOG(ERROR) << "Invalid address or address type not supported!";
      RET_CHECK_FAIL();
    }
    
    if(connect(notifications_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      LOG(ERROR) << "Connection failed!";
      RET_CHECK_FAIL();
    }


    LOG(INFO) << "Connection to GUI for notifications established.";
  }


  LOG(INFO) << "Start grabbing and processing frames.";
  bool grab_frames = true;

  while (grab_frames) {

    // Capture opencv camera or video frame.
    cv::Mat camera_frame_raw;
    capture >> camera_frame_raw;
    if (camera_frame_raw.empty()) break;  // End of video.
    cv::Mat camera_frame;
    cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGB);
    if (!load_video) {
      cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);
    }


    float IMG_WIDTH = camera_frame_raw.size[1];
    float IMG_HEIGHT = camera_frame_raw.size[0];


    // Wrap Mat into an ImageFrame.
    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, camera_frame.cols, camera_frame.rows,
        mediapipe::ImageFrame::kDefaultAlignmentBoundary);
    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
    camera_frame.copyTo(input_frame_mat);


    // Send image packet into the graph.
    size_t frame_timestamp_us =
        (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
        kInputStream, mediapipe::Adopt(input_frame.release())
                          .At(mediapipe::Timestamp(frame_timestamp_us))));


    // Get the graph result packet, or stop if that fails.
    mediapipe::Packet packet;

    if (!poller_output_video.Next(&packet)) break;
    auto& output_frame = packet.Get<mediapipe::ImageFrame>();

    // Convert back to opencv for display or saving.
    cv::Mat output_frame_mat = mediapipe::formats::MatView(&output_frame);
    cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);
    if (FLAGS_show_video) {
      cv::imshow(kWindowName, output_frame_mat);
      cv::waitKey(5);
    }


    // Store hand and face points
    auto points_hand = std::vector<std::tuple<float,float,int>>();
    auto points_face = std::vector<std::tuple<float,float,int>>();

    // Retrieve and process hand landmarks
    if (!poller_output_hand_landmarks.Next(&packet)) break;
    auto& output_hand_landmarks = packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();

    LOG(INFO) << "# DETECTED HANDS: " << output_hand_landmarks.size();
    for(int i_hand = 0; i_hand < output_hand_landmarks.size(); i_hand++) {
      LOG(INFO) << "- hand " << i_hand;
      const mediapipe::NormalizedLandmarkList& landmarklist = output_hand_landmarks[i_hand];
      assert(landmarklist.landmark().size() == 21);
      for(int i_finger = 0; i_finger < 5; i_finger++) {
        LOG(INFO) << "  - finger tip " << i_finger;
        const mediapipe::NormalizedLandmark& landmark = landmarklist.landmark((i_finger+1)*4);
        LOG(INFO) << "    (x, y, z) = (" << landmark.x() << ", " << landmark.y() << ", " << landmark.z() << ")";
        points_hand.push_back({landmark.x(), landmark.y(), i_finger});
      }
    }


    // Retrieve and process face detections
    if (!poller_output_face_detections.Next(&packet)) break;
    auto& output_face_detections = packet.Get<std::vector<mediapipe::Detection>>();

    LOG(INFO) << "# DETECTED FACES: " << output_face_detections.size();
    for(int i_face = 0; i_face < output_face_detections.size(); i_face++) {
      LOG(INFO) << "- face " << i_face;
      const mediapipe::Detection& detection = output_face_detections[i_face];
      const mediapipe::LocationData& locationdata = detection.location_data();
      assert(locationdata.relative_keypoints().size() == 6);
      for(int i_keypoint = 0; i_keypoint < 6; i_keypoint++) {
        if(i_keypoint == 4 || i_keypoint == 5) {
          // TODO: skip ears for now, as they create false-positives without much added value
          continue;
        }

        LOG(INFO) << "  - key point " << i_keypoint;
        const mediapipe::LocationData_RelativeKeypoint& keypoint = locationdata.relative_keypoints(i_keypoint);
        // auto& keypoint = locationdata.relative_keypoints(i_keypoint);
        LOG(INFO) << "    (x, y, label, score) = (" << keypoint.x() << ", " << keypoint.y() << ", " << keypoint.keypoint_label() << ", " << keypoint.score() << ")";
        points_face.push_back({keypoint.x(), keypoint.y(), i_keypoint});
      }
    }


    // Retrieve hand and face landmarks and check for unallowed proximity
    bool is_proximity_detected = false;
    int is_proximity_detected_hand_keypoint;
    int is_proximity_detected_face_keypoint;
    const float PROXIMITY_THRESHOLD = 0.05 * 0.05;
    const float PROXIMITY_SCALE_X = IMG_WIDTH / IMG_HEIGHT; //1.0;
    const float PROXIMITY_SCALE_Y = 1.0;

    for(int i_face = 0; i_face < points_face.size(); i_face++) {
      for(int i_hand = 0; i_hand < points_hand.size(); i_hand++) {
        auto pt_face = points_face[i_face];
        auto pt_hand = points_hand[i_hand];

        if(powf(PROXIMITY_SCALE_X * (std::get<0>(pt_face) - std::get<0>(pt_hand)), 2) + powf(PROXIMITY_SCALE_Y * (std::get<1>(pt_face) - std::get<1>(pt_hand)), 2) <= PROXIMITY_THRESHOLD) {
          is_proximity_detected = true;
          is_proximity_detected_hand_keypoint = std::get<2>(pt_hand);
          is_proximity_detected_face_keypoint = std::get<2>(pt_face);
        }

        if(is_proximity_detected) { break; }
      }

      if(is_proximity_detected) { break; }
    }

    if(is_proximity_detected) {
      LOG(INFO) << "PROXIMITY DETECTED!";
      char msg[128];
      strcpy(msg, "");
      switch(is_proximity_detected_hand_keypoint) {
        case 0:
          strcat(msg, "thumb");
          break;
        case 1:
          strcat(msg, "index finger");
          break;
        case 2:
          strcat(msg, "middle finger");
          break;
        case 3:
          strcat(msg, "ring finger");
          break;
        case 4:
          strcat(msg, "baby finger");
          break;
        default:
          LOG(INFO) << "WARNING -- UNKNOWN HAND KEYPOINT!";
          break;
      }
      strcat(msg, ",");
      switch(is_proximity_detected_face_keypoint) {
        case 0:
          strcat(msg, "left eye");
          break;
        case 1:
          strcat(msg, "right eye");
          break;
        case 2:
          strcat(msg, "nose");
          break;
        case 3:
          strcat(msg, "mouth");
          break;
        case 4:
          strcat(msg, "left ear");
          break;
        case 5:
          strcat(msg, "right ear");
          break;
        default:
          LOG(INFO) << "WARNING -- UNKNOWN FACE KEYPOINT!";
          break;
      }
      strcat(msg, "\n");
      LOG(INFO) << msg;

      if(FLAGS_notifications_enabled) {
        send(notifications_socket, msg, strlen(msg), 0);
      }
    }
  }


  LOG(INFO) << "Shutting down.";
  if (FLAGS_show_video) {
      if (writer.isOpened()) writer.release();
  }
  MP_RETURN_IF_ERROR(graph.CloseInputStream(kInputStream));
  return graph.WaitUntilDone();
}


int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  ::mediapipe::Status run_status = RunMPPGraph();
  if (!run_status.ok()) {
    LOG(ERROR) << "Failed to run the graph: " << run_status.message();
    return EXIT_FAILURE;
  } else {
    LOG(INFO) << "Success!";
  }
  return EXIT_SUCCESS;
}
