//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedASC240StreamerVC.cpp : C++ demo program for Agilent Acqiris Analyzers
//----------------------------------------------------------------------------------------
//  Copyright (C) 2005-2009 Agilent Technologies, Inc.
//
//////////////////////////////////////////////////////////////////////////////////////////

/* Description:
 *
 * This GetStarted demonstrates the use of the Streamer2 firmware for the SC240. It
 * performs the following steps:
 *
 * - Configure the digitizer (sample rate, full scale range, trigger setup etc.)
 * - Start the acquisition and streaming of the data to the DPU
 * - Load the "sc240stream2.bit" bit file into the FPGA
 * - Initialize the firmware
 * - Start the data streaming through the optical data links
 * - Capture one monitor block each from the DS, Tx and (optionnally) Rx monitors
 *   for data link 0
 * - Stop the acquisition
 *
 * If you want to use the Rx Monitor buffer, uncomment the line "#define USE_RX_LINK"
 * below. See the comment just above that line for information about providing an
 * input signal.
 */

#include <fstream>
#include <iostream>
#include <windows.h> // for 'Sleep'.
using std::cout; using std::endl;

#include "AcqirisImport.h" // Common Import for all Agilent Acqiris product families
#include "AcqirisD1Import.h" // Import for Agilent Acqiris Digitizers


// Uncomment the following line to enable the odl receiver; You will have to loop the
// odl outputs back to the inputs, input 1 to output 2 and vice versa, for this to work
// (or provide your own input signal)
//#define USE_RX_LINK

// Uncomment the following line to log fpga read / write access in a file called "FpgaIo.log"
//#define FPGA_IO_LOG



// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }

// The following register addresses correspond to the default FPGA-firmware
static const long ReadAddrReg       =  0;          // Indirect Access Port
static const long StartAddrReg      =  1;          // Start address within block
static const long BufferIDReg       =  2;          // Buffer Identifier Register
static const long FPGACtrlReg       =  3;          // FPGA control register
static const long FPGAStatusReg     =  6;          // FPGA status register
static const long DECtrlReg         =  8;          // DE-bus control register (from MAC)
static const long TriggerCtrlReg    = 12;          // 1ns Trigger Manager control register

// The following register addresses are specific to the streamer firmware
static const long MainCtrlReg           =     64;       // Main Control for ALL Acqiris-defined applications
static const long DsMonCtrl             =     65;       // Control register for data stream monitor
static const long TxMonCtrl             =     66;       // Control register for transmit monitor
static const long RxMonCtrl             =     67;       // Control register for receive monitor
// - Streamer status registers
static const long StreamerStatusGlob    =     68;       // Status of all data links
static const long StreamerStatusSrcA    =     69;
static const long StreamerStatusSrcB    =     70;
// - Streamer configuration registers
static const long StreamerConfigGlob    =     72;       // Stream Configuration
static const long StreamerConfigSrcA    =     73;
static const long StreamerConfigSrcB    =     74;

// - Optical data link configuration registers:
static const long StreamerSLCbase1  =     80;           // Base address for data link 1 registers

static const long SLCbaseOffset     =      4;           // SLC base address offset to the next link
static const long SLCctrlOffset     =      0;           // SLC control register offset (wrt to base addr)
static const long SLCstatusOffset   =      1;           // SLC status  register offset (wrt to base addr)

// - The following addresses identify the various monitoring buffers
static const long DeBufferAddress   =   0x08;           // Address of data entry buffer
static const long DsMonitorAddress  =   0x0c;           // Address of data stream monitor
static const long TxMonitorAddress  =   0x10;           // Address of transmit monitor
static const long RxMonitorAddress  =   0x20;           // Address of receive monitor

// Name of the streamer bitfile.
static char fpgaFileName[] = "sc240stream2.bit";
// Name of acquisition data file.
static const char dataFileName[] = "Acqiris.data";
// Name of IO log file, only created if 'FPGA_IO_LOG' is defined.
static const char fpgaIoLogFileName[] = "FpgaIo.log";



