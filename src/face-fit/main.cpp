// CSIRO has filed various patents which cover the Software. 

// CSIRO grants to you a license to any patents granted for inventions
// implemented by the Software for academic, research and non-commercial
// use only.

// CSIRO hereby reserves all rights to its inventions implemented by the
// Software and any patents subsequently granted for those inventions
// that are not expressly granted to you.  Should you wish to license the
// patents relating to the Software for commercial use please contact
// CSIRO IP & Licensing, Gautam Tendulkar (gautam.tendulkar@csiro.au) or
// Nick Marsh (nick.marsh@csiro.au)

// This software is provided under the CSIRO OPEN SOURCE LICENSE
// (GPL2) which can be found in the LICENSE file located in the top
// most directory of the source code.

#include "utils/helpers.hpp"
#include "utils/command-line-arguments.hpp"
#include "tracker/FaceTracker.hpp"
#include <opencv2/highgui/highgui.hpp>

using namespace FACETRACKER;

void
print_usage()
{
  std::string text =
    "Usage: [options] <image-argument> [landmarks-argument]\n"
    "\n"
    "Options:\n"
    "  --help                    This helpful message.\n"    
    "  --lists                   Switch to list processing mode. See below.\n"
    "  --video                   Switch to video processing mode. See below.\n"
    "  --wait-time <double>      How many seconds to wait when displaying the\n"
    "                            registration results. The default depends on the mode.\n"
    "  --model <pathname>        The pathname to the tracker model to use.\n"
    "  --params <pathname>       The pathname to the parameters to use.\n"
    "  --threshold <int>         The threshold of the error detector.\n"
    "                            Can range from 0 to 10 where 10 is extremely picky.\n"
    "                            The default is 5.\n"
    "  --title <string>          The window title to use.\n"                  
    "  --verbose                 Display information whilst processing.\n"
    "\n"
    "Default mode:\n"
    "Perform fitting on an image located at <image-argument> and save\n"
    "the results to [landarks-argument] if specified, otherwise display\n"
    "the results.\n"
    "\n"
    "List mode:\n"
    "Perform fitting on the list of image pathnames contained in <image-argument>.\n"
    "If [landmarks-argument] is specified, then it must be a list with the same length as\n"
    "<image-argument> and contain pathnames to write the tracked points to.\n"
    "\n"
    "Video mode:\n"
    "Perform fitting on the video found at <image-pathname>. If\n"  
    "[landmarks-argument] is specified, then it represents a format string\n"
    "used by sprintf. The template must accept at most one unsigned integer\n"
    "value. If no [landmarks-argument] is given, then the tracking is displayed\n"
    "to the screen.\n"
    "\n";
  
  std::cout << text << std::endl;
}

class user_pressed_escape : public std::exception
{
public:
};

struct Configuration
{
  double wait_time;
  std::string model_pathname;
  std::string params_pathname;
  int tracking_threshold;
  std::string window_title;
  bool verbose;

  int circle_radius;
  int circle_thickness;
  int circle_linetype;
  int circle_shift;
};

int run_lists_mode(const Configuration &cfg,
		   const CommandLineArgument<std::string> &image_argument,
		   const CommandLineArgument<std::string> &landmarks_argument);

int run_video_mode(const Configuration &cfg,
		   const CommandLineArgument<std::string> &image_argument,
		   const CommandLineArgument<std::string> &landmarks_argument);

int run_image_mode(const Configuration &cfg,
		   const CommandLineArgument<std::string> &image_argument,
		   const CommandLineArgument<std::string> &landmarks_argument);

void display_image_and_points(const Configuration &cfg,
			      const cv::Mat &image,
			      const std::vector<cv::Point_<double> > &points);

