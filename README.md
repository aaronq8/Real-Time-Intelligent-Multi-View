# Real-Time-Intelligent-Multi-View
## TODO :
1. simplify get_next_frame
2. no need latest_frames in render call..already copied into textures


              input0.mp4              input1.mp4              input2.mp4
                  |                       |                       |
          Demux + video/audio      Demux + video/audio      Demux + video/audio
                  |                       |                       |
          Video decoder thread     Video decoder thread     Video decoder thread
          Audio decoder thread     Audio decoder thread     Audio decoder thread
                  |                       |                       |
             video SPSC              video SPSC              video SPSC
             audio SPSC              audio SPSC              audio SPSC
                  \                       |                       /
                   \                      |                      /
                    -------- Timeline / compositor thread --------
                                      |
                             choose big stream
                                      |
                         SDL render 1 big + 2 small



Per input:


Main/Render thread:
    drains video frame queues into timestamped GPU frame buffers
    drains score queues into score histories
    t = playback clock
    picks video frames for t
    picks audio scores for t + offset
    chooses big stream with hysteresis
    launches CUDA compose kernel
    displays output texture

## Thread Pinning 
TODO 
## Frame Ownership 
decoder creates VideoFrame
VideoFrame uniquely owns AVFrame
queue passes shared_ptr<VideoFrame>
main/compositor holds shared_ptr<VideoFrame>
when last shared_ptr dies:
    VideoFrame dies
    AVFrame is freed
