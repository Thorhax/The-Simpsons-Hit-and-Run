//=============================================================================
// File:        perflog.cpp
//
// Description: Frame time statistics printed to the log every few seconds.
//              See perflog.h.
//=============================================================================

#include <main/perflog.h>

#include <gameflow/gameflow.h>

#include <radtime.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __SWITCH__
#include <switch.h>

// Defined by the OpenGL display: wait for the GPU after each swap, never wait for vsync
extern bool g_pddiFinishAfterSwap;
extern bool g_pddiNoVSync;

//=============================================================================
// Sampling profiler: a thread on another core pauses the main thread about
// every millisecond and records where it is, including the return addresses
// along the frame pointer chain. The samples are written to the SD card on
// autoexit and symbolized on the PC.
//=============================================================================
namespace
{
    const int SAMPLE_FRAMES = 12;
    const unsigned int MAX_SAMPLES = 300000;    // about 5 minutes

    struct Sample
    {
        unsigned int context;
        unsigned int timeMs;    // since the sampler started (the first frame)
        unsigned long long pc;
        unsigned long long frames[ SAMPLE_FRAMES ];
    };

    bool sSampling = false;
    volatile bool sSamplerStop = false;
    Thread sSamplerThread;
    Sample* sSamples = NULL;
    unsigned int sNumSamples = 0;
    volatile int sSampleContext = -1;
    radTime64 sSamplerStart = 0;

    void SamplerMain( void* )
    {
        Handle main = envGetMainThreadHandle();

        // The code mapping starts at the module base, which is address 0 in the ELF
        MemoryInfo info;
        u32 pageInfo;
        svcQueryMemory( &info, &pageInfo, (u64)&SamplerMain );
        const unsigned long long base = info.addr;

        while( !sSamplerStop && sNumSamples < MAX_SAMPLES )
        {
            svcSleepThread( 1000000 );

            if( R_FAILED( svcSetThreadActivity( main, ThreadActivity_Paused ) ) )
            {
                continue;
            }

            ThreadContext ctx;
            Result rc = svcGetThreadContext3( &ctx, main );
            if( R_SUCCEEDED( rc ) )
            {
                Sample& s = sSamples[ sNumSamples ];
                memset( &s, 0, sizeof( s ) );
                s.context = sSampleContext;
                s.timeMs = (unsigned int)( ( radTimeGetMicroseconds64() - sSamplerStart ) / 1000 );
                s.pc = ctx.pc.x - base;

                // The leaf may not have stored its frame record yet, so keep the link register too
                s.frames[ 0 ] = ctx.lr - base;
                unsigned long long fp = ctx.fp;
                unsigned long long lo = ctx.sp, hi = ctx.sp + 0x400000;
                for( int i = 1; i < SAMPLE_FRAMES && fp >= lo && fp + 16 <= hi && ( fp & 7 ) == 0; i++ )
                {
                    s.frames[ i ] = ( (unsigned long long*)fp )[ 1 ] - base;
                    unsigned long long next = ( (unsigned long long*)fp )[ 0 ];
                    if( next <= fp )
                    {
                        break;
                    }
                    fp = next;
                }
                sNumSamples++;
            }

            svcSetThreadActivity( main, ThreadActivity_Runnable );
        }
    }

    void StartSampler()
    {
        sSamples = (Sample*)malloc( sizeof( Sample ) * MAX_SAMPLES );
        if( sSamples == NULL )
        {
            printf( "perf: no memory for samples\n" );
            return;
        }
        sSamplerStart = radTimeGetMicroseconds64();
        // Run on core 2, away from the main thread on core 0
        if( R_SUCCEEDED( threadCreate( &sSamplerThread, SamplerMain, NULL, NULL, 0x4000, 0x20, 2 ) ) )
        {
            threadStart( &sSamplerThread );
            sSampling = true;
        }
    }

    void StopSampler()
    {
        if( !sSampling )
        {
            return;
        }
        sSamplerStop = true;
        threadWaitForExit( &sSamplerThread );
        threadClose( &sSamplerThread );
        sSampling = false;

        FILE* f = fopen( "sdmc:/switch/simpsons/perf_samples.bin", "wb" );
        if( f != NULL )
        {
            fwrite( sSamples, sizeof( Sample ), sNumSamples, f );
            fclose( f );
        }
        printf( "perf: wrote %u samples\n", sNumSamples );
        fflush( stdout );
    }
}
#endif

namespace
{
    const radTime64 REPORT_INTERVAL = 5000000;     // microseconds
    const radTime64 FRAME_60HZ = 16950;            // a little over 1/60 s
    const radTime64 FRAME_30HZ = 33900;            // a little over 1/30 s

    bool sEnabled = false;
    unsigned int sAutoExitSeconds = 0;
    int sContext = -1;

    radTime64 sStartTime = 0;
    radTime64 sFrameStart = 0;
    radTime64 sReportStart = 0;
    radTime64 sSectionStart[ PerfLog::NUM_SECTIONS ];
    radTime64 sFrameSection[ PerfLog::NUM_SECTIONS ];   // this frame only

    // Totals over the current report interval
    unsigned int sFrames = 0;
    unsigned int sOver60 = 0;
    unsigned int sOver30 = 0;
    radTime64 sFrameTotal = 0;
    radTime64 sFrameMax = 0;
    radTime64 sSectionTotal[ PerfLog::NUM_SECTIONS ];

