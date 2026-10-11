"""Check the target compiler frames implicated in the X4 loopTask overflow.

These per-function bounds complement the real app/book tests and decoded device
backtrace. They are not a claim that all possible call paths fit a fixed stack.
"""
from pathlib import Path
LIMITS={'Engine::open(':512,'Engine::toggleBookmark(':512,
        'Engine::removeBookmark(':512,'Engine::renameBookmark(':512,'app_main(':1536}
def check(objects):
    rows=[]
    for path in Path(objects).glob('*.su'):
        for line in path.read_text().splitlines():
            name,size,kind=line.rsplit('\t',2)
            rows.append({'function':name.split(':',3)[-1],'bytes':int(size),'kind':kind})
    result={}
    for pattern,limit in LIMITS.items():
        matches=[r for r in rows if pattern in r['function']]
        if len(matches)!=1:raise ValueError('Missing/ambiguous target stack report: '+pattern)
        r=matches[0]
        if r['bytes']>limit or r['kind'] not in ('static','dynamic,bounded'):
            raise ValueError('Target stack budget exceeded: '+str(r))
        result[pattern]={**r,'limit_bytes':limit}
    return result
