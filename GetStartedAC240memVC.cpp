//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedAC240memVC.cpp : C++ demo program for Acqiris Analyzers
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 2005, 2006-2009
//
//////////////////////////////////////////////////////////////////////////////////////////

/* Description:
 *
 * This GetStarted demonstrates access to the dual-port SRAM in the AC240 with memory option.
 * It performs the following operations:
 *
 * - Configure the digitizer (sample rate, full scale range, trigger setup etc.)
 * - Start the acquisition and streaming of the data to the DPU
 * - Load the "ac240mem.bit" bit file into the FPGA
 * - Initialize the firmware
 * - Fill the SRAM with data aquired from both input channels
 * - Read the acuqired data out of the SRAM
 * - Stop the acquisition
 *
 * Most of the code is similar to the basic GetStartedAnalyzersVC.cpp example. The firmware-
 * specific parts are mostly in the InitFPGA() function (for firmware initialisation) and in
 * the two functions WriteToMemory() and ReadMemoryBlock().
 */


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "vpptype.h"

// ### Acqiris Digitizer Device Driver ###
#include "AcqirisImport.h"
#include "AcqirisD1Import.h"


// ### Options ###
// uncomment the following line to log fpga read / write access in a file called "FpgaIo.log"
//#define FPGA_IO_LOG


#ifdef FPGA_IO_LOG
FILE* ioLogFile = NULL;
#endif




// ### Simulation flag ###
bool simulation = false;
// Set to true to simulate digitizers (useful for application development)

// ### Global variables ###
static const int maxNbrInstruments = 10;
ViSession InstrumentID[maxNbrInstruments];  // Array of instrument handles
ViSession currentID;                        // ID of currently used instrument
long NumInstruments;                        // Number of instruments
ViStatus status;        // Status returned by calls to AcqrsD1... API functions




// ### Constants ###
// The following register addresses correspond to the default FPGA-firmware
static const long ReadAddrReg        =    0;    // Indirect Access Port
static const long StartAddrReg       =    1;    // Start address within block
static const long BufferIDReg        =    2;    // Buffer Identifier Register
static const long FPGACtrlReg        =    3;    // FPGA control register
static const long FPGAStatusReg      =    6;    // FPGA status register
static const long DECtrlReg          =    8;    // DE-bus control register (from MAC)
static const long SRAMCtrlReg        =   39;    // Dual Port Memory control register
static const long MainCtrlReg        =   64;    // Main Control register
static const long MemExampleCtrlReg  =   66;    // Control register for the memory example firmware
static const long SRAMBufAddress     = 0x04;    // Buffer identifier of dual port memory



// ### Function prototypes ###
void Acquire(void);
void WriteToMemory(void);
void Configure(void);
void FindDevices(void);
void InitFPGA(void);
void LoadFPGA(void);
long ReadFPGA(long regID, long nbrValues, long* dataArrayP);
void ReadMemoryBlock(void);
void Stop(void);
void WaitForOperator(void);
long WriteFPGA(long regID, long nbrValues, long* dataArrayP);


//////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    printf("\nAcqiris Analyzer - Getting Started\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n");

#ifdef FPGA_IO_LOG
    ioLogFile = fopen("FpgaIo.log", "w");
#endif

    FindDevices();          // ### Initialize the Analyzer(s) ###
    printf("I have found %d Acqiris Analyzer(s) on your PC\n", NumInstruments);
    WaitForOperator();
    if(NumInstruments < 1)
    {
        printf("No Acqiris Analyzers found; operation aborted\n");
        return -1;
    }

    LoadFPGA();             // ### Load the firmware into the FPGA ###
    Configure();            // ### Configure the digitizer ###
    Acquire();              // ### Start continuous acquisition + transfer to FPGA ###
    InitFPGA();             // ### Initialize the FPGA ###
    WriteToMemory();        // ### Write the acquired data into the dual port memory ###
    ReadMemoryBlock();      // ### Read a block of data from the memory ###
    Stop();                 // ### Stop the FPGA and the acquisition ###

    printf("Operation completed: Wrote 1 monitoring data block to disk\n");
    WaitForOperator();

    Acqrs_closeAll();       // ### Let the driver perform any necessary cleanup tasks ###

#ifdef FPGA_IO_LOG
    fclose(ioLogFile);
    ioLogFile = NULL;
#endif

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
void Acquire(void)
{
    printf("Starting acquisition\n");
    // ### Start digitizing the signal and stream the digital data to the FPGA ###
    status = AcqrsD1_configMode(currentID, 1, 0, 0);    // Set the mode to 'Streaming to DPU' ( = 1)
    status = AcqrsD1_acquire(currentID);                // Start the acquisition
    if(status != VI_SUCCESS)
        printf("acquire: Error (%08x)\n", status);

    // It makes no sense to wait for the end of acquisition, since the digitizer will stream data continuously
    // to the FPGA. Using "AcqrsD1_stopAcquisition(currentID);" is the only way to stop it.
}


