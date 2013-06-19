#
#  Makefile for Agilent - Acqiris Linux GetStarted
#
#  Copyright Agilent Technologies, Inc. 2001, 2002-2009
#

# Makefile behaviour can be changed by defining the following standard GNU make
# environment variables:
CC: gcc
CXX: gcc
#   CPPFLAGS: for C and C++ preprocessor options
#   CFLAGS: for C compiler options
#   CXXFLAGS: for C++ compiler options
#   LDFLAGS: for link options
#   RM: the rm command to use


TARGETS= \
  GetStartedC \
  GetStarted16bitMultiSegment \
  GetStarted16bitSingleSegment \
  GetStarted8bitMultiSegment \
  GetStarted8bitSingleSegment \
  GetStartedAvgVC \
  GetStartedHistoTDC \
  GetStartedPeakTDC \
  GetStartedSARmode \
  GetStartedTC84x \
  GetStartedTC890 \
  GetStartedThresholdGatesSSR \
  GetStartedU1084AAvg \
  GetStartedU1084APeakTDC \
  GetStartedUserGatesSSR \
  GetStartedVoltsMultiSegment \
  GetStartedVoltsSingleSegment \
  InstrumentDiscovery \
  RisAcquisitionVC \


LIBCPPFLAGS= -D_LINUX -D_ACQIRIS
LIBLDFLAGS= -lAqDrv4 -lpthread



all: $(TARGETS)

clean:
	$(RM) $(TARGETS)


%: ../%.c
	$(CC) $(LIBCPPFLAGS) $(CPPFLAGS) $(CFLAGS) $< $(LIBLDFLAGS) $(LDFLAGS) -o $@

%: ../%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LIBCPPFLAGS) $< $(LDFLAGS) $(LIBLDFLAGS) -o $@


