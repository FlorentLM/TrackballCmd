//
// Created by Florent Le Moël on 02/05/2019.
//

#include "Trackball.h"
#include "Visualizers.h"
#include <thread>


static void show_usage( std::string name )
{
    std::cerr << "Usage: " << name << " <option(s)> [OUTPUT_NAME]\n"
              << "Options:\n"
              << "\t-h,--help\t\tShow this help message.\n"
              << "\t-s,--sensorview\t\tEnable Sensor View mode.\n"
              << "\t-c,--camera\t\tEnable Camera mode.\n"
              << "\t-t,--trace\t\tEnable Trace mode.\n"
              << "\t-w,--write\t\tEnable writing output files.\n"
              << "\t-n,--network\t\tEnable network diffusion.\n"
              << "\t-q,--quiet\t\tDisable console output.\n"
              << "\t-v,--vid 0x0000\t\tSpecify the VID of the trackball device. Default is 0x04b4 (Cypress Semiconductor Corp.)\n"
              << "\t-p,--pid 0x0000\t\tSpecify the PID of the trackball device. Default is 0x8613 (CY7C68013 EZ-USB FX2)\n"
              << "\t-f,--firmware PATH\tSpecify the path of the firmware to flash. Default is ./firmware.hex\n"
              << std::endl;
}

void cameraLoopWrapper( Trackball& tb, bool& timeToStop )
{
    VisualizerCamera cameraViewer;

    int key = 0;

    cameraViewer.start(0, "test");

    while ( !timeToStop ) {

        if ( key == 27 ) {
            timeToStop = true;
        }

        if ( key == 'o' | key == 'p' ) {
            tb.marker( key );
        }

        key = cv::waitKey(30);

        cameraViewer.update();

        cameraViewer.display();
        cameraViewer.write();

    }
}

void traceLoopWrapper( Trackball& tb, bool& timeToStop )
{
    VisualizerTrace traceViewer;

    int key = 0;
    bool isFreeball = false;

    while ( !timeToStop  ) {

        if ( key == 27 ) {
            timeToStop = true;
        }

        if ( key == 'o' | key == 'p' ) {
            tb.marker( key );
        }

        if ( key == 'f' ) {
            isFreeball = !isFreeball;
        }

        key = cv::waitKey(30);

        traceViewer.update(tb.getMotionData(), isFreeball);

        traceViewer.display();
    }
}

void camtraceLoopWrapper( Trackball& tb, bool& timeToStop )
{
    VisualizerTrace traceViewer;
    VisualizerCamera cameraViewer;

    int key = 0;
    bool isFreeball = false;

    cameraViewer.start(0, "test");

    while ( !timeToStop  ) {

        if ( key == 27 ) {
            timeToStop = true;
        }

        if ( key == 'o' | key == 'p' ) {
            tb.marker( key );
        }

        if ( key == 'f' ) {
            isFreeball = !isFreeball;
        }

        key = cv::waitKey(30);

        traceViewer.update(tb.getMotionData(), isFreeball);
        traceViewer.display();

        cameraViewer.update();

        cameraViewer.display();
        cameraViewer.write();
    }
}

void mainLoopWrapper( Trackball& tb, bool& timeToStop )
{
    std::cout << "Currently acquiring..." << std::endl;

    while ( !timeToStop ) {

        tb.acquire();
    }
}