//! Start digitizing the signal and stream the digital data to the FPGA.
void Acquire(ViSession instrID)
{
    ViStatus status;

    cout << "Starting Acquisition\n";

    status = AcqrsD1_configMode(instrID, 1, 0, 0);      // Set the mode to 'Streaming to DPU' ( = 1)
    CHECK_API_CALL("AcqrsD1_configMode", status);
    status = AcqrsD1_acquire(instrID);                // Start the acquisition
    CHECK_API_CALL("AcqrsD1_acquire", status);

    // It makes no sense to wait for the end of acquisition, since the digitizer will stream data continuously
    // to the FPGA. Using "AcqrsD1_stopAcquisition(instrID);" is the only way to stop it.
}


//! If 'FPGA_IO_LOG' is defined, format and output the 'dataArrayP' into the specified 'ioLogFile'.
void OutputArray(std::ofstream &ioLogFile, char function[], long regID, long *dataArrayP, long nbrValues)
{
#ifdef FPGA_IO_LOG
    ioLogFile << function << std::dec << " Reg #" << regID << " (" << nbrValues << "x):\n";
    for(int i=0; i<nbrValues; ++i) 
    {
        ioLogFile << "0x" << std::hex << std::internal << dataArrayP[i] << "\n";
    }
#endif
}


//! Read the specified 'regID' from the FPGA.
ViStatus ReadFPGA(ViSession instrID, std::ofstream &ioLogFile, long regID, long nbrValues, long* dataArrayP)
{
    ViStatus status;
    
    status = Acqrs_logicDeviceIO(instrID, "Block1Dev1", regID, nbrValues, dataArrayP, 0, 0);
    CHECK_API_CALL("Acqrs_logicDeviceIO", status);   

    OutputArray(ioLogFile, "ReadFPGA", regID, dataArrayP, nbrValues);

    return status;
}


//! Write into the specified 'regID' of the FPGA.
ViStatus WriteFPGA(ViSession instrID, std::ofstream &ioLogFile, long regID, long nbrValues, long* dataArrayP)
{
    ViStatus status;
    
    status = Acqrs_logicDeviceIO(instrID, "Block1Dev1", regID, nbrValues, dataArrayP, 1, 0);
    CHECK_API_CALL("Acqrs_logicDeviceIO", status);   

    OutputArray(ioLogFile, "WriteFPGA", regID, dataArrayP, nbrValues);

    return status;
}


//! Capture monitor data from the 'DS', 'Tx' and (optionally) 'Rx' streams
void CaptureMonitorBlock(ViSession instrID, std::ofstream &ioLogFile)
{
    cout << "Capturing monitor data...\n";
    long monitorCtrl, mainCtrl;
    bool ready = false;
    long timeout;

    // Reset the 'capture' bits
    ReadFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
    mainCtrl &= ~0x00007000;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);

    // Set up the Data Stream capture mode:
    // Capture mode = event triggered
    // Stream to capture = Stream A
    monitorCtrl = 0x10000000L;
    WriteFPGA(instrID, ioLogFile, DsMonCtrl, 1, &monitorCtrl);

    // Set up the 'Tx' stream capture mode:
    // Capture mode = event masked link triggered
    // Stream to capture = Stream A
    // Link to capture = Link 0
    monitorCtrl = 0x20000000L;
    WriteFPGA(instrID, ioLogFile, TxMonCtrl, 1, &monitorCtrl);

#ifdef USE_RX_LINK
    // Set up the 'Rx' stream capture mode:
    // Capture mode = Tx triggered
    // Stream to capture = Stream A
    // Link to capture = Link 1

    // With a standard paired optical cable, output 1 will be connected to
    // input 2 and vice versa, so we capture Rx link 1 together with Tx link 0.
    // The two buffers should therefore contain the same data with this setup.
    monitorCtrl = 0x20010000L;
    WriteFPGA(instrID, ioLogFile, RxMonCtrl, 1, &monitorCtrl);
    // Enable capture of 'Rx' stream
    mainCtrl |= 0x00004000L;
