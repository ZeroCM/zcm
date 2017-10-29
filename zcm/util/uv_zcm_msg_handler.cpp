#include "uv_zcm_msg_handler.h"

#include <iostream>

#include <uv.h>

using namespace std;

struct uv_zcm_msg_handler_t
{
    zcm_msg_handler_t cb;
    void* usr;
    const zcm_recv_buf_t* rbuf;
    const char* channel;

	uv_barrier_t blocker;
    uv_async_t handle;
};

#ifdef __cplusplus
extern "C" {
#endif

uv_zcm_msg_handler_t* uv_zcm_msg_handler_create(zcm_msg_handler_t cb, void* usr)
{
    uv_zcm_msg_handler_t* ret = new uv_zcm_msg_handler_t();
    ret->cb = cb;
    ret->usr = usr;
    ret->rbuf = nullptr;
    ret->channel = nullptr;

    // Set the usr pointer of the async handler to be the "this" pointer
	ret->handle.data = ret;

    // Initialize the handle object with the main julia loop (uv_default_loop()),
    // and our callback.
    cout << "init: " << uv_async_init(uv_default_loop(), &ret->handle, [](uv_async_t *handle) {
        cout << "cb running" << endl;
		uv_zcm_msg_handler_t* uvCb = (uv_zcm_msg_handler_t*) handle->data;
        // Call the callback
		uvCb->cb(uvCb->rbuf, uvCb->channel, uvCb->usr);
        cout << "cb complete" << endl;
        // wait on the barrier (should instantly wake up)
        if (uv_barrier_wait(&uvCb->blocker) > 0)
            uv_barrier_destroy(&uvCb->blocker);
    }) << endl;

    return ret;
}

void uv_zcm_msg_handler_trigger(const zcm_recv_buf_t* rbuf, const char* channel, void* _uvCb)
{
    uv_zcm_msg_handler_t* uvCb = (uv_zcm_msg_handler_t*) _uvCb;

    uvCb->rbuf = rbuf;
    uvCb->channel = channel;

    // Init the barrier to wait for 2 people to be waiting on it before continuing
	uv_barrier_init(&uvCb->blocker, 2);

    // Schedule the callback to be called
    cout << "send: " << uv_async_send(&uvCb->handle) << endl;
    cout << "pending: " << uvCb->handle.pending << endl;

    cout << "waiting for cb to complete" << endl;
    // Wait for callback to complete
    if (uv_barrier_wait(&uvCb->blocker) > 0)
        uv_barrier_destroy(&uvCb->blocker);

}

void uv_zcm_msg_handler_destroy(uv_zcm_msg_handler_t* uvCb)
{
    // close async handle (probably would do this on a zcm_destroy() instead
    //                     of on each message)
    uv_close((uv_handle_t*)&uvCb->handle, NULL);

    delete uvCb;
}

#ifdef __cplusplus
}
#endif
