//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedSC24BaseStrmVC.cpp : C++ demo program for Acqiris Analyzers
//----------------------------------------------------------------------------------------
//  Copyright (C) 2006-2009 Agilent Technologies, Inc.
//
//////////////////////////////////////////////////////////////////////////////////////////

/* Description:
 *
 * This GetStarted demonstrates the use of the BaseStreamer firmware for the SC240. It
 * performs the following steps:
 *
 * - Configure the digitizer (sample rate, full scale range, trigger setup etc.)
 * - Start the acquisition and streaming of the data to the DPU
 * - Load the "SC240str1.bit" bit file into the FPGA
 * - Initialize the firmware
 * - Start the data streaming through the optical data link
 * - Capture one monitor block each from the Rx and Tx monitor buffers
 * - Stop the acquisition
 *
 * In the function ReadMonitorBlock(), you can see an example of how to interpret the
 * data in the monitor blocks, which have a relatively complex, heterogenous format.
 *
 * You will need to loop the Tx port of first optical data link (link 0) back to it's
 * Rx port for this example to work properly. Otherwise you will get a timeout on
 * capture and the contents of the Rx Monitor buffer will be undefined.
 */

#include <fstream>
#include <iostream>
#include <windows.h> // for 'Sleep'.
using std::cout; using std::endl;

#include "AcqirisImport.h" // Common Import for all Agilent Acqiris product families
#include "AcqirisD1Import.h" // Import for Agilent Acqiris Digitizers

// Uncomment the following line to log fpga read / write access in a file called "FpgaIo.log"
//#define FPGA_IO_LOG


// Macro for status code checking
char ErrMsg[256];
#define CHECK_API_CALL(f, s) { if (s)\
{ Acqrs_errorMessage(VI_NULL, s, ErrMsg, 256); cout<<f<<": "<<ErrMsg<<endl; } }

// Register addresses
static const long ReadAddrReg       =   0;          // Indirect Access Port
static const long StartAddrReg      =   1;          // Start address within block
static const long BufferIDReg       =   2;          // Buffer Identifier Register
static const long FPGACtrlReg       =   3;          // FPGA control register
static const long FPGAStatusReg     =   6;          // FPGA status register
static const long DECtrlReg         =   8;          // DE-bus control register (from MAC)
static const long TriggerCtrlReg    =  12;
static const long MainCtrlReg       =  64;          // Main streamer control register
static const long TxMonCtrlReg      =  66;          // Transmit Monitor control and status register
static const long RxMonCtrlReg      =  67;          // Receive Monitor control and status register
static const long StrmStatusReg     =  68;          // Stream Status register
static const long StrmConfReg       =  73;          // Stream Configuration register
static const long SLC0CtrlReg       =  80;          // SLC Control register for data link 0
static const long SLC0StatusReg     =  81;          // SLC Status register for data link 0

// Buffer IDs for indirect addressing
static const long TxMonitorID       = 0x10;
static const long RxMonitorID       = 0x20;

// Streamer configuration
static const long NbrSamples        = 2048;         // Number of samples per frame
static const long NbrAccum          =   64;         // Number of acquisitions to accumulate

// Name of the streamer bitfile.
static char fpgaFileName[] = "SC240str1.bit";
// Name of acquisition data file.
static const char dataFileName[] = "Acqiris.data";
// Name of IO log file, only created if 'FPGA_IO_LOG' is defined.
static const char fpgaIoLogFileName[] = "FpgaIo.log";



//! Start digitizing the signal and stream to digital data to the FPGA.
void Acquire(ViSession instrID)
{
    ViStatus status;
    cout << "Starting acquisition\n";

    status = AcqrsD1_configMode(instrID, 1, 0, 0);    // Set the mode to 'Streaming to DPU' ( = 1)
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


//! Capture data into the 'Tx' and 'Rx' monitoring buffers
void CaptureMonitorBlock(ViSession instrID, std::ofstream &ioLogFile)
{
    cout << "Capturing monitor block...\n";

    long bufCtrl = 0;
    long mainCtrl = 0;
    // Set the 'capture' bits in the main control register
    ReadFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
    mainCtrl |= 0x00006000L;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);

    // Wait for end of capture
    bool ready = false;
    long timeout = 100;

    while (!ready && timeout > 0)
    {
        --timeout;

        // Test the 'buffer ready' bit of the Monitor Control and Status registers
        // For Tx...
        ReadFPGA(instrID, ioLogFile, TxMonCtrlReg, 1, &bufCtrl);
        ready = (bufCtrl & 0x80000000L) != 0;
        // ... and Rx
        ReadFPGA(instrID, ioLogFile, RxMonCtrlReg, 1, &bufCtrl);
        ready = ready && (bufCtrl & 0x80000000L) != 0;

        if (!ready) Sleep(10);
    }

    // Reset the capture bits to 0
    mainCtrl &= ~0x00006000L;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);

    if (timeout <= 0)
        cout << "WaitForEndOfCapture: Timeout on Capture\n";
}


