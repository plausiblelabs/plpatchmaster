PLPatchMaster
-----------

PLPatchMaster provides an easy-to-use block-based swizzling API, using the block trampoline
library provided by PLBlockIMP and a set of custom assembly trampolines for ARMv7, ARMv7s,
ARM64, and x86-64.

The implementation can apply patches to currently loading classes, as well as be used
to register patches that will be applied when a class becomes available.

Use it at your own risk; swizzling in production software is rarely, if ever, a particularly
good idea.

PLPatchMaster is released under the MIT license.

Basic Use
-----------

Use a block to swizzle -[UIWindow sendEvent:]:

    [UIWindow pl_patchInstanceSelector: @selector(sendEvent:) withReplacementBlock: ^(PLPatchIMP *patch, UIEvent *event) {
        NSObject *obj = PLPatchGetSelf(patch);
        
        // Ignore 'remote control' events
        if (event.type == UIEventTypeRemoteControl)
            return;

        // Forward everything else
        return PLPatchIMPFoward(patch, void (*)(id, SEL, UIEvent *));
    }];

Advanced Use
------------

Register a future patch on a class that has not been loaded yet (eg, it's loaded dynamically at runtime):

    [[PLPatchMaster master] patchFutureClassWithName: @"ExFATCameraDeviceManager" withReplacementBlock: ^(PLPatchIMP *patch, id *arg) {
        /* Forward the message to the next IMP */
        PLPatchIMPFoward(patch, void (*)(id, SEL, id *));
        
        /* Log the event */
        NSLog(@"FAT camera device ejected");
    }];

PLPatchMaster registers a listener for dyld image events, and will automatically swizzle the target class when
its Mach-O image is loaded.