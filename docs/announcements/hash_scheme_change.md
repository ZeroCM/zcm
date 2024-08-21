<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../../README.md)
#Announcement:

Hi All,

Zcm has gone through a relatively minor change, but one that you should be aware of.

Zcm employs a hashing scheme to help with the decoding of messages. This is meant to ensure that you don't accidentally try to decode a message that has a structural makeup different than the makeup you're expecting.

The old hashing scheme took into account all variable types and variable names in your zcmtype. For example, if you had a zcm type that looked like the following:

    struct example_t
    {
        int64_t utime;
        float q[4];
        int8_t flags;
        const int8_t FLAGS_VALID = 0x01;
    };

and you changed the `q` field to be an array of 3 floats, instead of 4, we wanted to be able to catch that in the decoding stage, and warn the user that there was a type mismatch. </p>

Here's how the old system worked.

We hashed (in order) all of the types of all of the variables **as well as their names** (we did not include const variables in the creation of the hash). This means that if you changed the name of a variable, the hash for that type changed. The system was intentionally designed that way so you could have 10 different types all with individually unique hashes, even if their only member was 1 float (as long as each type had a different name for that member). For example, we could have had the following two zcmtypes used in one system without chance of accidentally decoding one for the other.

    struct system_flags_t
    {
        int8_t system_flags;
        const int8_t FLAGS_SYSTEM_ON = 0x01;
    };

    struct vehicle_flags_t
    {
        int8_t vehicle_flags;
        const int8_t FLAGS_VEHICLE_DRIVING = 0x01;
    };

The problem with this though, is that a user might want to change the name of a variable without invalidating all of their old logs. The assumption here is that a change in variable name doesn't necessarily mean that the underlying data is different. So we decided to give the user the option to choose which hashing scheme is desired.

ZCM can create the type hashes using any of the following information. Some of these are hard coded to always be used because it has been deemed unsafe to ignore them. Others are optional and may be configured by the user at configure-time.

Always On Hash Scheme:

-  member types (in order)

Configureable Hash Scheme:

- zcmtype name (default is ON)
- member names (in order) (default is OFF)

Previously, the default values for the two configurable hash schemes were inverted. However, we've recently decided that using the new scheme is more stable and intuitive. In any case, both of these options may be configured during your `./waf configure` call:

option: `--hash-typename={true,false}`
set to true to have zcmtype hashing use the typename in the computed has

option: `--hash-member-names={true,false}`
set to true to have zcmtype hashing use the member variable names (in order) in the computed has

If you want to remain compatible with the zcmtype hashs in your previous logs, configure with
`--hash-typename=false --hash-member-names=true`
If you want to remain compatible with the way that lcm hashes are created, configure with
`--hash-typename=false --hash-member-names=false`
However, we believe that it will benefit you to switch to the new default scheme
`--use-typename=true --use-member-names=false`

A utility to run through zcm logs and update old hashes to the new hashes has been made, but it is still in it's early stages. If you are interested in switching to the new hash scheme but need to migrate old logs, let us know and we can make that utility a higher development priority.

As always, feel free to reach out on our [forums](https://groups.google.com/forum/#!forum/zcm-users) or on [Discord](https://discord.gg/T6jYM3eMjw) with any questions!
~The ZCM Team

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../../README.md)
