#!/usr/bin/env python3
"""
RISC-V Pipeline Simulator Visualizer
用法: python riscv_visualizer.py <log_file> [-o output.html]
将模拟器输出解析为可交互的 HTML 可视化页面。
"""

import re
import json
import sys
import argparse
from pathlib import Path


# ─── 解析器 ────────────────────────────────────────────────────────────────────

def parse_simulator_output(text: str) -> dict:
    data = {
        "program": [],
        "cycles": [],
        "statistics": {},
        "watchpoints": {},
    }

    # 1. 程序列表
    for m in re.finditer(r"^([0-9a-fA-F]{8}): (.+)$", text, re.MULTILINE):
        data["program"].append({"addr": m.group(1), "instr": m.group(2).strip()})

    # 2. 按 CYCLE 分块
    parts = re.split(r"={10} CYCLE (\d+) ={10}", text)
    i = 1
    while i + 1 < len(parts):
        cycle_num = int(parts[i])
        content   = parts[i + 1]
        data["cycles"].append(_parse_cycle(cycle_num, content))
        i += 2

    # 3. 统计信息
    sm = re.search(r"===== STATISTICS =====(.*?)(?:=====|$)", text, re.DOTALL)
    if sm:
        for line in sm.group(1).strip().splitlines():
            m = re.match(r"(\w+)\s*=\s*(.+)", line.strip())
            if m:
                k, v = m.group(1), m.group(2).strip()
                try:
                    data["statistics"][k] = float(v) if "." in v else int(v)
                except ValueError:
                    data["statistics"][k] = v

    # 4. Watchpoint 最终值
    wm = re.search(r"===== WATCHPOINT VALUES =====(.*?)$", text, re.DOTALL)
    if wm:
        for line in wm.group(1).strip().splitlines():
            m = re.match(r"addr=(0x[\da-fA-F]+)\s+value=(0x[\da-fA-F]+)", line.strip())
            if m:
                data["watchpoints"][m.group(1)] = m.group(2)

    return data


def _parse_cycle(num: int, content: str) -> dict:
    cycle: dict = {"num": num, "stages": {}, "regs": {}, "mem_events": []}

    # PC / halted
    m = re.search(r"PC=(0x[\da-fA-F]+)\s+halted=(\d)", content)
    if m:
        cycle["pc"]     = m.group(1)
        cycle["halted"] = int(m.group(2))

    # 流水线寄存器
    stage_names = ["IF/ID", "ID/EX", "EX/MEM", "MEM/WB"]
    stage_keys  = ["IF_ID", "ID_EX", "EX_MEM", "MEM_WB"]

    # 把内容按行分，找到每个 stage 行
    lines = content.splitlines()
    stage_lines: dict[str, str] = {}
    for line in lines:
        for sn, sk in zip(stage_names, stage_keys):
            if f"[{sn}]" in line:
                stage_lines[sk] = line

    for sk in stage_keys:
        raw = stage_lines.get(sk, "")
        m = re.search(r"valid=(\d)", raw)
        valid = int(m.group(1)) if m else 0
        sd: dict = {"valid": valid}
        op_m = re.search(r"op=(\w+)", raw)
        if op_m:
            sd["op"] = op_m.group(1)
        for fm in re.finditer(r"(rs1v|rs2v|imm|alu|wb|rd)=(0x[\da-fA-F]+|-?\d+|x\d+)", raw):
            sd[fm.group(1)] = fm.group(2)
        cycle["stages"][sk] = sd

    # 寄存器堆（取最后一次出现，避免被 MEM-WATCH 行干扰）
    reg_section = re.search(r"REGFILE(.*?)(?:\[MEM-WATCH\]|={10}|$)", content, re.DOTALL)
    if reg_section:
        for rm in re.finditer(r"x(\d{2})=(0x[\da-fA-F]{8})", reg_section.group(1)):
            cycle["regs"][f"x{int(rm.group(1))}"] = rm.group(2)

    # 内存访问事件
    for mm in re.finditer(
        r"\[MEM-WATCH\]\s+cycle=(\d+)\s+pc=(0x[\da-fA-F]+)\s+#(\d+)"
        r"\s+(READ|WRITE)\s+addr=(0x[\da-fA-F]+)\s+value=(0x[\da-fA-F]+)",
        content,
    ):
        cycle["mem_events"].append({
            "cycle": int(mm.group(1)),
            "pc":    mm.group(2),
            "id":    int(mm.group(3)),
            "type":  mm.group(4),
            "addr":  mm.group(5),
            "value": mm.group(6),
        })

    return cycle


