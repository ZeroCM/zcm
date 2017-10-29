#include "uvCallback.h"

#include <uv.h>

using namespace std;

struct uvCallback
{
    void (*cb)(void* usr);
    void* usr;
	uv_barrier_t blocker;
    uv_async_t handle;
};

#ifdef __cplusplus
extern "C" {
#endif

uvCallback* uvCallbackCreate()
{
    uvCallback* ret = new uvCallback();
    ret->cb = nullptr;
    ret->usr = nullptr;

    // Set the usr pointer of the async handler to be the "this" pointer
	ret->handle.data = ret;

    // Initialize the handle object with the main julia loop (uv_default_loop()),
    // and our callback.
    uv_async_init(uv_default_loop(), &ret->handle, [](uv_async_t *handle) {
		uvCallback* uvCb = (uvCallback*) handle->data;
        // Call the callback
		uvCb->cb(uvCb->usr);
        // wait on the barrier (should instantly wake up)
        if (uv_barrier_wait(&uvCb->blocker) > 0)
            uv_barrier_destroy(&uvCb->blocker);
    });

    return ret;
}

void uvCallbackTrigger(uvCallback* uvCb, void (*cb)(void* usr), void* usr)
{
    // Init the barrier to wait for 2 people to be waiting on it before continuing
	uv_barrier_init(&uvCb->blocker, 2);

    // Store the goods for the callback
    uvCb->cb = cb;
    uvCb->usr = usr;

    // Schedule the callback to be called
    uv_async_send(&uvCb->handle);

    // Wait for callback to complete
    if (uv_barrier_wait(&uvCb->blocker) > 0)
        uv_barrier_destroy(&uvCb->blocker);

}

void uvCallbackDestroy(uvCallback* uvCb)
{
    // close async handle (probably would do this on a zcm_destroy() instead
    //                     of on each message)
    uv_close((uv_handle_t*)&uvCb->handle, NULL);

    delete uvCb;
}

#ifdef __cplusplus
}
#endif
