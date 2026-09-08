# SPDX-License-Identifier: GPL-3.0-only
# Copyright (c) 2026 SamGCoder (original contributions).
# See THIRD_PARTY_NOTICES.md for upstream attribution.
"""Local authoring console for MojaveIsoNative. No network server or elevated access."""
from __future__ import annotations
import configparser
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import tkinter as tk
from tkinter import ttk, messagebox

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT.parent
RUNTIME = ROOT / "runtime"
NATIVE = GAME / "Data" / "NVSE" / "Plugins" / "MojaveIsoNative.dll"
BUILD = ROOT / "build" / "native-plugin" / "Release" / "MojaveIsoNative.dll"

def send(operation="configure", values=None, text=""):
    RUNTIME.mkdir(exist_ok=True)
    if values:
        profile = RUNTIME / "tool-profile.json"
        try: saved = json.loads(profile.read_text())
        except (OSError, ValueError): saved = {}
        saved.update({k:v for k,v in values.items() if k in ("yaw","pitch","distance","span","orthographic")})
        profile.write_text(json.dumps(saved))
    c = configparser.ConfigParser()
    c["command"] = {"sequence": str(time.time_ns() // 1000000 % 2147483647), "operation": operation, "text": text}
    c["camera"] = {k: str(v) for k, v in (values or {}).items()}
    tmp = RUNTIME / "command.tmp"
    with tmp.open("w", encoding="ascii") as f:
        c.write(f)
    os.replace(tmp, RUNTIME / "command.ini")

def install_plugin():
    source = BUILD if BUILD.exists() else ROOT / "native-plugin" / "prebuilt" / "MojaveIsoNative.dll"
    if not source.exists():
        raise RuntimeError("Build the native plugin first.")
    NATIVE.parent.mkdir(parents=True, exist_ok=True)
    if NATIVE.exists():
        backup = ROOT / "backups" / ("MojaveIsoNative-" + str(time.time_ns()) + ".dll")
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(NATIVE, backup)
    shutil.copy2(source, NATIVE)

class Tool(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Mojave Isometric Modding Tool — Native Renderer Lab")
        self.geometry("1150x800")
        self.minsize(980, 660)
        self.configure(bg="#101915")
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(".", background="#18231d", foreground="#e3e8dd", fieldbackground="#101915", font=("Segoe UI", 10))
        style.configure("TButton", padding=(12, 8))
        style.map("TButton", background=[("active", "#3d5541")])
        style.configure("TLabel", background="#18231d")
        style.configure("Title.TLabel", font=("Segoe UI Semibold", 23), foreground="#efd38b")
        style.configure("Sub.TLabel", foreground="#a8baa5")
        header = ttk.Frame(self, padding=18); header.pack(fill="x")
        ttk.Label(header, text="MOJAVE / ISOMETRIC LAB", style="Title.TLabel").pack(anchor="w")
        ttk.Label(header, text="Native C++ camera + D3D9 frame capture  ·  Fallout New Vegas 1.4.0.525", style="Sub.TLabel").pack(anchor="w", pady=(4,0))
        self.connection = tk.StringVar(value="Waiting for the native plugin")
        ttk.Label(header, textvariable=self.connection).pack(anchor="w", pady=(10,0))
        body=ttk.Panedwindow(self, orient="horizontal"); body.pack(fill="both", expand=True, padx=12, pady=8)
        left_host=ttk.Frame(body); right=ttk.Frame(body, padding=12)
        body.add(left_host, weight=1);body.add(right, weight=3)
        scroll=ttk.Scrollbar(left_host,orient="vertical");scroll.pack(side="right",fill="y")
        controls_canvas=tk.Canvas(left_host,bg="#18231d",highlightthickness=0,width=320,yscrollcommand=scroll.set)
        controls_canvas.pack(side="left",fill="both",expand=True);scroll.configure(command=controls_canvas.yview)
        left=ttk.Frame(controls_canvas,padding=12);left_item=controls_canvas.create_window(0,0,window=left,anchor="nw")
        left.bind("<Configure>",lambda _:controls_canvas.configure(scrollregion=controls_canvas.bbox("all")))
        controls_canvas.bind("<Configure>",lambda e:controls_canvas.itemconfigure(left_item,width=e.width))
        try: saved=json.loads((RUNTIME/"tool-profile.json").read_text())
        except (OSError,ValueError): saved={}
        self.vars={k:tk.DoubleVar(value=saved.get(k,v)) for k,v in {"yaw":45,"pitch":50,"distance":1100,"span":1500}.items()}
        self.ortho=tk.IntVar(value=saved.get("orthographic",0))
        self.pending=None
        ttk.Label(left,text="CAMERA",font=("Segoe UI Semibold",12)).pack(anchor="w")
        for key,label,lo,hi in [("yaw","Orbit / yaw",0,360),("pitch","Downward angle",20,80),("distance","Camera distance",200,4000),("span","Orthographic width",300,5000)]:
            row=ttk.Frame(left);row.pack(fill="x",pady=(12,0))
            ttk.Label(row,text=label).pack(side="left")
            ttk.Entry(row,textvariable=self.vars[key],width=8).pack(side="right")
            ttk.Scale(left,from_=lo,to=hi,variable=self.vars[key],command=lambda _:self.schedule()).pack(fill="x",pady=5)
        ttk.Checkbutton(left,text="Orthographic projection (experimental)",variable=self.ortho,command=self.configure_camera).pack(anchor="w",pady=12)
        for text,cmd in [("Enable isometric · F8",lambda:self.command("enable")),("Restore normal controls · F8",lambda:self.command("disable")),("Stop movement · right click",lambda:self.command("stop")),("Capture renderer frame",lambda:self.command("capture")),("Launch New Vegas + plugin",self.launch),("Build native plugin",self.build),("Install built plugin",self.install)]:
            ttk.Button(left,text=text,command=cmd).pack(fill="x",pady=3)
        ttk.Label(left,text="Prototype controls\nLeft click: walk toward ground hit\nRight click: stop · Wheel: zoom\nF8: return to normal controls\n\nObstacle routing and roof cutaways\nare not implemented.",style="Sub.TLabel",wraplength=280).pack(anchor="w",pady=14)
        tabs=ttk.Notebook(right);tabs.pack(fill="both",expand=True)
        preview=ttk.Frame(tabs); diagnostic=ttk.Frame(tabs); console=ttk.Frame(tabs)
        tabs.add(preview,text="Renderer frame");tabs.add(diagnostic,text="Native diagnostics");tabs.add(console,text="Game console")
        self.canvas=tk.Canvas(preview,bg="#090e0b",highlightthickness=0)
        self.canvas.pack(fill="both",expand=True)
        self.canvas.create_text(280,170,text="Launch the game, load a save, then capture a frame.\nCaptured directly from the game's Direct3D9 backbuffer.",fill="#a8baa5",width=460,font=("Segoe UI",12),tags="placeholder")
        self.canvas.bind("<Button-1>",self.preview_click)
        ttk.Label(preview,text="Click a captured frame to send a test destination. Use a fresh capture after moving.",wraplength=600,style="Sub.TLabel").pack(fill="x",pady=8)
        self.details=tk.Text(diagnostic,bg="#0e1711",fg="#c4d7bc",insertbackground="#efd38b",font=("Consolas",10),wrap="word",relief="flat")
        self.details.pack(fill="both",expand=True)
        ttk.Label(console,text="Runs commands inside Fallout's console. No operating-system commands.",wraplength=550).pack(anchor="w",pady=10)
        self.console_line=tk.StringVar()
        entry=ttk.Entry(console,textvariable=self.console_line);entry.pack(fill="x",pady=8);entry.bind("<Return>",lambda _:self.console_send())
        ttk.Button(console,text="Submit to game",command=self.console_send).pack(anchor="w")
        ttk.Label(console,text="Development examples:\nGetNVSEVersion\ncoc Goodsprings\n\nUse a separate test character. World-changing commands affect the current session.",style="Sub.TLabel").pack(anchor="w",pady=20)
        self.notice=tk.StringVar(value="Native source: IsometricModdingTool / native-plugin")
        ttk.Label(self,textvariable=self.notice,padding=10).pack(fill="x")
        self.frame_mtime=0;self.preview_geometry=None;self.photo=None;self.worker=None
        self.after(500,self.poll)
    def values(self):
        return {**{k:round(v.get(),3) for k,v in self.vars.items()},"orthographic":self.ortho.get()}
    def schedule(self):
        if self.pending:self.after_cancel(self.pending)
        self.pending=self.after(250,self.configure_camera)
    def configure_camera(self):
        self.pending=None;self.command("configure")
    def command(self,op,extra=None):
        try:send(op,{**self.values(),**(extra or {})});self.notice.set("Submitted: "+op)
        except (ValueError,tk.TclError,OSError) as e:messagebox.showerror("Command",str(e))
    def console_send(self):
        line=self.console_line.get().strip()
        if line:send("console",self.values(),line);self.notice.set("Console submitted: "+line)
    def launch(self):
        existing=ctypes.windll.user32.FindWindowW(None,"Fallout: New Vegas")
        if existing:
            ctypes.windll.user32.ShowWindow(existing,9)
            ctypes.windll.user32.SetForegroundWindow(existing)
            self.notice.set("Game already running; switch to its window to apply commands.")
            return
        loader=GAME/"nvse_loader.exe"
        if not loader.exists():messagebox.showerror("Missing loader","Install xNVSE into the game folder first.");return
        subprocess.Popen([str(loader)],cwd=GAME);self.notice.set("Game launch requested")
    def build(self):
        if self.worker and self.worker.poll() is None:return
        self.build_log=(RUNTIME/"build.log").open("w")
        self.worker=subprocess.Popen(["cmake","--build",str(ROOT/"build"),"--config","Release"],stdout=self.build_log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        self.notice.set("Building native plugin…")
    def install(self):
        try:install_plugin();self.notice.set("Plugin installed. Restart the game to load the new DLL.")
        except Exception as e:messagebox.showerror("Install",str(e))
    def preview_click(self,event):
        if not self.preview_geometry:return
        ox,oy,w,h,sw,sh=self.preview_geometry
        if ox<=event.x<ox+w and oy<=event.y<oy+h:self.command("move",{"x":(event.x-ox)*sw/w,"y":(event.y-oy)*sh/h})
    def poll(self):
        try:
            p=RUNTIME/"status.json"
            data=json.loads(p.read_text())
            age=time.time()-p.stat().st_mtime
            state="LIVE" if age<4 else "PAUSED / NO RECENT UPDATE"
            projection="ORTHOGRAPHIC" if data.get('rendered_orthographic') and data['enabled'] else "PERSPECTIVE"
            self.connection.set(f"{state}  ·  PID {data['pid']}  ·  Native hooks {'ready' if data['hooks_ready'] else 'unavailable'}  ·  {projection}  ·  {'ENABLED' if data['enabled'] else 'NORMAL'}")
            text=json.dumps(data,indent=2)+"\n\n"+(RUNTIME/"native.log").read_text(errors="replace")[-4000:]
            self.details.delete("1.0","end");self.details.insert("1.0",text)
            frame=RUNTIME/"frame.bmp"
            if frame.exists() and frame.stat().st_mtime!=self.frame_mtime:
                from PIL import Image,ImageTk
                img=Image.open(frame).convert("RGB");sw,sh=img.size
                w,h=max(200,self.canvas.winfo_width()),max(150,self.canvas.winfo_height())
                img.thumbnail((w,h));iw,ih=img.size;ox,oy=(w-iw)//2,(h-ih)//2
                self.photo=ImageTk.PhotoImage(img);self.canvas.delete("all");self.canvas.create_image(ox,oy,image=self.photo,anchor="nw")
                self.preview_geometry=(ox,oy,iw,ih,sw,sh);self.frame_mtime=frame.stat().st_mtime
        except (OSError,ValueError,KeyError):pass
        if self.worker and self.worker.poll() is not None:
            self.build_log.close();self.notice.set("Build succeeded" if self.worker.returncode==0 else "Build failed: see runtime/build.log");self.worker=None
        self.after(600,self.poll)

if __name__=="__main__":
    if "--send" in sys.argv:
        idx=sys.argv.index("--send");send(sys.argv[idx+1],json.loads(sys.argv[idx+2]) if len(sys.argv)>idx+2 else {})
    elif "--console" in sys.argv:send("console",text=sys.argv[sys.argv.index("--console")+1])
    elif "--install" in sys.argv:install_plugin()
    else:Tool().mainloop()