# ─── HTML 生成器 ───────────────────────────────────────────────────────────────

HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RISC-V Pipeline Visualizer</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@300;400;500;700&family=Share+Tech+Mono&display=swap');

  :root {
    --bg:        #080b10;
    --surface:   #0e1420;
    --border:    #1c2a3a;
    --text:      #a8c0d8;
    --dim:       #3a5070;
    --green:     #00e57a;
    --amber:     #ffa420;
    --red:       #ff4466;
    --blue:      #38b6ff;
    --purple:    #b06aff;
    --cyan:      #00d4c8;
    --inactive:  #1c2a3a;
    --glow-g:    0 0 12px #00e57a44;
    --glow-a:    0 0 12px #ffa42044;
    --glow-b:    0 0 12px #38b6ff44;
  }

  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'JetBrains Mono', 'Share Tech Mono', monospace;
    font-size: 12px;
    min-height: 100vh;
    overflow-x: hidden;
  }

  /* ── 顶栏 ── */
  #header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 10px 20px;
    border-bottom: 1px solid var(--border);
    background: var(--surface);
  }
  #header h1 {
    font-size: 14px;
    font-weight: 700;
    color: var(--green);
    letter-spacing: 3px;
    text-transform: uppercase;
  }
  #header .meta { color: var(--dim); font-size: 11px; }
  #status-badge {
    padding: 3px 10px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1px;
  }
  .badge-running  { background: #001a0a; color: var(--green); border: 1px solid var(--green); }
  .badge-halted   { background: #1a0010; color: var(--red);   border: 1px solid var(--red);   }

  /* ── 主体布局 ── */
  #main {
    display: grid;
    grid-template-columns: 200px 1fr 220px;
    grid-template-rows: 1fr auto;
    height: calc(100vh - 44px - 80px);
    gap: 1px;
    background: var(--border);
  }

  /* ── 面板通用 ── */
  .panel {
    background: var(--bg);
    overflow: hidden;
    display: flex;
    flex-direction: column;
  }
  .panel-title {
    padding: 6px 12px;
    font-size: 10px;
    letter-spacing: 2px;
    text-transform: uppercase;
    color: var(--dim);
    border-bottom: 1px solid var(--border);
    background: var(--surface);
    flex-shrink: 0;
  }
  .panel-body {
    overflow-y: auto;
    flex: 1;
    padding: 8px;
  }
  .panel-body::-webkit-scrollbar { width: 4px; }
  .panel-body::-webkit-scrollbar-thumb { background: var(--border); }

  /* ── 程序列表 ── */
  .prog-line {
    display: flex;
    gap: 8px;
    padding: 3px 6px;
    border-radius: 3px;
    cursor: default;
    transition: background 0.1s;
  }
  .prog-line:hover { background: var(--surface); }
  .prog-line.current-pc {
    background: #001a0a;
    border-left: 2px solid var(--green);
    box-shadow: var(--glow-g);
  }
  .prog-addr { color: var(--dim); min-width: 68px; font-size: 11px; }
  .prog-instr { color: var(--blue); }
  .prog-line.current-pc .prog-instr { color: var(--green); font-weight: 700; }

  /* ── 流水线中心区域 ── */
  #center {
    display: flex;
    flex-direction: column;
  }

  /* ── 流水线图 ── */
  #pipeline-area {
    flex: 0 0 auto;
    padding: 20px 16px 10px;
  }
  #pipeline {
    display: flex;
    align-items: center;
    gap: 0;
  }

  .stage-box {
    flex: 1;
    border: 1px solid var(--border);
    border-radius: 4px;
    background: var(--surface);
    padding: 8px 6px;
    min-height: 110px;
    position: relative;
    transition: all 0.2s;
  }
  .stage-box.active {
    border-color: var(--blue);
    box-shadow: var(--glow-b);
    background: #050d18;
  }
  .stage-name {
    font-size: 9px;
    letter-spacing: 2px;
    color: var(--dim);
    text-transform: uppercase;
    margin-bottom: 6px;
    text-align: center;
  }
  .stage-box.active .stage-name { color: var(--blue); }

  .pipe-reg {
    width: 28px;
    flex-shrink: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    position: relative;
  }
  .pipe-reg-box {
    width: 22px;
    height: 80px;
    border: 1px solid var(--border);
    border-radius: 3px;
    background: var(--surface);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 3px;
    position: relative;
    transition: all 0.2s;
  }
  .pipe-reg-box.valid {
    border-color: var(--amber);
    background: #100a00;
    box-shadow: var(--glow-a);
  }
  .pipe-reg-box.invalid {
    border-color: var(--inactive);
    background: var(--inactive);
    opacity: 0.4;
  }
  .pipe-reg-label {
    font-size: 8px;
    color: var(--dim);
    text-align: center;
    margin-top: 4px;
    letter-spacing: 1px;
  }
  .pipe-reg-box.valid .pipe-reg-op {
    color: var(--amber);
    font-weight: 700;
    font-size: 9px;
    writing-mode: vertical-rl;
    text-orientation: mixed;
    letter-spacing: 1px;
  }
  .pipe-reg-op { color: var(--dim); font-size: 8px; writing-mode: vertical-rl; }

  /* stage 内字段 */
  .stage-field {
    display: flex;
    justify-content: space-between;
    gap: 4px;
    padding: 1px 0;
    border-bottom: 1px solid #0e1420;
  }
  .field-key { color: var(--dim); font-size: 10px; }
  .field-val { color: var(--cyan); font-size: 10px; font-weight: 500; }
  .stage-op {
    text-align: center;
    font-size: 13px;
    font-weight: 700;
    color: var(--green);
    margin-bottom: 4px;
    letter-spacing: 1px;
  }
  .stage-empty {
    color: var(--dim);
    font-size: 10px;
    text-align: center;
    margin-top: 20px;
  }

  /* ── 信息面板（内存 + 当前指令详情） ── */
  #info-area {
    flex: 1;
    overflow-y: auto;
    padding: 8px 16px;
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
    align-content: start;
  }
  .info-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 8px 10px;
  }
  .info-card-title {
    font-size: 9px;
    letter-spacing: 2px;
    color: var(--dim);
    text-transform: uppercase;
    margin-bottom: 6px;
  }
  .mem-event {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 2px 0;
  }
  .mem-type-read  { color: var(--green); font-size: 9px; font-weight: 700; }
  .mem-type-write { color: var(--red);   font-size: 9px; font-weight: 700; }
  .mem-addr  { color: var(--purple); }
  .mem-value { color: var(--amber); }
  .mem-empty { color: var(--dim); font-size: 10px; }

  /* ── 寄存器堆 ── */
  #regfile-panel .panel-body {
    padding: 6px;
  }
  .reg-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 2px;
  }
  .reg-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 2px 5px;
    border-radius: 2px;
    transition: background 0.15s;
  }
  .reg-item.changed {
    background: #100a00;
    border-left: 2px solid var(--amber);
  }
  .reg-name { color: var(--dim); font-size: 10px; min-width: 26px; }
  .reg-val  { color: var(--text); font-size: 10px; }
  .reg-item.changed .reg-val { color: var(--amber); font-weight: 700; }
  .reg-item.nonzero .reg-val { color: var(--cyan); }

  /* ── 控制栏 ── */
  #controls {
    height: 80px;
    background: var(--surface);
    border-top: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 0 20px;
  }
  .btn {
    background: var(--surface);
    color: var(--text);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 6px 14px;
    font-family: inherit;
    font-size: 11px;
    cursor: pointer;
    transition: all 0.15s;
    letter-spacing: 1px;
  }
  .btn:hover { border-color: var(--blue); color: var(--blue); box-shadow: var(--glow-b); }
  .btn.play  { border-color: var(--green); color: var(--green); }
  .btn.play:hover { box-shadow: var(--glow-g); }
  .btn:disabled { opacity: 0.3; cursor: not-allowed; }

  #slider-wrap {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  #slider-info {
    display: flex;
    justify-content: space-between;
    font-size: 11px;
    color: var(--dim);
  }
  #cycle-display {
    font-size: 18px;
    font-weight: 700;
    color: var(--green);
    letter-spacing: 2px;
  }
  input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 4px;
    background: var(--border);
    border-radius: 2px;
    outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 14px;
    height: 14px;
    border-radius: 50%;
    background: var(--green);
    cursor: pointer;
    box-shadow: var(--glow-g);
  }

  /* ── 统计面板（折叠） ── */
  #stats-panel {
    grid-column: 1 / -1;
    display: none;
    background: var(--surface);
    border-top: 1px solid var(--border);
    padding: 12px 20px;
  }
  #stats-panel.open { display: block; }
  #stats-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
    gap: 6px;
    margin-top: 8px;
  }
  .stat-item {
    display: flex;
    justify-content: space-between;
    padding: 3px 8px;
    background: var(--bg);
    border-radius: 3px;
    border: 1px solid var(--border);
  }
  .stat-key { color: var(--dim); font-size: 10px; }
  .stat-val { color: var(--amber); font-size: 10px; font-weight: 700; }

  /* ── 速度标签 ── */
  #speed-label { font-size: 10px; color: var(--dim); white-space: nowrap; }
