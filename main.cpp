#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <google/protobuf/util/json_util.h>
#include "Rectangle.pb.h"
#include <fstream>
#include <vector>
#include <string>

const int BOUNDING_BOX_MARGIN = 90;
const double GREEN_FRACTION_THRESHOLD = 0.9;
const char QUIT_KEY = 'q';

void processBoundingBox(const cv::Rect& bbox, const cv::Mat& greenMask, const std::vector<cv::Rect>& previousBboxes, cv::Mat& frame, std::vector<std::string>& json_strings) {
    if (bbox.y - BOUNDING_BOX_MARGIN >= 0 && bbox.y + bbox.height + BOUNDING_BOX_MARGIN < greenMask.rows && bbox.x - BOUNDING_BOX_MARGIN >= 0 && bbox.x + bbox.width + BOUNDING_BOX_MARGIN < greenMask.cols) {
        cv::Mat regionAbove = greenMask(cv::Rect(bbox.x, bbox.y - BOUNDING_BOX_MARGIN, bbox.width, BOUNDING_BOX_MARGIN));
        cv::Mat regionBelow = greenMask(cv::Rect(bbox.x, bbox.y + bbox.height, bbox.width, BOUNDING_BOX_MARGIN));
        cv::Mat regionLeft = greenMask(cv::Rect(bbox.x - BOUNDING_BOX_MARGIN, bbox.y, BOUNDING_BOX_MARGIN, bbox.height));
        cv::Mat regionRight = greenMask(cv::Rect(bbox.x + bbox.width, bbox.y, BOUNDING_BOX_MARGIN, bbox.height));

        double greenFractionAbove = cv::countNonZero(regionAbove) / static_cast<double>(regionAbove.total());
        double greenFractionBelow = cv::countNonZero(regionBelow) / static_cast<double>(regionBelow.total());
        double greenFractionLeft = cv::countNonZero(regionLeft) / static_cast<double>(regionLeft.total());
        double greenFractionRight = cv::countNonZero(regionRight) / static_cast<double>(regionRight.total());
        //Calculate the average green fraction(areas with green pixels/ total area) around the bounding box
        double greenFraction = (greenFractionAbove + greenFractionBelow + greenFractionLeft + greenFractionRight) / 4;
        //if the average is above the treshold, check if the bounding box fills all other criteria
        if (greenFraction > GREEN_FRACTION_THRESHOLD) {
            //Calculate the aspect ratio and area of the bounding rectangle
            double aspectRatio = static_cast<double>(bbox.width) / bbox.height;
            double area = bbox.area();
            //Check if the bounding box has not moved
            bool hasNotMoved = std::any_of(previousBboxes.begin(), previousBboxes.end(), [&bbox](const cv::Rect& previousBbox) {
                return cv::norm(bbox.tl() - previousBbox.tl()) == 0; // adjust this threshold as needed
            });
            //bounding box has not moved, perform the remaining checks
            if (hasNotMoved) {
                //If the aspect ratio is way more than 1 we know we are looking at a horizontally orientend rectangle
                if (aspectRatio >= 3  && area > 50) {
                    cv::rectangle(frame, bbox, cv::Scalar(0, 255, 0), 2);
                    // Create a Rectangle protobuf object and set its fields
                    Rectangle rectangle;
                    rectangle.set_x(bbox.x);
                    rectangle.set_y(bbox.y);
                    rectangle.set_angle(0); //Angle with X-axis is 0 for green
                    // Serialize the Rectangle object to a JSON string
                    google::protobuf::util::JsonPrintOptions options;
                    options.always_print_primitive_fields = true; //Without this angle 0 will not be shown
                    std::string json_string;
                    absl::Status status = google::protobuf::util::MessageToJsonString(rectangle, &json_string, options);
                    if (!status.ok()) {
                        std::cerr << "Failed to convert to JSON: " << status.ToString() << std::endl;
                    } else {
                        json_strings.push_back(json_string);
                    }
                }
                //If aspectRation less than 1 we are looking at a vertically oriented rectangle
                if (aspectRatio <= 1 && (area > 250 && area < 400)) {
                    cv::rectangle(frame, bbox, cv::Scalar(0, 0, 255), 2);
                    //Create a Rectangle protobuf object and set its fields
                    Rectangle rectangle;
                    rectangle.set_x(bbox.x);
                    rectangle.set_y(bbox.y);
                    rectangle.set_angle(90); // Angle is 90 for red
                    //Serialize the Rectangle object to a JSON string
                    google::protobuf::util::JsonPrintOptions options;
                    options.always_print_primitive_fields = true;
                    std::string json_string;
                    absl::Status status = google::protobuf::util::MessageToJsonString(rectangle, &json_string, options);
                    if (!status.ok()) {
                        std::cerr << "Failed to convert message to JSON: " << status.ToString() << std::endl;
                    } else {
                        json_strings.push_back(json_string);
                    }
                }
            }
        }
    }
}

