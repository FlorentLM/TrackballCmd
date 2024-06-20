//
// Created by Florent Le Moel on 02/05/2019.
//


#include "Trackball.h"
#include "fx2flash.cpp"
#include <sys/stat.h>


// Constructor & Destructor
Trackball::Trackball( unsigned short VID, unsigned short PID ) {
    this->VID = VID;        // Default Cypress Vendor ID is 0x04b4
    this->PID = PID;        // Default Cypress CY68013-56PVC microcontroller Product ID is 0x8613

    // Initialize the button to 1 (= non pressed)       TODO: 0x01 or 0x10 ??
    readBuffer[6] = 0x01;
}

Trackball::~Trackball() {

    reset(true);
    // Close USB connection with trackball
    disconnectUSB();
}


// Modes initializers

// [Disk Write Mode]
int Trackball::setOutputName( const std::string &manualName ) {

    formattedName = manualName;

    return 0;
}

int Trackball::askOutputName() {

    // Get ant number info
    std::cout << "Ant number: " << std::endl;
    std::string antNb;
    getline(std::cin, antNb);

    // Get condition info
    std::cout << "Condition name? " << std::endl;
    std::string condition;
    getline(std::cin, condition);

    // Format the base name
    formattedName = "ant" + antNb + "_" + condition;

    return 0;

}

int Trackball::enableDiskwrite() {

    std::string fnameA = outpath + formattedName + ".csv";
    std::string fnameP = outpath + formattedName + "_P.csv";
    std::string fnameO = outpath + formattedName + "_O.csv";

    dataA.open(fnameA.c_str(), std::ios::out);
    dataP.open(fnameP.c_str(), std::ios::out);
    dataO.open(fnameO.c_str(), std::ios::out);

    // Check if output folder exists
    struct stat st = {0};
    if (stat(outpath.c_str(), &st) == -1) {
        mkdir(outpath.c_str(), 0700);
        for (int i = 1; i < 1500; ++i) {}           // Just a little delay
    }

    // Check if files are writable
    if ( !dataA || !dataP || !dataO ) {
        std::cout << "Error writing files." << std::endl;
        return -1;
    }

    // Initialize the files headers
    const std::string filesHeader = "  Count;     X0;     Y0;     X1;     Y1;    SQ0;    SQ1";

    dataA << filesHeader << std::endl;
    dataA.flush();

    dataP << filesHeader << std::endl;
    dataP.flush();

    dataO << filesHeader << std::endl;
    dataO.flush();

    diskwriteEnabled = true;

    return 0;

}

void Trackball::saveFiles() {
    dataA.close();
    dataP.close();
    dataO.close();
}

void Trackball::disableDiskwrite() {
    saveFiles();
    diskwriteEnabled = false;
}

void Trackball::enableConsoleOutput() {
    consoleOutput = true;
}

void Trackball::disableConsoleOutput() {
    consoleOutput = false;
}


// [Sensor View Mode]
int Trackball::enableSensorView() {

    // Dynamically allocate an array of uchar and assign its address to ptrImages pointer
    ptrImages = new (std::nothrow) unsigned char[sensorSize * sensorSize * 2];  // 19x19 pixels * 2 sensors = array[722]

    // Check if memory has been correctly allocated
    if ( !ptrImages ) {
        std::cout << "Could not allocate memory!" << std::endl;
        return -1;
    }

    sensorviewEnabled = true;

    return 0;
}

int Trackball::disableSensorView() {

    delete[] ptrImages;
    ptrImages = { nullptr };

    sensorviewEnabled = false;

    return 0;
}

