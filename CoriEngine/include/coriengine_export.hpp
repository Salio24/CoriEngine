
#ifndef CORI_ENGINE_API_H
#define CORI_ENGINE_API_H

#ifdef CORIENGINE_STATIC_DEFINE
#  define CORI_ENGINE_API
#  define CORIENGINE_NO_EXPORT
#else
#  ifndef CORI_ENGINE_API
#    ifdef CoriEngine_shared_EXPORTS
        /* We are building this library */
#      define CORI_ENGINE_API __declspec(dllexport)
#    else
        /* We are using this library */
#      define CORI_ENGINE_API __declspec(dllimport)
#    endif
#  endif

#  ifndef CORIENGINE_NO_EXPORT
#    define CORIENGINE_NO_EXPORT 
#  endif
#endif

#ifndef CORIENGINE_DEPRECATED
#  define CORIENGINE_DEPRECATED __declspec(deprecated)
#endif

#ifndef CORIENGINE_DEPRECATED_EXPORT
#  define CORIENGINE_DEPRECATED_EXPORT CORI_ENGINE_API CORIENGINE_DEPRECATED
#endif

#ifndef CORIENGINE_DEPRECATED_NO_EXPORT
#  define CORIENGINE_DEPRECATED_NO_EXPORT CORIENGINE_NO_EXPORT CORIENGINE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef CORIENGINE_NO_DEPRECATED
#    define CORIENGINE_NO_DEPRECATED
#  endif
#endif

#endif /* CORI_ENGINE_API_H */