int
run_program(int argc, char **argv)
{
  CommandLineArgument<std::string> image_argument;
  CommandLineArgument<std::string> landmarks_argument;

  bool lists_mode = false;
  bool video_mode = false;
  bool wait_time_specified = false;

  Configuration cfg;
  cfg.wait_time = 0;
  cfg.model_pathname = DefaultFaceTrackerModelPathname();
  cfg.params_pathname = DefaultFaceTrackerParamsPathname();
  cfg.tracking_threshold = 5;
  cfg.window_title = "CSIRO Face Fit";
  cfg.verbose = false;
  cfg.circle_radius = 2;
  cfg.circle_thickness = 1;
  cfg.circle_linetype = 8;
  cfg.circle_shift = 0;

  for (int i = 1; i < argc; i++) {
    std::string argument(argv[i]);

    if ((argument == "--help") || (argument == "-h")) {
      print_usage();
      return 0;
    } else if (argument == "--lists") {
      lists_mode = true;
    } else if (argument == "--video") {
      video_mode = true;
    } else if (argument == "--wait-time") {
      wait_time_specified = true;
      cfg.wait_time = get_argument<double>(&i, argc, argv);
    } else if (argument == "--model") {
      cfg.model_pathname = get_argument(&i, argc, argv);
    } else if (argument == "--params") {
      cfg.params_pathname = get_argument(&i, argc, argv);
    } else if (argument == "--title") {
      cfg.window_title = get_argument(&i, argc, argv);
    } else if (argument == "--threshold") {
      cfg.tracking_threshold = get_argument<int>(&i, argc, argv);
    } else if (argument == "--verbose") {
      cfg.verbose = true;
    } else if (!assign_argument(argument, image_argument, landmarks_argument)) {
      throw make_runtime_error("Unable to process argument '%s'", argument.c_str());
    }
  }

  if (!have_argument_p(image_argument)) {
    print_usage();
    return 0;
  }

  if (lists_mode && video_mode)
    throw make_runtime_error("The operator is confused as the switches --lists and --video are present on the command line.");

  if (lists_mode) {
    if (!wait_time_specified)
      cfg.wait_time = 1.0 / 30;
    return run_lists_mode(cfg, image_argument, landmarks_argument);
  } else if (video_mode) {
    if (!wait_time_specified)
      cfg.wait_time = 1.0 / 30;
    return run_video_mode(cfg, image_argument, landmarks_argument);
  } else {
    if (!wait_time_specified) 
      cfg.wait_time = 0;      
    return run_image_mode(cfg, image_argument, landmarks_argument);			  
  }

  return 0;
}

int
main(int argc, char **argv)
{
  try {
    return run_program(argc, argv);
  } catch (user_pressed_escape &e) {
    std::cout << "Stopping prematurely." << std::endl;
    return 1;
  } catch (std::exception &e) {
    std::cerr << "Caught unhandled exception: " << e.what() << std::endl;
    return 2;
  }
}

// Helpers
int
run_lists_mode(const Configuration &cfg,
	       const CommandLineArgument<std::string> &image_argument,
	       const CommandLineArgument<std::string> &landmarks_argument)
{
  FaceTracker * tracker = LoadFaceTracker(cfg.model_pathname.c_str());
  FaceTrackerParams *tracker_params  = LoadFaceTrackerParams(cfg.params_pathname.c_str());

  std::list<std::string> image_pathnames = read_list(image_argument->c_str());
  std::list<std::string> landmark_pathnames;
  if (have_argument_p(landmarks_argument)) {
    landmark_pathnames = read_list(landmarks_argument->c_str());
    if (landmark_pathnames.size() != image_pathnames.size())
      throw make_runtime_error("Number of pathnames in list '%s' does not match the number in '%s'",
			       image_argument->c_str(), landmarks_argument->c_str());
  }

  std::list<std::string>::const_iterator image_it     = image_pathnames.begin();
  std::list<std::string>::const_iterator landmarks_it = landmark_pathnames.begin();
  const int number_of_images = image_pathnames.size();
  int current_image_index = 1;

  for (; image_it != image_pathnames.end(); image_it++) {
    if (cfg.verbose) {
      printf(" Image %d/%d\r", current_image_index, number_of_images);    
      fflush(stdout);
    }
    current_image_index++;

    cv::Mat image;
    cv::Mat_<uint8_t> gray_image = load_grayscale_image(image_it->c_str(), &image);
    int result = tracker->NewFrame(gray_image, tracker_params);

    std::vector<cv::Point_<double> > shape;
    if (result >= cfg.tracking_threshold) {
      shape = tracker->getShape();
    } else {
      tracker->Reset();
    }

    if (!have_argument_p(landmarks_argument)) {
      display_image_and_points(cfg, image, shape);    
    } else if (shape.size() > 0) {
      IO::SavePts(landmarks_it->c_str(), shape);
      if (cfg.verbose)
	display_image_and_points(cfg, image, shape);
    } else if (cfg.verbose) {
      display_image_and_points(cfg, image, shape);
    }

    if (have_argument_p(landmarks_argument))
      landmarks_it++;
  }  

  delete tracker;
  delete tracker_params; 
  
  return 0;
}

