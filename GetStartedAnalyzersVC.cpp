//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedAnalyzersVC.cpp : C++ demo program for Acqiris Analyzers
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 2005, 2006-2009
//
//////////////////////////////////////////////////////////////////////////////////////////

/* Description:
 *
 * This GetStarted illustrates basic interaction with the firmware of an AC or SC analyzer
 * card. It performs the following steps which are useful with most firmware designs:
 *
 * - Configure the digitizer (sample rate, full scale range, trigger setup etc.)
 * - Start the acquisition and streaming of the data to the DPU
 * - Optionally load a custom bit file into the FPGA
 * - Initialize the firmware
 * - Capture a monitor block from the DE interface
 * - Stop the acquisition
 *
 * This example also shows how to use the "processing done" interrupt to wait for an operation
 * to terminate. The interrupt is raised by the "Monitor buffer full" flag in the BaseTest
 * firmware. In custom firmware designs, it can be routed to any signal.
 *
 * By default, the BaseTest firmware is used, which is loaded automatically on initialization
 * of the Analyzer. If you want to use a custom bit file, you will have to edit the LoadFPGA()
 * function. The necessary code can be found between the "#ifdef MY_FPGA" and the "#endif"
 * compiler directives inside this function (it is skipped by the compiler by default).
 */


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "vpptype.h"

// ### Acqiris Digitizer Device Driver ###
#include "AcqirisImport.h"
#include "AcqirisD1Import.h"

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
static const long ReadAddrReg       =   0;          // Indirect Access Port
static const long StartAddrReg      =   1;          // Start address within block
static const long BufferIDReg       =   2;          // Buffer Identifier Register
static const long FPGACtrlReg       =   3;          // FPGA control register
static const long FPGAStatusReg     =   6;          // FPGA status register
static const long DECtrlReg         =   8;          // DE-bus control register (from MAC)
static const long MainCtrlReg       =  64;          // Main Control register
static const long DeMonCtrl         =  65;          // Control register for data entry monitor
static const long DeMonitorAddress  = 0x0c;         // Identifier of data entry monitor

void Acquire(void);
void CaptureMonitorBlock();
void Configure(void);
void FindDevices(void);
void InitFPGA(void);
void LoadFPGA(void);
long ReadFPGA(long regID, long nbrValues, long* dataArrayP);
void ReadMonitorBlock(void);
void Stop(void);
void WaitForOperator(void);
long WriteFPGA(long regID, long nbrValues, long* dataArrayP);


//////////////////////////////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
    printf("\nAcqiris Analyzer - Getting Started\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n");

    FindDevices();          // ### Initialize the Analyzer(s) ###
    printf("I have found %d Acqiris Analyzer(s) on your PC\n", NumInstruments);
    WaitForOperator();

    LoadFPGA();             // ### Load the firmware into the FPGA (if not using default file) ###
    Configure();            // ### Configure the digitizer ###
    Acquire();              // ### Start continuous acquisition + transfer to FPGA ###
    InitFPGA();             // ### Initialize the FPGA ###
    CaptureMonitorBlock();  // ### Capture a monitoring block in the FPGA ###
    ReadMonitorBlock();     // ### Read a block of data from the FPGA ###
    Stop();                 // ### Stop the FPGA and the acquisition ###

    printf("Operation terminated: Wrote 1 monitoring data block to disk\n");
    WaitForOperator();

    Acqrs_closeAll();       // ### Let the driver perform any necessary cleanup tasks ###

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
void Acquire(void)
{
    // ### Start digitizing the signal and stream to digital data to the FPGA ###
    status = AcqrsD1_configMode(currentID, 1, 0, 0);    // Set the mode to 'Streaming to DPU' ( = 1)
    status = AcqrsD1_acquire(currentID);                // Start the acquisition

    // It makes no sense to wait for the end of acquisition, since the digitizer will stream data continuously
    // to the FPGA. Using "AcqrsD1_stopAcquisition(currentID);" is the only way to stop it.
}


//////////////////////////////////////////////////////////////////////////////////////////
void CaptureMonitorBlock()
{
    // ### Capture data into the 'In' monitoring buffer ###

    // Reset the 'capture' bit and write it at 1 again (at the end of this routine)
    long monitorCtrl, mainCtrl;
    ReadFPGA(MainCtrlReg, 1, &mainCtrl);
    mainCtrl &= 0xffffefff;
    WriteFPGA(MainCtrlReg, 1, &mainCtrl);

    mainCtrl |= 0x00001000;                 // Enable capture into DE monitor
    WriteFPGA(MainCtrlReg, 1, &mainCtrl);

    // Wait until data has been captured in the DE monitor
    bool ready = false;
    long timeout = 100;         // With a 'Sleep' of 1 ms, should be a timeout of 100 ms

    while ((!ready) && (timeout > 0))
    {
        timeout--;
        ReadFPGA(DeMonCtrl, 1, &monitorCtrl);
        ready = ((monitorCtrl & 0x80000000) != 0);
        if (!ready)
            Sleep(1);
    }

    if (timeout <= 0)
        printf("WaitForEndOfCapture: Timeout on Capture\n");
}

