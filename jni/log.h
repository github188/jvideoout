#ifndef _LOG_H_
#define _LOG_H_

#if (defined WIN32) || (defined LINUX)
int LOGI(char *fmt, ...);
#else
//logs for logcat
#include "android/log.h"

#define TAG "jvs_JVideoOut_jni"

#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO,  TAG, fmt, ##args)
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#endif

#endif // _LOG_H_
