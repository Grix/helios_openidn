

#include <stdio.h>
#include <stdarg.h>

// Module header
#include "OpenIDN.hpp"



void logError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    printf("\x1B[1;31m");
    vprintf(fmt, ap);
    printf("\x1B[0m");
    printf("\n");
    fflush(stdout);

    va_end(ap);
}


void logWarn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    printf("\x1B[1;33m");
    vprintf(fmt, ap);
    printf("\x1B[0m");
    printf("\n");
    fflush(stdout);

    va_end(ap);
}


void logInfo(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vprintf(fmt, ap);
    printf("\n");
    fflush(stdout);

    va_end(ap);
}



// =================================================================================================
//  Class TracePrinter
//
// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

TracePrinter::TracePrinter(ODF_ENV *env, const char *frameName)
{
}


TracePrinter::~TracePrinter()
{
}


void TracePrinter::logError(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    printf("\x1B[1;31m");
    vprintf(format, ap);
    printf("\x1B[0m");
    printf("\n");
    fflush(stdout);

    va_end(ap);
}


void TracePrinter::logWarn(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    printf("\x1B[1;33m");
    vprintf(format, ap);
    printf("\x1B[0m");
    printf("\n");
    fflush(stdout);

    va_end(ap);
}


void TracePrinter::logInfo(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vprintf(format, ap);
    printf("\n");
    fflush(stdout);

    va_end(ap);
}


void TracePrinter::logDebug(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    vprintf(format, ap);
    printf("\n");
    fflush(stdout);

    va_end(ap);
}

