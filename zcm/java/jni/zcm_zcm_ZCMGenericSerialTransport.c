#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#include "zcm/java/jni/zcm_zcm_ZCMGenericSerialTransport.h"
#include "zcm/transport/generic_serial_transport.h"
#include "zcm/zcm.h"

typedef struct JavaSerialTransport JavaSerialTransport;
struct JavaSerialTransport {
    JavaVM *jvm;
    jobject javaObj;  // Global reference to the Java object
    zcm_trans_t *transport;

    // Method IDs for callbacks
    jmethodID getNativeGetMethodID;
    jmethodID getNativePutMethodID;
};

// Helper function to get native pointer field
static jfieldID getNativePtrField(JNIEnv *env, jobject self)
{
    jclass cls = (*env)->GetObjectClass(env, self);
    return (*env)->GetFieldID(env, cls, "nativePtr", "J");
}

// Helper function to get native pointer
static JavaSerialTransport *getNativePtr(JNIEnv *env, jobject self)
{
    jfieldID fld = getNativePtrField(env, self);
    jlong nativePtr = (*env)->GetLongField(env, self, fld);
    return (JavaSerialTransport*)(intptr_t)nativePtr;
}

// Helper function to set native pointer
static void setNativePtr(JNIEnv *env, jobject self, JavaSerialTransport *transport)
{
    jfieldID fld = getNativePtrField(env, self);
    (*env)->SetLongField(env, self, fld, (jlong)(intptr_t)transport);
}

// Timestamp function - uses system time
static uint64_t getSystemTimestamp(void *usr)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}

// Get function callback - calls Java nativeGet method
static size_t javaGetCallback(uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr)
{
    JavaSerialTransport *jst = (JavaSerialTransport*)usr;

    JNIEnv *env = NULL;
    bool isAttached = false;

    // Get JNI environment
    int rc = (*jst->jvm)->GetEnv(jst->jvm, (void**)&env, JNI_VERSION_1_6);
    if (rc == JNI_EDETACHED) {
        if ((*jst->jvm)->AttachCurrentThread(jst->jvm, (void**)&env, NULL) != 0) {
            fprintf(stderr, "ZCMGenericSerialTransport: Failed to attach thread for get callback\n");
            return 0;
        }
        isAttached = true;
    } else if (rc != JNI_OK) {
        fprintf(stderr, "ZCMGenericSerialTransport: Failed to get JNI environment for get callback\n");
        return 0;
    }

    // Create direct byte buffer wrapping the native array
    jobject directBuffer = (*env)->NewDirectByteBuffer(env, data, nData);
    if (directBuffer == NULL) {
        if (isAttached) (*jst->jvm)->DetachCurrentThread(jst->jvm);
        return 0;
    }

    // Call Java nativeGet method
    jint bytesRead =
        (*env)->CallIntMethod(env, jst->javaObj, jst->getNativeGetMethodID,
                              directBuffer, (jint)nData, (jint)timeoutMs);

    // Validate return value
    if (bytesRead < 0 || bytesRead > nData) {
        bytesRead = 0;
    }

    // Clean up
    (*env)->DeleteLocalRef(env, directBuffer);

    if (isAttached) {
        (*jst->jvm)->DetachCurrentThread(jst->jvm);
    }

    return (size_t)bytesRead;
}

// Put function callback - calls Java nativePut method
static size_t javaPutCallback(const uint8_t* data, size_t nData, uint32_t timeoutMs, void* usr)
{
    JavaSerialTransport *jst = (JavaSerialTransport*)usr;

    JNIEnv *env = NULL;
    bool isAttached = false;

    // Get JNI environment
    int rc = (*jst->jvm)->GetEnv(jst->jvm, (void**)&env, JNI_VERSION_1_6);
    if (rc == JNI_EDETACHED) {
        if ((*jst->jvm)->AttachCurrentThread(jst->jvm, (void**)&env, NULL) != 0) {
            fprintf(stderr, "ZCMGenericSerialTransport: Failed to attach thread for put callback\n");
            return 0;
        }
        isAttached = true;
    } else if (rc != JNI_OK) {
        fprintf(stderr, "ZCMGenericSerialTransport: Failed to get JNI environment for put callback\n");
        return 0;
    }

    // Create direct byte buffer wrapping the native array
    jobject directBuffer = (*env)->NewDirectByteBuffer(env, (void*)data, nData);
    if (directBuffer == NULL) {
        if (isAttached) (*jst->jvm)->DetachCurrentThread(jst->jvm);
        return 0;
    }

    // Call Java nativePut method
    jint bytesWritten =
        (*env)->CallIntMethod(env, jst->javaObj, jst->getNativePutMethodID,
                              directBuffer, (jint)nData, (jint)timeoutMs);

    // Check for exceptions
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        bytesWritten = 0;
    }

    // Ensure valid return value
    if (bytesWritten < 0 || bytesWritten > nData) {
        bytesWritten = 0;
    }

    // Clean up
    (*env)->DeleteLocalRef(env, directBuffer);

    if (isAttached) {
        (*jst->jvm)->DetachCurrentThread(jst->jvm);
    }

    return (size_t)bytesWritten;
}

