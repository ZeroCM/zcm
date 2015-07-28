#include "zcm/java/jni/zcm_zcm_ZCMJNI.h"
#include "zcm/zcm.h"
#include <assert.h>

typedef struct Internal Internal;
struct Internal
{
    JavaVM *jvm;
    jobject self;
    zcm_t *zcm;
};

// J is the type signature for long
static jfieldID getNativePtrField(JNIEnv *env, jobject self)
{
    jclass cls = (*env)->GetObjectClass(env, self);
    return (*env)->GetFieldID(env, cls, "nativePtr", "J");
}

static Internal *getNativePtr(JNIEnv *env, jobject self)
{
    jfieldID fld = getNativePtrField(env, self);
    jlong nativePtr = (*env)->GetLongField(env, self, fld);
    return (Internal*) nativePtr;
}

static void setNativePtr(JNIEnv *env, jobject self, Internal *zcm)
{
    jfieldID fld = getNativePtrField(env, self);
    (*env)->SetLongField(env, self, fld, (jlong)zcm);
}

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    initializeNative
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_zcm_zcm_ZCMJNI_initializeNative
(JNIEnv *env, jobject self, jstring urlJ)
{
    Internal *I = calloc(1, sizeof(Internal));
    int rc = (*env)->GetJavaVM(env, &I->jvm);
    assert(rc == 0);

    const char *url = (*env)->GetStringUTFChars(env, urlJ, 0);
    I->zcm = zcm_create(url);
    (*env)->ReleaseStringUTFChars(env, urlJ, url);

    setNativePtr(env, self, I);

    return I->zcm ? 1 : 0;
}

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    publish
 * Signature: (Ljava/lang/String;[BII)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_publish
(JNIEnv *env, jobject self, jstring channelJ, jbyteArray dataJ, jint offsetJ, jint lenJ)
{
    Internal *I = getNativePtr(env, self);
    assert(I);

    const char *channel = (*env)->GetStringUTFChars(env, channelJ, 0);

    jboolean isCopy;
    jbyte* data = (*env)->GetByteArrayElements(env, dataJ,&isCopy);

    assert(offsetJ == 0);
    int ret = zcm_publish(I->zcm, channel, (char*)data, lenJ);

    (*env)->ReleaseStringUTFChars(env, channelJ, channel);
    return ret;
}

static JNIEnv *getEnv(JavaVM *vm)
{
    JNIEnv *env;

    int rc = (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);
    if (rc == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, (void **)&env, NULL) != 0) {
            fprintf(stderr, "ZCMJNI: getEnv: Failed to attach Thread in JNI!\n");
        }
    } else if (rc == JNI_EVERSION) {
        fprintf(stderr, "ZCMJNI: getEnv: JNI version not supported!\n");
    }

    return env;
}

static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    Internal *I = (Internal *)usr;
    jobject self = I->self;
    JNIEnv *env = getEnv(I->jvm);

    jstring channelJ = (*env)->NewStringUTF(env, channel);

    jbyteArray dataJ = (*env)->NewByteArray(env, rbuf->len);
    (*env)->SetByteArrayRegion(env, dataJ, 0, rbuf->len, (signed char*)rbuf->data);

    jint offsetJ = 0;
    jint lenJ = rbuf->len;

    jclass cls = (*env)->GetObjectClass(env, self);
    assert(cls);
    jmethodID receiveMessage = (*env)->GetMethodID(env, cls, "receiveMessage", "(Ljava/lang/String;[BII)V");
    assert(receiveMessage);

    (*env)->CallVoidMethod(env, self, receiveMessage,
                           channelJ, dataJ, offsetJ, lenJ);
}

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    subscribe
 * Signature: (Ljava/lang/String;Lzcm/zcm/ZCM;)I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_subscribe
(JNIEnv *env, jobject self, jstring channelJ, jobject zcmObjJ)
{
    Internal *I = getNativePtr(env, self);
    assert(I);
    I->self = (*env)->NewGlobalRef(env, zcmObjJ);

    const char *channel = (*env)->GetStringUTFChars(env, channelJ, 0);

    int ret = zcm_subscribe(I->zcm, channel, handler, (void*)I);

    (*env)->ReleaseStringUTFChars(env, channelJ, channel);
    return ret;
}

/*
 * Class:     zcm_zcm_ZCMJNI
 * Method:    handle
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_zcm_zcm_ZCMJNI_handle
(JNIEnv *env, jobject self)
{
    Internal *I = getNativePtr(env, self);
    assert(I);
    int rc = zcm_handle(I->zcm);
    return rc;
}
