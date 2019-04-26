#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(VIDEORENDERFILTER_LIB)
#  define VIDEORENDERFILTER_EXPORT Q_DECL_EXPORT
# else
#  define VIDEORENDERFILTER_EXPORT Q_DECL_IMPORT
# endif
#else
# define VIDEORENDERFILTER_EXPORT
#endif
