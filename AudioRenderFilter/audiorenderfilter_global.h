#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(AUDIORENDERFILTER_LIB)
#  define AUDIORENDERFILTER_EXPORT Q_DECL_EXPORT
# else
#  define AUDIORENDERFILTER_EXPORT Q_DECL_IMPORT
# endif
#else
# define AUDIORENDERFILTER_EXPORT
#endif