//! Digitizer Configuration
void Configure(ViSession instrID)
{
    ViStatus status;

    cout << "Configuring digitizer\n";

    double sampInterval = 1.e-9;
    double delayTime = 0.0;
    long coupling = 3;
    long bandwidth = 0;
    double fullScale = 2.0;
    double offset = 0.0;
    long trigCoupling = 0;
    long trigSlope = 0;
    double trigLevel = 20.0; // In % of vertical Full Scale !!

    // Configure timebase
    status = AcqrsD1_configHorizontal(instrID, sampInterval, delayTime);
    CHECK_API_CALL("AcqrsD1_configHorizontal", status);   

    // Configure vertical settings of channel 1
    status = AcqrsD1_configVertical(instrID, 1, fullScale, offset, coupling, bandwidth);
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


//! Initialization of the FPGA.
void InitFPGA(ViSession instrID, std::ofstream &ioLogFile)
{
    ViStatus status;
    cout << "Initializing FPGA\n";

    // Initialize and start the 1ns trigger manager AFTER acquisition has started
    long value = 4; // initialize the DCM
    WriteFPGA(instrID, ioLogFile, TriggerCtrlReg, 1, &value);
    Sleep(10);      // Wait some time
    value = 8;      // Reset the timestamp counter
    WriteFPGA(instrID, ioLogFile, TriggerCtrlReg, 1, &value);
    Sleep(10);      // Wait some time
    value = 1;      // Start the 1ns Trigger Manager
    WriteFPGA(instrID, ioLogFile, TriggerCtrlReg, 1, &value);

    // Turn on the PLL reference clock for the Rocket IO
    status = Acqrs_setAttributeString(instrID, 0, "odlTxBitRate", "2.5G");
    CHECK_API_CALL("Acqrs_setAttributeString", status);   

    Sleep(10);      // Wait for PLL to stabilize

    // Set the DCM enable bits in the FPGA configuration register
    long fpgaCtrl = 0;
    WriteFPGA(instrID, ioLogFile, FPGACtrlReg, 1, &fpgaCtrl);       // First disable everything
    fpgaCtrl |= 0x00ff0000;                     // Enable all DCMs
    WriteFPGA(instrID, ioLogFile, FPGACtrlReg, 1, &fpgaCtrl);
    Sleep(10);                                  // Wait some time

    // Start the DE interface
    long deCtrl = 0x80000000;
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

    if (timeout <= 0)
        cout << "Timeout while waiting for DE clock!\n";

   // Clear the Main Control Register
   long mainCtrl = 0;
   WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
}


//! Load the firmware into the FPGA.
void LoadFPGA(ViSession instrID)
{
    ViStatus status;
    cout << "Loading firmware '" << fpgaFileName << "'\n";

    // Clear the FPGA first
    status = Acqrs_configLogicDevice(instrID, "Block1Dev1", NULL, 1);
    CHECK_API_CALL("Acqrs_configLogicDevice", status);   

    // Load the FPGA (flag = 3 will allow a search for FPGAPATH in the 'AqDrv4.ini' file)
    status = Acqrs_configLogicDevice(instrID, "Block1Dev1", fpgaFileName, 3);
    CHECK_API_CALL("Acqrs_configLogicDevice", status);   

    if (status == VI_SUCCESS)
    {
        // Display some information about the loaded firmware
        char text[256];
        status = Acqrs_getInstrumentInfo(instrID, "LogDevHdrBlock1Dev1S name", &text);
        CHECK_API_CALL("Acqrs_getInstrumentInfo", status);   

        cout << "Firmware file name: '" << text << "'\n";

        status = Acqrs_getInstrumentInfo(instrID, "LogDevHdrBlock1Dev1S version", &text);
        CHECK_API_CALL("Acqrs_getInstrumentInfo", status);   

        cout << "Version: " << text << " --- ";

        status = Acqrs_getInstrumentInfo(instrID, "LogDevHdrBlock1Dev1S compDate", &text);
        CHECK_API_CALL("Acqrs_getInstrumentInfo", status);   

        cout << text << "\n\n";
    }
}


//! Write the frame headers for Rx and Tx to the output file.
void WriteHeaders(std::ofstream &outFile, unsigned long* txHeaderP, unsigned long* rxHeaderP)
{
    outFile.width(20); outFile << "Frame type";
    outFile.width(20); outFile << ((txHeaderP[0] & 0xFF000000L) >> 24);
    outFile.width(20); outFile << ((rxHeaderP[0] & 0xFF000000L) >> 24);
    outFile << "\n";

    // Convert the timestamps to seconds
    double const txTimestamp = (txHeaderP[0] & 0x00FFFFFFL) * 0.016777216 + txHeaderP[1] * 1e-9;
    double const rxTimestamp = (rxHeaderP[0] & 0x00FFFFFFL) * 0.016777216 + rxHeaderP[1] * 1e-9;

    outFile.width(20); outFile << "Timestamp";
    outFile.width(20); outFile << txTimestamp;
    outFile.width(20); outFile << rxTimestamp;
    outFile << "\n";
}


//! Wait an 'enter' from the user.
void WaitForOperatorTo(char const* text)
{
    cout << "Please press 'Enter' to " << text << "\n";
    char ch = getchar();
}


//! Read out the monitor data.
void ReadMonitorBlock(ViSession instrID, std::ofstream &ioLogFile)
{
    cout << "Reading monitor data\n";

    // MONITOR DATA STRUCTURE:
    // NOTE: the value <sample count> corresponds to 16 * <nbr blocks> where <nbr blocks>
    //       is 1 plus the value written to the Stream Configuration register
    // 
    // 1) 16 bytes for the 'raw waveform' frame header
    // 2) <sample count> interleaved 'raw waveform' samples of 1 byte each
    // 3) 16 bytes for the 'accumulated waveform' frame header
    // 4) <sample count> interleaved 'accumulated waveform' samples of 2 bytes each
    // 5) 16 bytes for the 'parameter data' frame header
    // 6) 1024 parameter data values of 4 bytes each
    //
    // The frame headers consist of 4 4-byte integers ('longs') containing the following data:
    // long #0: Timestamp high part, with frame type in most significant byte
    // long #1: Timestamp low part
    // long #2+3: dummy data
    //
    // The frame type is:
    // 0x00 for the raw waveform,
    // 0x01 for the accumulated waveform,
    // 0x02 for parameter data

    static const long NbrLongs =
          4                 // for 'raw waveform' frame header
        + NbrSamples / 4    // for 'raw waveform' samples
        + 4                 // for 'accumulated waveform' frame header
        + NbrSamples / 2    // for 'accumulated waveform' samples
        + 4                 // for 'parameter data' header
        + 1024;             // for 'parameter data'

    long txData[NbrLongs];  // Buffer for 'Tx Monitor' data
    long rxData[NbrLongs];  // Buffer for 'Rx Monitor' data

    long startAddr  = 0;    // Read from beginning of monitor buffers

    // read Tx Monitor data
    long bufferID = TxMonitorID;
    WriteFPGA(instrID, ioLogFile, StartAddrReg, 1, &startAddr);
    WriteFPGA(instrID, ioLogFile, BufferIDReg,  1, &bufferID);
    ReadFPGA(instrID, ioLogFile, ReadAddrReg, NbrLongs, txData);

    // read Rx Monitor data
    bufferID = RxMonitorID;
    WriteFPGA(instrID, ioLogFile, StartAddrReg, 1, &startAddr);
    WriteFPGA(instrID, ioLogFile, BufferIDReg,  1, &bufferID);
    ReadFPGA(instrID, ioLogFile, ReadAddrReg, NbrLongs, rxData);

    // Write the data to a file
    std::ofstream outFile(dataFileName);
    if (!outFile.is_open())
    {
        cout << "Error opening file '" << dataFileName << "'\n";
        WaitForOperatorTo("continue");
        return;
    }

    cout << "Writing acquired monitor data to file\n";
    outFile.width(20); outFile << "Data Kind";
    outFile.width(20); outFile << "Tx Monitor Data";
    outFile.width(20); outFile << "Rx Monitor Data";
    outFile << "\n";
    
    unsigned long* txCurrentP = (unsigned long*)txData;
    unsigned long* rxCurrentP = (unsigned long*)rxData;

    // Raw waveform data
    outFile << "Raw Waveform\n";
    WriteHeaders(outFile, txCurrentP, rxCurrentP); // Write out the 'Raw Waveform' header

    txCurrentP += 4; rxCurrentP += 4;           // Advance to sample data
    char* txRawSamplesP = (char*)txCurrentP;
    char* rxRawSamplesP = (char*)rxCurrentP;
    outFile.width(20); outFile << "Samples";
    outFile.width(20); outFile << (int)txRawSamplesP[0];
    outFile.width(20); outFile << (int)rxRawSamplesP[0];
    outFile << "\n";

    for(int i=1; i<NbrSamples; ++i)
    {
        outFile.width(20); outFile << "";
        outFile.width(20); outFile << (int)txRawSamplesP[i];
        outFile.width(20); outFile << (int)rxRawSamplesP[i];
        outFile << "\n";
    }

    // Accumulated waveform data
    outFile << "Accumulated Waveform\n";
    txCurrentP += NbrSamples/4; rxCurrentP += NbrSamples/4; // Advance to 'Accumulated Waveform' header
    WriteHeaders(outFile, txCurrentP, rxCurrentP); // Write out the 'Accumulated Waveform' header

    txCurrentP += 4; rxCurrentP += 4;           // Advance to accumulated sample data
    short* txAccumSamplesP = (short*)txCurrentP;
    short* rxAccumSamplesP = (short*)rxCurrentP;
    outFile.width(20); outFile << "Samples";
    outFile.width(20); outFile << (int)txAccumSamplesP[0];
    outFile.width(20); outFile << (int)rxAccumSamplesP[0];
    outFile << "\n";

    long i;
    for(i=1; i<NbrSamples; ++i)
    {
        outFile.width(20); outFile << "";
        outFile.width(20); outFile << (int)txAccumSamplesP[i];
        outFile.width(20); outFile << (int)rxAccumSamplesP[i];
        outFile << "\n";
    }

    // parameter data
    outFile.width(20); outFile << "Parameter data\n";
    txCurrentP += NbrSamples/2; rxCurrentP += NbrSamples/2; // Advance to 'parameter data' header
    WriteHeaders(outFile, txCurrentP, rxCurrentP);             // Write out the 'parameter data' header

    txCurrentP += 4; rxCurrentP += 4;           // Advance to parameter data
    outFile.width(20); outFile << "Data";
    outFile.width(20); outFile << txCurrentP[0];
    outFile.width(20); outFile << rxCurrentP[0];
    outFile << "\n";

    for(i=1; i<1024; ++i)
    {
        outFile.width(20); outFile << "";
        outFile.width(20); outFile << txCurrentP[i];
        outFile.width(20); outFile << rxCurrentP[i];
        outFile << "\n";
    }

    outFile.close();
    cout << "Operation completed: Wrote 1 monitoring data block to disk\n";
}


//! Initialization of the optical data links.
/*! NOTE: The polarity of the links depends on the hardware option. For more info, see
    the section on programming the streamer firmware in the streamer user manual. */
void StartLink(ViSession instrID, std::ofstream &ioLogFile)
{
    ViStatus status;

    long regAddr = 0;
    long slcCtrl = 0;
    long slcStatus = 0;
    long timeout = 1000;    // With a 'Sleep' of 10 ms, should be a timeout of 10 s
    bool ready = false;
    long nbrLinks = 0;
    
    // Check if you have the '2 link' or the '12 link' option
    status = Acqrs_getInstrumentInfo(instrID, "LogDevDataLinks", &nbrLinks);
    CHECK_API_CALL("Acqrs_getInstrumentInfo", status);   

    cout << "Initializing optical data links...\n";
    if (nbrLinks <= 2)
    {
        // For '2 link' hardware:
        // Tx polarity = default, Rx polarity = inverted
        // Rx FIFO threshold = 0x3f
        // Tx Enable = 1, Rx Enable = 1
        slcCtrl = 0x023f0003L;
    }
    else
    {
        // For '12 link' hardware:
        // Tx polarity = inverted, Rx polarity = inverted
        // Rx FIFO threshold = 0x3f
        // Tx Enable = 1, Rx Enable = 1
        slcCtrl = 0x033f0003L;
    }

    WriteFPGA(instrID, ioLogFile, SLC0CtrlReg, 1, &slcCtrl);

    do
    {
        --timeout;
        // Check the data link status
        ReadFPGA(instrID, ioLogFile, SLC0StatusReg, 1, &slcStatus);
        // Test the "Tx physical layer ready" and "Tx link layer ready" bits for the transmitter
        // Test the "Rx physical layer ready" and "Rx link layer ready" bits for the receiver
        ready = (slcStatus & 0x0000005cL) == 0x0000005cL;

        if (!ready) Sleep(10);
    }
    while (!ready && timeout > 0);

    if (timeout <= 0)
    {
        cout << "Timeout while waiting for data links (Is Tx connected to Rx ?)\n";
    } else
    {
        // Reset the link status flags
        ReadFPGA(instrID, ioLogFile, SLC0CtrlReg, 1, &slcCtrl);
        slcCtrl |= 0x80000000L;         // "Reset status flags" bit
        WriteFPGA(instrID, ioLogFile, SLC0CtrlReg, 1, &slcCtrl);

        cout << "Data links ready!\n";
    }
}


//! Start the Streamer core
void StartStream(ViSession instrID, std::ofstream &ioLogFile)
{
    printf("Starting data streaming\n");
    // Configure the frame size
    long strmConf = NbrSamples / 16 - 1;
    WriteFPGA(instrID, ioLogFile, StrmConfReg, 1, &strmConf);

    // Configure and start the Streamer
    // Transfer on = 1
    // Use bidirectional link = 1
    long mainCtrl = 0x00008100L;
    // Set the number of accumulations
    mainCtrl |= (NbrAccum - 1) << 24;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);
}


