/////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStarted8bitMultiSegment.cpp : C++ demo program for Agilent Acqiris Digitizers
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 1999-2009. All rights reserved.
//
//  Altered for usage in a DAQ setup by Joseph T Wolf CWRU
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <sstream>
#include <string>
#include <fstream>
#include <iostream>
#include <assert.h>
#include <ctime>
#include "vpptype.h"
#include <time.h>
// Include file for all families of Agilent Acqiris products
#include "AcqirisImport.h"

// Include file for Agilent Acqiris Digitizers Device Driver
#include "AcqirisD1Import.h"

// Simulation flag, set to true to simulate digitizers (for application development)
bool simulation = false;

// Global variables
const ViInt32 MAX_SUPPORTED_DEVICES = 10;
ViSession InstrumentID[MAX_SUPPORTED_DEVICES];	// Array of instrument handles
ViInt32 p;
ViInt32 NumInstruments; 	// Number of instruments
ViChar l [20];
ViChar q [50];
ViStatus status; 		// Functions return a status code that needs to be checked


	//Settings file variables
		int v,w,e,r,t,y,u,o,b,a,s,d;
                	ViReal64 sampInterval, delayTime;
                	ViInt32 nbrSamples, nbrSegments;
                	ViInt32 coupling, bandwidth;
                	ViReal64 fullScale, offset;
                	ViInt32 trigCoupling, trigSlope;
                	ViReal64 trigLevel;
    			ViInt32 Timeout; 

ViInt32 tbNextSegmentPad;	// Additional array space (in samples) per segment needed for the read data array

