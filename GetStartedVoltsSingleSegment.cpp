//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedVoltsSingleSegment.cpp : C++ demo program for Agilent Acqiris Digitizers
//----------------------------------------------------------------------------------------
//  Copyright Agilent Technologies, Inc. 1999-2009. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <iostream>
#include <assert.h>

#include "vpptype.h"

// Include file for all families of Agilent Acqiris products
#include "AcqirisImport.h"

// Include file for Agilent Acqiris Digitizers Device Driver
#include "AcqirisD1Import.h"

// Simulation flag, set to true to simulate digitizers (for application development)
bool simulation = false;

// Global variables
const ViInt32 MAX_SUPPORTED_DEVICES = 10;
ViSession InstrumentID[MAX_SUPPORTED_DEVICES];	// Array of instrument handles

ViInt32 NumInstruments; 	// Number of instruments
ViStatus status; 			// Functions return a status code that needs to be checked

using namespace std;

//////////////////////////////////////////////////////////////////////////////////////////
void FindDevices(void)
{
	// This function will detect and initialize the digitizers

	if (simulation) // We are in simulation mode
	{	
		// List of four simulated instruments	
		static char simulated[4][16] = { "PCI::DC110", "PCI::DC270", "PCI::DP240", "PCI::DP110" };

		// initialization option is set to simulation
		ViChar options[32] = "simulate=TRUE";
		
		NumInstruments = 2;

		// Set the simulation options BEFORE initializing simulated digitizers
		status = Acqrs_setSimulationOptions("M2M");
		assert(status==VI_SUCCESS);

		// Initialize the simulated digitizers
		for (int i = 0; i < NumInstruments; i++)
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
		ViChar options[32] = "";

		// The following call will find the number of digitizers on the computer,
		// regardless of their connection(s) to ASBus.
		//		status = Acqrs_getNbrInstruments(&NumInstruments);
		//		assert(status==VI_SUCCESS);

		// The following call will automatically detect ASBus connections between digitizers 
		// and combine connected digitizers (of identical model!) into multi-instruments.
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
void Configure(void)
{
	// Configuration of the first digitizer found
	ViReal64 sampInterval = 1.e-8, delayTime = 0.0;
	ViInt32 nbrSamples = 1000, nbrSegments = 1;
	ViInt32 coupling = 3, bandwidth = 0;
	ViReal64 fullScale = 2.0, offset = 0.0;
	ViInt32 trigCoupling = 0, trigSlope = 0;
	ViReal64 trigLevel = 20.0; // In % of vertical full scale when using internal trigger

	// Configure timebase
	status = AcqrsD1_configHorizontal(InstrumentID[0], sampInterval, delayTime);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	status = AcqrsD1_configMemory(InstrumentID[0], nbrSamples, nbrSegments);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	
	// Configure vertical settings of channel 1
	status = AcqrsD1_configVertical(InstrumentID[0], 1, fullScale, offset, coupling, bandwidth);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	
	// Configure edge trigger on channel 1
	status = AcqrsD1_configTrigClass(InstrumentID[0], 0, 0x00000001, 0, 0, 0.0, 0.0);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
	
	// Configure the trigger conditions of channel 1 (internal trigger)
	status = AcqrsD1_configTrigSource(InstrumentID[0], 1, trigCoupling, trigSlope, trigLevel, 0.0);
	assert((status==VI_SUCCESS) || (status>VI_SUCCESS));
}

//////////////////////////////////////////////////////////////////////////////////////////
void Acquire(void)
{
	// Acquisition of a waveform on the first digitizer
	
	// Start the acquisition
	status = AcqrsD1_acquire(InstrumentID[0]); 
	assert(status==VI_SUCCESS);

	// Wait for the interrupt to signal the end of the acquisition
	// with a timeout value of 2 seconds
	status = AcqrsD1_waitForEndOfAcquisition(InstrumentID[0], 2000);

	if (status != VI_SUCCESS)
	{
		// Acquisition did not complete successfully
		AcqrsD1_stopAcquisition(InstrumentID[0]);
		cout << endl << "Acquisition timeout!" << endl;
		cout << endl << "The acquisition has been stopped - data invalid!" << endl;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void Readout(void)
{
	// Readout of the acquired data
	ViInt32 channel = 1; // channel to be read
	ViInt32 nbrSamples, nbrSegments;
	
	// Retrieval of the memory settings
	status = AcqrsD1_getMemory(InstrumentID[0], &nbrSamples, &nbrSegments);
	assert(status==VI_SUCCESS);
	
	// Definition of the read parameters for raw ADC readout
	AqReadParameters readPar;
	AqDataDescriptor descriptor;
	AqSegmentDescriptor segDesc;

	readPar.dataType = ReadReal64; // 64 bit (double), voltages values data type
	readPar.readMode = ReadModeStdW; // Single-segment read mode
	readPar.firstSegment = 0;
	readPar.nbrSegments = 1;
	readPar.firstSampleInSeg = 0;
	readPar.nbrSamplesInSeg = nbrSamples;
	readPar.segmentOffset = 0;
	readPar.dataArraySize = (nbrSamples + 40) * 8; // Array size in bytes
	readPar.segDescArraySize = sizeof(AqSegmentDescriptor);

	readPar.flags		= 0;
	readPar.reserved	= 0;
	readPar.reserved2	= 0;
	readPar.reserved3	= 0;

	// Read the channel 1 waveform as voltages
	ViReal64* dataArray = new ViReal64[nbrSamples+40];
	status = AcqrsD1_readData(InstrumentID[0], channel, &readPar, dataArray, 
							  &descriptor, &segDesc);
	assert(status==VI_SUCCESS);
	
	// Write the waveform into a file
	ofstream outFile("Acqiris.data");
	outFile << "# Acqiris Waveforms" << endl;
	outFile << "# Channel: " <<channel << endl;
	outFile << "# Samples acquired: " << descriptor.returnedSamplesPerSeg << endl;
		
	outFile << "# Voltage" << endl;
	for (int i = 0; i < descriptor.returnedSamplesPerSeg; i++) 
		outFile << dataArray[i] << endl;

	outFile.close();
	delete [] dataArray;
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
	cout << endl << "Agilent Acqiris Digitizer - Demo"
		 << endl << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
		 << endl << endl;

	FindDevices(); // Initialization of the instruments

	cout << "I have found " << NumInstruments << " Agilent Acqiris Digitizer(s) on your PC" << endl;

	Configure();	// Configuration of the first digitizer
	Acquire();		// Acquisition of a waveform
	Readout();		// Readout of the waveform
	Close();		// Close all instruments

	cout << "End of process..." << endl;

	return 0;
}
