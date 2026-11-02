"""Exact ELF loader-visible equivalence for non-runtime metadata compaction."""
import hashlib,struct

def sha(data):return hashlib.sha256(data).hexdigest()
def runtime_witness(data):
 if len(data)<52 or data[:7]!=b'\x7fELF\x01\x01\x01':raise ValueError('Expected ELF32 little endian')
 h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
 _,kind,machine,version,entry,phoff,shoff,flags,ehsize,phsize,phcount,shsize,shcount,names=h
 if machine!=94 or shsize!=40 or not shcount or names>=shcount:raise ValueError('Unsupported target ELF')
 if shoff+shcount*shsize>len(data) or phoff+phcount*phsize>len(data):raise ValueError('Truncated ELF tables')
 sections=[struct.unpack_from('<IIIIIIIIII',data,shoff+i*shsize) for i in range(shcount)]
 def payload(s):
  if s[1]==8:return b''
  if s[4]+s[5]>len(data):raise ValueError('Truncated section')
  return data[s[4]:s[4]+s[5]]
 strings=payload(sections[names])
 def name(s):
  start=s[0];end=strings.find(b'\0',start)
  if start>=len(strings) or end<0:raise ValueError('Invalid section name')
  return strings[start:end].decode('ascii')
 labels=[name(s) for s in sections];result={}
 for s,label in zip(sections,labels):
  _,typ,flg,addr,off,size,link,info,align,entsize=s
  if not (flg&2 or typ in (4,9)):continue
  if label in result or link>=shcount:raise ValueError('Ambiguous runtime section')
  result[label]={'type':typ,'flags':flg,'address':addr,'size':size,'alignment':align,'entry_size':entsize,
    'link':labels[link] if link else '',
    'relocation_target':labels[info] if typ in (4,9) and info else '',
    'sha256':sha(payload(s))}
 return {'kind':kind,'machine':machine,'version':version,'entry':entry,'flags':flags,
  'program_headers_sha256':sha(data[phoff:phoff+phcount*phsize]),'program_header_count':phcount,
  'sections':result}
def require_runtime_equal(original,candidate):
 before=runtime_witness(original);after=runtime_witness(candidate)
 if before!=after:raise ValueError('Non-runtime metadata operation changed loader-visible ELF semantics')
 return before