#endif

    // Enable capture of 'DS' and 'Tx' streams
    mainCtrl |= 0x00003000L;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);


    // Wait until data have been captured in the monitoring buffers
    timeout = 100;      // With a 'Sleep' of 1 ms, should be a timeout of 100 ms

    while ((!ready) && (timeout > 0))
    {
        --timeout;

        // test for 'DS' monitor ready
        ReadFPGA(instrID, ioLogFile, DsMonCtrl, 1, &monitorCtrl);
        ready = ((monitorCtrl & 0x80000000) != 0);
        
        // test for 'Tx' monitor ready
        ReadFPGA(instrID, ioLogFile, TxMonCtrl, 1, &monitorCtrl);
        ready = ready && ((monitorCtrl & 0x80000000) != 0);

#ifdef USE_RX_LINK
        // test for 'Rx' monitor ready
        ReadFPGA(instrID, ioLogFile, RxMonCtrl, 1, &monitorCtrl);
        ready = ready && ((monitorCtrl & 0x80000000) != 0);
#endif

        if (!ready)
            Sleep(1);
    }

    if (timeout <= 0)
        cout << "Timeout on Capture\n";
}


//! Configure the timebase, front-end and trigger of the instrument.
void Configure(ViSession instrID)
{
    ViStatus status;

    // ### Configuration ###
    double sampInterval = 1.e-9;
    double delayTime = 0.0;  // 1ns Trigger Manager only works with maximum sample rate!
    long coupling = 3;
    long bandwidth = 0;
    double fullScale = 5.0;
    double offset = 0.0;
    long trigCoupling = 0;
    long trigSlope = 0;
    double trigLevel = 10.0; // In % of vertical Full Scale !!

    // Configure timebase
    status = AcqrsD1_configHorizontal(instrID, sampInterval, delayTime);
    CHECK_API_CALL("AcqrsD1_configHorizontal", status);   

    // Configure vertical settings of channel 1
    status = AcqrsD1_configVertical(instrID, 1, fullScale, offset, coupling, bandwidth);
    CHECK_API_CALL("AcqrsD1_configVertical", status);   

    // Configure vertical settings of channel 2
    status = AcqrsD1_configVertical(instrID, 2, fullScale, offset, coupling, bandwidth);
    CHECK_API_CALL("AcqrsD1_configVertical", status);   

    // Configure edge trigger on channel 1
    status = AcqrsD1_configTrigClass(instrID, 0, 0x00000001, 0, 0, 0.0, 0.0);
    CHECK_API_CALL("AcqrsD1_configTrigClass", status);   

    // Configure the trigger conditions of channel 1 internal trigger
    status = AcqrsD1_configTrigSource(instrID, 1, trigCoupling, trigSlope, trigLevel, 0.0);
    CHECK_API_CALL("AcqrsD1_configTrigSource", status);   
}


//! Find all digitizers (virtual multi-instruments or individual instruments).
ViSession FindDevices(void)
{
    ViStatus status;

    long numInstr; // Number of instruments
    ViString rscStr; // Resource string
    ViString options; // Initialization options

    // The following call will automatically detect ASBus connections between digitizers
    // and combine connected digitizers (of identical model!) into multi-instruments.
    // The call returns the number of multi-instruments and/or single instruments.
    status = AcqrsD1_multiInstrAutoDefine("", &numInstr);
    CHECK_API_CALL("AcqrsD1_multiInstrAutoDefine", status);

    if (numInstr < 1)
    {
        cout << "No instrument found!" << endl;
        return -1; // No instrument found
    }
    // Use the first digitizer
    rscStr = "PCI::INSTR0";
    options = ""; // No options necessary

    cout << numInstr << " Agilent Acqiris Digitizer(s) found on your PC\n";

    // Initialization of the instrument //////////////////////////////////////////////////
    ViSession instrID; // Instrument handle

    status = Acqrs_InitWithOptions(rscStr, VI_FALSE, VI_FALSE, options, &instrID);
    CHECK_API_CALL("Acqrs_InitWithOptions", status);

    return instrID;
}