int
run_video_mode(const Configuration &cfg,
	       const CommandLineArgument<std::string> &image_argument,
	       const CommandLineArgument<std::string> &landmarks_argument)
{
  FaceTracker *tracker = LoadFaceTracker(cfg.model_pathname.c_str());
  FaceTrackerParams *tracker_params = LoadFaceTrackerParams(cfg.params_pathname.c_str());

  assert(tracker);
  assert(tracker_params);

  cv::VideoCapture input(image_argument->c_str());
  if (!input.isOpened())
    throw make_runtime_error("Unable to open video file '%s'", image_argument->c_str());

  cv::Mat image;

  std::vector<char> pathname_buffer;
  pathname_buffer.resize(1000);

  input >> image;
  int frame_number = 1;

  while ((image.rows > 0) && (image.cols > 0)) {
    if (cfg.verbose) {
      printf(" Frame number %d\r", frame_number);
      fflush(stdout);
    }

    cv::Mat_<uint8_t> gray_image;
    if (image.type() == cv::DataType<cv::Vec<uint8_t,3> >::type)
      cv::cvtColor(image, gray_image, CV_BGR2GRAY);
    else if (image.type() == cv::DataType<uint8_t>::type)
      gray_image = image;
    else
      throw make_runtime_error("Do not know how to convert video frame to a grayscale image.");

    int result = tracker->Track(gray_image, tracker_params);

    std::vector<cv::Point_<double> > shape;
    if (result >= cfg.tracking_threshold) {
      shape = tracker->getShape();
    } else {
      tracker->Reset();
    }

    if (!have_argument_p(landmarks_argument)) {
      display_image_and_points(cfg, image, shape);
    } else if (shape.size() > 0) {
      snprintf(pathname_buffer.data(), pathname_buffer.size(), landmarks_argument->c_str(), frame_number);
      IO::SavePts(pathname_buffer.data(), shape);
      if (cfg.verbose)
	display_image_and_points(cfg, image, shape);
    } else if (cfg.verbose) {
      display_image_and_points(cfg, image, shape);
    }

    input >> image;
    frame_number++;
  }

  delete tracker;
  delete tracker_params; 

  return 0;
}

int
run_image_mode(const Configuration &cfg,
	       const CommandLineArgument<std::string> &image_argument,
	       const CommandLineArgument<std::string> &landmarks_argument)
{  
  FaceTracker * tracker = LoadFaceTracker(cfg.model_pathname.c_str());
  FaceTrackerParams *tracker_params  = LoadFaceTrackerParams(cfg.params_pathname.c_str());

  cv::Mat image;
  cv::Mat_<uint8_t> gray_image = load_grayscale_image(image_argument->c_str(), &image);

  int result = tracker->NewFrame(gray_image, tracker_params);

  std::vector<cv::Point_<double> > shape;
  
  if (result >= cfg.tracking_threshold)
    shape = tracker->getShape();

  if (!have_argument_p(landmarks_argument)) {
    display_image_and_points(cfg, image, shape);    
  } else if (shape.size() > 0) {
    IO::SavePts(landmarks_argument->c_str(), shape);
  }
 
  delete tracker;
  delete tracker_params; 
  
  return 0;
}

void
display_image_and_points(const Configuration &cfg,
			 const cv::Mat &image,
			 const std::vector<cv::Point_<double> > &points)
{

  cv::Scalar colour;
  if (image.type() == cv::DataType<uint8_t>::type)
    colour = cv::Scalar(255);
  else if (image.type() == cv::DataType<cv::Vec<uint8_t,3> >::type)
    colour = cv::Scalar(0,0,255);
  else
    colour = cv::Scalar(255);

  cv::Mat displayed_image(image.clone());

  for (size_t i = 0; i < points.size(); i++) {
    cv::circle(displayed_image, points[i], cfg.circle_radius, colour, cfg.circle_thickness, cfg.circle_linetype, cfg.circle_shift);
  }

  cv::imshow(cfg.window_title, displayed_image);

  if (cfg.wait_time == 0)
    std::cout << "Press any key to continue." << std::endl;

  char ch = cv::waitKey(cfg.wait_time * 1000);

  if (ch == 27) // escape
    throw user_pressed_escape();
}