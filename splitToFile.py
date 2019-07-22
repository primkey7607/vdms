import os
import sys
import subprocess
import datetime
import time
import math
def genClips(fname, csize):
    
    #Use ffprobe to find the duration:
    #WARNING: currently, we just call ffprobe as is, without
    #adding a count_frames argument. That means ffprobe estimates
    #the duration instead of determining the exact duration.
    result = subprocess.Popen(['ffprobe', fname],
      stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
    dur = [x for x in result.stdout.readlines() if "Duration" in x]
    print(dur)
    inf = dur[0].split(',')
    print(inf)
    #assuming 0th element has Duration
    print(inf[0])
    dInfo = "".join(inf[0].split())
    print(dInfo)
    acdur = dInfo[len("Duration:"):]
    print(acdur)
    x = time.strptime(acdur.split('.')[0], '%H:%M:%S')
    fdur = datetime.timedelta(hours=x.tm_hour,minutes=x.tm_min,seconds=x.tm_sec).total_seconds()    
    print(fdur)
    idur = int(fdur)
    #Note: right now, I think this might cut off the last second of a video due
    #to loss of milliseconds. For now, I will not fix this, as it seems to be
    #a minor issue at this point
    nclips = math.ceil(float(idur) / float(csize))
    inclips = int(nclips)
    #ffmpeg string
    for i in range(inclips):
        st = i * csize 
        en = (i+1) * csize 
        cmdstr = "ffmpeg -i " + fname + " -ss " + str(st) + " -t " + str(en) + " -c copy " + "-flags +global_header " + "tmp" + str(i) + ".mp4"
        os.system(cmdstr)
    
genClips(sys.argv[1],2)