//////////////////////////////////////////////////////////////////////////////////////////
void Configure(void)
{
    // ### Configuration ###
    double sampInterval = 1.e-9, delayTime = 0.0;
    long coupling = 3, bandwidth = 0;
    double fullScale = 5.0, offset = 0.0;
    long trigCoupling = 0, trigSlope = 0;
    double trigLevel = 20.0; // In % of vertical Full Scale !!

    // Configure timebase
    status = AcqrsD1_configHorizontal(currentID, sampInterval, delayTime);

    // Configure vertical settings of channel 1
    status = AcqrsD1_configVertical(currentID, 1, fullScale, offset, coupling, bandwidth);
    // Configure vertical settings of channel 2
    status = AcqrsD1_configVertical(currentID, 2, fullScale, offset, coupling, bandwidth);

    // Configure edge trigger on channel 1
    status = AcqrsD1_configTrigClass(currentID, 0, 0x00000001, 0, 0, 0.0, 0.0);
    // Configure the trigger conditions of channel 1 internal trigger
    status = AcqrsD1_configTrigSource(currentID, 1, trigCoupling, trigSlope, trigLevel, 0.0);

}

//////////////////////////////////////////////////////////////////////////////////////////
void FindDevices(void)
{
    static char * simulated[1] = { "PCI::AC240" };

    if (simulation)
    {
        NumInstruments = 1;

        // Initialize the digitizers in simulation mode
        for (int i = 0; i < NumInstruments; i++)
        {
            status = Acqrs_InitWithOptions(
                simulated[i], VI_FALSE, VI_FALSE, "simulate=TRUE", &(InstrumentID[i]));
        }
    }
    else
    {
        // Find all digitizers
        status = Acqrs_getNbrInstruments(&NumInstruments);

        if (NumInstruments > maxNbrInstruments)         // Protect against too many instruments
            NumInstruments = maxNbrInstruments;

        // Initialize the digitizers
        for (int i = 0; i < NumInstruments; i++)
        {
            char resourceName[20];
            sprintf(resourceName, "PCI::INSTR%d", i);

            status = Acqrs_InitWithOptions(
                resourceName, VI_FALSE, VI_FALSE, "", &(InstrumentID[i]));
        }
    }
    currentID = InstrumentID[0];                // Use the first instrument only
}

/////////////////////////////////////////////////////////////////////////////////////////
void InitFPGA()
{
    // ### Initialization of the FPGA ###
    printf("Initializing firmware\n");
   
    // Set the DCM enable bits in the FPGA configuration register
    long fpgaCtrl = 0;
    WriteFPGA(FPGACtrlReg, 1, &fpgaCtrl);       // First disable everything
    fpgaCtrl |= 0x00ff0000;                     // Enable all DCMs
//  fpgaCtrl |= 0x00000100;                     // Enable readout in Big-Endian format (if needed)
    WriteFPGA(FPGACtrlReg, 1, &fpgaCtrl);
    Sleep(10);                                  // Wait some time


    // Initialize the DE interface
    long deCtrl = 0;
    WriteFPGA(DECtrlReg, 1, &deCtrl);
    deCtrl = 0x80000000;                    // Starts the DE interface in the FPGA
    WriteFPGA(DECtrlReg, 1, &deCtrl);

    // Wait until the DE clock is ready
    bool ready = false;
    long timeout = 100;             // With a 'Sleep' of 1 ms, should be a timeout of 100 ms

    while ((!ready) && (timeout > 0))
    {
        timeout--;
        
        long fpgaStatus;
        ReadFPGA(FPGAStatusReg, 1, &fpgaStatus);
        ready = ((fpgaStatus & 0x00100000) != 0);
        if (!ready) 
            Sleep(1);
    }

    if (timeout <= 0)
        printf("Timeout while waiting for DE clock!\n");

    // Reset the dual port memory
    long dpMemCtrl = 0x1;       // set the "reset" bit
    WriteFPGA(SRAMCtrlReg, 1, &dpMemCtrl);
    dpMemCtrl = 0x0;            // clear it again
    WriteFPGA(SRAMCtrlReg, 1, &dpMemCtrl);
    printf("\n");
}

