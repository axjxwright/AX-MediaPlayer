# AX-MediaPlayer
Media Playback Engine for Cinder

This is effectively a wrapper around `IMFMediaEngine` (meaning it's currently windows 8+ only) which allows for CPU side playback
of video and audio in Cinder. According to the documentation there appears to be a fast-path
that goes through DXGI but that has yet to be implemented. 

I've taken _reasonable_ care as it pertains to leaks but that is my first COM heavy library
so my first priority was to get it running and make sure it's leak-free later. Please report any
that you find, i've tried to use `ComPtr` et al judiciously so hopefully there's not too many.

I couldn't find any substantial information about thread safety when calling into the `IMFMediaEngine`,
but the audio related functions did _not_ like being called from the main thread, so there's a provided 
way to perform a lambda on the MTA thread which seems to make it happy. `RunSynchronousInMTAThread ( ... )`. 

There's no documentation, please just have a look at the provided sample to see the basic usage. It's all fairly straight forward
video-related stuff. You should be able to just check this out into your cinder install's block's folder and off you go.

This was just a one night project for fun so it comes with absolutely no warranty. Hopefully it still comes in handy for someone :)

- @axjxwright