//! Initialization of the FPGA
/*! NOTE: This is specific to the 'sc240stream2.bit' firmware */
void InitFPGA(ViSession instrID, std::ofstream &ioLogFile)
{
    ViStatus status;

    cout << "Initializing firmware\n";
    // Enable the trigger manager AFTER acquisition has started
    long value = 1;   
    WriteFPGA(instrID, ioLogFile, TriggerCtrlReg, 1, &value);

    // Turn on the PLL reference clock for the Rocket IO
    status = Acqrs_setAttributeString(instrID, 0, "odlTxBitRate", "2.5G");
    CHECK_API_CALL("Acqrs_setAttributeString", status);

    Sleep(10);      // Wait for PLL to stabilize

    // Set the DCM enable bits in the FPGA configuration register
    long fpgaCtrl = 0;
    WriteFPGA(instrID, ioLogFile, FPGACtrlReg, 1, &fpgaCtrl);       // First disable everything
    fpgaCtrl |= 0x000c0000;                     // Enable bits for DCMA and DCMB
//  fpgaCtrl |= 0x00000100;                     // Enable readout in Big-Endian format (if needed)
    WriteFPGA(instrID, ioLogFile, FPGACtrlReg, 1, &fpgaCtrl);
    Sleep(10);                                  // Wait some time

    // Start the DE interface in the FPGA
    long deCtrl = 0;
    WriteFPGA(instrID, ioLogFile, DECtrlReg, 1, &deCtrl);
    deCtrl = 0x80000000;
    WriteFPGA(instrID, ioLogFile, DECtrlReg, 1, &deCtrl);

    // Wait until the DE clock is ready
    bool ready = false;
    long timeout = 100;             // With a 'Sleep' of 1 ms, should be a timeout of 100 ms

    while ((!ready) && (timeout > 0))
    {
        timeout--;
        
        long fpgaStatus;
        ReadFPGA(instrID, ioLogFile, FPGAStatusReg, 1, &fpgaStatus);
        ready = ((fpgaStatus & 0x00100000) != 0);
        if (!ready) 
            Sleep(1);
    }

    // Clear the Main Control Register
    long mainCtrl = 0;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
}


//! Load the streamer firmware into the FPGA
void LoadFPGA(ViSession instrID)
{
    ViStatus status;

    cout << "loading firmware '" << fpgaFileName << "'\n";
    // Clear the FPGA first
    status = Acqrs_configLogicDevice(instrID, "Block1Dev1", NULL, 1);
    CHECK_API_CALL("Acqrs_configLogicDevice", status);

    // Load the FPGA (flag = 3 will allow a search for FPGAPATH in the 'AqDrv4.ini' file)
    status = Acqrs_configLogicDevice(instrID, "Block1Dev1", fpgaFileName, 3);
    CHECK_API_CALL("Acqrs_configLogicDevice", status);
}


//! Wait an 'enter' from the user.
void WaitForOperatorTo(char const* text)
{
    cout << "Please press 'Enter' to " << text << "\n";
    char ch = getchar();
}