// [Network Mode]
int Trackball::enableNetwork( const std::string& hostname, const std::string& service_or_port ) {

    int r{ 1 };

    // Populate the socket object
    sockUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    int reusePort = 1;
    setsockopt(sockUDP, SOL_SOCKET, SO_REUSEPORT, &reusePort, sizeof(reusePort));

    // Populate the sockaddr_in struct
    addrTb.sin_family = AF_INET;    // IPv4 only
    addrTb.sin_port = 0x0A1A;       // Port 6666

    // Bind: Assigns a network name to our socket
    r = bind( sockUDP, (sockaddr*)&addrTb, sizeof(addrTb) );

    if ( r != 0 ) {
        std::cout << errno;
        return 1;
    }

    // Temporary struct of discovered internet addresses
    addrinfo* foundAddresses { nullptr };

    // This hints struct is a skeleton of which results we want to select from the discovered addresses struct
    addrinfo hints { };
    hints.ai_family = AF_INET;             // Typically AF_INET or AF_INET6 (IPv4 or IPv6), or AF_UNSPEC (= 0) for "any"
    hints.ai_socktype = SOCK_DGRAM;        // SOCK_STREAM (TCP) or SOCK_DGRAM (UDP), or 0 for "any"
    hints.ai_protocol = IPPROTO_UDP;       // IPPROTO_UDP or IPPROTO_TCP, or IPPROTO_IP (= 0) for "any". When ai_protocol is 0, UDP is used for SOCK_DGRAM and TCP is used for SOCK_STREAM
    hints.ai_flags = AI_NUMERICSERV;       // AI_NUMERICSERV specifies not to try to do server name resolution

    // Query network and get all matching addresses
    r = getaddrinfo( hostname.c_str(), service_or_port.c_str(), &hints, &foundAddresses );

    if ( r == 0 ) {
        // If getaddrinfo found something, copy that to the addrDest struct and delete the foundAddresses struct
        memcpy( &addrDest, foundAddresses->ai_addr, foundAddresses->ai_addrlen );
        freeaddrinfo( foundAddresses );
    } else {
        std::cout << errno;
        return 2;
    }

    networkEnabled = true;

    return 0;
}

int Trackball::disableNetwork() {

    sockUDP = 0;
    addrTb = { };
    addrDest = { };

    networkEnabled = false;

    return 0;
}