</style>
</head>
<body>

<div id="header">
  <h1>&#9654; RISC-V Pipeline Debugger</h1>
  <div class="meta" id="header-meta"></div>
  <span id="status-badge" class="badge-running">RUNNING</span>
</div>

<div id="main">
  <!-- 程序列表 -->
  <div class="panel" id="prog-panel">
    <div class="panel-title">&#9654; Program</div>
    <div class="panel-body" id="prog-list"></div>
  </div>

  <!-- 中心：流水线 + 信息 -->
  <div class="panel" id="center">
    <div id="pipeline-area">
      <div id="pipeline"></div>
    </div>
    <div id="info-area"></div>
  </div>

  <!-- 寄存器堆 -->
  <div class="panel" id="regfile-panel">
    <div class="panel-title">&#9654; Register File</div>
    <div class="panel-body">
      <div class="reg-grid" id="reg-grid"></div>
    </div>
  </div>
</div>

<!-- 控制栏 -->
<div id="controls">
  <button class="btn" id="btn-first" title="第一帧">&#8676;</button>
  <button class="btn" id="btn-prev"  title="上一周期">&#9664;</button>
  <button class="btn play" id="btn-play" title="播放">&#9654; PLAY</button>
  <button class="btn" id="btn-next"  title="下一周期">&#9654;</button>
  <button class="btn" id="btn-last"  title="最后帧">&#8677;</button>

  <div id="slider-wrap">
    <div id="slider-info">
      <span id="cycle-display">CYCLE 1</span>
      <span id="pc-display"></span>
    </div>
    <input type="range" id="cycle-slider" min="1" step="1">
  </div>

  <div id="speed-label">
    Speed<br>
    <select id="speed-sel" style="background:var(--surface);color:var(--text);border:1px solid var(--border);font-family:inherit;font-size:11px;padding:2px;">
      <option value="500">0.5s</option>
      <option value="250" selected>0.25s</option>
      <option value="100">0.1s</option>
      <option value="50">0.05s</option>
    </select>
  </div>

  <button class="btn" id="btn-stats" title="统计">&#9776; STATS</button>
