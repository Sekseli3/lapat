#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

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

    cv::Scalar lowerGreen = cv::Scalar(30, 0, 30); // Lower bound for green
    cv::Scalar upperGreen = cv::Scalar(170, 255, 255); // Upper bound for green
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

        // Find the contours in the yellow mask
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(yellowMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Store the locations of the bounding boxes from the previous frame
    static std::vector<cv::Rect> previousBboxes;

// Create a mask that selects the green pixels
cv::Mat greenMask;
cv::inRange(hsv, lowerGreen, upperGreen, greenMask); // define lowerGreen and upperGreen according to the green color range in HSV

// Draw bounding boxes around the contours
for (const auto& contour : contours) {
    // Calculate the bounding rectangle of the contour
    cv::Rect bbox = cv::boundingRect(contour);

// Check if the regions around the bounding box are green
if (bbox.y - 70 >= 0 && bbox.y + bbox.height + 70 < greenMask.rows && bbox.x - 70 >= 0 && bbox.x + bbox.width + 70 < greenMask.cols) {
    cv::Mat regionAbove = greenMask(cv::Rect(bbox.x, bbox.y - 70, bbox.width, 70));
    cv::Mat regionBelow = greenMask(cv::Rect(bbox.x, bbox.y + bbox.height, bbox.width, 70));
    cv::Mat regionLeft = greenMask(cv::Rect(bbox.x - 70, bbox.y, 70, bbox.height));
    cv::Mat regionRight = greenMask(cv::Rect(bbox.x + bbox.width, bbox.y, 70, bbox.height));

    double greenFractionAbove = cv::countNonZero(regionAbove) / static_cast<double>(regionAbove.total());
    double greenFractionBelow = cv::countNonZero(regionBelow) / static_cast<double>(regionBelow.total());
    double greenFractionLeft = cv::countNonZero(regionLeft) / static_cast<double>(regionLeft.total());
    double greenFractionRight = cv::countNonZero(regionRight) / static_cast<double>(regionRight.total());

    // If a significant fraction of the regions around the bounding box are green, consider the slab as lying on green ground
    if (greenFractionAbove > 0.8 && greenFractionBelow > 0.8 && greenFractionLeft > 0.8 && greenFractionRight > 0.8) { // adjust these thresholds as needed
        // Calculate the aspect ratio and area of the bounding rectangle
        double aspectRatio = static_cast<double>(bbox.width) / bbox.height;
        double area = bbox.area();

        // Check if the bounding box has not moved
        bool hasNotMoved = std::any_of(previousBboxes.begin(), previousBboxes.end(), [&bbox](const cv::Rect& previousBbox) {
            return cv::norm(bbox.tl() - previousBbox.tl()) == 0; // adjust this threshold as needed
        });

        // If the bounding box has not moved, perform the remaining checks
        if (hasNotMoved) {
            // If the aspect ratio is close to 1 and the area is within a certain range, consider the contour as a potential rectangle
            if (aspectRatio >= 3  && area > 50) {
                cv::rectangle(frame, bbox, cv::Scalar(0, 255, 0), 2);
            }
            if (aspectRatio <= 1 && (area > 270 && area < 410)) {
                cv::rectangle(frame, bbox, cv::Scalar(0, 0, 255), 2);
            }
        }
    }
}
}

    // Update the locations of the bounding boxes for the next frame
    previousBboxes = std::vector<cv::Rect>(contours.size());
    std::transform(contours.begin(), contours.end(), previousBboxes.begin(), [](const std::vector<cv::Point>& contour) {
        return cv::boundingRect(contour);
});
        // Display the resulting frame
        cv::imshow("Video", frame);



        // Press 'q' to quit
        if(cv::waitKey(1) == 'q')
            break;
    }    

    // When everything done, release the video capture object
    cap.release();

    // Close all the frames
    cv::destroyAllWindows();

    return 0;
}