//! Read out the monitor data
void ReadMonitorBlock(ViSession instrID, std::ofstream &ioLogFile)
{
    static const long nbrValues = 8192;
    static const long nbrLongs = (nbrValues + 3)/4; // number of longs to read = ceil(nbrValues / 4)

    // allocate memory for the acquired data
    long dsMonitorData[nbrLongs];
    long txMonitorData[nbrLongs];

    long startAddr  = 0;
    long bufAddress;

    // Find start address for the (circular) 'DS' monitor
    ReadFPGA(instrID, ioLogFile, DsMonCtrl, 1, &startAddr);
    startAddr &= 0x0000ffff;

    // Read 'DS' monitor data
    bufAddress = DsMonitorAddress;
    WriteFPGA(instrID, ioLogFile, StartAddrReg, 1, &startAddr);
    WriteFPGA(instrID, ioLogFile, BufferIDReg, 1, &bufAddress);
    ReadFPGA(instrID, ioLogFile, ReadAddrReg, nbrLongs, dsMonitorData);

    // Read 'Tx' monitor data
    startAddr = 0;      // 'Tx' monitor starts at 0
    bufAddress = TxMonitorAddress;
    WriteFPGA(instrID, ioLogFile, StartAddrReg, 1, &startAddr);
    WriteFPGA(instrID, ioLogFile, BufferIDReg, 1, &bufAddress);
    ReadFPGA(instrID, ioLogFile, ReadAddrReg, nbrLongs, txMonitorData);

#ifdef USE_RX_LINK
   // allocate memory for the acquired data
   long rxMonitorData[nbrLongs];

    // Read 'Rx' monitor data
    startAddr = 0;      // 'Rx' monitor starts at 0
    bufAddress = RxMonitorAddress;
    WriteFPGA(instrID, ioLogFile, StartAddrReg, 1, &startAddr);
    WriteFPGA(instrID, ioLogFile, BufferIDReg, 1, &bufAddress);
    ReadFPGA(instrID, ioLogFile, ReadAddrReg, nbrLongs, rxMonitorData);
#endif

    // Write the data to a file
    std::ofstream outFile(dataFileName);
    if (!outFile.is_open())
    {
        cout << "Error opening file '" << dataFileName << "'\n";
        WaitForOperatorTo("continue");
        return;
    }

#ifdef USE_RX_LINK
    // With the 'Rx' link enabled, the file contains 3 tab-separated columns
    outFile.width(12); outFile << "In Buffer";
    outFile.width(12); outFile << "Tx Buffer";
    outFile.width(12); outFile << "Rx Buffer\n";
    char* dsBufChars = (char*)dsMonitorData;   // Treat monitoring buffers as array of 8-bit values
    char* txBufChars = (char*)txMonitorData;
    char* rxBufChars = (char*)rxMonitorData;
    for (long i = 0; i < nbrValues; i++)
    {
        outFile.width(12); outFile << (short)dsBufChars[i];
        outFile.width(12); outFile << (short)txBufChars[i];
        outFile.width(12); outFile << (short)rxBufChars[i] << "\n";
    }
#else
    // Without the 'Rx' link enabled, the file contains 2 tab-separated columns
    char* dsBufChars = (char*)dsMonitorData;   // Treat monitoring buffer as array of 8-bit values
    char* txBufChars = (char*)txMonitorData;
    outFile.width(12); outFile << "In Buffer";
    outFile.width(12); outFile << "Tx Buffer\n";
    for (long i = 0; i < nbrValues; i++)
    {
        outFile.width(12); outFile << (short)dsBufChars[i];
        outFile.width(12); outFile << (short)txBufChars[i] << "\n";
    }
#endif
    
    outFile.close();
    cout << "Operation completed: Wrote 1 monitoring data block to disk\n";
}


//! Initialization of the optical data links.
/*! NOTE: The polarity of the links depends on the hardware option. For more info, see
    the section on programming the streamer firmware in the streamer user manual. */