using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////
void FindDevices(void)
{
	
	// This function will detect and initialize the digitizers

	if (simulation) // We are in simulation mode
	{	
		// List of four simulated instruments	
		static char simulated[4][16] = { "PCI::DC270", "PCI::DC110", "PCI::DP240", "PCI::DP110" };

		// initialization option is set to simulation
		ViChar options[32] = "simulate=TRUE";
		
		NumInstruments = 2;

		// Set the simulation options BEFORE initializing simulated digitizers
		status = Acqrs_setSimulationOptions("M2M");
		assert(status==VI_SUCCESS);

		// Initialize the simulated digitizers
		for (ViInt32 i = 0; i < NumInstruments; i++)
		{
			status = Acqrs_InitWithOptions(simulated[i], VI_FALSE, VI_FALSE, 
											options, &(InstrumentID[i]));
			assert(status==VI_SUCCESS);
		}
	}
	else
	{
		// Find all digitizers connected to the PC
		
		// no options are needed in this case
		ViChar options[32] = ""; // no initialization options

		// The following call will find the number of digitizers on the computer,
		// regardless of their connection(s) to ASBus.
		//		status = Acqrs_getNbrInstruments(&NumInstruments);
		//		assert(status==VI_SUCCESS);

		// The following call will automatically detect ASBus connections between digitizers 
		// and combine connected digitizers (of indentical model!) into multi-instruments.
		// The call returns the number of multi-instruments and/or single instruments.
		status = AcqrsD1_multiInstrAutoDefine(options, &NumInstruments);
		assert(status==VI_SUCCESS);

		if (NumInstruments < 1)	
		{
			cout << "Instrument not found!" << endl;
			assert(NumInstruments > 0);	// No instrument found
		}

		if (NumInstruments > MAX_SUPPORTED_DEVICES)
			NumInstruments = MAX_SUPPORTED_DEVICES;
		

		// Initialize all digitizers
		for (int i = 0; i < NumInstruments; i++)
		{
			// create a resource name for each digitizer
			char resourceName[20];
			sprintf(resourceName, "PCI::INSTR%d", i);

			status = Acqrs_InitWithOptions(resourceName, VI_FALSE, VI_FALSE, 
											options, &(InstrumentID[i]));
			assert(status==VI_SUCCESS);
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////
void LoadSettings(void)
{
	string input;
	ViInt32 i;
	
	for (i = 0;i < NumInstruments;i++){
        ifstream settings ("Settings.txt");
        std::getline(settings, input);
            v = atoi( input.c_str() );
        std::getline(settings, input);
            w = atoi( input.c_str());
        std::getline(settings, input);
            e = atoi( input.c_str() );
        std::getline(settings, input);
            r = atoi( input.c_str() );
        std::getline(settings, input);
            t = atoi( input.c_str() );
        std::getline(settings, input);
            y = atoi( input.c_str() );
        std::getline(settings, input);
            u = atoi( input.c_str() );
        std::getline(settings, input);
            o = atoi( input.c_str() );
        std::getline(settings, input);
            b = atoi( input.c_str() );
        std::getline(settings, input);
            a = atoi( input.c_str() );
        std::getline(settings, input);
            s = atoi( input.c_str() );
        std::getline(settings, input);
            d = atoi( input.c_str() );
            settings.close();
}
}
//////////////////////////////////////////////////////////////////////////////////////////
void Configure(void)
{    
	        sampInterval = v, delayTime = w;
                nbrSamples = e, nbrSegments = r;
                coupling = t, bandwidth = y;
                fullScale = u, offset = o;
                trigCoupling = b, trigSlope = a;
                trigLevel = s;
    //Loop to account for multiple instruments
    ViInt32 i;
    for (i = 0;i < NumInstruments;i++){
	// Configuration of the first digitizer found
//	ViReal64 sampInterval = 1.e-8, delayTime = -0.0001; //Added one zero to test
//	ViInt32 nbrSamples = 1000, nbrSegments = 10;
//	ViInt32 coupling = 3, bandwidth = 0;   
//	ViReal64 fullScale = 2.0, offset = 0.0;
//	ViInt32 trigCoupling = 0, trigSlope = 0;
//	ViReal64 trigLevel = 20.0; // In % of vertical full scale when using internal trigger

	// Configure timebase
	status = AcqrsD1_configHorizontal(InstrumentID[i], sampInterval, delayTime);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	status = AcqrsD1_configMemory(InstrumentID[i], nbrSamples, nbrSegments);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	
	// Configure vertical settings of channel 1
	status = AcqrsD1_configVertical(InstrumentID[i], 1, fullScale, offset, coupling, bandwidth);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	
	// Configure edge trigger on channel 1
	status = AcqrsD1_configTrigClass(InstrumentID[i], 0, 0x00000001, 0, 0, 0.0, 0.0);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	
	// Configure the trigger conditions of channel 1 (internal trigger)
	status = AcqrsD1_configTrigSource(InstrumentID[i], 1, trigCoupling, trigSlope, trigLevel, 0.0);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));

	// Reading of the TbNextSegmentPad value, necessary for multi-segment readout
	status = Acqrs_getInstrumentInfo(InstrumentID[i], "TbNextSegmentPad", &tbNextSegmentPad);
	assert(status==VI_SUCCESS); 
}
}
//////////////////////////////////////////////////////////////////////////////////////////
void Acquire(void)
{
	Timeout = d;
	// Acquisition of a waveform on the first digitizer
    ViInt8 i;

    for (i=0;i < NumInstruments;i++){
	// Start the acquisition
	status = AcqrsD1_acquire(InstrumentID[i]); 
	assert(status==VI_SUCCESS);
    }
    for (i=0;i < NumInstruments;i++){
	// Wait for the interrupt to signal the end of the acquisition
	// with a timeout value of 2 seconds (originally 2000)
	status = AcqrsD1_waitForEndOfAcquisition(InstrumentID[i], Timeout);
    }
    for (i=0;i < NumInstruments;i++){
	if (status != VI_SUCCESS)
	{
		// Acquisition did not complete successfully
		AcqrsD1_stopAcquisition(InstrumentID[i]);
		cout << endl << "Acquisition timeout!" << endl;
		cout << endl << "The acquisition has been stopped - data invalid!" << endl;
	}
}
}
//////////////////////////////////////////////////////////////////////////////////////////
void Readout(void)
{
    ViInt32 z,i;
    for (z = 0; z < NumInstruments;z++){

    // Readout of the acquired data on the first digitizer
	ViInt32 channel = 1; // channel to be read
    ViInt32 nbrSamples, nbrSegments;
	// Retrieval of the memory settings
	status = AcqrsD1_getMemory(InstrumentID[z], &nbrSamples, &nbrSegments);
	assert(status==VI_SUCCESS);
		
	// Definition of the read parameters for raw ADC readout
	AqReadParameters readPar;
	AqDataDescriptor dataDesc;
	AqSegmentDescriptor* segDesc = new AqSegmentDescriptor[nbrSegments];

	readPar.dataType = ReadInt8; // 8bit, raw ADC values data type
	readPar.readMode = ReadModeSeqW; // Multi-segment read mode
	readPar.firstSegment = 0;
	readPar.nbrSegments = nbrSegments;
	readPar.firstSampleInSeg = 0;
	readPar.nbrSamplesInSeg = nbrSamples;
	readPar.segmentOffset = nbrSamples;
	readPar.dataArraySize = (nbrSamples + tbNextSegmentPad) * (nbrSegments + 1)
							* (readPar.dataType + 1); // Array size in bytes
	readPar.segDescArraySize = nbrSegments * sizeof(AqSegmentDescriptor);

	readPar.flags		= 0;
	readPar.reserved	= 0;
	readPar.reserved2	= 0;
	readPar.reserved3	= 0;
	
	// Read the channel 1 waveform as raw ADC values
	ViInt8* adcArray = new ViInt8[(nbrSamples + tbNextSegmentPad)*(nbrSegments + 1)];
	status = AcqrsD1_readData(InstrumentID[z], channel, &readPar, adcArray, 
							  &dataDesc, segDesc);
	assert(status==VI_SUCCESS);

    //Enables timestamps for data
    time_t now = time(0);
    struct tm tstruct;
    tstruct = *localtime(&now);
    strftime(q,sizeof(q), "%Y-%m-%d.%X", &tstruct);

    //Enables multiple intruments to record data files
    int aInt = z;
    char str[80];
    char str2[80];
    sprintf(str, "Acq-%s-Inst%d-%d-%s.info",l,aInt,p,q);
    sprintf(str2, "Acq-%s-Inst%d-%d-%s.dat",l,aInt,p,q);

	// Write the waveform into a file
	ofstream outFile(str);
	outFile << "# Acqiris Waveforms" << endl;
	outFile << "# Channel: " <<channel << endl;
	outFile << "# Samples acquired: " << dataDesc.returnedSamplesPerSeg << endl;
	outFile << "# Segments acquired: " << dataDesc.returnedSegments << endl;
    outFile << "# Time per trigger set: " << 1.e-8 * nbrSamples * nbrSegments << endl;
    outFile.close();
    ofstream outFile2(str2);
	ViInt32 j;
	for (j = 0; j < dataDesc.returnedSegments; j++) {
		for (i = 0 ; i < dataDesc.returnedSamplesPerSeg; i++)
			outFile2 << int(adcArray[j*readPar.segmentOffset+i]) << endl;
	}
    outFile2.close();
	status = AcqrsD1_freeBank(InstrumentID[z],0);
    delete [] segDesc;
    delete [] adcArray;
}
}

//////////////////////////////////////////////////////////////////////////////////////////
void Close(void)
{
	// Close all instruments 
	status = Acqrs_closeAll(); 
	assert(status==VI_SUCCESS);
}

//////////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char *argv[])
{
    p = 1;
    ViInt32 t;
    ViInt32 e;
	cout << endl << "Agilent Acqiris Digitizer - Demo"
		 << endl << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
		 << endl << endl;
    
	FindDevices(); // Initialization of the instruments

	cout << "I have found " << NumInstruments << " Agilent Acqiris Digitizer(s) on your PC" << endl;
	LoadSettings(); //Load Settings for the digitizers
	cout << "I have configured settings for both digitizers" <<endl;
	Configure();	// Configuration of the first digitizer
    cout << "Please enter the amount of time you wish to record(in minutes): ";
    cin >> t;
    if (t != 0){
   // e = (t * 600);
        e = t;
    cout << "Please enter the name of the dataset: ";
    cin >> l;
    while (p <= e) {
	Acquire();		// Acquisition of a waveform
	Readout();		// Readout of the waveform
    p = p+1;
    }
    }
	Close();		// Close all instruments

	cout << "End of process..." << endl;

	return 0;
}
