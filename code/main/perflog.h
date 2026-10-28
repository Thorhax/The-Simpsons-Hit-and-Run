//=============================================================================
// File:        perflog.h
//
// Description: Frame time statistics printed to the log every few seconds,
//              used to measure the console ports. Enabled with the "perflog"
//              command line option; "autoexit=<seconds>" quits the game after
//              the given time so test runs can end unattended.
//
//              On the Switch, "perfsync" waits for the GPU after each swap so
//              the GPU and vsync wait shows up as swap time, "perfnovsync"
//              turns off vsync to show how fast frames can go, and "perfsample"
//              records where the main thread spends its time (written to
//              sdmc:/switch/simpsons/perf_samples.bin on autoexit).
//=============================================================================

#ifndef PERFLOG_H
#define PERFLOG_H

namespace PerfLog
{
    enum Section
    {
        SECTION_UPDATE,     // Game flow (logic, physics, AI)
        SECTION_RENDER,     // Render flow, excluding the buffer swap
        SECTION_SWAP,       // Buffer swap (vsync and GPU wait)
        NUM_SECTIONS
    };

    // Returns true when the option was one of ours
    bool HandleOption( const char* option );

    bool IsEnabled();

    void BeginFrame();
    void EndFrame();

    void BeginSection( Section section );
    void EndSection( Section section );

    // True once the "autoexit" time has passed
    bool ShouldExit();

    // Stops the sampling profiler and writes its samples
    void Shutdown();
}

#endif // PERFLOG_H
