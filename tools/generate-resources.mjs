// SPDX-License-Identifier: GPL-3.0-only
// Deterministic Windows resources for the native command-line build.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const word=n=>{const b=Buffer.alloc(2);b.writeUInt16LE(n);return b;};
const dword=n=>{const b=Buffer.alloc(4);b.writeUInt32LE(n>>>0);return b;};
const pad=b=>Buffer.concat([b,Buffer.alloc((4-b.length%4)%4)]);
const wide=s=>Buffer.from(s+'\0','utf16le');
const size=64, pixels=Buffer.alloc(size*size*4), mask=Buffer.alloc(size*size/8);
for(let y=0;y<size;y++) for(let x=0;x<size;x++) {
  const dx=x-31.5,dy=y-31.5,r=Math.hypot(dx,dy),a=Math.atan2(dy,dx);
  const filled=r<=13 || (r>=20&&r<=28&&Math.abs(Math.sin(a*4))<0.22);
  const row=size-1-y;
  if(filled) pixels.writeUInt32LE(0xffffb340,row*size*4+x*4);
  else mask[row*8+(x>>3)]|=0x80>>(x&7);
}
const dibHeader=Buffer.alloc(40);dibHeader.writeUInt32LE(40);dibHeader.writeInt32LE(size,4);dibHeader.writeInt32LE(size*2,8);dibHeader.writeUInt16LE(1,12);dibHeader.writeUInt16LE(32,14);dibHeader.writeUInt32LE(pixels.length+mask.length,20);
const dib=Buffer.concat([dibHeader,pixels,mask]);
const icoHeader=Buffer.concat([word(0),word(1),word(1)]);
const icoEntry=Buffer.concat([Buffer.from([size,size,0,0]),word(1),word(32),dword(dib.length),dword(22)]);
fs.mkdirSync(path.join(root,'assets'),{recursive:true});
fs.mkdirSync(path.join(root,'build'),{recursive:true});
fs.writeFileSync(path.join(root,'assets/icon.ico'),Buffer.concat([icoHeader,icoEntry,dib]));
function block(key,type,valueLength,value=Buffer.alloc(0),children=[]) {
  const head=pad(Buffer.concat([word(0),word(valueLength),word(type),wide(key)]));
  const content=Buffer.concat([head,pad(value),...children.map(pad)]);content.writeUInt16LE(content.length,0);return content;
}
const fixed=Buffer.concat([0xfeef04bd,0x10000,0x10002,0x10000,0x10002,0x10000,0x3f,0,0x40004,1,0,0,0].map(dword));
const strings={FileDescription:'Cursor-targeted monitor brightness',FileVersion:'1.2.1',InternalName:'CursorBrightness',OriginalFilename:'CursorBrightness.exe',ProductName:'Cursor Brightness',ProductVersion:'1.2.1',LegalCopyright:'GPL-3.0-only; see LICENSE and THIRD_PARTY_NOTICES.md'};
const stringBlocks=Object.entries(strings).map(([key,value])=>block(key,1,value.length+1,wide(value)));
const version=block('VS_VERSION_INFO',0,fixed.length,fixed,[block('StringFileInfo',1,0,undefined,[block('040904b0',1,0,undefined,stringBlocks)]),block('VarFileInfo',1,0,undefined,[block('Translation',0,4,Buffer.concat([word(0x409),word(1200)]))])]);
function resource(type,id,data) {
  const header=Buffer.concat([dword(data.length),dword(32),word(0xffff),word(type),word(0xffff),word(id),dword(0),word(0x1030),word(0x409),dword(0),dword(0)]);
  return Buffer.concat([header,pad(data)]);
}
const nullResource=Buffer.concat([dword(0),dword(32),word(0xffff),word(0),word(0xffff),word(0),Buffer.alloc(16)]);
const group=Buffer.concat([icoHeader,Buffer.from([size,size,0,0]),word(1),word(32),dword(dib.length),word(1)]);
fs.writeFileSync(path.join(root,'build/app.res'),Buffer.concat([nullResource,resource(3,1,dib),resource(14,101,group),resource(16,1,version),resource(24,1,fs.readFileSync(path.join(root,'app/app.manifest')))]));
console.log('Generated icon and Windows resources');