</div>

<!-- 统计面板 -->
<div id="stats-panel">
  <div class="panel-title">&#9654; Statistics</div>
  <div id="stats-grid"></div>
</div>

<script>
// ── 数据 ──────────────────────────────────────────────────────────────────────
const DATA = __DATA_PLACEHOLDER__;

const cycles  = DATA.cycles;
const program = DATA.program;
const stats   = DATA.statistics;
const MAX     = cycles.length;

// ── 状态 ─────────────────────────────────────────────────────────────────────
let currentIdx = 0;
let playTimer  = null;

// ── DOM refs ──────────────────────────────────────────────────────────────────
const slider    = document.getElementById('cycle-slider');
const cycleLbl  = document.getElementById('cycle-display');
const pcLbl     = document.getElementById('pc-display');
const statusBdg = document.getElementById('status-badge');
const headerMeta= document.getElementById('header-meta');

// ── 初始化程序列表 ────────────────────────────────────────────────────────────
function initProgList() {
  const container = document.getElementById('prog-list');
  program.forEach(p => {
    const div = document.createElement('div');
    div.className = 'prog-line';
    div.id = `pl-${p.addr}`;
    div.innerHTML = `<span class="prog-addr">0x${p.addr}</span><span class="prog-instr">${p.instr}</span>`;
    container.appendChild(div);
  });
}