    void Report( radTime64 now )
    {
        if( sFrames == 0 )
        {
            return;
        }

        double seconds = ( now - sReportStart ) / 1000000.0;
        double update = sSectionTotal[ PerfLog::SECTION_UPDATE ] / 1000.0 / sFrames;
        double swap = sSectionTotal[ PerfLog::SECTION_SWAP ] / 1000.0 / sFrames;
        // The swap happens inside the render flow
        double render = sSectionTotal[ PerfLog::SECTION_RENDER ] / 1000.0 / sFrames - swap;
        double frame = sFrameTotal / 1000.0 / sFrames;
        double other = frame - update - render - swap;

        printf( "perf: t=%.0fs ctx %d %.1f fps | frame avg %.2f max %.2f ms | >16.9ms %u >33.9ms %u | "
                "update %.2f render %.2f swap %.2f other %.2f ms\n",
                ( now - sStartTime ) / 1000000.0, sContext, sFrames / seconds, frame, sFrameMax / 1000.0,
                sOver60, sOver30, update, render, swap, other );
        fflush( stdout );

        sFrames = 0;
        sOver60 = 0;
        sOver30 = 0;
        sFrameTotal = 0;
        sFrameMax = 0;
        memset( sSectionTotal, 0, sizeof( sSectionTotal ) );
        sReportStart = now;
    }
}

bool PerfLog::HandleOption( const char* option )
{
    if( strcmp( option, "perflog" ) == 0 )
    {
        sEnabled = true;
        return true;
    }
#ifdef __SWITCH__
    if( strcmp( option, "perfsample" ) == 0 )
    {
        sEnabled = true;
        sSampling = true;   // started on the first frame
        return true;
    }
    if( strcmp( option, "perfsync" ) == 0 )
    {
        g_pddiFinishAfterSwap = true;
        return true;
    }
    if( strcmp( option, "perfnovsync" ) == 0 )
    {
        g_pddiNoVSync = true;
        return true;
    }
#endif
    if( strncmp( option, "autoexit=", 9 ) == 0 )
    {
        sAutoExitSeconds = atoi( option + 9 );
        return true;
    }
    return false;
}

bool PerfLog::IsEnabled()
{
    return sEnabled;
}

void PerfLog::BeginFrame()
{
    if( !sEnabled && sAutoExitSeconds == 0 )
    {
        return;
    }

    sFrameStart = radTimeGetMicroseconds64();
    memset( sFrameSection, 0, sizeof( sFrameSection ) );
    if( sStartTime == 0 )
    {
        sStartTime = sFrameStart;
        sReportStart = sFrameStart;
        memset( sSectionTotal, 0, sizeof( sSectionTotal ) );
#ifdef __SWITCH__
        if( sSampling )
        {
            sSampling = false;
            StartSampler();
        }
#endif
    }
}

void PerfLog::EndFrame()
{
    if( !sEnabled )
    {
        return;
    }

    radTime64 now = radTimeGetMicroseconds64();
    radTime64 frame = now - sFrameStart;

    sFrames++;
    sFrameTotal += frame;
    if( frame > sFrameMax ) sFrameMax = frame;
    if( frame > FRAME_60HZ ) sOver60++;
    if( frame > FRAME_30HZ )
    {
        sOver30++;

        // Report each hitch with what the frame spent its time on
        printf( "perf: hitch t=%.3fs %.1f ms | update %.1f render %.1f swap %.1f ms\n",
                ( now - sStartTime ) / 1000000.0, frame / 1000.0,
                sFrameSection[ SECTION_UPDATE ] / 1000.0,
                ( sFrameSection[ SECTION_RENDER ] - sFrameSection[ SECTION_SWAP ] ) / 1000.0,
                sFrameSection[ SECTION_SWAP ] / 1000.0 );
    }

    // Report each context change, closing the interval so it covers one context
    int context = GetGameFlow() ? GetGameFlow()->GetCurrentContext() : -1;
    if( context != sContext )
    {
        Report( now );
        sContext = context;
#ifdef __SWITCH__
        sSampleContext = context;
#endif
        printf( "perf: t=%.1fs context %d\n", ( now - sStartTime ) / 1000000.0, context );
        fflush( stdout );
    }
    else if( now - sReportStart >= REPORT_INTERVAL )
    {
        Report( now );
    }
}

void PerfLog::BeginSection( Section section )
{
    if( sEnabled )
    {
        sSectionStart[ section ] = radTimeGetMicroseconds64();
    }
}

void PerfLog::EndSection( Section section )
{
    if( sEnabled )
    {
        radTime64 elapsed = radTimeGetMicroseconds64() - sSectionStart[ section ];
        sSectionTotal[ section ] += elapsed;
        sFrameSection[ section ] += elapsed;
    }
}

bool PerfLog::ShouldExit()
{
    return sAutoExitSeconds != 0 && sStartTime != 0 &&
           radTimeGetMicroseconds64() - sStartTime >= (radTime64)sAutoExitSeconds * 1000000;
}

void PerfLog::Shutdown()
{
#ifdef __SWITCH__
    StopSampler();
#endif
}
