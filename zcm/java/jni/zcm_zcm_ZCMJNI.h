/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class zcm_zcm_ZCMJNI */

#ifndef _Included_zcm_zcm_ZCMJNI
#define _Included_zcm_zcm_ZCMJNI
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    initializeNative
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_zcm_zcm_ZCMJNI_initializeNative
  (JNIEnv *, jobject, jstring);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMJNI_destroy
  (JNIEnv *, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMJNI_start
  (JNIEnv *, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMJNI_stop
  (JNIEnv *, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    publish
 * Signature: (Ljava/lang/String;[BII)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_publish
  (JNIEnv *, jobject, jstring, jbyteArray, jint, jint);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    subscribe
 * Signature: (Ljava/lang/String;Lzcm/zcm/ZCM;Lzcm/zcm/ZCM;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_zcm_zcm_ZCMJNI_subscribe
  (JNIEnv *, jobject, jstring, jobject, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    unsubscribe
 * Signature: (Lzcm/zcm/ZCM;Lzcm/zcm/ZCM/Subscription;)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_unsubscribe
  (JNIEnv *, jobject, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    flush
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_flush
  (JNIEnv *, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    pause
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMJNI_pause
  (JNIEnv *, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    resume
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMJNI_resume
  (JNIEnv *, jobject);

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    handle
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_handle
  (JNIEnv *, jobject, jint);

#ifdef __cplusplus
}
#endif
#endif
