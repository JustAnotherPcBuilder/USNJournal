SRCS = USNJournalReader.cpp
EXES = $(SRCS:.cpp=.exe)

.cpp.exe:
	cl $(CXXFLAGS) $< /Fe$@

USNJournalReader.exe: USNJournalReader.cpp

all: $(EXES)

clean:
	del /Q $(EXES)