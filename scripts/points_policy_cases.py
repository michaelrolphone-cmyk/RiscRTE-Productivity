"""Frozen production Watch Points workflow cases, plus policy boundaries."""
def tap(delay,x,y,gate='app',release=40):return [(delay,gate,'down',x,y),(release,gate,'up',x,y)]
def nav(delay,kind,gate='app'):return [(delay,gate,kind,0,0)]
def swipe(delay,x,y,to_y):return [(delay,'app','down',x,y),(40,'app','move',x,to_y),(40,'app','up',x,to_y)]
def scenarios():
 edit=tap(200,100,100);types=edit+tap(250,100,100)
 custom=types+swipe(250,120,195,85)+swipe(250,120,195,85)+tap(250,195,142)
 name=custom+tap(250,100,103)
 key=tap(250,47,97,'text');done=tap(250,194,188,'text');cancel=tap(250,28,20,'text')
 save=tap(250,180,220);back=nav(250,'back','text');home=tap(250,210,20,'text')
 schedule=edit+tap(250,100,142)+tap(250,60,180)+tap(250,180,180)+tap(250,120,220)+tap(250,100,182)+tap(250,60,180)+tap(250,180,180)+tap(250,120,220)+swipe(250,120,190,90)+tap(250,195,110)+tap(250,195,150)+tap(250,100,190)+tap(250,100,182)+swipe(250,120,190,90)+tap(250,55,172)+tap(250,185,194)
 result={'idle':[], 'schedule-save':schedule+tap(250,180,220), 'schedule-cancel':schedule+tap(250,60,220), 'delete-point':edit+swipe(250,120,190,85)*3+tap(250,100,178)*2, 'panel-wide':[], 'panel-mono':[], 'edit':edit, 'types':types, 'custom':custom,
 'save':name+key+done+save, 'done-draft':name+key+done,
 'cancel':name+key+cancel, 'navigation-back':name+key+back,
 'unavailable':name,'retained':name+key+cancel,
 'back-return':name+key+back+nav(250,'back')+nav(250,'back')+nav(250,'back')+nav(250,'back'),
 'repeat':name+key+done+tap(250,100,103)+key+cancel+tap(250,100,103)+key+done+save,
 'navigation-home':name+key+home,
 'save-delayed':name+key+done+save,
 'rapid-repeat':name+tap(10,47,97,'text',release=4)+tap(4,47,97,'text',release=4)+done+save,
 'color-save':name+key+done+tap(250,55,185)+save,
 'limit':name+key*20+done+save,
 'delete':name+tap(250,211,156,'text')+done+save,
 'empty':name+tap(250,211,156,'text')*5+done+save,
 'custom-back':name+key+done+tap(250,60,220),
 'quick':tap(250,200,15)+tap(250,30,20),
 }
 for boundary in ('acquire','open','poll','close','release','kv-get','kv-put','kv-release','acquire-fail'):
  result['loss-'+boundary]=name+key+done+save
 return result
