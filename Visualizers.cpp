//
// Created by Florent Le Moel on 02/05/2019.
//

#include "Visualizers.h"


std::string msTime() {
    // From https://stackoverflow.com/a/35157784

    using namespace std::chrono;

    // get current time
    auto now = system_clock::now();

    // get number of milliseconds for the current second
    // (remainder after division into seconds)
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    // convert to std::time_t in order to convert to std::tm (broken time)
    auto timer = system_clock::to_time_t(now);

    // convert to broken time
    std::tm bt = *std::localtime(&timer);

    std::ostringstream oss;
    oss << std::put_time(&bt, "%d/%m/%Y %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}



int VisualizerTrace::update( const int *motionData, bool &isFreeball ) {

    // Initialize a matrix filled with white
    cv::Mat imageTrace( viewportSize, viewportSize, CV_8UC1, 255 );

    // Move all pixels coords by 1 to the right (i.e. get rid of the last one and make an empty space at the beginning)
    for ( int i = traceLength-1; i > 0; i-- ) {
        allPixelsX[i] = allPixelsX[i-1];
        allPixelsY[i] = allPixelsY[i-1];
    }

    if ( isFreeball ) {

        double DX0, DX1, DY0, DY1;
        double Dalpha, Ds;

        DX0 = motionData[4] / cal0;    // DX0, measured in ball rotations
        DX1 = motionData[5] / cal1;

        DY0 = motionData[6] / cal0;
        DY1 = motionData[7] / cal1;

        Ds = 152.9955 * std::sqrt(DY0 * DY0 + DY1 * DY1); // 152.99 = ball perimeter in mm

        Dalpha = M_PI * (DX0 + DX1);   // Dalpha = 2pi * (DX0 + DX1)/2

        Xs += Ds * std::cos(alpha);
        Ys += Ds * std::sin(alpha);

        alpha += Dalpha;

        if ( alpha < 0 )
            alpha += 2 * M_PI;

        if ( alpha > (2 * M_PI) )
            alpha -= 2 * M_PI;

        // put new pixel's X and Y at the beginning of the array (to display in the center)
        allPixelsX[0] = static_cast<int>(std::round( Xs * 2 ));
        allPixelsY[0] = static_cast<int>(std::round( Ys * 2 ));

    } else {

        double Y0, Y1;

        // We only take the vertical component of both sensors (X component is the hindered ball rotation)
        Y0 = motionData[2];
        Y1 = motionData[3];

        Y0 = Y0 * sensitivity / cal0;
        Y1 = Y1 * sensitivity / cal1;

        // put new pixel's X and Y at the beginning of the array (to display in the center)
        allPixelsX[0] = static_cast<int>(std::round( Y0 ));
        allPixelsY[0] = static_cast<int>(std::round( Y1 ));
    }

    // Now iterate over the coord arrays to color the trace to black
    int j, k;

    for ( int i = 0; i < traceLength; i++ ) {
        j = allPixelsX[i] - allPixelsX[0] + viewportSize/2;
        k = allPixelsY[i] - allPixelsY[0] + viewportSize/2;

        // If the pixel is inside the viewport, change it to black
        if ( j > 0 && j < viewportSize-1 && k > 0 && k < viewportSize-1 )
            imageTrace.at<uchar>(j, k) = 0; // TODO: .at is supposed to range-check, why does this break without the if?
    }

    std::string ballStatus;

    if ( isFreeball ) {
        ballStatus = "ON";
    } else {
        ballStatus = "OFF";
    }

    // Put Surface Quality SQ0
    cv::putText(imageTrace, "Freeball is " + ballStatus + " (press F to toggle)", cv::Point(10, viewportSize-10),     // Coordinates
                cv::FONT_HERSHEY_COMPLEX_SMALL,   // Font
                0.5,                              // Scale
                0,                              // Color (255 = white)
                0.5,                                // Line Thickness
                cv::LINE_AA);                     // Anti-alias


    output = imageTrace;

    return 0;
}

void VisualizerTrace::display() {

    cv::imshow( "Trace", output );
}

cv::Mat VisualizerTrace::get() {

    return output;
}

int VisualizerSensors::update( unsigned char *images, const int *motionData ) {

    // First image starts at the pointed address
    cv::Mat imgS1(1, matLength, CV_8UC1, images);

    // Second image starts at the pointed address + the length of the first image
    cv::Mat imgS2(1, matLength, CV_8UC1, images + matLength);

    // Reshape the two vectors into squares of correct resolution
    imgS1 = imgS1.reshape(1, resolution);
    imgS2 = imgS2.reshape(1, resolution);

    // Create a compound image (of h x 2w)
    cv::Mat bothSensorsImage(resolution, 2 * resolution, CV_8UC1);

    // Move the ROI to the left
    bothSensorsImage.adjustROI(0, 0, 0, -resolution);
    imgS1.copyTo(bothSensorsImage);

    // Move the ROI to the right
    bothSensorsImage.adjustROI(0, 0, -resolution, resolution);
    imgS2.copyTo(bothSensorsImage);

    // Restore normal ROI
    bothSensorsImage.adjustROI(0, 0, resolution, 0);

    // Resize the image to something we can actually see without squinting like mf*ers
    cv::resize(bothSensorsImage, bothSensorsImage, cv::Size(600, 300), 0, 0, cv::INTER_NEAREST);  // Nearest interpolation is important

    if ( motionData ) {
        // Extract SQ readings from passed formattedBuffer array
        std::string SQ0 = std::to_string(motionData[8]);
        std::string SQ1 = std::to_string(motionData[9]);

        // Put Surface Quality SQ0
        cv::putText(bothSensorsImage, SQ0, cv::Point(20, 280),     // Coordinates
                    cv::FONT_HERSHEY_COMPLEX_SMALL,   // Font
                    1.5,                              // Scale
                    255,                              // Color (255 = white)
                    2,                                // Line Thickness
                    cv::LINE_AA);                     // Anti-alias

        // Put Surface Quality SQ1
        cv::putText(bothSensorsImage, SQ1, cv::Point(320, 280),    // Coordinates
                    cv::FONT_HERSHEY_COMPLEX_SMALL,   // Font
                    1.5,                              // Scale
                    255,                              // Color (255 = white)
                    2,                                // Line Thickness
                    cv::LINE_AA);                     // Anti-alias
    }

    output = bothSensorsImage;

    return 0;
}

void VisualizerSensors::display() {

    cv::imshow( "Sensor View", output );

}

cv::Mat VisualizerSensors::get() {

    return output;
}

VisualizerCamera::~VisualizerCamera() {
    stop();
    cv::destroyAllWindows();
}

int VisualizerCamera::start( int cam, const std::string& outputFilename ) {

    // Open the capture object (typically 0 for inbuilt webcam, 1 for first USB camera, etc)
    cap.open(cv::CAP_V4L + cam);

    if ( !cap.isOpened() ) {
        std::cerr << "Could not open the camera\n";
        return -1;
    }

    // Set video params
    cap.set(cv::CAP_PROP_FRAME_WIDTH, frameWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, frameHeight);
    cap.set(cv::CAP_PROP_FOURCC, codec);
    cap.set(cv::CAP_PROP_FPS, fps);
    //cap.set(cv::CAP_PROP_AUTOFOCUS, 0);
    //cap.set(cv::CAP_PROP_FOCUS, 20); // focus (min: 0, max: 255, increment: 5) - Trackball 2.0 best parameter: 20-22

    if ( !outputFilename.empty() ) {
        writer.open(outputFilename + "_video.mp4", codec, fps, cv::Size(frameWidth, frameHeight));

        if ( !writer.isOpened() ) {
            std::cerr << "Could not open the video file to write\n";
            return -1;
        }
    }

    return 0;
}

int VisualizerCamera::stop() {

    // Release the video capture
    cap.release();
    if ( writer.isOpened() )
        writer.release();
    return 0;
}

int VisualizerCamera::update() {

    // Get a new frame from camera
    cv::Mat frame;
    cap >> frame;

    // If the frame is empty, break immediately
    if ( frame.empty() )
        return -1;

    // Put current time
    cv::putText(frame, msTime(),
                cv::Point(10, frameHeight-10),               // Coordinates
                cv::FONT_HERSHEY_COMPLEX_SMALL,   // Font
                1.0,                              // Scale
                cv::Scalar(0, 255, 255),          // BGR Color (yellow)
                1,                                // Line Thickness
                cv::LINE_AA);                     // Anti-alias

    output = frame;

    return 0;
}

void VisualizerCamera::display() {

    cv::imshow( "Live video", output );

}

void VisualizerCamera::write() {

    if ( !writer.isOpened() ) {
        std::cout << "Cannot record video without a file name!" << std::endl;
        exit(-1);
    }

    // Write the frame into the output video file
    writer.write( output );

}

cv::Mat VisualizerCamera::get() {

    return output;
}
