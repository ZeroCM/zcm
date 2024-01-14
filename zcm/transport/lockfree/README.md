# RRR (Bendes): A couple things:
##              1) Lockfree isn't licensed in the original repo
##              2) Is it allowed to license a subdirectory of a licensed repo?
##              3) Since it's the same license, can't we just omit the license in this subdir?
##              4) Should we add lockfree and `transport_ipcshm` to `third_party` if you're worried about licensing? I kinda wanna keep zcm clean of other licensed code in the same repo
# RRR (xorvoid): We agree. I thought to add this notice to make it clear that it's licensed the same as ZCM.
#                Unlicensed generally means "author retains all the default copyright laws". LGPL and other copyleft licenses are sort of "relinquishing rights"
#                I thought this would make it clear, and prehaps also allow backporting any fixes. I think backporting fixes without being forced to LPGL elsewhere is the only real concern I have.
#                If you want to move this to third-party, I'm okay with that also. I wasn't sure how you did that generally.

# Lockfree

Upstream https://github.com/xorvoid/lockfree
Vendorized to avoid having to manage third-party source dependencies
This subset, used by ZCM has been relicensed to LGPL with permission.
