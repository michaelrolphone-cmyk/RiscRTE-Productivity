"""Original synthetic books: no downloaded or copyrighted book fixtures."""
from pathlib import Path
import zipfile


def create(root):
    books = Path(root) / 'Books'
    books.mkdir(parents=True)
    paragraph = 'The reader preserves complete paragraphs and every word across page turns. Mixed punctuation — café, naïve and “quotes” — should remain readable. '
    (books / 'sample.txt').write_text((paragraph * 8 + '\n\n') * 32)
    (books / 'sample.md').write_text('# Portable Reader\n\n' + ('## Chapter\n\n' + paragraph * 5 + '\n\n') * 32)
    with zipfile.ZipFile(books / 'sample.epub', 'w', zipfile.ZIP_DEFLATED) as book:
        book.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
        book.writestr('META-INF/container.xml', '<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        book.writestr('OPS/content.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="3.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Portable Reader</dc:title><dc:creator>Test Author</dc:creator><dc:language>en</dc:language></metadata><manifest><item id="c1" href="text/chapter.xhtml" media-type="application/xhtml+xml"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/></manifest><spine><itemref idref="c1"/></spine></package>')
        book.writestr('OPS/text/chapter.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><h1 id="start">Portable Reader</h1>' + ('<p>' + paragraph * 5 + '</p>') * 32 + '</body></html>')
        book.writestr('OPS/nav.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol><li><a href="text/chapter.xhtml#start">Chapter</a></li></ol></nav></body></html>')

    # EPUB2 NCX lives below the OPF directory; its href must resolve from there.
    with zipfile.ZipFile(books / 'sample.epub') as original, zipfile.ZipFile(books / 'sample-ncx.epub', 'w', zipfile.ZIP_DEFLATED) as book:
        for name in original.namelist():
            if name == 'OPS/nav.xhtml':
                continue
            data = original.read(name)
            if name == 'OPS/content.opf':
                data = data.decode().replace('version="3.0"', 'version="2.0"').replace('<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>', '<item id="ncx" href="navigation/book.ncx" media-type="application/x-dtbncx+xml"/>').replace('<spine>', '<spine toc="ncx">').encode()
            book.writestr(name, data)
        book.writestr('OPS/navigation/book.ncx', '<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><navMap><navPoint id="chapter" playOrder="1"><navLabel><text>Nested NCX chapter</text></navLabel><content src="../text/chapter.xhtml#start"/></navPoint></navMap></ncx>')
