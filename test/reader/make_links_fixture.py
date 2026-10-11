"""Small original EPUB exercising touch links and nontrivial URI resolution."""
from pathlib import Path
import zipfile

def create(root):
    books=Path(root)/'Books';books.mkdir(parents=True,exist_ok=True)
    text='<p>'+'The source document keeps a separate path and a stable reading position. '*20+'</p>'
    def html(s):return '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Links</title></head><body>'+s+'</body></html>'
    with zipfile.ZipFile(books/'links.epub','w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        paths=['Text/one/chapter.xhtml','Text/unused/chapter.xhtml','Text/two/chapter.xhtml','Text/two/part%23two.xhtml']
        manifest=''.join(f'<item id="c{i}" href="{p}" media-type="application/xhtml+xml"/>' for i,p in enumerate(paths))
        spine=''.join(f'<itemref idref="c{i}"/>' for i in range(4))
        z.writestr('OPS/content.opf','<package xmlns="http://www.idpf.org/2007/opf" version="3.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Navigation Test</dc:title><dc:creator>Test Author</dc:creator><dc:language>en</dc:language></metadata><manifest>'+manifest+'<item id="nav" href="nav.xhtml" properties="nav" media-type="application/xhtml+xml"/></manifest><spine>'+spine+'</spine></package>')
        links=[('../two/chapter.xhtml#target%20spot','TARGET CHAPTER'),('#late','LOCAL END'),('../two/part%23two.xhtml#literal%2520id','ENCODED PATH'),('missing.xhtml#bad','MISSING CHAPTER'),('../two/chapter.xhtml#absent','MISSING ANCHOR')]
        z.writestr('OPS/Text/one/chapter.xhtml',html('<h1 id="contents">Source</h1>'+''.join(f'<p><a href="{h}">{t}</a></p>' for h,t in links)+text*20+'<h2 id="late">LOCAL DESTINATION</h2>'+text*20))
        z.writestr('OPS/Text/unused/chapter.xhtml',html('<h1>Wrong directory</h1>'+text*20))
        z.writestr('OPS/Text/two/chapter.xhtml',html('<h1 id="target spot">Correct destination</h1><p><a href="../one/chapter.xhtml#contents">RETURN TO CONTENTS</a></p>'+text*40))
        z.writestr('OPS/Text/two/part#two.xhtml',html('<h1 id="literal%20id">Escaped destination</h1>'+text*40))
        targets=[('Text/one/chapter.xhtml#contents','SOURCE'),('Text/unused/chapter.xhtml','DECOY'),('Text/two/chapter.xhtml#target%20spot','DESTINATION'),('Text/one/chapter.xhtml#late','LATE'),('Text/two/part%23two.xhtml#literal%2520id','ENCODED')]
        nav=''.join(f'<li><a href="{h}">{t}</a></li>' for h,t in targets)
        z.writestr('OPS/nav.xhtml','<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol>'+nav+'</ol></nav></body></html>')