// Public methods
int Trackball::connectUSB() {

    int r{ 1 };

    // Look for Trackball device using VID and PID
    r = cyusb_open(VID, PID);
    if ( r == 1 )                        // If detected (r = 1), generate Cyusb pointer
        device = cyusb_gethandle(0);
    else {
        std::cout << "Trackball device not found!" << std::endl;
        // If not found, throw the cyusb error and return
        cyusb_error(r);
        return -1;
    }

    // Before claiming the interface, check if the device is in use by the OS
    r = cyusb_kernel_driver_active(device, INTF);
    if ( r < 0 ) {
        cyusb_error(r);
        return -1;
    }

    // If so, try to detach
    else if ( r == 1 ) {
        std::cout << "This interface is occupied by a kernel driver." << std::endl;

        r = cyusb_detach_kernel_driver(device, INTF);
        if ( r != 0 ) {
            std::cout << "Could not detach it..." << std::endl;
            cyusb_error(r);
            return -1;
        }
        std::cout << "Previous kernel driver successfully detached." << std::endl;
    }

    // Now we can claim the interface
    r = cyusb_claim_interface(device, INTF);
    if ( r != 0 ) {
        std::cout << "Interface " << INTF << " could not be claimed" << std::endl;
        cyusb_error(r);
        return -1;
    }

    // And the alternative interface
    r = cyusb_set_interface_alt_setting(device, INTF, ALTINTF);
    if ( r != 0 ) {
        std::cout << "Alternative interface " << ALTINTF << " could not be claimed" << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Clear any halt on endpoint OUT
    r = cyusb_clear_halt(device, endpointOUT);
    if ( r != 0 ) {
        std::cout << "Error clearing halt on endpoint OUT: " << std::hex << endpointOUT << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Clear any halt on endpoint IN
    r = cyusb_clear_halt(device, endpointIN);
    if ( r != 0 ) {
        std::cout << "Error clearing halt on endpoint IN: " << endpointIN << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Try to initialize the optical sensors
    r = prepareSensors();
    if ( r != 0 ) {
        std::cout << "Looks like the firmware is not flashed..." << std::endl;

        // If this fails, try to reflash the microcontroller with the Trackball firmware
        r = flashCypress( fwpath );
        if ( r != 0 ) {
            // If the flashing fails, return
            std::cout << "Could not flash firmware." << std::endl;
            return -1;
        } else {
            std::cout << "Firmware flashed successfully." << std::endl;
        }

        // Try again to initialize the optical sensors
        r = prepareSensors();
        if ( r != 0 ) {
            std::cout << "Impossible to initiate connection with the ADNS sensors." << std::endl;
            return -1;
        }

    }

    // If all went well
    std::cout << "Trackball ready." << std::endl;

    return 0;
}

int Trackball::disconnectUSB() {

    cyusb_clear_halt(device, endpointOUT);
    cyusb_clear_halt(device, endpointIN);
    cyusb_release_interface(device, INTF);
    cyusb_release_interface(device, ALTINTF);
    cyusb_close();

    return 0;
}

int Trackball::printSensorsInfo() {

    int r { 1 };
    unsigned char bufID[4] { 0 };

    // Read Product ID of the 2 chips

    // Product_ID address: 0x00 (0000 0000)
    // 1st bit must stay 0 (because Read) so no need to change
    writeBuffer[0] = PRODUCT_ID;
    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 1, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Error writing to optical chips PROD_ID address" << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Get back the response
    r = cyusb_bulk_transfer(device, endpointIN, readBuffer, 2, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Error reading optical chips PROD_ID address" << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Put the response into the ID buffer
    bufID[0] = readBuffer[0];
    bufID[1] = readBuffer[1];

    // Rev_ID address: 0x01 (0000 0001)
    // 1st bit must stay 0 (because Read) so no need to change
    writeBuffer[0] = REV_ID;
    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 2, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Error writing to optical chips REV_ID address" << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Get back the response
    r = cyusb_bulk_transfer(device, endpointIN, readBuffer, 2, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Error reading optical chips REV_ID address" << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Put the response into the ID buffer
    bufID[2] = readBuffer[0];
    bufID[3] = readBuffer[1];

    // Print the info
    for ( int i = 0; i < 2; i++ ) {

        switch ( bufID[i] ) {
            case ADNS5090_ID :
                std::cout << "Chip " << i << ": ADNS-5090 rev." << +bufID[i+2] << std::endl; //"+" to force-print a nb
                break;
            case ADNS3050_ID :
                std::cout << "Chip " << i << ": ADNS-3050 rev." << +bufID[i+2] << std::endl;
                break;
            default:
                std::cout << "Chip " << i << ": Unknown" << std::endl;
                break;
        }
    }

    return 0;
}


void Trackball::reset( bool resetAll ) {

    transferred = 0;
    ackCount = 0;

    if (diskwriteEnabled) {
        saveFiles();
    }

    // Reset buffers
    for ( int i : readBuffer )
        readBuffer[i] = 0;

    for ( int i : writeBuffer )
        writeBuffer[i] = 0;

    for ( int i : formattedBuffer )
        formattedBuffer[i] = 0;

    // Reinitialize the button to 1 (= non pressed)
    readBuffer[6] = 0x01;

    if ( resetAll ) {
        disableDiskwrite();
        disableSensorView();
        disableNetwork();
    }

}


// Routines
void Trackball::acquire() {

    int r { 1 };

    // Send 'Motion status' command to device and acquire back DX, DY, and SQ for both sensors
    // Motion address: 0x02
    writeBuffer[0] = MOTION_ST; // Ask to read address 02 (0000 0010), Read mode so no need to change 1st bit

    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 1, &transferred, 1000);
    if ( r != 0 ) {
        std::cout << "Failed to send 'get_motion_status' command." << std::endl;
        return;
    }

    // Read 6 bytes: two DX bytes, two DY bytes, two SQ bytes, in the read buffer
    r = cyusb_bulk_transfer(device, endpointIN, readBuffer, 7, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Failed to read DX, DY, SQ bytes" << std::endl;
        return;
    }

    // Interpret the readBuffer and fill the formattedBuffer variable

    // ------------------------ formattedBuffer -------------------------
    // |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |
    // |  X0 |  X1 |  Y0 |  Y1 | DX0 | DX1 | DY0 | DY1 | SQ0 | SQ0 |

    int dx, dy;

    for ( int i = 0; i < 2; i++ ) {

        // To safely store the formattedBuffer into normal signed int variables,
        // we must correctly interpret readBuffer's binary values:
        dx = static_cast<signed char>(readBuffer[i]);    // DX and DY are signed 8-bit (see datasheet),
        dy = static_cast<signed char>(readBuffer[i+2]);  // so we must explicitly cast to that

        // X: increment X by DX
        formattedBuffer[i] += dx;

        // Y: increment Y by DY
        formattedBuffer[i+2] += dy;

        // DY: store DX
        formattedBuffer[i+4] = dx;

        // DY: store DY
        formattedBuffer[i+6] = dy;

        // SQ: store SQ
        formattedBuffer[i+8] = readBuffer[i+4];    // SQ is weird (it is the upper 8 bits of an unsigned 9-bit integer),
    }                                         // but we can interpret it as an unsigned char (i.e. just like readBuffer)

    std::stringstream txtbuffer;

    txtbuffer << std::setw(7) << ackCount << ";"
              << std::setw(7) << formattedBuffer[0] << ";"
              << std::setw(7) << formattedBuffer[2] << ";"
              << std::setw(7) << formattedBuffer[1] << ";"
              << std::setw(7) << formattedBuffer[3] << ";"
              << std::setw(7) << formattedBuffer[8] << ";"
              << std::setw(7) << formattedBuffer[9] << std::endl;

    if ( consoleOutput ) {
        std::cout << txtbuffer.str();
    }

    if ( diskwriteEnabled ) {

        dataA << txtbuffer.str();
        dataA.flush();

//      // Finally check if the button is pressed
//        if ( readBuffer[6] == 0 ) {
//            std::cout << "Button: " << ackCount << std::endl;
//            dataB << txtbuffer.str();
//            dataB.flush();
//
//        }
    }

    if ( networkEnabled )
        transmit();

    ackCount++;
}

void Trackball::marker( const char& key ) {

    std::cout << ackCount << ": " << key << " pressed" << std::endl;
    if ( diskwriteEnabled ) {

        std::stringstream txtbuffer;

        txtbuffer << std::setw(7) << ackCount << ";"
                  << std::setw(7) << formattedBuffer[0] << ";"
                  << std::setw(7) << formattedBuffer[2] << ";"
                  << std::setw(7) << formattedBuffer[1] << ";"
                  << std::setw(7) << formattedBuffer[3] << ";"
                  << std::setw(7) << formattedBuffer[8] << ";"
                  << std::setw(7) << formattedBuffer[9] << std::endl;

        std::cout << ackCount << " : " << key << " pressed" << std::endl;
        dataP << txtbuffer.str();
        dataP.flush();
    }
}

unsigned char* Trackball::sensorView() {

    int r { 1 };

    // If the memory is not yet allocated, do it
    if ( !ptrImages || !sensorviewEnabled )
    {
        r = enableSensorView();
    }

    if ( r < 0 )
    {
        return nullptr;
    }

    // Send 'pixel_grab' command to the device
    // PIX_GRAB address: 0x0B (0000 1011)
    writeBuffer[0] = PIX_GRAB;
    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 1, &transferred, 1000);

    if ( r < 0 ) {
        std::cout << "Error sending 'pixel grab' command." << std::endl;
        cyusb_error(r);
        return nullptr;
    }

    // Perform as many read operations as there are of pixels per sensor
    for ( int k = 0; k < sensorSize*sensorSize; k++ )
    {
        r = cyusb_bulk_transfer(device, endpointIN, readBuffer, 2, &transferred, 1000);

        if ( r < 0 ) {
            std::cout << "Error reading 1 pixel." << std::endl;
            cyusb_error(r);
            return nullptr;
        }

        ptrImages[k] = readBuffer[0];                           // 1st image starting from 0
        ptrImages[k + sensorSize*sensorSize] = readBuffer[1];   // 2nd image starting from end of 1st image
    }

    // Now get a live reading of the Surface Quality
    writeBuffer[0] = SQUAL;

    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 1, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Error querying Surface Quality." << std::endl;
        cyusb_error(r);
        // Not critical, no need to return
    }

    r = cyusb_bulk_transfer(device, endpointIN, readBuffer, 2, &transferred, 1000);
    if ( r < 0 ) {
        std::cout << "Error reading Surface Quality." << std::endl;
        cyusb_error(r);
        // Not critical, no need to return
    }

    // Interpret the readBuffer and fill SQ sections of the formattedBuffer variable
    for ( int i = 0; i < 2; i++ ) {
        formattedBuffer[i+8] = readBuffer[i]; // SQ is weird (it is the upper 8 bits of an unsigned 9-bit integer),
    }                                         // but we can interpret it as an unsigned char (i.e. just like readBuffer)

    ackCount++;

    return ptrImages;
}



// Private methods
int Trackball::flashCypress( const std::string& firmwarefile, Memory dest, RAM ramType, EEPROM romType ) {

    int r { 1 };

    if ( dest == Memory::RAM ) {

        if ( firmwarefile.substr( firmwarefile.find_last_of('.') + 1 ) == "hex" ) {

            if (ramType == RAM::Internal)
                r = fx2_ram_download(device, firmwarefile.c_str(), 0);

            else if (ramType == RAM::External)
                r = fx2_ram_download(device, firmwarefile.c_str(), 1);

        }
    }

    else if ( dest == Memory::EEPROM ) {

        if ( firmwarefile.substr( firmwarefile.find_last_of('.') + 1 ) == "iic" ) {

            if (romType == EEPROM::Small)
                r = fx2_eeprom_download(device, firmwarefile.c_str(), 0);

            else if (romType == EEPROM::Large)
                r = fx2_eeprom_download(device, firmwarefile.c_str(), 1);
        }
    }

    return r;
}

int Trackball::prepareSensors() {

    int r { 1 };
    unsigned char rwBit { 0x80 };

    // Send RESET signal to the two optical chips

    // CHIP_RESET address: 0x3A (0011 1010)
    // This is a Read/Write address so we set 1st bit to 1: 0x80 (1000 0000) to indicate Write
    // Reset command 0x5A (0101 1010) is then sent by the microcontroller (see firmware.c file)
    writeBuffer[0] = ( CHIP_RESET | rwBit );        // Effectively send 0xBA to the microcontroller

    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 1, &transferred, 1000);
    if ( r != 0 ) {
        std::cout << "Could not send reset signal to the optical sensors." << std::endl;
        cyusb_error(r);
        return -1;
    }

    // Disable the LEDs rest mode

    // NAV_CTRL2 address: 0x22 (0010 0010)
    // Read/Write address so we set 1st bit to 1: 0x80 (1000 0000) to indicate Write
    writeBuffer[0] = ( NAV_CTRL2 | rwBit );         // Effectively send 0xA2 to the microcontroller

    r = cyusb_bulk_transfer(device, endpointOUT, writeBuffer, 1, &transferred, 1000);
    if ( r != 0 ) {
        std::cout << "Error disabling LEDs rest mode" << std::endl;
        cyusb_error(r);
        return -1;
    }

    return 0;
}

void Trackball::transmit() {


    size_t msgLen = sizeof(readBuffer);

    int sendFlags = 0;

    // ssize_t is just a signed size_t (because sendto returns -1 on error)
    ssize_t sentBytes = sendto( sockUDP, readBuffer, msgLen, sendFlags, (sockaddr*)&addrDest, sizeof(addrDest) );

    if ( sentBytes < 0 )
        std::cout << "Error sending data" << std::endl;
}



// Getters
int Trackball::getCount() const {
    return ackCount;
}

int* Trackball::getMotionData() {
    return formattedBuffer;
}

std::string Trackball::getName() const {
    return formattedName;
}


// Setters
void Trackball::setFirmwarePath( const std::string& path ) {
    this->fwpath = path;
}

void Trackball::setOutputPath( const std::string& outputFolder ) {
    this->outpath = outputFolder;
}
