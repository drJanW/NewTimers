//TODO

interface between all modules
will randomize audio and lightshow between boundaries

//input: 
1. calender.csv (once/day, at startup)
2. PseudoRTC - clock, after init thru fetch from internet, updated by SWtimer, stored in Globals
3. webinterface - voting for audio, adjusting light intensity, audio-volume
4. sensors: todo-will wake-up measurements
5. audio - status (isPlaying, audioLevel)
6. lightshow - status (isShowing, intensity)

//output (using swTimers):
1. bounderized random lightshow
2a.bounderized random audio-fragment
2b. audio-speak (saytime, sayDate, calender-events, error, log)


