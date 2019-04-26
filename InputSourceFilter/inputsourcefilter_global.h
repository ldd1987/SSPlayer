#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(INPUTSOURCEFILTER_LIB)
#  define INPUTSOURCEFILTER_EXPORT Q_DECL_EXPORT
# else
#  define INPUTSOURCEFILTER_EXPORT Q_DECL_IMPORT
# endif
#else
# define INPUTSOURCEFILTER_EXPORT
#endif