void StartLinks(ViSession instrID, std::ofstream &ioLogFile)
{
    ViStatus status;

    long regAddr;
    long slcCtrl;
    long slcStatus = 0;
    long timeout = 1000;    // With a 'Sleep' of 10 ms, should be a timeout of 10 s
    bool ready = false;
    long nbrLinks = 0;
    // The following bit mask contains the bits of the SLC status register which indicate
    // that the Tx Link is ready (Tx Physical Layer Ready (TXR), Tx Link Layer Ready (TXK))
    long linkReadyBits = 0x00000014L;   

#ifdef USE_RX_LINK
    // If we use the Rx link, we need to check for some more bits
    // (Rx Physical Layer Ready (RXR), Rx Link Layer Ready (RXK))
    linkReadyBits |= 0x00000048L;
#endif

    // Check if you have the '2 link' or the '12 link' option
    status = Acqrs_getInstrumentInfo(instrID, "LogDevDataLinks", &nbrLinks);
    CHECK_API_CALL("Acqrs_getInstrumentInfo", status);

    printf("Initializing optical data links...\n");
    // Configure data link 0
    regAddr = StreamerSLCbase1 + 0*SLCbaseOffset + SLCctrlOffset;

    if (nbrLinks <= 2)
    {
        // For '2 link' hardware:
        // Tx polarity = default, Rx polarity = inverted
        // Rx FIFO threshold = 0x3f
        // Tx Enable = 1
        slcCtrl = 0x023f0001L;
        
#ifdef USE_RX_LINK
        // Rx Enable = 1
        slcCtrl |= 0x00000002L;
#endif

    }
    else
    {
        // For '12 link' hardware:
        // Tx polarity = inverted, Rx polarity = inverted
        // Rx FIFO threshold = 0x3f
        // Tx Enable = 1
        slcCtrl = 0x033f0001L;
        
#ifdef USE_RX_LINK
        // Rx Enable = 1
        slcCtrl |= 0x00000002L;
#endif

    }

    WriteFPGA(instrID, ioLogFile, regAddr, 1, &slcCtrl);

    // Configure data link 1
    regAddr = StreamerSLCbase1 + 1*SLCbaseOffset + SLCctrlOffset;
    // This is the same for both hardware variants:
    // Tx polarity = inverted, Rx polarity = inverted
    // Rx FIFO threshold = 0x3f
    // Tx Enable = 1
    slcCtrl = 0x033f0001L;
    
#ifdef USE_RX_LINK
    // Rx Enable = 1
    slcCtrl |= 0x00000002L;
#endif

    WriteFPGA(instrID, ioLogFile, regAddr, 1, &slcCtrl);

    // Wait until the links are ready
    do
    {
        --timeout;
        // Check data link 0 status
        regAddr = StreamerSLCbase1 + 0*SLCbaseOffset + SLCstatusOffset;
        ReadFPGA(instrID, ioLogFile, regAddr, 1, &slcStatus);
        // Test the "Tx physical layer ready" and "Tx link layer ready" bits for the transmitter
        // Test the "Rx physical layer ready" and "Rx link layer ready" bits for the receiver
        ready = (slcStatus & linkReadyBits) == linkReadyBits;

        // Check data link 1 status
        regAddr = StreamerSLCbase1 + 1*SLCbaseOffset + SLCstatusOffset;
        ReadFPGA(instrID, ioLogFile, regAddr, 1, &slcStatus);
        // Test the "Tx physical layer ready" and "Tx link layer ready" bits for the transmitter
        // Test the "Rx physical layer ready" and "Rx link layer ready" bits for the receiver
        ready = ready && (slcStatus & linkReadyBits) == linkReadyBits;

        if (!ready) Sleep(10);
    }
    while (!ready && timeout > 0);

    if (timeout <= 0)
        cout << "Timeout while waiting for data links\n";
    else
    {
        // Reset the link status flags
        regAddr = StreamerSLCbase1 + 0*SLCbaseOffset + SLCctrlOffset;
        ReadFPGA(instrID, ioLogFile, regAddr, 1, &slcCtrl);
        slcCtrl |= 0x80000000L;         // "Reset status flags" bit
        WriteFPGA(instrID, ioLogFile, regAddr, 1, &slcCtrl);

        regAddr = StreamerSLCbase1 + 1*SLCbaseOffset + SLCctrlOffset;
        ReadFPGA(instrID, ioLogFile, regAddr, 1, &slcCtrl);
        slcCtrl |= 0x80000000L;         // "Reset status flags" bit
        WriteFPGA(instrID, ioLogFile, regAddr, 1, &slcCtrl);

        cout << "Data links ready!\n";
    }
}


