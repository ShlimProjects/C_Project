//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedAC24fftVC.cpp : C++ demo program for the AC240 2GS/s Spectrum Analyzer firmware
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 2006, 2007-2009
//
//////////////////////////////////////////////////////////////////////////////////////////

/* Description:
 *
 * This GetStarted shows how to use the 2GS/s FFT firmware for the AC240. It performs the
 * following basic steps:
 *
 * - Configure the digitizer (sample rate, full scale range, trigger setup etc.)
 * - Start the acquisition and streaming of the data to the DPU
 * - Load the spectrum analyzer bit file into the FPGA
 * - Initialize the firmware
 * - Acquire a block of spectral data and read it from the FPGA
 * - Stop the acquisition
 *
 * Most of the code is similar to the basic GetStartedAnalyzersVC.cpp example. The firmware-
 * specific parts are mostly in the InitFPGA() function (for firmware initialisation) and in
 * the two functions WaitForSpectrum() and ReadSpectrum().
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
ViStatus status;                            // Status returned by calls to AcqrsD1... API functions

// ### Constants ###
// Register addresses
static const long ReadAddrReg       =   0;          // Indirect Access Port
static const long StartAddrReg      =   1;          // Start address within block
static const long BufferIDReg       =   2;          // Buffer Identifier Register
static const long FPGACtrlReg       =   3;          // FPGA control register
static const long FPGAStatusReg     =   6;          // FPGA status register
static const long DECtrlReg         =   8;          // DE-bus control register (from MAC)
static const long MainCtrlReg       =  64;          // Main Control register
static const long MainStatusReg     =  65;          // Main Status register
static const long NbrAccReg         =  66;          // Number of accumulations
static const long FFTConfReg        =  67;          // Configuration register for FFT processing

static const long SumOfSpectrum     = 0x81;         // Identifier of buffer holding the summed power spectrum

long const NbrSpectralLines         = 16*1024;      // Number of bins in the acquired spectrum (fixed in firmware)


// ### Function prototypes ###
void Acquire(void);
void Configure(void);
void FindDevices(void);
void InitFPGA(void);
void LoadFPGA(void);
long ReadFPGA(long regID, long nbrValues, long* dataArrayP);
void ReadSpectrum(void);
void Stop(void);
void WaitForOperator(void);
void WaitForSpectrum(void);
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

    LoadFPGA();             // ### Load the firmware into the FPGA ###
    Configure();            // ### Configure the digitizer ###
    Acquire();              // ### Start continuous acquisition + transfer to FPGA ###
    InitFPGA();             // ### Initialize the FPGA ###
    WaitForSpectrum();      // ### Wait until the FFT is done ###
    ReadSpectrum();         // ### Read the last acquired power spectrum ###
    Stop();                 // ### Stop the FPGA and the acquisition ###

    printf("Operation terminated: Wrote 1 power spectrum to disk\n");
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
    printf("Starting Acquisition\n");
    // ### Start digitizing the signal and stream to digital data to the FPGA ###
    status = AcqrsD1_configMode(currentID, 1, 0, 0);    // Set the mode to 'Streaming to DPU' ( = 1)
    if (status != 0)
        printf("configMode: Error %08x\n", status);

    status = AcqrsD1_acquire(currentID);                // Start the acquisition
    if (status != 0)
        printf("acquire: Error %08x\n", status);

    // It makes no sense to wait for the end of acquisition, since the digitizer will stream data continuously
    // to the FPGA. Using "AcqrsD1_stopAcquisition(currentID);" is the only way to stop it.
}


//////////////////////////////////////////////////////////////////////////////////////////
void Configure(void)
{
    // ### Digitizer Configuration ###

    printf("Configuring Digitizer\n");

    double sampInterval = 0.5e-9, delayTime = 0.0;
    long coupling = 3, bandwidth = 0;
    double fullScale = 2.0, offset = 0.0;
    long trigCoupling = 0, trigSlope = 0;
    double trigLevel = 20.0; // In % of vertical Full Scale !!

    // interlace ADCs to get 2GS/s
    status = AcqrsD1_configChannelCombination(currentID, 2, 1);

    // Configure timebase
    status = AcqrsD1_configHorizontal(currentID, sampInterval, delayTime);

    // Configure vertical settings of channel 1
    status = AcqrsD1_configVertical(currentID, 1, fullScale, offset, coupling, bandwidth);

    // NOTE: The following 2 statements are not necessary in the default firmware configuration,
    //       because FFT calculation is performed continuously, ignoring the trigger. They are
    //       necessary only if you plan to use the triggered mode.

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

        // Initialize the analyzers
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
    // NOTE: Firmware initialisation should be done AFTER the acquisition + transfer
    //       to the FPGA have started, because the DE-input clock is started/stopped
    //       with the acquisition.

    // Set the DCM enable bits in the FPGA configuration register
    printf("Initializing FPGA\n");
    long fpgaCtrl = 0;
    WriteFPGA(FPGACtrlReg, 1, &fpgaCtrl);       // First disable everything
    fpgaCtrl |= 0x00ff0000;                     // Enable all DCMs
//  fpgaCtrl |= 0x00000100;                     // Enable readout in Big-Endian format (if needed)
    WriteFPGA(FPGACtrlReg, 1, &fpgaCtrl);
    Sleep(10);                                  // Wait some time

    // Start the DE interface in the FPGA
    long deCtrl = 0x00000000;
    WriteFPGA(DECtrlReg, 1, &deCtrl);
    deCtrl = 0x80000000;
    WriteFPGA(DECtrlReg, 1, &deCtrl);

    // Set number of power spectra to accumulate per pipeline.
    // Note that with both pipelines enabled, the actual number
    // of accumulations is twice the value set here.
    long nbrAcc = 30;
    WriteFPGA(NbrAccReg, 1, &nbrAcc);

    // Configure the FFT core. We use the following settings:
    // readMode = 0 (read low 32 bits)
    // overwrite = 1 (permit buffer overwrite)
    // bufClear = 0 (clear automatically)
    // shift = 0 (no bit shift)
    long fftConfig = 0x00000020;
    WriteFPGA(FFTConfReg, 1, &fftConfig);

    // Wait until the DE clock is ready
    bool ready = false;
    long timeout = 100;             // With a 'Sleep' of 1 ms, should be a timeout of 100 ms

    while ((!ready) && (timeout > 0))
    {
        --timeout;
        
        long fpgaStatus;
        ReadFPGA(FPGAStatusReg, 1, &fpgaStatus);
        ready = ((fpgaStatus & 0x00100000) != 0);
        if (!ready) 
            Sleep(1);
    }

    if (timeout <= 0)
        printf("Timeout while waiting for DE clock!\n");

    // Start the FFT core in continous mode (trigger is ignored)
    long mainCtrl = 0x00000001;
    WriteFPGA(MainCtrlReg, 1, &mainCtrl);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LoadFPGA(void)
{
    // ### Load the firmware into the FPGA
    char fileName[50] = "AC240FFT2GSs.bit";

    printf("Loading bit file \"%s\" into FPGA\n", fileName);
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
void ReadSpectrum(void)
{
    // ### Read the accumulated power spectrum out of the FPGA buffer ###

    printf("Reading spectrum data\n");
    long spectrum[NbrSpectralLines];
    
    // Set up indirect addressing
	long startAddr  = 0x0;
	long bufAddress = SumOfSpectrum;
	WriteFPGA(StartAddrReg, 1, &startAddr);
	WriteFPGA(BufferIDReg,  1, &bufAddress);
    // Read out the data
	ReadFPGA(ReadAddrReg, NbrSpectralLines, spectrum);

    // Write the data to a file
    FILE* file = fopen("Acqiris.data", "w");
    if (file == NULL)
        printf("Couldn't open output file \"Acqiris.data\"!\n");
    else
    {
        fprintf(file, "Power Spectrum\n");
        for (long i = 0; i < NbrSpectralLines; i++)
        {
            fprintf(file, "%i\n", spectrum[i]);
        }
        fclose(file);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
void Stop(void)
{
    printf("Stopping data processing\n");
    // Disable FFT core
    long mainCtrl = 0x00000000;
    WriteFPGA(MainCtrlReg, 1, &mainCtrl);

    // Wait until the FFT core stops
    // Acquisition and processing might continue after
    // disabling the core until the last data block has
    // been completely processed.
    long timeout = 100;
    bool done = false;
    long mainStatus;

    while (!done && timeout > 0)
    {
        --timeout;
        ReadFPGA(MainStatusReg, 1, &mainStatus);
        done = (mainStatus & 0x00010000) == 0;
        if (!done)
            Sleep(10);
    }

    if (timeout <= 0)
        printf("Timeout while waiting for end of processing!\n");

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
void WaitForSpectrum()
{
    // ### Wait until FFT computation is finished ###
    printf("Processing data\n");

    // Wait until the "buffer full" flag of the Main Status
    // register goes to 1
    long timeout = 100;
    bool ready = false;
    long mainStatus = 0;

    while (!ready && timeout > 0)
    {
        --timeout;
        ReadFPGA(MainStatusReg, 1, &mainStatus);
        ready = (mainStatus & 0x80000000L) != 0;
        if (!ready)
            Sleep(10);
    }

    if (timeout <= 0)
        printf("Timeout while waiting for spectrum!\n");
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


