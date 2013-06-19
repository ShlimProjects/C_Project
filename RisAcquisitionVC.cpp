//////////////////////////////////////////////////////////////////////////////////////////
//
//  RisAcquisitionVC.cpp : C++ RIS demo program for Agilent Acqiris Digitizers
//  (C) Copyright 2007, 2009 Agilent Technologies, Inc.
//
//  To obtain usage help, type RisAcquisitionVC -h on the command line.
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <iostream>

#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>

// ### Agilent Acqiris Generic and Digitizer Device Driver ###
#include "AcqirisImport.h"
#include "AcqirisD1Import.h"

using namespace std;

const ViInt32 MAX_SUPPORTED_DEVICES = 10;

// ### Global variables ###
ViSession InstrumentID[MAX_SUPPORTED_DEVICES];	// Array of instrument handles
ViInt32 InstrIdx = 0;								// Index of the selected instrument
ViInt32 NumInstruments = 0;						// Number of instruments detected

ViReal64 si = 1.0e-12;			  // Sampling Interval
ViInt32 nbrSamples = 1000;			  // Number of samples
ViInt32 of = 10;					  // Oversampling Factor
ViInt32 oa = 100;					  // Oversampling Accuracy
char const *OutputFile = "RIS.data"; // Output file

// ### Configuration ###
/*
The following values have been used to acquire a step signal (0.8Vpp, 150ps)
*/
ViReal64 delayTime = -50e-9;
ViInt32 nbrSegments = 1;
ViInt32 coupling = 3;		// DC 50 Ohms
ViInt32 bandwidth = 0;
ViReal64 fullScale = 1.0;
ViReal64 offset = 0.4;
ViInt32 trigCoupling = 0;
ViInt32 trigSlope = 0;
ViReal64 trigLevel = -20.0;

// the RIS data structure (once per bin is allocated)
struct RISData
{
	ViReal64 horPos;			// horpos
	ViReal64 *waveformArray;		// the acquired waveform
	ViReal64 c_bin;				// center of the bin
	ViReal64 upper_bin;			// upper limit of the bin range
	ViReal64 lower_bin;			// lower limit of the bin range
};


// Forward declarations
ViStatus CheckInputArguments(int argc, char *argv[]);
ViStatus FindAndSelectDevices(void);
void Configure(ViInt32 id);
void ConfigureReadParameters(ViInt32 buffer_size, AqReadParameters *readPar);
ViStatus Acquire(ViInt32 id);
void saveData(int channel, AqDataDescriptor & descriptor, int nb_iter, int skipped, RISData *ris_data);
ViStatus CloseDevices();
void PrintStatus(ViChar const description[], ViStatus errorCode);