// ── 初始化统计 ────────────────────────────────────────────────────────────────
function initStats() {
  const grid = document.getElementById('stats-grid');
  for (const [k, v] of Object.entries(stats)) {
    const div = document.createElement('div');
    div.className = 'stat-item';
    div.innerHTML = `<span class="stat-key">${k}</span><span class="stat-val">${v}</span>`;
    grid.appendChild(div);
  }
  document.getElementById('btn-stats').addEventListener('click', () => {
    document.getElementById('stats-panel').classList.toggle('open');
  });
}

// ── 渲染某一周期 ──────────────────────────────────────────────────────────────
const STAGE_DEFS = [
  { key: 'IF',     label: 'IF',     regKey: null,     displayFields: [] },
  { key: 'IF_ID',  label: 'IF/ID',  regKey: 'IF_ID',  isPipeReg: true },
  { key: 'ID',     label: 'ID',     regKey: null,     displayFields: [] },
  { key: 'ID_EX',  label: 'ID/EX',  regKey: 'ID_EX',  isPipeReg: true },
  { key: 'EX',     label: 'EX',     regKey: null,     displayFields: [] },
  { key: 'EX_MEM', label: 'EX/MEM', regKey: 'EX_MEM', isPipeReg: true },
  { key: 'MEM',    label: 'MEM',    regKey: null,     displayFields: [] },
  { key: 'MEM_WB', label: 'MEM/WB', regKey: 'MEM_WB', isPipeReg: true },
  { key: 'WB',     label: 'WB',     regKey: null,     displayFields: [] },
];

// pipeline reg 展示字段顺序
const REG_FIELDS = {
  IF_ID:  ['op'],
  ID_EX:  ['op', 'rs1v', 'rs2v', 'imm'],
  EX_MEM: ['op', 'alu', 'rd'],
  MEM_WB: ['op', 'wb', 'rd'],
};

function renderCycle(idx) {
  const c = cycles[idx];
  const prev = idx > 0 ? cycles[idx - 1] : null;

  // 头部
  cycleLbl.textContent = `CYCLE ${c.num}`;
  pcLbl.textContent    = `PC = ${c.pc}`;
  slider.value         = idx + 1;
  statusBdg.textContent = c.halted ? 'HALTED' : 'RUNNING';
  statusBdg.className  = c.halted ? 'badge-halted' : 'badge-running';

  // 高亮当前 PC 在程序列表
  document.querySelectorAll('.prog-line.current-pc').forEach(el => el.classList.remove('current-pc'));
  const pcHex = c.pc.replace('0x', '').padStart(8, '0');
  const el = document.getElementById(`pl-${pcHex}`);
  if (el) {
    el.classList.add('current-pc');
    el.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }

  // 流水线
  renderPipeline(c);

  // 寄存器
  renderRegs(c, prev);

  // 信息区
  renderInfo(c);
}

function renderPipeline(c) {
  const container = document.getElementById('pipeline');
  container.innerHTML = '';

  // 从 stages 对象推断各实际 stage 的"活跃指令"
  // IF: 当前 cycle 的 PC 正在取指
  // ID: IF/ID 里的内容
  // EX: ID/EX 里的内容
  // MEM: EX/MEM 里的内容
  // WB: MEM/WB 里的内容
  const stageActive = {
    IF:  { valid: 1, op: 'FETCH' },
    ID:  c.stages['IF_ID']  || { valid: 0 },
    EX:  c.stages['ID_EX']  || { valid: 0 },
    MEM: c.stages['EX_MEM'] || { valid: 0 },
    WB:  c.stages['MEM_WB'] || { valid: 0 },
  };

  const pipeRegs = ['IF_ID', 'ID_EX', 'EX_MEM', 'MEM_WB'];
  const stageOrder = ['IF', 'ID', 'EX', 'MEM', 'WB'];

  stageOrder.forEach((stageName, i) => {
    const info = stageActive[stageName];
    const box = document.createElement('div');
    box.className = `stage-box${info.valid ? ' active' : ''}`;

    let inner = `<div class="stage-name">${stageName}</div>`;
    if (info.valid && info.op) {
      inner += `<div class="stage-op">${info.op}</div>`;
      const fields = REG_FIELDS[pipeRegs[i-1]] || [];
      if (i > 0) {
        const rd = c.stages[pipeRegs[i-1]] || {};
        const showFields = Object.entries(rd).filter(([k]) => k !== 'op' && k !== 'valid');
        showFields.forEach(([k, v]) => {
          inner += `<div class="stage-field">
            <span class="field-key">${k}</span>
            <span class="field-val">${v}</span>
          </div>`;
        });
      }
    } else {
      inner += `<div class="stage-empty">─ idle ─</div>`;
    }
    box.innerHTML = inner;
    container.appendChild(box);

    // 流水线寄存器（除最后一个 stage 后）
    if (i < stageOrder.length - 1) {
      const regKey = pipeRegs[i];
      const regData = c.stages[regKey] || { valid: 0 };
      const preg = document.createElement('div');
      preg.className = 'pipe-reg';
      const validCls = regData.valid ? 'valid' : 'invalid';
      const opText   = regData.op || '?';
      preg.innerHTML = `
        <div class="pipe-reg-box ${validCls}">
          <span class="pipe-reg-op">${regData.valid ? opText : '─'}</span>
        </div>
        <div class="pipe-reg-label">${regKey.replace('_', '/')}</div>`;
      container.appendChild(preg);
    }
  });
}