int main(int argc, char* argv[])
{

    bool sensorViewMode;
    bool camera;
    bool trace;
    bool silentConsole;
    bool networkOutput;
    bool diskwriteOutput;

    unsigned short vid, pid;
    std::string fpath;

    std::string ip;
    std::string port;

    // Defaults
    sensorViewMode = false;
    camera = false;
    trace = false;
    silentConsole = false;
    networkOutput = false;
    diskwriteOutput = false;
    vid = 0x04b4;
    pid = 0x8613;
    fpath = "./firmware.hex";                 // for final build
//    fpath = "../Firmware/firmware.hex";       // for debug

    ip = "127.0.0.1";
    port = "45944";

    std::vector <std::string> remaining_args;

    // Parse commandline options
    for (int i = 1; i < argc; ++i) {

        std::string arg = argv[i];

        if ((arg == "-h") || (arg == "--help")) {
            show_usage(argv[0]);
            return 0;

        } else if ((arg == "-s") || (arg == "--sensorview")) {
            sensorViewMode = true;
            i++;

        }  else if ((arg == "-c") || (arg == "--camera")) {
            camera = true;
            i++;

        }  else if ((arg == "-t") || (arg == "--trace")) {
            trace = true;
            i++;

        } else if ((arg == "-q") || (arg == "--quiet")) {
            silentConsole = true;
            i++;

        } else if ((arg == "-n") || (arg == "--network")) {
            networkOutput = true;
            i++;

        } else if ((arg == "-w") || (arg == "--write")) {
            diskwriteOutput = true;
            i++;

        } else if ((arg == "-v") || (arg == "--vid")) {
            if (i + 1 < argc) {                 // Make sure we aren't at the end of argv!
                i++;                            // Increment 'i' so we don't get the argument as the next argv[i]
                vid = std::stoul(argv[i], nullptr, 16);

            } else {                            // if there was no argument to the destination option
                std::cerr << "--vid option requires one argument." << std::endl;
                return 1;
            }

        } else if ((arg == "-p") || (arg == "--pid")) {
            if (i + 1 < argc) {
                i++;
                pid = std::stoul(argv[i], nullptr, 16);

            } else {
                std::cerr << "--pid option requires one argument." << std::endl;
                return 1;
            }

        } else if ((arg == "-f") || (arg == "--firmware")) {
            if (i + 1 < argc) {
                i++;

                fpath = argv[i];

            } else {
                std::cerr << "--firmware option requires one argument." << std::endl;
                return 1;
            }

        } else {

            remaining_args.push_back(argv[i]);
        }

    }

    // Initialise trackball

    Trackball tb(vid, pid);

    tb.setFirmwarePath(fpath);
    tb.connectUSB();

    if ( silentConsole ) {
        tb.disableConsoleOutput();
    }

    if ( sensorViewMode ) {
        VisualizerSensors viewer;
        tb.enableSensorView();

        std::cout << "Sensor View is running..." << std::endl;

        int key = 0;

        while ( key != 27 ) {

            key = cv::waitKey(30);
            tb.acquire();

            viewer.update(tb.sensorView(), tb.getMotionData());
            viewer.display();
        }

    } else {

        if ( diskwriteOutput ) {
            if ( remaining_args.size() == 0 ) {
                tb.askOutputName();
            } else if ( remaining_args.size() == 1 ) {
                tb.setOutputName(remaining_args[0]);
            }

            tb.enableDiskwrite();
        }

        if ( networkOutput ) {
            tb.enableNetwork(ip, port);     // TODO: add args for setting IP and Port from commandline
            std::cout << "Transmitting over " << ip << ":" << port << std::endl;

        }

        if ( trace && !camera ) {

            bool stopAllThreads = false;

            std::thread t1(mainLoopWrapper, std::ref(tb), std::ref(stopAllThreads));
            std::thread t2(traceLoopWrapper, std::ref(tb), std::ref(stopAllThreads));

            t1.join();
            t2.join();

        } else if ( camera && !trace ) {

            bool stopAllThreads = false;

            std::thread t1(mainLoopWrapper, std::ref(tb), std::ref(stopAllThreads));
            std::thread t2(cameraLoopWrapper, std::ref(tb), std::ref(stopAllThreads));

            t1.join();
            t2.join();

        } else if ( trace && camera ) {

            bool stopAllThreads = false;

            std::thread t1(mainLoopWrapper, std::ref(tb), std::ref(stopAllThreads));
            std::thread t2(camtraceLoopWrapper, std::ref(tb), std::ref(stopAllThreads));

            t1.join();
            t2.join();

        } else {

            bool stopAllThreads = false;

            std::thread t1(mainLoopWrapper, std::ref(tb), std::ref(stopAllThreads));

            t1.join();

        }

    }

    return 0;
}
