#include "zcm_zcm_ZCMJNI.h"

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    publish
 * Signature: (Ljava/lang/String;[BII)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_publish
(JNIEnv *env, jobject self, jstring channel, jbyteArray data, jint offset, jint len)
{
    printf("JNI publish\n");
    return 0;
}

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    subscribe
 * Signature: (Ljava/lang/String;Lzcm/zcm/ZCM;)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_subscribe
(JNIEnv *env, jobject self, jstring channel, jobject zcmObj)
{
    printf("JNI subscribe\n");
    return 0;
}