//////////////////////////////////////////////////////////////////////////////////////////
void LoadFPGA(void)
{
    // ### Load the firmware into the FPGA (if not using default file)
    char fileName[50] = "ac240mem.bit";

    printf("loading firmware...\n", fileName);

    // Clear the FPGA first
    status = Acqrs_configLogicDevice(currentID, "Block1Dev1", NULL, 1);
    
    // Load the FPGA (flag = 3 will allow a search for FPGAPATH in the 'AqDrv4.ini' file)
    status = Acqrs_configLogicDevice(currentID, "Block1Dev1", fileName, 3);
    // If there is a problem, report it
    if (status != VI_SUCCESS)
    {
        char message [256];
        Acqrs_errorMessage(currentID, status, message, sizeof(message));
        printf("Problem with loading bit-file '%s' into FPGA: %s\n", fileName, message);
    }
    else
    {
        // Display some information about the loaded firmware
        char text[256];
        status = Acqrs_getInstrumentInfo(currentID, "LogDevHdrBlock1Dev1S name", &text);
        printf("Firmware file name: %s\n", text);
        status = Acqrs_getInstrumentInfo(currentID, "LogDevHdrBlock1Dev1S version", &text);
        printf("Version: %s --- ", text);
        status = Acqrs_getInstrumentInfo(currentID, "LogDevHdrBlock1Dev1S compDate", &text);
        printf("%s\n\n", text);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
long ReadFPGA(long regID, long nbrValues, long* dataArrayP)
{
    ViStatus status = Acqrs_logicDeviceIO(currentID, "Block1Dev1", regID, nbrValues, dataArrayP, 0, 0);

#ifdef FPGA_IO_LOG
    fprintf(ioLogFile, "Read  Reg #%3i (%ix):", regID, nbrValues);
    for(int i=0; i<nbrValues; ++i) fprintf(ioLogFile, " %08x", dataArrayP[i]);
    fprintf(ioLogFile, " => 0x%08x\n", status);
#endif

    return status;
}

//////////////////////////////////////////////////////////////////////////////////////////
void ReadMemoryBlock(void)
{
    // ### Read the stored data out of the dual port memory ###
    static const long nbrValues = 16384;
    static const long nbrLongs = (nbrValues + 3)/4;
    long chan1Data[nbrLongs];
    long chan2Data[nbrLongs];
    long startAddr  = 0;
    long bufAddress = SRAMBufAddress;

    printf("Reading acquired data\n");

    // Set port A for access by the program
    long dpMemCtrl = 0x00000002L;
    WriteFPGA(SRAMCtrlReg, 1, &dpMemCtrl);
    
    // Read channel 1 data
    WriteFPGA(StartAddrReg, 1, &startAddr);
    WriteFPGA(BufferIDReg, 1, &bufAddress);
    ReadFPGA(ReadAddrReg, nbrLongs, chan1Data);

    // Read channel 2 data
    startAddr = 0x00080000L;        // start address for channel 2 (middle of DP memory)
    WriteFPGA(StartAddrReg, 1, &startAddr);
    WriteFPGA(BufferIDReg, 1, &bufAddress);
    ReadFPGA(ReadAddrReg, nbrLongs, chan2Data);


    // Print the waveform
    char* chan1Chars = (char*)chan1Data;                    // Treat the buffers as arrays of 8-bit values
    char* chan2Chars = (char*)chan2Data;
    FILE* file = fopen("Acqiris.data", "w");
    if (!file)
    {
        printf("Couldn't open output file 'Acqiris.data'!\n");
        return;
    }

    fprintf(file, "Channel 1\tChannel 2\n");
    for (long i = 0; i < nbrValues; i++)
    {
        fprintf(file, "%d\t%d\n", chan1Chars[i], chan2Chars[i]);
    }
    fclose(file);
}

//////////////////////////////////////////////////////////////////////////////////////////
void Stop(void)
{
    //  ### Stop data conversion ###
    status = AcqrsD1_stopAcquisition(currentID);
}

//////////////////////////////////////////////////////////////////////////////////////////
void WaitForOperator(void)
{
    printf("Please press 'Enter' to continue\n");
    char ch = getchar();
    fflush(stdin);
}

//////////////////////////////////////////////////////////////////////////////////////////
long WriteFPGA(long regID, long nbrValues, long* dataArrayP)
{
    ViStatus status = Acqrs_logicDeviceIO(currentID, "Block1Dev1", regID, nbrValues, dataArrayP, 1, 0);

#ifdef FPGA_IO_LOG
    fprintf(ioLogFile, "Write Reg #%3i (%ix):", regID, nbrValues);
    for(int i=0; i<nbrValues; ++i) fprintf(ioLogFile, " %08x", dataArrayP[i]);
    fprintf(ioLogFile, " => 0x%08x\n", status);
#endif

    return status;
}


//////////////////////////////////////////////////////////////////////////////////////////
void WriteToMemory(void)
{
    // ### Acquire some data and store it in the dual port memory ###

    // Connect port A to FPGA Internal Bus port
    long dpMemCtrl = 0x0;
    WriteFPGA(SRAMCtrlReg, 1, &dpMemCtrl);
    Sleep(10);
    // Start storage into the dual port memory
    long exampleCtrl = 0x3;     // start writing stream data to memory on next trigger
                                // note that bit 0 will still read as 0 until the memory is full

    WriteFPGA(MemExampleCtrlReg, 1, &exampleCtrl);


    printf("Streaming data to memory...\n");

    // Wait until the buffer is full
    bool memFull = false;
    long timeout = 200;    // with a sleep of 10 ms this should be a timeout of ~10 s

    while(!memFull && timeout > 0) {
        --timeout;
        ReadFPGA(MemExampleCtrlReg, 1, &exampleCtrl);
        memFull = (exampleCtrl & 0x01L) == 0x01;

        if (!memFull)
            Sleep(10);
    }

    if (timeout <= 0)
        printf("Timeout on waiting for memory full!\n");
    else
        printf("Memory full\n");
    
}




