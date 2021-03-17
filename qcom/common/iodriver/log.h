
#ifndef __MIC_LOG_H__
#define __MIC_LOG_H__

#ifdef __ANDROID__
#define LOG_TAG "iodriver"
#include <cutils/log.h>
#endif


#ifdef __ANDROID__
#define DTRACE(x, ...) do { ALOGV("%s:%d %s(): " x "\n", basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define DINFO(x, ...) do { ALOGI("%s:%d %s(): " x "\n", basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define DERR(x, ...) do { ALOGE("%s:%d %s(): " x "\n", basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); } while(0)
//#define DLOG(x) do { ALOGD
#else

#ifndef DEBUG_TRACE
#define DTRACE(x, ... ) ((void)0)
#else
#define DTRACE(x, ...) do { fprintf(stderr, "TRACE:%s:%d %s(): " x "\n", basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); } while(0)
#endif

#define DINFO(x, ...) do { fprintf(stderr, "INFO:%s:%d %s(): " x "\n", basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define DERR(x, ...) do { fprintf(stderr, "ERR:%s:%d %s(): " x "\n", basename(__FILE__), __LINE__, __func__, ##__VA_ARGS__); } while(0)

#endif //__ANDROID__

#endif //__MIC_LOG_H__