//! Stop the streaming firmware and data acquisition.
void Stop(ViSession instrID, std::ofstream &ioLogFile)
{
    ViStatus status;

    cout << "Stopping\n";
    // Turn off the streamer
    long mainCtrl = 0;
    WriteFPGA(instrID, ioLogFile, MainCtrlReg, 1, &mainCtrl);

    // Turn off the data links
    long slcCtrl = 0xc0000000;            // Reset Rx Controller and status flags
    WriteFPGA(instrID, ioLogFile, SLC0CtrlReg, 1, &slcCtrl);
    slcCtrl = 0;                          // Turn everything off
    WriteFPGA(instrID, ioLogFile, SLC0CtrlReg, 1, &slcCtrl);

    // Turn off the link frequency PLL
    status = Acqrs_setAttributeString(instrID, 0, "odlTxBitRate", "None");
    CHECK_API_CALL("Acqrs_setAttributeString", status);   

    // Turn off the 1 ns Trigger Manager
    long trigCtrl = 0;
    WriteFPGA(instrID, ioLogFile, TriggerCtrlReg, 1, &trigCtrl);

    // Stop the DE interface
    long deCtrl = 0x0;
    WriteFPGA(instrID, ioLogFile, DECtrlReg, 1, &deCtrl);

    // Stop the DCMs
    long fpgaCtrl = 0;
    WriteFPGA(instrID, ioLogFile, FPGACtrlReg, 1, &fpgaCtrl);

    // Stop data conversion
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

    ViSession instrID = FindDevices();          // Initialize the Analyzer(s)
    WaitForOperatorTo("continue");

    LoadFPGA(instrID);             // Load the firmware into the FPGA
    Configure(instrID);            // Configure the digitizer
    Acquire(instrID);              // Start continuous acquisition + transfer to FPGA
    InitFPGA(instrID, ioLogFile);             // Initialize the FPGA
    StartLink(instrID, ioLogFile);            // Start the optical data link
    StartStream(instrID, ioLogFile);          // Start the Streamer core
    CaptureMonitorBlock(instrID, ioLogFile);  // Capture a monitoring block in the FPGA
    ReadMonitorBlock(instrID, ioLogFile);     // Read a block of data from the FPGA
    Stop(instrID, ioLogFile);                 // Stop the FPGA and the acquisition

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

