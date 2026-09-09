# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 SamGCoder
"""Prepare a user-supplied QuantMotion shader for private native execution.

The shader implementation is never vendored or relicensed by this script.
Generated files and their original licence stay in ignored runtime storage.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
args = parser.parse_args()
sources = list(args.package.rglob('lumenite_QuantMotion.fx'))
if len(sources) != 1:
    raise SystemExit('Expected exactly one lumenite_QuantMotion.fx in supplied package')
source = sources[0]
text = source.read_text(encoding='utf-8-sig')
textures = re.findall(r'texture2D\s+(\w+)\s*\{', text)
samplers = re.findall(r'sampler2D\s+(\w+)\s*\{\s*Texture\s*=\s*(\w+)\s*;([^}]+)', text)
expected = ['tFlow', 'tConfidence', 'tCurrLuma', 'tPrevLuma', 'tFlow128',
            'tFlow64A', 'tFlow64B', 'tFlow32A', 'tFlow32B', 'tFlow16A',
            'tFlow16B', 'tFlow8', 'tPrevFrameFlow', 'tPrevConfidence']
if textures != expected or len(samplers) != len(expected):
    raise SystemExit('Unsupported QuantMotion resource layout; no files installed')
begin = text.index('bool IsOOB(')
end = text.index('/*----------------.\n| :: TECHNIQUE') if '/*----------------.\n| :: TECHNIQUE' in text else text.index('technique Lumenite_QuantMotion')
body = text[begin:end].replace('ReShade::BackBuffer', 'MojaveBackBuffer')
prelude = r'''
#define EPSILON 1e-6
#define DEBUG_FLOW 0
#define BUFFER_PIXEL_SIZE float2(1.0/BUFFER_WIDTH,1.0/BUFFER_HEIGHT)
Texture2D<float4> MojaveTextures[15]:register(t0);
SamplerState MojavePoint:register(s0);
SamplerState MojaveLinear:register(s1);
cbuffer MojaveFrame:register(b0){uint FRAME_COUNT;};
struct sampler2D {uint index;uint filtered;};
static const sampler2D MojaveBackBuffer={0,0};
float4 tex2Dlod(sampler2D s,float4 p){return s.filtered?MojaveTextures[s.index].SampleLevel(MojaveLinear,p.xy,p.w):MojaveTextures[s.index].SampleLevel(MojavePoint,p.xy,p.w);}
float4 tex2D(sampler2D s,float2 p){return tex2Dlod(s,float4(p,0,0));}
uint2 tex2Dsize(sampler2D s,uint mip){uint w,h,n;MojaveTextures[s.index].GetDimensions(mip,w,h,n);return uint2(w,h);}
struct MojaveVertex {float4 pos:SV_Position;float2 uv:TEXCOORD;};
MojaveVertex MojaveVS(uint id:SV_VertexID){MojaveVertex o;o.uv=float2(id==2?2:0,id==1?2:0);o.pos=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 MojaveDownsample(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target{return MojaveTextures[0].SampleLevel(MojaveLinear,uv,0);}
'''
for name, texture, options in samplers:
    linear = int(bool(re.search(r'MagFilter\s*=\s*LINEAR', options, re.I)))
    prelude += f'static const sampler2D {name}={{{textures.index(texture)+1},{linear}}};\n'
licences = list(source.parent.parent.glob('LICENSE.md'))
if len(licences) != 1:
    raise SystemExit('Original Lumenite licence is required')
target = args.root / 'runtime/dlss5/lumenite'
target.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, target / source.name)
shutil.copy2(licences[0], target / 'LICENSE.md')
(target / 'QuantMotion.hlsl').write_text(text[:text.index('/*------------------.')] + prelude.replace('sampler2D', 'MojaveSampler') + body.replace('sampler2D', 'MojaveSampler'), encoding='utf-8')
(target / 'source.json').write_text(json.dumps({'source': str(source), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'author': 'Afzaal (Kaido)', 'licence': 'AGNYA, private local adaptation; not redistributed'}, indent=2))
print(f'Prepared private QuantMotion runtime at {target}')
