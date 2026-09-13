#!/usr/bin/env python3
"""Create a disposable editor/Git/image test project; never overwrite a project."""
import argparse
import pathlib
import struct
import subprocess
import zlib

def main():
    p=argparse.ArgumentParser()
    p.add_argument('directory',type=pathlib.Path)
    p.add_argument('--commits',type=int,default=230)
    args=p.parse_args()
    root=args.directory.resolve()
    if root.exists() and any(root.iterdir()):
        raise SystemExit('Refusing to overwrite a non-empty directory: '+str(root))
    root.mkdir(parents=True,exist_ok=True)
    def git(*values):
        subprocess.run(['git',*values],cwd=root,check=True,stdout=subprocess.DEVNULL)
    def write(path,text):
        target=root/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text)
    git('init','-b','main')
    git('config','user.name','Kiri Demo')
    git('config','user.email','demo@kiri.invalid')
    git('config','commit.gpgsign','false')
    git('remote','add','origin','https://github.com/example/kiri-demo.git')
    write('README.md','# Kiri workspace demo\n\nA disposable project for native UI tests.\n')
    write('.gitignore','build/\nnode_modules/\n')
    write('src/main.cpp','''#include <iostream>
#include <string>

// A small native application — UTF-8: 日本語
int main()
{
    const std::string greeting = "Hello from Haiku";
    std::cout << greeting << std::endl;
    return 0;
}
''')
    write('web/index.html','''<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>Kiri workspace</title>
    <link rel="stylesheet" href="style.css">
  </head>
  <body>
    <!-- Preview assets and edit native code in one workspace. -->
    <main class="workspace">
      <h1>Hello, Haiku.</h1>
      <p>A home for your next project.</p>
    </main>
  </body>
</html>
''')
    write('web/style.css',':root { color-scheme: dark; }\n.workspace { color: #78b7ff; padding: 2rem; }\n')
    write('config/settings.json','{\n  "name": "Kiri",\n  "native": true,\n  "version": 1,\n  "features": ["editor", "git", "terminal"]\n}\n')
    write('config/workspace.xml','<?xml version="1.0" encoding="utf-8"?>\n<workspace name="Kiri">\n  <project path="src" enabled="true" />\n</workspace>\n')
    write('scripts/build.py','''from pathlib import Path

def sources(root: Path):
    """Find source files in the current project."""
    for path in root.rglob("*.cpp"):
        print(f"Source: {path}")

if __name__ == "__main__":
    sources(Path("src"))
''')
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    width,height=640,400
    pixels=bytearray()
    for y in range(height):
        pixels.append(0)
        for x in range(width):
            inside=(x-width/2)**2/220**2+(y-height/2)**2/130**2<1
            pixels.extend((int(70+140*x/width),int(120+100*y/height),220,255 if inside else 0))
    image=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b'')
    (root/'assets').mkdir();(root/'assets/alpha-preview.png').write_bytes(image)
    (root/'assets/sample.bin').write_bytes(bytes(range(256))*1024)
    git('add','--all');git('commit','-m','Create the native workspace example')
    git('switch','-c','feature/themes')
    write('config/theme.toml','name = "Obsidian"\nbackground = "#171b22"\naccent = "#78b7ff"\n')
    git('add','--all');git('commit','-m','Add an Obsidian color theme')
    git('switch','main')
    write('docs/notes.md','# Notes\n\nKeep typing while Git reads history.\n')
    git('add','--all');git('commit','-m','Document background operations')
    git('merge','--no-ff','feature/themes','-m','Merge theme work')
    for n in range(max(0,args.commits-4)):
        git('commit','--allow-empty','-m',f'History pagination checkpoint {n+1:03d}')
    write('src/main.cpp',(root/'src/main.cpp').read_text().replace('Hello from Haiku','Hello from Kiri'))
    write('web/new file 日本.json','{"ready": true}\n')
    print(root)

if __name__=='__main__':
    main()
