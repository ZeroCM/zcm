<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../../README.md)
#Announcement:

Hi All,

Quick update on a recent change to the ZCM transport API. The semantics of using the `zcm_trans_recvmsg_enable` call are changing. Note: many transports will not be affected by this change because they do not make use of this call, but if you have written one that does, read on.

Previously, passing NULL as the channel argument to `zcm_trans_recvmsg_enable` would hint to the transport that it should receive data on all channels. This was used as a proxy for passing regex subscriptions down to the transport layer. Unfortunately, it had the side effect of enabling significantly more data traffic than strictly necessary if the regex subscription in question was anything other than `".*"`.

Now, `zcm_trans_recvmsg_enable` will never be called with NULL as the argument. Instead, it should be able to accept a `channel` argument that potentially contains a regex string. Note that because this function only acts as a "hint" to the underlying transport, retaining previous performance behavior would be as simple as checking for any special characters in the input channel and then treating it the same as if the channel had been NULL previously. However, now transports have the ability to handle the various regex channels in a more intelligent way to trim down on the traffic they are handling (if applicable for that transport).

It is also worth noting that the zcm blocking / nonblocking layer actually already handles some of the bookkeeping for you. Specifically, the zcm layer will *only* call `zcm_trans_recvmsg_enable(channel, false)` if :

- channel *is not* a regex string *and* there are no more explicit subscriptions on that channel
- channel *is* a regex string *and* there are no more identical regex subscriptions

This means there are effectively 2 more conditions that the transport itself needs to handle (again, only if they choose to in order to be more precise). Those cases are :

- An explicit subscription channel is disabled, but messages on that channel still match a previously enabled regex. In this case, messages should still be received on that channel; however, the transport can now know that there are no remaining explicit subscriptions to it.
- "Overlapping" regex subscriptions where more than 1 regex matches a single channel. Even if one of those regex is disabled, messages that match any enabled regex should still be received.

As always, feel free to reach out on our [forums](https://groups.google.com/forum/#!forum/zcm-users) or on [Discord](https://discord.gg/T6jYM3eMjw) with any questions!
~The ZCM Team

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../../README.md)
