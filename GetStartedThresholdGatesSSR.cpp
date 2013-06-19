//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedThresholdGatesSSR.cpp: Example program for Threshold Gated SSR on AP240 modules
//
//----------------------------------------------------------------------------------------
//
//  Copyright Agilent Technologies Inc., 2000, 2001-2009
//
//////////////////////////////////////////////////////////////////////////////////////////

#include <AcqirisImport.h>
#include <AcqirisD1Import.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
	ViSession idInstrument;
	ViStatus status = Acqrs_InitWithOptions((ViRsrc)"PCI::INSTR0", VI_FALSE,
			VI_FALSE, "CAL=0", &idInstrument);

	if (status != VI_SUCCESS)
		return fprintf(stderr, "ERROR: Instrument not found.\n"), 1;

	printf("# Calibrating instrument %d\n", (int)idInstrument);
	status = Acqrs_calibrate(idInstrument);

	// Configure instrument mode and timebase
	ViInt32 modeSSR = 7;
	status = AcqrsD1_configMode(idInstrument, modeSSR, 0, 0);

	ViReal64 sampInterval = 1e-9;    // sampling interval in seconds
	ViReal64 delayTime = 0.0;        // trigger delay time in seconds
	status = AcqrsD1_configHorizontal(idInstrument, sampInterval, delayTime);

    long const idChannel = 1;

    ViReal64 fullscale = 2.0;        // fullscale value in Volts
    ViReal64 offset = 0.0;           // offset value in Volts
    ViInt32 coupling = 3;            // coupling DC, 50 ohm
    ViInt32 bandwidth = 0;           // no bandwidth limit
    status = AcqrsD1_configVertical(idInstrument, idChannel, fullscale, offset, coupling, bandwidth);

	// Configure edge trigger on channel 1
	ViInt32 trigClass = 0; 				
	ViInt32 sourcePattern = 0x1;		// Trigger on Channel 1	
	status = AcqrsD1_configTrigClass(idInstrument, trigClass, sourcePattern, 0x0, 0, 0.0, 0.0);

	// Configure the trigger conditions of channel 1 internal trigger
	ViInt32 trigCoupling = 0;			// DC coupling
	ViInt32 trigSlope = 0;				// Positive slope
	ViReal64 trigLevel = 10.0;			// Trigger level = +10% of FSR (i.e. + 50 mV)		
	status = AcqrsD1_configTrigSource(idInstrument, 1, trigCoupling, trigSlope, trigLevel, 0.0);

	// Configure analyzer parameters
	ViInt32 nbrSamples = 1024;
	ViInt32 nbrSegments = 12;
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "NbrSamples", &nbrSamples);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "NbrSegments", &nbrSegments);

	ViInt32 startDelay = 0;
	ViInt32 stopDelay = 0;
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "StartDelay", &startDelay);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "StopDelay", &stopDelay);

	// Configure gates parameters
	ViInt32 gateType = 2;         // 1 = user defined, 2 = threshold
	ViReal64 threshold = -0.125;
	ViInt32 invertData = 0;       // 0 = normal data, 1 = invert data

	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "GateType", &gateType);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "Threshold", &threshold);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "InvertData", &invertData);

	// Allocate buffers for readout
	long const lenGate = 512; // need to be estimated 
	long const nbrGates = 3;  // need to be estimated
	long const nbrSamplePerSeg = (8 + lenGate) * nbrGates;
	long const nbrBytesAlloc = (8 + nbrSamplePerSeg) * nbrSegments;

	signed char *dataArray = new signed char[nbrBytesAlloc];

	AqReadParameters readParam;
	AqDataDescriptor dataDesc;

	printf("# Prepared readout for %ld bytes\n", nbrBytesAlloc);

	// Perform acquisitions
	long nbrWforms = 10;

	status = AcqrsD1_acquire(idInstrument);  // Start the first acquisition

	for (long nWform = 0 ; nWform < nbrWforms ; ++nWform)
    {
	    // Check for last acquisition
        long const lastswitch = (nWform == nbrWforms - 1) ? 1 : 0;

	    // Readies next acquisition
		status = AcqrsD1_processData(idInstrument, 0, 1 + lastswitch);
		status = AcqrsD1_waitForEndOfProcessing(idInstrument, 2000);

		// Test for timeout
		if (status == (ViStatus)ACQIRIS_ERROR_TIMEOUT)
		{
			status = AcqrsD1_stopAcquisition(idInstrument);

			break;
		}

		printf("# Acquired %d segments, %d samples\n", (int)nbrSegments, (int)nbrSamples);

		// Readout gated data (only the one of the last acquisition)
		::memset(&readParam, 0, sizeof(readParam));
		::memset(&dataDesc, 0, sizeof(dataDesc));
		readParam.dataType = ReadInt8;
		readParam.readMode = ReadModeSSRW;
		readParam.firstSegment = 0;
		readParam.nbrSegments = nbrSegments;
		readParam.firstSampleInSeg = 0;
		readParam.nbrSamplesInSeg = nbrSamples;
		readParam.segmentOffset = nbrSamples;
		readParam.dataArraySize = nbrBytesAlloc;
		readParam.segDescArraySize = 0;

		status = AcqrsD1_readData(idInstrument, idChannel, &readParam, dataArray,
								  &dataDesc, NULL);

		if (status != VI_SUCCESS)
		    printf("# readData() error %d (0x%08x)\n", (int)status, (int)status);

		printf("# Read %d bytes: %d segments\n", (int)dataDesc.actualDataSize,
			   (int)dataDesc.returnedSegments);

	}

	// Print data of last readout
	signed char const *dataP = dataArray;
	signed char const * const dataEndP = dataArray + dataDesc.actualDataSize;
	int nSeg = 0;

	while (dataP[3] == 0x04 && dataP < dataEndP)
    {
        unsigned int const *segmentP = (unsigned int const *)dataP;
		dataP += 8;

		unsigned int tsHi = segmentP[0] & 0x00FFFFFF;
		unsigned int tsLo = segmentP[1];
        printf("# Segment %d, timestamp %06x:%08x\n", nSeg++, tsHi, tsLo);

		int nGate = 0;

		while (dataP[3] == 0x00 && dataP < dataEndP)
		{
		    unsigned int const *gateP = (unsigned int const *)dataP;
			dataP += 8;

			unsigned int const posGate = gateP[0] & 0x00FFFFFF;
			unsigned int const lenGate = gateP[1];
            printf("#   Gate %d: %u samples at position %u\n", nGate++, lenGate, posGate);

			signed char const *samplesP = dataP;
			dataP += lenGate;

			for (unsigned int n = 0 ; n < lenGate ; ++n)
				printf("%u\t%d\n", posGate + n, samplesP[n]);

		}

	}

	// Cleanup data buffer and instruments
	delete[] dataArray;

	status = Acqrs_closeAll();

	return 0;
}