/*
 * Class:     zcm_zcm_ZCMGenericSerialTransport
 * Method:    initializeNative
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_zcm_zcm_ZCMGenericSerialTransport_initializeNative
(JNIEnv *env, jobject self, jint mtu, jint bufSize)
{
    // Allocate and initialize our structure
    JavaSerialTransport *jst = calloc(1, sizeof(JavaSerialTransport));
    if (jst == NULL) {
        return JNI_FALSE;
    }

    // Get JavaVM
    int rc = (*env)->GetJavaVM(env, &jst->jvm);
    if (rc != 0) {
        free(jst);
        return JNI_FALSE;
    }

    // Create global reference to the Java object
    jst->javaObj = (*env)->NewGlobalRef(env, self);
    if (jst->javaObj == NULL) {
        free(jst);
        return JNI_FALSE;
    }

    // Get method IDs for the callback methods
    jclass cls = (*env)->GetObjectClass(env, self);
    jst->getNativeGetMethodID = (*env)->GetMethodID(env, cls, "nativeGet", "(Ljava/nio/ByteBuffer;II)I");
    jst->getNativePutMethodID = (*env)->GetMethodID(env, cls, "nativePut", "(Ljava/nio/ByteBuffer;II)I");

    if (jst->getNativeGetMethodID == NULL || jst->getNativePutMethodID == NULL) {
        (*env)->DeleteGlobalRef(env, jst->javaObj);
        free(jst);
        return JNI_FALSE;
    }

    // Create the C transport
    jst->transport = zcm_trans_generic_serial_blocking_create(
        javaGetCallback,        // get function
        javaPutCallback,        // put function
        jst,                   // user data for get/put
        getSystemTimestamp,    // timestamp function
        NULL,                  // user data for timestamp (not needed)
        (size_t)mtu,           // MTU
        (size_t)bufSize        // buffer size
    );

    if (jst->transport == NULL) {
        (*env)->DeleteGlobalRef(env, jst->javaObj);
        free(jst);
        return JNI_FALSE;
    }

    // Store the pointer in the Java object
    setNativePtr(env, self, jst);

    return JNI_TRUE;
}

/*
 * Class:     zcm_zcm_ZCMGenericSerialTransport
 * Method:    getTransportPtr
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_zcm_zcm_ZCMGenericSerialTransport_getTransportPtr
(JNIEnv *env, jobject self)
{
    JavaSerialTransport *jst = getNativePtr(env, self);
    if (jst == NULL || jst->transport == NULL) {
        return 0;
    }
    return (jlong)(intptr_t)jst->transport;
}

/*
 * Class:     zcm_zcm_ZCMGenericSerialTransport
 * Method:    destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMGenericSerialTransport_destroy
(JNIEnv *env, jobject self)
{
    JavaSerialTransport *jst = getNativePtr(env, self);
    if (jst == NULL) {
        return;
    }

    if (jst->transport != NULL) {
        zcm_trans_generic_serial_destroy(jst->transport);
        jst->transport = NULL;
    }
    // Release the global reference
    if (jst->javaObj != NULL) {
        (*env)->DeleteGlobalRef(env, jst->javaObj);
        jst->javaObj = NULL;
    }

    // Free our structure
    free(jst);

    // Clear the native pointer in the Java object
    setNativePtr(env, self, NULL);
}

/*
 * Class:     zcm_zcm_ZCMGenericSerialTransport
 * Method:    releaseNativeTransport
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_zcm_zcm_ZCMGenericSerialTransport_releaseNativeTransport
(JNIEnv *env, jobject self)
{
    JavaSerialTransport *jst = getNativePtr(env, self);
    if (jst == NULL) {
        return;
    }
    jst->transport = NULL;
}
