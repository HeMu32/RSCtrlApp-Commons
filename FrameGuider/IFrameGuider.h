// Heavy header.
// Frame guider is some modules that controls the PTZ camera by frame input, 
// aka. controls the composition of the picture, like a "smart operator" for the PTZ camera. 
// Inherient from IFrameRecv.
// Should holds a reference to a IGimbalDev.
// In: UniAVFrames. Out: PTZ camera control signals.