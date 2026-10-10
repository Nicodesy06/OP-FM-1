import sys, struct, zlib, lzma, tarfile, io, hashlib, os, json
src, outdir = sys.argv[1], sys.argv[2]
d = open(src,'rb').read()
crc_stored = struct.unpack('<I', d[:4])[0]
crc_calc = zlib.crc32(d[4:]) & 0xffffffff
print(f"file={src} size={len(d)} crc_stored=0x{crc_stored:08x} crc_calc(payload)=0x{crc_calc:08x} match={crc_stored==crc_calc}")
hdr = d[4:4+13]
props, dict_size, usize = hdr[0], struct.unpack('<I',hdr[1:5])[0], struct.unpack('<Q',hdr[5:13])[0]
print(f"lzma props=0x{props:02x} dict={dict_size} uncompressed_size={usize if usize!=0xffffffffffffffff else 'unknown'}")
raw = lzma.decompress(d[4:], format=lzma.FORMAT_ALONE)
print(f"tar size={len(raw)} sha256={hashlib.sha256(raw).hexdigest()}")
tf = tarfile.open(fileobj=io.BytesIO(raw))
inv=[]
for m in tf.getmembers():
    if m.isfile():
        b = tf.extractfile(m).read()
        p = os.path.join(outdir, m.name.lstrip('./'))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p,'wb').write(b)
        inv.append(dict(name=m.name, size=m.size, mtime=m.mtime, sha256=hashlib.sha256(b).hexdigest()))
json.dump(inv, open(os.path.join(outdir,'..', os.path.basename(outdir)+'.inventory.json'),'w'), indent=1)
print("files", len(inv), "total", sum(i['size'] for i in inv))
