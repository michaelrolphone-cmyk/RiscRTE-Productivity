#!/usr/bin/env python3
"""Make 1:1 sheets from supplied design pixels and actual production frames."""
import argparse,hashlib,json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference-dir',type=Path,required=True)
    p.add_argument('--frames-dir',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,required=True)
    a=p.parse_args();a.output_dir.mkdir(parents=True,exist_ok=True)
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',16)
    receipt=[]
    screens=[('Points list','4.png','reference-points.png','Hours below 10 are unpadded; symbols belong to types.'),('Edit Point','5.png','reference-edit.png','X4 reports visual alerts. Scroll down for explicit Cancel / Save.'),('Set Time','6.png','reference-time.png','Hour and minute controls preserve one-minute precision.'),('Custom type keyboard','7.png','reference-keyboard.png','The accepted model retains 31-character names.')]
    for title,ref_name,actual_name,note in screens:
        reference=a.reference_dir/ref_name;actual=a.frames_dir/actual_name
        left,right=Image.open(reference).convert('RGB'),Image.open(actual).convert('RGB')
        if left.size!=(480,800) or right.size!=(480,800):raise ValueError('Expected exact 480x800 source pixels')
        sheet=Image.new('RGB',(1008,892),'#e6e6e6');draw=ImageDraw.Draw(sheet)
        draw.text((16,10),title,font=font,fill='black');draw.text((16,38),'SUPPLIED REFERENCE',font=font,fill='black');draw.text((512,38),'ACTUAL PRODUCTION RENDERER',font=font,fill='black')
        sheet.paste(left,(16,66));sheet.paste(right,(512,66));draw.text((16,869),note,font=font,fill='black')
        output=a.output_dir/(actual.stem+'-comparison.png');sheet.save(output)
        receipt.append({'screen':title,'reference':str(reference.resolve()),'reference_sha256':sha(reference),'actual':str(actual.resolve()),'actual_sha256':sha(actual),'comparison_sha256':sha(output),'pixels':'Unscaled 480x800, reference antialiased SVG raster and actual 1-bit rotated X4 frame','intentional_difference':note})
    (a.output_dir/'comparison-evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Four unscaled reference/production comparison sheets written')
if __name__=='__main__':main()
