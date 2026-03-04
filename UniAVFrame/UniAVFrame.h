// Heavy header. 
// An impl. for managing Qt, OpenCV, ffmpeg, and BMD DeckLink SDK audio&video frames. 
// Manages original pixel fmt and offer single-step conversion, also a cache for data reuse for Qt and OpenCV, in compatible pixel fmt. 
// Manages a reference-based lifecycle management, thread-safe for frames.
// Manages pixel data and metadata, and timestamps. Timestamps recorded in a structure similar to BMD DeckLink SDK.
