//
// Created by Florent Le Moel on 02/05/2019.
//

#ifndef TRACKBALLCONTROL_VISUALIZERS_H
#define TRACKBALLCONTROL_VISUALIZERS_H


#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/videoio/videoio.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
//#include <libavdevice/avdevice.h>

#include <chrono>

std::string msTime();


class Visualizer {

public:
    Visualizer() = default;
    ~Visualizer() { cv::destroyAllWindows(); }
};

class VisualizerTrace : public Visualizer {

public:
    cv::Mat output;
    int update( const int *motionData, bool &isFreeball );
    void display();
    cv::Mat get();

private:

    // Calibration: sensor counts per 1 ball rotation
    // Resolution: 1000 dpi
    // Ball diameter: 48.7 mm
    // 1 inch = 25.4 mm
    // Perimeter = 152.9955 mm = 6.0234 inches
    //double cal0 { 6023.4473 }, cal1 { 6023.4473 };
    //double cal0 { -1550.0 }, cal1 { 1105.15 };

    // TODO: Calibrate these
    double cal0 = 1000.0;
    double cal1 = 1000.0;
    double sensitivity = 500.0;		 // Sensitivity of the display for 'no yaw ball': 500 = low, 2000 = high

    // Initialize values to increment
    double Xs = 0.0;
    double Ys = 0.0;
    double alpha = 0.0;

    // Params for the displayed image vertices
    static const int traceLength = 500;
    static const int viewportSize = 500;

    // Arrays for the trace coords
    int allPixelsX[viewportSize] { 0 };
    int allPixelsY[viewportSize] { 0 };

};

class VisualizerSensors : public Visualizer {

public:
    cv::Mat output;

    int update( unsigned char *images, const int *motionData = nullptr );
    void display();
    cv::Mat get();

private:

    const int resolution = 19;
    const int matLength = resolution * resolution;

};

class VisualizerCamera : public Visualizer {

public:
    ~VisualizerCamera();

    int start( int cam = 0, const std::string& outputFilename = "" );
    int stop();

    cv::Mat output;

    int update();
    void display();
    void write();
    cv::Mat get();

private:

    // Capture resolution and framerate
    int frameWidth = 640;
    int frameHeight = 360;
    int fps = 30;
//    int codec = cv::VideoWriter::fourcc('m','p','4','v');
//    int codec = cv::VideoWriter::fourcc('m','j','p','g');
//    int codec = cv::VideoWriter::fourcc('x','v','i','d');
//    int codec = cv::VideoWriter::fourcc('m','p','e','g');
//    int codec = cv::VideoWriter::fourcc('h','2','6','4');
    int codec = cv::VideoWriter::fourcc('h','2','6','4');
//    int codec = cv::VideoWriter::fourcc('a','v','c','1');
//    int codec = cv::VideoWriter::fourcc('d','i','v','x');

    int videodelay = 1000 / fps;

    cv::VideoCapture cap;       // Connection to the camera
    cv::VideoWriter writer;     // Video writer

};


#endif //TRACKBALLCONTROL_VISUALIZERS_H
