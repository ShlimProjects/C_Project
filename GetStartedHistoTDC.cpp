//////////////////////////////////////////////////////////////////////////////////////////
//
//  GetStartedHistoTDC.cpp: Example program for PeakTDC histogram on AP240 modules
//
//----------------------------------------------------------------------------------------
//
//  Copyright Agilent Technologies Inc., 2000, 2001-2009
//
//////////////////////////////////////////////////////////////////////////////////////////
//
//  How to connect the AP240 board when using GetStarted.cpp
//========================================================================================
//  To obtain best results with the interpolation algorithm the AP240 trigger out signal
//  must be used to start the stimulation that causes the measured event.
//
//  To simulate this behaviour without a complete machine, please follow the steps below:
//
//  1.- The AP240 ext trigger input has to be connected to a pulse generator
//  2.- Connect AP240 trig out to a bandwidth or high pass filter
//  3.- Connect the filter output to the digitizer input.
//  4.- DemoSSR.exe can be used to check your signal and to adjust parameters
//  (The delays and fullscale parameters depends on your filter characteristics)
//  5.- Adjust AP full scale, gates, and delay settings in GetStarted.cpp
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

	// Configure instrument input and timebase
    long const idChannel = 1;

    ViReal64 fullscale = 0.2;        // fullscale value in Volts
    ViReal64 offset = 0.0;           // offset value in Volts
    ViInt32 coupling = 3;            // coupling DC, 50 ohm
    ViInt32 bandwidth = 0;           // no bandwidth limit
    status = AcqrsD1_configVertical(idInstrument, idChannel, fullscale,
                                    offset, coupling, bandwidth);

	ViReal64 sampInterval = 1e-9;    // sampling interval in seconds
	ViReal64 delayTime = 0.0;        // trigger delay time in seconds
	status = AcqrsD1_configHorizontal(idInstrument, sampInterval, delayTime);

    status = AcqrsD1_configTrigClass(idInstrument, 0, 0x80000000, 0, 0, 0.0, 0.0);
    status = AcqrsD1_configTrigSource(idInstrument, -1, 0, 0, 500.0, 0.0);

    // Configure instrument mode and calibrate
    status = Acqrs_calibrate(idInstrument);

	ViInt32 modePeakTDC = 5;
	status = AcqrsD1_configMode(idInstrument, modePeakTDC, 0, 0);

	// Configure analyzer parameters
	ViInt32 nbrSamples = 1024;
	ViInt32 nbrSamplesDelay = 32;
	ViInt32 nbrSegments = 1;
	ViInt32 nbrWaveforms = 100000;
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "NbrSamples", &nbrSamples);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "StartDelay", &nbrSamplesDelay);
	status = AcqrsD1_configAvgConfig(idInstrument, 0, "NbrSegments", &nbrSegments);
	//AcqrsD1_configAvgConfig(idInstrument, 0, "NbrRoundRobins", &nbrWaveforms);


	// Configure gates parameters
	ViInt32 gateType = 2;         // 1 = user defined, 2 = threshold
    ViInt32 thresholdEnable = 1;  // enable threshold
    ViReal64 threshold = -0.1;    // threshold value
	ViInt32 invertData = 0;       // invert data

	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "GateType", &gateType);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "ThresholdEnable", &thresholdEnable);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "Threshold", &threshold);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "InvertData", &invertData);


    // Configure histogram parameters
	typedef unsigned long ValueType;	// will set histoDepth to 1: 32 bits bins
	//typedef unsigned short ValueType; // will set histoDepth to 0: 16 bits bins
	ViInt32 histoMode = 1;        // 1 = simple histogram, 3 = histo with interpolation
	ViInt32 histoIncrement = 2;   // 1 = increment by 1, 2 = increment by value
	ViInt32 histoHorzRes = 4;     // n = 0-4: increase resolution by 2 ** n
	ViInt32 histoVertRes = 4;     // n = 0-4: increase resolution by 2 ** n
	ViInt32 overlaySegments = 0;  // 0 = individual segments, 1 = overlay segments

	ViInt32 histoDepth = sizeof(ValueType) == 2 ? 0 : 1; // 0 = 16 bits, 1 = 32 bits
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcHistogramMode", &histoMode);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcHistogramIncrement", &histoIncrement);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcHistogramDepth", &histoDepth);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcHistogramHorzRes", &histoHorzRes);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcHistogramVertRes", &histoVertRes);
	status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcOverlaySegments", &overlaySegments);

    ViInt32 processType = 2;      // 1 = std peak, 2 = interpolated peaks
	ViReal64 startPeak = 0.02;    // start hysteresis in Volts
	ViReal64 validPeak = 0.02;    // valid hysteresis in Volts
    status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "TdcProcessType", &processType);
    status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "StartDeltaPosPeakV", &startPeak);
    status = AcqrsD1_configAvgConfig(idInstrument, idChannel, "ValidDeltaPosPeakV", &validPeak);


    // Perform first acquisition for void
	status = AcqrsD1_acquire(idInstrument);
	status = AcqrsD1_waitForEndOfAcquisition(idInstrument, 200);

    if (status == (ViStatus)ACQIRIS_ERROR_ACQ_TIMEOUT)
        fprintf(stderr, "Error: Acquisition timeout.\n");

	// Readout histogram data once to zero all data
	long const nbrBinsPerSeg = nbrSamples * (1 << histoHorzRes);
	long const nbrReadSegs = (histoMode == 2) ? 1 : nbrSegments;
	long const nbrReadBins = nbrBinsPerSeg * nbrReadSegs;

	ValueType *dataArray = new ValueType[nbrReadBins];
	long const nbrBytesAlloc = nbrReadBins * sizeof(ValueType);

	AqReadParameters readParam;
    AqDataDescriptor dataDesc;
    ::memset(&dataDesc, 0, sizeof(dataDesc));
	readParam.dataType = histoDepth == 0 ? ReadInt16 : ReadInt32;
	readParam.readMode = ReadModeHistogram;
	readParam.firstSegment = 0;
	readParam.nbrSegments = nbrReadSegs;
    readParam.firstSampleInSeg = 0;
	readParam.nbrSamplesInSeg = nbrBinsPerSeg;
	readParam.segmentOffset = 0;
	readParam.dataArraySize = nbrBytesAlloc;
    readParam.segDescArraySize = 0;
    readParam.flags = 0; //AqSkipClearHistogram;

	status = AcqrsD1_readData(idInstrument, idChannel, &readParam, dataArray, &dataDesc, NULL);

    if (status < VI_SUCCESS)
        fprintf(stderr, "Error: Read error %d (%08x).\n", (int)status, (int)status);


	// Perform acquisition with real number of acquisitions
	AcqrsD1_configAvgConfig(idInstrument, 0, "NbrRoundRobins", &nbrWaveforms);

    AcqrsD1_acquire(idInstrument);
	status = AcqrsD1_waitForEndOfAcquisition(idInstrument, 2000);

    if (status == (ViStatus)ACQIRIS_ERROR_ACQ_TIMEOUT)
        fprintf(stderr, "Error: Acquisition timeout.\n");

	printf("# Acquired %d acquisitions, %d segments, %d samples\n",
			 (int)nbrWaveforms, (int)nbrSegments, (int)nbrSamples);


    // Readout histogram data
    ::memset(&dataDesc, 0, sizeof(dataDesc));
	readParam.dataType = histoDepth == 0 ? ReadInt16 : ReadInt32;
	readParam.readMode = ReadModeHistogram;
	readParam.firstSegment = 0;
	readParam.nbrSegments = nbrReadSegs;
    readParam.firstSampleInSeg = 0;
	readParam.nbrSamplesInSeg = nbrBinsPerSeg;
	readParam.segmentOffset = 0;
	readParam.dataArraySize = nbrBytesAlloc;
    readParam.segDescArraySize = 0;
    readParam.flags = 0; //AqSkipClearHistogram;

	status = AcqrsD1_readData(idInstrument, idChannel, &readParam, dataArray, &dataDesc, NULL);

    if (status < VI_SUCCESS)
        fprintf(stderr, "Error: Read error %d (%08x).\n", (int)status, (int)status);
	else
	{   // Printout the data
		printf("# Read %d bytes: %d segments of %d values\n", (int)dataDesc.actualDataSize,
				(int)dataDesc.returnedSegments, (int)dataDesc.returnedSamplesPerSeg);

		for (int s = 0 ; s < dataDesc.returnedSegments ; ++s)
		{
			printf("# Segment %d\n", s);
			ValueType *segmentArray = dataArray + (s * nbrBinsPerSeg);

			for (int n = 0 ; n < dataDesc.returnedSamplesPerSeg ; ++n)
				if (segmentArray[n] > 0)
					printf("%d\t%lu\n", n, segmentArray[n]);

		}

	}

	delete[] dataArray;
	status = Acqrs_closeAll();

	return 0;
}