//! Start data streams.
void StartStreams(ViSession instrID, std::ofstream &ioLogFile)
{
    long globalConfig;
    long streamConfig;
    long mainCtrl;

    cout << "Starting data streams\n";

    // Global streamer configuration:
    // Stream A => Link 0, Stream B => Link 1
    // Enable streams A & B
    // Streaming mode = triggered
    globalConfig = 0x85000009L;
    WriteFPGA(instrID, ioLogFile, StreamerConfigGlob, 1, &globalConfig);
    
    // Individual stream configuration:
    // User frame size = 1, stripe frame size = 512 blocks (= 8192 samples)
    streamConfig = 0x00010200L;
    WriteFPGA(instrID, ioLogFile, StreamerConfigSrcA, 1, &streamConfig);
    WriteFPGA(instrID, ioLogFile, StreamerConfigSrcB, 1, &streamConfig);

    ReadFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
    // Clear the bits we want to change
    mainCtrl &= ~0x000001F0L;
    // Main control configuration:
    // Channel A => Stream A
    // Channel B => Stream B
    // Start framing process
    mainCtrl |=  0x00000140L;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
    cout << "Streaming...\n\n";
}


//! Stop data conversion.
void Stop(ViSession instrID, std::ofstream &ioLogFile)
{
    ViStatus status;

    long mainCtrl = 0;
    long linkCtrl = 0;

    // Stop the streams
    ReadFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
    mainCtrl &= ~0x00000100L;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
    
    // Stop the links
    WriteFPGA(instrID, ioLogFile, StreamerSLCbase1 + 0*SLCbaseOffset + SLCctrlOffset, 1, &linkCtrl);
    WriteFPGA(instrID, ioLogFile, StreamerSLCbase1 + 1*SLCbaseOffset + SLCctrlOffset, 1, &linkCtrl);

    // Stop the acquisition
    status = AcqrsD1_stopAcquisition(instrID);
    CHECK_API_CALL("AcqrsD1_stopAcquisition", status);
}



//! The main routine, called by the operating system.
int main (int argc, char *argv[])
{
    ViStatus status;

    std::ofstream ioLogFile;
 
#ifdef FPGA_IO_LOG
    ioLogFile.open(fpgaIoLogFileName);
    if (!ioLogFile.is_open())
    {
        cout << "Error opening file '" << fpgaIoLogFileName << "'\n";
        WaitForOperatorTo("continue");
        return -1;
    }
#endif

    cout << "Agilent Acqiris Analyzer - Getting Started\n\n";

    ViSession instrID;
    instrID = FindDevices();                    // Initialize the Analyzer
    WaitForOperatorTo("continue");

    LoadFPGA(instrID);                          // Load the firmware into the FPGA (if not using default file)
    Configure(instrID);                         // Configure the digitizer
    Acquire(instrID);                           // Start continuous acquisition + transfer to FPGA
    InitFPGA(instrID, ioLogFile);               // Initialize the FPGA
    StartLinks(instrID, ioLogFile);             // Configure the optical data links
    StartStreams(instrID, ioLogFile);           // Start data streaming
    CaptureMonitorBlock(instrID, ioLogFile);    // Capture a monitoring block in the FPGA
    ReadMonitorBlock(instrID, ioLogFile);       // Read a block of data from the FPGA
    Stop(instrID, ioLogFile);                   // Stop the FPGA and the acquisition

    status = Acqrs_close(instrID);
    CHECK_API_CALL("Acqrs_close", status);

    status = Acqrs_closeAll();
    CHECK_API_CALL("Acqrs_closeAll", status);

#ifdef FPGA_IO_LOG
    ioLogFile.close();
#endif

    WaitForOperatorTo("exit");

    return 0;
}
