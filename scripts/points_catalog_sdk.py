"""Header staging for the explicitly selected storage-scaled Points build.

Only private presentation/SDK headers are staged. System implementations stay
in their owning repositories, and the legacy build never imports this module.
"""
from pathlib import Path

def stage_headers(out, adapter, presentation, utilities, runtime):
    stage=Path(out)/'sdk';inc=stage/'include';inc.mkdir(parents=True,exist_ok=True)
    headers={p.name:p for p in (adapter/'lib/PortableApps/include').glob('*.h')}
    for p in (runtime/'sdk/app').glob('*.h'):
        headers[p.name]=p
    for p in (utilities/'lib/Alarm/include').glob('*.h'):
        headers[p.name]=p
    for name in ('PaperPresentation.h','PaperFrame.h'):
        headers[name]=presentation/'Apps'/name
    for name,path in headers.items():
        link=inc/name
        if link.is_symlink():link.unlink()
        elif link.exists():raise ValueError('Refuse overwriting SDK file: '+str(link))
        if not path.is_file():raise ValueError('Missing selected header: '+str(path))
        link.symlink_to(path.resolve())
    # Native timezone headers resolve their frozen data relative to include/.
    for folder in ('time',):
        for p in (presentation/'lib/PortableApps'/folder).rglob('*'):
            if not p.is_file():continue
            link=stage/folder/p.relative_to(presentation/'lib/PortableApps'/folder)
            link.parent.mkdir(parents=True,exist_ok=True)
            if link.is_symlink():link.unlink()
            elif link.exists():raise ValueError('Refuse overwriting SDK data: '+str(link))
            link.symlink_to(p.resolve())
    return [inc,adapter/'lib/NativeApps/include',adapter/'Apps']