function renderRegs(c, prev) {
  const grid = document.getElementById('reg-grid');
  grid.innerHTML = '';
  const prevRegs = prev ? prev.regs : {};

  for (let i = 0; i <= 31; i++) {
    const key = `x${i}`;
    const val = c.regs[key] || '0x00000000';
    const prevVal = prevRegs[key] || '0x00000000';
    const changed = val !== prevVal;
    const nonzero = val !== '0x00000000';

    const item = document.createElement('div');
    item.className = `reg-item${changed ? ' changed' : (nonzero ? ' nonzero' : '')}`;
    const shortVal = parseInt(val, 16).toString(16).padStart(8, '0');
    item.innerHTML = `<span class="reg-name">x${String(i).padStart(2,'0')}</span><span class="reg-val">${val}</span>`;
    grid.appendChild(item);
  }
}

function renderInfo(c) {
  const area = document.getElementById('info-area');
  area.innerHTML = '';

  // 内存访问卡
  const memCard = document.createElement('div');
  memCard.className = 'info-card';
  memCard.innerHTML = '<div class="info-card-title">&#9654; Memory Events</div>';
  if (c.mem_events && c.mem_events.length > 0) {
    c.mem_events.forEach(ev => {
      const row = document.createElement('div');
      row.className = 'mem-event';
      const typeCls = ev.type === 'READ' ? 'mem-type-read' : 'mem-type-write';
      row.innerHTML = `<span class="${typeCls}">${ev.type}</span>
        <span class="mem-addr">${ev.addr}</span>
        <span style="color:var(--dim)">→</span>
        <span class="mem-value">${ev.value}</span>`;
      memCard.appendChild(row);
    });
  } else {
    memCard.innerHTML += '<div class="mem-empty">no memory access</div>';
  }
  area.appendChild(memCard);

  // 流水线寄存器详情卡
  const regsCard = document.createElement('div');
  regsCard.className = 'info-card';
  regsCard.innerHTML = '<div class="info-card-title">&#9654; Pipeline Registers</div>';
  const regKeys = ['IF_ID','ID_EX','EX_MEM','MEM_WB'];
  regKeys.forEach(rk => {
    const rd = c.stages[rk];
    if (!rd) return;
    const label = document.createElement('div');
    label.style.cssText = 'color:var(--amber);font-size:10px;margin-top:4px;letter-spacing:1px;';
    label.textContent = rk.replace('_','/');
    regsCard.appendChild(label);
    Object.entries(rd).forEach(([k, v]) => {
      if (k === 'valid') return;
      const row = document.createElement('div');
      row.className = 'stage-field';
      row.innerHTML = `<span class="field-key" style="font-size:10px;">${k}</span><span class="field-val" style="font-size:10px;">${v}</span>`;
      regsCard.appendChild(row);
    });
  });
  area.appendChild(regsCard);

  // 周期统计速览
  const statsCard = document.createElement('div');
  statsCard.className = 'info-card';
  statsCard.innerHTML = `<div class="info-card-title">&#9654; Cycle Info</div>
    <div class="stage-field"><span class="field-key">pc</span><span class="field-val">${c.pc}</span></div>
    <div class="stage-field"><span class="field-key">halted</span><span class="field-val" style="color:${c.halted ? 'var(--red)':'var(--green)'}">${c.halted}</span></div>`;
  area.appendChild(statsCard);
}