//////////////////////////////////////////////////////////////////////////////////////////
void Configure(void)
{
    // ### Configuration ###
    double sampInterval = 1e-9, delayTime = 0.0;
    long coupling = 1, bandwidth = 0;
    double fullScale = 2.0, offset = 0.0;
    long trigCoupling = 0, trigSlope = 0;
    double trigLevel = 20.0; // In % of vertical Full Scale !!

    // Configure timebase
    status = AcqrsD1_configHorizontal(currentID, sampInterval, delayTime);

    // Configure vertical settings of channel 1
    status = AcqrsD1_configVertical(currentID, 1, fullScale, offset, coupling, bandwidth);

    // NOTE: The following 2 statements are not necessary as long as the FPGA-firmware does not 
    // require a trigger signal for the execution of its algorithms.

    // Configure edge trigger on channel 1
    status = AcqrsD1_configTrigClass(currentID, 0, 0x00000001, 0, 0, 0.0, 0.0);
    // Configure the trigger conditions of channel 1 internal trigger
    status = AcqrsD1_configTrigSource(currentID, 1, trigCoupling, trigSlope, trigLevel, 0.0);

}

//////////////////////////////////////////////////////////////////////////////////////////
void FindDevices(void)
{
    static char * simulated[2] = { "PCI::AC210", "PCI::SC240" };

    if (simulation)
    {
        NumInstruments = 2;

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
    // NOTE: The initialization depends on the firmware. You may need to initialize additional registers,
    //       if your firmware demands it.
    //       This sample program assumes that the default firmware 'AC2x0.bit' or 'SC2x0.bit' is loaded
    //       However, all firmware designs need to start the DCM for the DE-input buffer. The DCM must be
    //       enabled AFTER the acquisition + transfer to the FPGA have started, because the DE-input clock
    //       is started/stopped with the acquisition.

    // Set the DCM enable bits in the FPGA configuration register
    long fpgaCtrl = 0;
    WriteFPGA(FPGACtrlReg, 1, &fpgaCtrl);       // First disable everything
    fpgaCtrl |= 0x00ff0000;                     // Enable bits for DCMA and DCMB
//  fpgaCtrl |= 0x00000100;                     // Enable readout in Big-Endian format (if needed)
    WriteFPGA(FPGACtrlReg, 1, &fpgaCtrl);
    Sleep(10);                                  // Wait some time
    long deCtrl = 0x80000000;                   // Start the DE interface in the FPGA
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
}

//////////////////////////////////////////////////////////////////////////////////////////
void LoadFPGA(void)
{
    // ### Load the firmware into the FPGA (if not using default file)
    char fileName[50] = "MyTestFile.bit";
#ifdef MY_FPGA
    // Clear the FPGA first
    status = Acqrs_configLogicDevice(currentID, "Block1Dev1", NULL, 1);

    // Load the FPGA (flag = 3 will allow a search for FPGAPATH in the 'AqDrv4.ini' file)
    status = Acqrs_configLogicDevice(currentID, "Block1Dev1", fileName, 3);
    // If there is a problem, report it
    if (status != VI_SUCCESS)
    {
        char message [256];
        Acqrs_errorMessage(currentID, status, message, sizeof(message));
        printf("Problem with loading firmware into FPGA: %s\n", message);
    }
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////
long ReadFPGA(long regID, long nbrValues, long* dataArrayP)
{
    return Acqrs_logicDeviceIO(currentID, "Block1Dev1", regID, nbrValues, dataArrayP, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////
void ReadMonitorBlock(void)
{
    static const long nbrValues = 1000;
    long monitorArray[nbrValues];
    long startAddr  = 0;
    long bufAddress = DeMonitorAddress;

    WriteFPGA(StartAddrReg, 1, &startAddr);
    WriteFPGA(BufferIDReg, 1, &bufAddress);
    ReadFPGA(ReadAddrReg, nbrValues/4, monitorArray);       // 'nbrValues/4' = number of 32-bit words

    // Print the waveform
    char buffer[100];
    char* monitorP = (char*)monitorArray;                   // Treat monitoring buffer as array of 8-bit values
    FILE* file = fopen("Acqiris.data", "w");
    fwrite("Monitoring Buffer\n", 1, strlen("Monitoring Buffer\n"), file); 
    for (long i = 0; i < nbrValues; i++)
    {
        sprintf(buffer, "%d\n", monitorP[i]);
        fwrite(buffer, 1, strlen(buffer), file);
    }
    fwrite(buffer, 1, strlen(buffer),file);
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
}

//////////////////////////////////////////////////////////////////////////////////////////
long WriteFPGA(long regID, long nbrValues, long* dataArrayP)
{
    return Acqrs_logicDeviceIO(currentID, "Block1Dev1", regID, nbrValues, dataArrayP, 1, 0);
}


