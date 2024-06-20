//
// Created by Florent Le Moel on 02/05/2019.
//

#ifndef TRACKBALLCONTROL_TRACKBALL_H
#define TRACKBALLCONTROL_TRACKBALL_H

#include "./Cypress/include/cyusb.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <netdb.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <sstream>

// ADNS-5090 and ADNS-3050 addresses
const unsigned char PRODUCT_ID = 0x00;
const unsigned char REV_ID = 0x01;
const unsigned char MOTION_ST = 0x02;
const unsigned char SQUAL = 0x05;
const unsigned char PIX_GRAB = 0x0B;
const unsigned char CHIP_RESET = 0x3A;
const unsigned char NAV_CTRL2 = 0x22;

// ADNS Product IDs
const unsigned char ADNS3050_ID = 0x09;
const unsigned char ADNS5090_ID = 0x29;

// Cypress microcontroller-specific configuration
constexpr int INTF = 0;
constexpr int ALTINTF = 1;
const unsigned char endpointIN = 0x81;
const unsigned char endpointOUT = 0x01;

// Prototype for the flasher functions (see cypress code)
extern int fx2_ram_download(const char *filename, int extension);
extern int fx2_eeprom_download(const char *filename, int large);


class Trackball {

public:

    explicit Trackball( unsigned short VID = 0x04b4, unsigned short PID = 0x8613 );
    ~Trackball();

    // ADNS-3050 and ADNS-5090 sensors have a 19x19 pixels Array Address Map
    const int sensorSize = 19;

    // General methods
    int connectUSB();
    int disconnectUSB();
    int printSensorsInfo();
    void reset(bool resetAll = false);

    // Routines
    void acquire();
    unsigned char* sensorView();
    void marker( const char& key );

    // [Console Output]
    void enableConsoleOutput();
    void disableConsoleOutput();

    // [Disk Write Mode]
    int setOutputName(const std::string &manualName);
    int askOutputName();
    void saveFiles();
    int enableDiskwrite();
    void disableDiskwrite();

    // [Live Images Mode]
    int enableSensorView();
    int disableSensorView();

    // [Network Mode]
    int enableNetwork( const std::string& hostname = "127.0.0.1", const std::string& service_or_port = "45944" );
    int disableNetwork();

    // Getters
    int getCount() const;
    int* getMotionData();
    std::string getName() const;

    // Setters
    void setFirmwarePath(  const std::string& path );
    void setOutputPath(  const std::string& outputFolder );


private:

    // Some strongly typed enums for the firmware flasher, just for safety
    enum class Memory {
        RAM,
        EEPROM
    };

    enum class RAM {
        Internal,
        External
    };

    enum class EEPROM {
        Small,
        Large
    };

    // Cypress microcontroller IDs
    unsigned short VID;
    unsigned short PID;

    std::string fwpath = "./Firmware/firmware.hex";
    std::string outpath = "./Output/";

    // Detected device pointer, and transfer termination flag (see cyusb header for details)
    cyusb_handle *device { nullptr };
    int transferred = 0;

    // Read and Write buffers (unsigned char because they store raw 8-bit binary values)
    unsigned char readBuffer[8] { 0 };
    unsigned char writeBuffer[2] { 0 };

    // Output array for motion data (we handle the appropriate casting from binary to integer in the acquire() function)
    //
    // -------------------- int formattedBuffer --------------------
    // |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |
    // |  X0 |  X1 |  Y0 |  Y1 | DX0 | DX1 | DY0 | DY1 | SQ0 | SQ0 |
    //
    int formattedBuffer[10] {0};


    // Some internal statuses
    bool consoleOutput = true;
    bool diskwriteEnabled = false;
    bool networkEnabled = false;
    bool sensorviewEnabled = false;

    // Acquisition count
    int ackCount = 0;


    // Private methods
    int flashCypress( const std::string& firmwarefile, Memory dest=Memory::RAM,           // Strongly typed, for safety
                                                        RAM ramType=RAM::Internal,
                                                        EEPROM romType=EEPROM::Small );
    int prepareSensors();
    void transmit();


    // Prepare the outputs

    // [Disk Write mode]
    std::ofstream dataA, dataP, dataO;      // Output streams
    std::string formattedName = "NONAME";   // Formatted files name (from experimental condition)

    // [Sensor View mode]
    unsigned char *ptrImages { nullptr };   // Pointer to the generated images

    // [Network mode]
    int sockUDP = 0;                  // Socket to bind
    sockaddr_in addrTb = { };           // Trackball address struct
    sockaddr_storage addrDest = { };    // Destination address struct
};


#endif //TRACKBALLCONTROL_TRACKBALL_H