// ── 播放控制 ─────────────────────────────────────────────────────────────────
function goTo(idx) {
  currentIdx = Math.max(0, Math.min(MAX - 1, idx));
  renderCycle(currentIdx);
  document.getElementById('btn-prev').disabled  = currentIdx === 0;
  document.getElementById('btn-first').disabled = currentIdx === 0;
  document.getElementById('btn-next').disabled  = currentIdx === MAX - 1;
  document.getElementById('btn-last').disabled  = currentIdx === MAX - 1;
}

function startPlay() {
  if (playTimer) return;
  const btn = document.getElementById('btn-play');
  btn.textContent = '⏸ PAUSE';
  const speed = parseInt(document.getElementById('speed-sel').value);
  playTimer = setInterval(() => {
    if (currentIdx >= MAX - 1) {
      stopPlay();
      return;
    }
    goTo(currentIdx + 1);
  }, speed);
}

function stopPlay() {
  clearInterval(playTimer);
  playTimer = null;
  document.getElementById('btn-play').textContent = '▶ PLAY';
}

// ── 事件绑定 ─────────────────────────────────────────────────────────────────
document.getElementById('btn-first').addEventListener('click', () => { stopPlay(); goTo(0); });
document.getElementById('btn-prev').addEventListener('click',  () => { stopPlay(); goTo(currentIdx - 1); });
document.getElementById('btn-next').addEventListener('click',  () => { stopPlay(); goTo(currentIdx + 1); });
document.getElementById('btn-last').addEventListener('click',  () => { stopPlay(); goTo(MAX - 1); });
document.getElementById('btn-play').addEventListener('click',  () => {
  playTimer ? stopPlay() : startPlay();
});
slider.addEventListener('input', () => { stopPlay(); goTo(parseInt(slider.value) - 1); });

document.addEventListener('keydown', e => {
  if (e.key === 'ArrowLeft')  { stopPlay(); goTo(currentIdx - 1); }
  if (e.key === 'ArrowRight') { stopPlay(); goTo(currentIdx + 1); }
  if (e.key === ' ')          { e.preventDefault(); playTimer ? stopPlay() : startPlay(); }
  if (e.key === 'Home')       { stopPlay(); goTo(0); }
  if (e.key === 'End')        { stopPlay(); goTo(MAX - 1); }
});

// ── 启动 ─────────────────────────────────────────────────────────────────────
slider.max = MAX;
headerMeta.textContent = `Total ${MAX} cycles · ${program.length} instructions`;
initProgList();
initStats();
goTo(0);
</script>
</body>
</html>
"""


def generate_html(data: dict) -> str:
    json_str = json.dumps(data, ensure_ascii=False, separators=(",", ":"))
    return HTML_TEMPLATE.replace("__DATA_PLACEHOLDER__", json_str)


# ─── CLI ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="RISC-V Pipeline Simulator Visualizer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="用法示例:\n  python riscv_visualizer.py sim_output.txt\n  python riscv_visualizer.py sim_output.txt -o viz.html"
    )
    parser.add_argument("input", nargs="?", default="-",
                        help="模拟器输出文件路径（默认从 stdin 读取）")
    parser.add_argument("-o", "--output", default=None,
                        help="输出 HTML 文件路径（默认：<input>.html）")
    args = parser.parse_args()

    # 读取输入
    if args.input == "-":
        text = sys.stdin.read()
        out_path = Path("riscv_viz.html")
    else:
        in_path = Path(args.input)
        text    = in_path.read_text(encoding="utf-8", errors="replace")
        out_path = in_path.with_suffix(".html")

    if args.output:
        out_path = Path(args.output)

    # 解析 + 生成
    data = parse_simulator_output(text)
    if not data["cycles"]:
        print("[ERROR] 未找到任何 CYCLE 块，请检查输入文件格式。", file=sys.stderr)
        sys.exit(1)

    html = generate_html(data)
    out_path.write_text(html, encoding="utf-8")

    print(f"[OK] 已生成可视化文件：{out_path}")
    print(f"     共 {len(data['cycles'])} 个周期，{len(data['program'])} 条指令")
    print(f"     用浏览器打开该文件即可交互查看。")


if __name__ == "__main__":
    main()
