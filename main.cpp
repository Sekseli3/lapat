#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <google/protobuf/util/json_util.h>
#include "Rectangle.pb.h"
#include <fstream>
#include <vector>
#include <string>

//Larpakkeiden tunnistusohjelma
//Ohjelma tunnistaa videosta larpakkeet ja tallentaa niiden sijainnit JSON-tiedostoon
//Perus toiminta perustuu keltaisen värin löytämissen ja sitten lisätarkastuskiin.
//Lisätarkastuksissa tarkastetaan onko keltaisen ympärillä vihreää,onko seliikkunut ja onko keltaisen koko ja muoto oikea

//Define constants
const std::string VIDEO_PATH = "/Users/akselituominen/Desktop/larpake/video.mkv";
const int BOUNDING_BOX_MARGIN = 80;
//how large of a fraction has to be green
const double GREEN_FRACTION_THRESHOLD = 0.9;
const char QUIT_KEY = 'q';

//function to calculate the green fraction for a given region
double calculateGreenFraction(const cv::Mat& region) {
    return cv::countNonZero(region) / static_cast<double>(region.total());
}
//function to get a region from the green mask
cv::Mat getRegion(const cv::Mat& greenMask, const cv::Rect& bbox, int dx, int dy, int width, int height) {
    return greenMask(cv::Rect(bbox.x + dx, bbox.y + dy, width, height));
}
//function to process a bounding box
void processBoundingBox(const cv::Rect& bbox, const cv::Mat& greenMask, const std::vector<cv::Rect>& previousBboxes, cv::Mat& frame) {
    if (bbox.y - BOUNDING_BOX_MARGIN >= 0 && bbox.y + bbox.height + BOUNDING_BOX_MARGIN < greenMask.rows
    && bbox.x - BOUNDING_BOX_MARGIN >= 0 && bbox.x + bbox.width + BOUNDING_BOX_MARGIN < greenMask.cols) {
        // Calculate the green fraction for each region
        double greenFractionAbove = calculateGreenFraction(getRegion(greenMask, bbox, 0, -BOUNDING_BOX_MARGIN, bbox.width, BOUNDING_BOX_MARGIN));
        double greenFractionBelow = calculateGreenFraction(getRegion(greenMask, bbox, 0, bbox.height, bbox.width, BOUNDING_BOX_MARGIN));
        double greenFractionLeft = calculateGreenFraction(getRegion(greenMask, bbox, -BOUNDING_BOX_MARGIN, 0, BOUNDING_BOX_MARGIN, bbox.height));
        double greenFractionRight = calculateGreenFraction(getRegion(greenMask, bbox, bbox.width, 0, BOUNDING_BOX_MARGIN, bbox.height));

        //Calculate the average green fraction(areas with green pixels/ total area) around the bounding box
        double greenFraction = (greenFractionAbove + greenFractionBelow + greenFractionLeft + greenFractionRight) / 4;
        //if the average is above the treshold, check if the bounding box fills all other criteria
        if (greenFraction > GREEN_FRACTION_THRESHOLD) {
            //Check if the bounding box has not moved
            bool hasNotMoved = std::any_of(previousBboxes.begin(), previousBboxes.end(), [&bbox](const cv::Rect& previousBbox) {
                return cv::norm(bbox.tl() - previousBbox.tl()) == 0; // adjust this threshold as needed
            });
            //bounding box has not moved, perform the remaining checks
            if (hasNotMoved) {
                //Calculate the aspect ratio and area of the bounding rectangle
                double aspectRatio = static_cast<double>(bbox.width) / bbox.height;
                double area = bbox.area();
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
                        std::ofstream file("rectangles.json", std::ios::app);
                        file << json_string << std::endl;
                    }
                }
                //If aspectRation less than 1 we are looking at a vertically oriented rectangle
                else if (aspectRatio <= 1 && (area > 250 && area < 400)) {
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
                        //Add the JSON string to the vector
                        std::ofstream file("rectangles.json", std::ios::app);
                        file << json_string << std::endl;
                    }
                }
            }
        }
    }
}

int main() {
    //Open the video file, change the path to the video file as needed
    cv::VideoCapture cap(VIDEO_PATH);
    //Check if successful
    if(!cap.isOpened()) {
        std::cout << "Could not open the video file" << std::endl;
        return -1;
    }
    //Create display window
    cv::namedWindow("Video", cv::WINDOW_AUTOSIZE);
    //Define the color range, (neon-ish yellow)
    cv::Scalar lowerYellow(20, 40, 140);
    cv::Scalar upperYellow(30, 200, 255);
    //Lower and upper bounds for the green mask
    cv::Scalar lowerGreen = cv::Scalar(50, 0, 30);
    cv::Scalar upperGreen = cv::Scalar(170, 255, 255);
    //Vector for storing the JSON strings
    static std::vector<cv::Rect> previousBboxes;
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
        // Draw bounding boxes around the contours
        // Calculate bounding boxes for each contour
        std::vector<cv::Rect> bboxes(contours.size());
        std::transform(contours.begin(), contours.end(), bboxes.begin(), [](const std::vector<cv::Point>& contour) {
            return cv::boundingRect(contour);
        });
        // Process each bounding box in parallel
        cv::parallel_for_(cv::Range(0, bboxes.size()), [&](const cv::Range& range) {
            for (int i = range.start; i < range.end; i++) {
                processBoundingBox(bboxes[i], greenMask, previousBboxes, frame);
            }
        });
        //Update the locations of the bounding boxes for the next frame
        previousBboxes = bboxes;
        //Display the frame
        cv::imshow("Video", frame);
        //Press 'q' to quit
        if(cv::waitKey(1) == QUIT_KEY)
            break;
    }
    //When everything done, release the video capture object
    cap.release();
    //Close all the frames
    cv::destroyAllWindows();
    return 0;
}