int main() {
    //Open the video file
    cv::VideoCapture cap("/Users/akselituominen/Desktop/larpake/video.mkv");
    //Check if successful
    if(!cap.isOpened()) {
        std::cout << "Could not open the video file" << std::endl;
        return -1;
    }
    //Create display window
    cv::namedWindow("Video", cv::WINDOW_AUTOSIZE);
    //Define the color range, (neon-ish yellow)
    //ottaa kaikki läpät(ja muutakin) arvoilla 15-45, 50-255, 50-255
    cv::Scalar lowerYellow(20, 40, 140);
    cv::Scalar upperYellow(30, 255, 255);
    //Lower and upper bounds for the green mask
    cv::Scalar lowerGreen = cv::Scalar(30, 0, 30);
    cv::Scalar upperGreen = cv::Scalar(170, 255, 255);
    //Vector for storing the JSON strings
    std::vector<std::string> json_strings;

    while(true) {
        cv::Mat frame;
        //Capture frame-by-frame
        cap >> frame;
        //If the frame is empty, break
        if (frame.empty())
            break;
        //convert the frame to HSV color space
        cv::Mat hsv;
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        //Create a mask that selects the yellow pixels
        cv::Mat yellowMask;
        cv::inRange(hsv, lowerYellow, upperYellow, yellowMask);
        //Find the contours in the yellow mask
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(yellowMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        // Create a mask that selects the green pixels
        cv::Mat greenMask;
        cv::inRange(hsv, lowerGreen, upperGreen, greenMask);

        //Store the locations of the bounding boxes from the previous frame
        static std::vector<cv::Rect> previousBboxes;
        // Draw bounding boxes around the contours
        for (const auto& contour : contours) {
            // Calculate the bounding rectangle of the contour
            cv::Rect bbox = cv::boundingRect(contour);
            //We call function to process bounding box
            for (const auto& contour : contours) {
                cv::Rect bbox = cv::boundingRect(contour);
                processBoundingBox(bbox, greenMask, previousBboxes, frame, json_strings);
            }
        }
        //Update the locations of the bounding boxes for the next frame
        previousBboxes = std::vector<cv::Rect>(contours.size());
        std::transform(contours.begin(), contours.end(), previousBboxes.begin(), [](const std::vector<cv::Point>& contour) {
        return cv::boundingRect(contour);
        });
        //Display the frame
        cv::imshow("Video", frame);
        //Press 'q' to quit
        if(cv::waitKey(1) == QUIT_KEY)
            break;
    }
    //After loop write the JSON strings to a file
    std::ofstream file("rectangles.json", std::ios::app);
    for (const auto& json_string : json_strings) {
        file << json_string << std::endl;
    }
    //When everything done, release the video capture object
    cap.release();
    //Close all the frames
    cv::destroyAllWindows();
    return 0;
}