//////////////////////////////////////////////////////////////////////////////////////////
/*
0. Check input arguments
1. Device detection and selection
2. Configuration of the selected device
3. Resources allocation
4. RIS acquisitions
5. Data saving
6. Resources freeing
*/
int main(int argc, char *argv[])
{
	// Check input arguments
	// ---------------------------------------------------------------------------
	ViStatus status = CheckInputArguments(argc, argv);
	if (status) return VI_SUCCESS;	// -h option

	// Find and select a device to performs RIS acquisitions
	// ---------------------------------------------------------------------------
	status = FindAndSelectDevices();
	if (status) return CloseDevices();
	
	// Configuration 
	// ---------------------------------------------------------------------------
	Configure(InstrumentID[InstrIdx]); 

	ViInt32 channel = 1;
	ViInt32 buffer_size = nbrSamples * sizeof(ViReal64);
	ViReal64 *waveformArray = new ViReal64[nbrSamples];

	AqReadParameters readPar;
	ConfigureReadParameters(buffer_size, &readPar);

	// Resources allocation and init
	// ---------------------------------------------------------------------------
	ViReal64 fc = 0.01 * (oa/2) * si / of;
	RISData *ris_data = new RISData[of];

	for (int k=0; k<of; k++)
	{
		ris_data[k].horPos = 1.0;
		ris_data[k].waveformArray = new ViReal64[nbrSamples];

		ris_data[k].c_bin = -si*(k+0.5)/of;	// center of the bin

		ris_data[k].upper_bin = ris_data[k].c_bin + fc;	// upper limit of the bin range
		ris_data[k].lower_bin = ris_data[k].c_bin - fc;	// lower limit of the bin range
	}
	cout << endl;

	// RIS acquisitions
	// ---------------------------------------------------------------------------
	AqDataDescriptor descriptor;
	AqSegmentDescriptor segDesc;
	int index;
	int nb_iter = 0;
	int skipped = 0;

	cout << "Acquire ";
	ViInt32 nb_bin = 0;		// number of bins that have been filled
	while (nb_bin < of)	// the RIS acquisition is done when all bins have been filled
	{
		status = Acquire(InstrumentID[InstrIdx]);
		if (status) return CloseDevices();

		status = AcqrsD1_readData(	InstrumentID[InstrIdx], 
									channel, 
									&readPar, 
									waveformArray, 
									&descriptor, 
									&segDesc);

		PrintStatus("AcqrsD1_readData", status);

		if (!(nb_iter % 10)) cout << ".";	// show progress
		
		nb_iter++;

		index = int(fabs(segDesc.horPos) * of / si);	// bin index

		// horPos in range check (oa mode only)
		if ((oa < 100) && 
			((segDesc.horPos < ris_data[index].lower_bin) || 
			(segDesc.horPos > ris_data[index].upper_bin)))
		{
			skipped++;
			continue;	// next acquisition
		}
		
		// check if a valid horPos value is already set for this bin
		if (ris_data[index].horPos < 1.0)
		{
			// Yes, compare the stored horPos value with the new horPos value
            // to known which one is the better centered.
            // Continue to the next acquisition if the stored horPos value
            // is better centered, else replace the stored value with the new
            // horPos value.
			if (fabs(ris_data[index].c_bin - segDesc.horPos) >
				fabs(ris_data[index].c_bin - ris_data[index].horPos))
					continue;	// next acquisition
		}
		else
			nb_bin++; 
			

		// Set the horPos and the waveform for this bin
		ris_data[index].horPos = segDesc.horPos;
		memcpy(ris_data[index].waveformArray, waveformArray, buffer_size);
 	}

	cout << " done." << endl << "Iterations: " << nb_iter << endl;
	cout << "Skipped acquisitions: " << skipped << endl;

	// Save data to file
	// ---------------------------------------------------------------------------
	saveData(channel, descriptor, nb_iter, skipped, ris_data);
	
	
	// Free resources
	// ---------------------------------------------------------------------------
	for (int k=0; k<of; k++)
	{
		delete [] ris_data[k].waveformArray;
	}
	delete [] ris_data;

	delete [] waveformArray;

	return CloseDevices();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Check input arguments
ViStatus CheckInputArguments(int argc, char *argv[])
{
	int iv;
	char *strP;
	while (--argc>0)
	{
		strP = argv[argc];

		if (argc == 1 && strcmp(strP, "-h") == 0)
		{
			cout << endl
				<< "Usage: RisAcquisitionVC [-h] | [-si] [-ns] [-of] [-oa] [-f]" << endl << endl
				<< "Options:" << endl
				<< "\t-h Displays this help" << endl
				<< "\t-si Sampling interval" << endl
				<< "\t-ns Number of samples" << endl << endl				
				<< "\t-of Oversampling factor" << endl
				<< "\t-oa Oversampling accuracy (1..100%)" << endl 
				<< "\t-f Output file" << endl

				<< "Note: An option value must be glued to the option" << endl << endl
				<< "Ex:" << endl
				<< "\tRisAcquisitionVC -si1e-8 -ns2000 -of5 -oa25 -fMyRIS.data" << endl << endl
				<< "\tSampling interval = 1.0E-8" << endl
				<< "\tNumber of samples = 2000" << endl
				<< "\tOversampling factor = 5" << endl
				<< "\tOversampling accuracy = 25%" << endl
				<< "\tOutput file = MyRIS.data"	<< endl;

			return 1;
		}
		
		else if (strstr(strP, "-si"))		// Sampling Interval
		{
			if (strlen(strP+3)) si = atof(strP+3);
		}

		else if (strstr(strP, "-ns"))		// Number of Samples
		{
			if (strlen(strP+3))
			{
				iv = atoi(strP+3);
				if (iv>100)	nbrSamples = iv;
			}
		}

		else if (strstr(strP, "-of"))		// Oversampling Factor
		{
			if (strlen(strP+3))
			{
				iv = atoi(strP+3);
				if (iv>0) of = iv;				
			}
		}

		else if (strstr(strP, "-oa"))		// Oversampling accuracy
		{
			if (strlen(strP+3))
			{
				iv = atoi(strP+3);
				if (iv>0) oa = iv;
				if (oa>100) oa = 100;
			}
		}

		else if (strstr(strP, "-f"))		// Output file
		{
			if (strlen(strP+2)) OutputFile = strP+2;
		}

	}

	cout	<< endl << "Agilent Acqiris Digitizer - RIS Demo"
			<< endl << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl
			<< "Output file: " << OutputFile << endl
			<< "Sampling interval: " << si << endl
			<< "Number of samples: " << nbrSamples << endl
			<< "Oversampling factor: " << of << endl
			<< "Oversampling accuracy: " << oa << endl << endl;

	return VI_SUCCESS;
}



//////////////////////////////////////////////////////////////////////////////////////////
//! Devices detection and digitizer selection by the user
/*!
The size list of devices displayed is limited to MAX_SUPPORTED_DEVICES
The device selected must be a digitizer
*/
ViStatus FindAndSelectDevices(void)
{
	ViStatus status;

	char name[20];
	ViInt32 sn, bus, slot;

	cout << "Device detection in progress..." << endl;

	// Find all digitizers
	ViChar options[32] = "";

	status = Acqrs_getNbrInstruments(&NumInstruments);
	PrintStatus("Acqrs_getNbrInstruments", status);

	if (NumInstruments < 1)	
	{
		cout << "Instrument not found!" << endl;
		return -1;	// No instrument found
	}

	if (NumInstruments > MAX_SUPPORTED_DEVICES)
	{
		NumInstruments = MAX_SUPPORTED_DEVICES;
	}

	// Initialize instruments
	for (int i = 0; i < NumInstruments; i++)
	{
		char resourceName[20];
		sprintf(resourceName, "PCI::INSTR%d", i);

		status = Acqrs_InitWithOptions(	resourceName, 
										VI_FALSE, 
										VI_FALSE, 
										options, 
										&(InstrumentID[i]));

		PrintStatus("Acqrs_InitWithOptions", status);

		status = Acqrs_getInstrumentData(InstrumentID[i], name, &sn, &bus, &slot);
		PrintStatus("Acqrs_getInstrumentData", status);

		cout << endl << i << ": " << name << " [" << sn << "] on bus " << bus << ", slot " << slot;
	}
	
	// Device selection
	cout << endl << endl << "Select an instrument: ";
	cin >> InstrIdx;
	
	if (InstrIdx >= NumInstruments || InstrIdx < 0) 
	{
		cout << "No instrument selected!" << endl;		
		return -1;	// selection out of range
	}

	ViInt32 devType;
	status = Acqrs_getDevTypeByIndex(InstrIdx, &devType);
	PrintStatus("Acqrs_getDevTypeByIndex", status);
	if (devType != AqD1)
	{
		cout << "You must select a digitizer!" << endl;		
		return -1;	// Not a digitizer
	}

	return VI_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////////////////
void Configure(ViInt32 id)
{
	// Configure timebase
	ViStatus status = AcqrsD1_configHorizontal(id, si, delayTime);
	PrintStatus("AcqrsD1_configHorizontal", status);

	status = AcqrsD1_getHorizontal(id, &si, &delayTime);
	PrintStatus("AcqrsD1_getHorizontal", status);

	// Output the si value (could be adapated)
	cout << "Sampling interval: " << si << endl;

	status = AcqrsD1_configMemory(id, nbrSamples, nbrSegments);
	PrintStatus("AcqrsD1_configMemory", status);

	// Configure vertical settings of channel 1
	status = AcqrsD1_configVertical(id, 1, fullScale, offset, coupling, bandwidth);
	PrintStatus("AcqrsD1_configVertical", status);

	// Configure edge trigger on channel 1
	status = AcqrsD1_configTrigClass(id, 0, 0x00000001, 0, 0, 0.0, 0.0);
	PrintStatus("AcqrsD1_configTrigClass", status);

	// Configure the trigger conditions of channel 1 internal trigger
	status = AcqrsD1_configTrigSource(id, 1, trigCoupling, trigSlope, trigLevel, 0.0);
	PrintStatus("AcqrsD1_configTrigSource",status);
}

//////////////////////////////////////////////////////////////////////////////////////////
//! Configure the read parameters of the digitizer
/*!
Standard waveform, one segment, 64-bit per sample
*/
void ConfigureReadParameters(ViInt32 buffer_size, AqReadParameters *readPar)
{
	readPar->dataType = ReadReal64;
	readPar->readMode = ReadModeStdW;
	readPar->firstSegment = 0;
	readPar->nbrSegments = 1;
	readPar->firstSampleInSeg = 0;
	readPar->nbrSamplesInSeg = nbrSamples;
	readPar->segmentOffset = 0;	
	readPar->dataArraySize = buffer_size + 40; // Buffer size + padding
	readPar->segDescArraySize = sizeof(AqSegmentDescriptor);
	readPar->flags		= 0;
	readPar->reserved	= 0;
	readPar->reserved2	= 0;
	readPar->reserved3	= 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//! Acquisition
ViStatus Acquire(ViInt32 id)
{
	// ### Acquiring a waveform ###
	ViBoolean done = 0;	
	ViInt32 errTimeoutCounter = 500000; // Timeout for acquisition completion

	ViStatus status = AcqrsD1_acquire(id); // Start the acquisition
	PrintStatus("AcqrsD1_acquire", status);

	while (!done && errTimeoutCounter--)
	{
		status = AcqrsD1_acqDone(id, &done); // Poll for the end of the acquisition
	}

	if (!done)
	{
		// Acquisition do not complete successfully
		status = AcqrsD1_stopAcquisition(id);
		PrintStatus("AcqrsD1_stopAcquisition", status);

		cout << endl << "Acquisition timeout!" << endl;
		cout << endl << "The acquisition has been stopped!" << endl;
		return -1;
	}

	return VI_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////////////////
//! Save results in a file
/*!

*/
void saveData(int channel, AqDataDescriptor & descriptor, int nb_iter, int skipped, RISData *ris_data)
{
	cout << "Saving data ";
	ofstream outFile(OutputFile);
	outFile << "# Number of samples: " << nbrSamples << " S" << endl;
	outFile << "# Time increment: " << si / of << " s" << endl;
	outFile << "# Initial time: " << ris_data[of-1].c_bin << " s" << endl;
	outFile << "# Channel: " << channel << endl;
	outFile << "# Oversampling factor: " << of << endl;
	outFile << "# Oversampling accuracy: " << oa << endl;
	outFile << "# Iterations: " << nb_iter << endl;
	outFile << "# Skipped acquisitions: " << skipped << endl;	
	outFile << '\x20' << endl;

	for (int d=0; d<descriptor.returnedSamplesPerSeg; d++)
	{
		for (int k=of-1; k>=0; k--)
			outFile << ris_data[k].waveformArray[d] << endl;

		if (!(d % (descriptor.returnedSamplesPerSeg /10)))
			cout << ".";
	}

	cout << " done.";
	outFile.close();
}

//////////////////////////////////////////////////////////////////////////////////////////
//! Close all devices
ViStatus CloseDevices()
{
	// Close all instruments 
	ViStatus status = Acqrs_closeAll(); 

	PrintStatus("AcqrsD1_configTrigSource",status);

	return status;
}

//////////////////////////////////////////////////////////////////////////////////////////
//! Output the error message corresponding to 'errorCode'
/*!
No output if 'errorCode == VI_SUCCESS'
The string 'description' is prepended to the error string returned by the driver.
*/
void PrintStatus(ViChar const description[], ViStatus errorCode)
{
	if (errorCode == VI_SUCCESS) return;

	ViChar errorMessage[512];
	Acqrs_errorMessage(NULL, errorCode, errorMessage, 512);
	cout << endl << description << ": " << errorMessage << endl;
}



