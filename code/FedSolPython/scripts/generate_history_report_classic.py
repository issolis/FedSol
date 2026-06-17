#!/usr/bin/env python3
"""
generate_history_report_classic.py — FedSol per-round history report for classic models

For each of the last N global models saved in output_server/models/, runs the
classic evaluator (classification / regression) and crosses with the matching
timing JSON to produce a single HTML report with all metrics per round.

Works for any task_type that is NOT object_detection (e.g. classification,
regression). The HTML shows accuracy / loss evolution, per-class metrics,
confusion matrix, timing breakdown, and a per-round summary table.

Usage (from repo root):
    python3 -m FedSolPython.scripts.generate_history_report_classic [options]
  or:
    python3 code/FedSolPython/scripts/generate_history_report_classic.py [options]

Options:
    --rounds N          Number of most-recent rounds to evaluate (default: 10)
    --models-dir PATH   Where .pt files live  (default: output_server/models)
    --stats-dir PATH    Where timing JSONs live (default: output_server/globalResults/stats)
    --config PATH       Client config JSON used to set up evaluation
                        (default: input_client/configClient.json)
    --base-dir PATH     Root results dir for output (default: output_server/globalResults)
    --out PATH          Output HTML path (default: <base-dir>/history_report_classic.html)
    --no-open           Don't open the report in the browser after generating
"""

import argparse
import glob
import json
import os
import sys
import webbrowser
from datetime import datetime


# ---------------------------------------------------------------------------
# Timestamp helpers
# ---------------------------------------------------------------------------

def _parse_model_ts(stem: str) -> datetime:
    """Parse model_20260528_204258 → datetime."""
    part = stem.replace("model_", "")
    for fmt in ("%Y%m%d_%H%M%S", "%Y-%m-%d_%H-%M-%S"):
        try:
            return datetime.strptime(part, fmt)
        except ValueError:
            pass
    return datetime.min


def _parse_timing_ts(ts_str: str) -> datetime:
    """Parse 2026-05-29_00-00-05 → datetime."""
    try:
        return datetime.strptime(ts_str, "%Y-%m-%d_%H-%M-%S")
    except ValueError:
        return datetime.min


def _fmt_ts(dt: datetime) -> str:
    return dt.strftime("%Y-%m-%d %H:%M:%S") if dt != datetime.min else "—"


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def load_models(models_dir: str, n: int) -> list:
    """Return the last `n` .pt files sorted chronologically."""
    files = sorted(glob.glob(os.path.join(models_dir, "model_*.pt")))
    records = []
    for fpath in files:
        stem = os.path.splitext(os.path.basename(fpath))[0]
        dt   = _parse_model_ts(stem)
        records.append({"path": fpath, "stem": stem, "dt": dt})
    records = sorted(records, key=lambda r: r["dt"])
    return records[-n:]


def load_timing_map(stats_dir: str) -> dict:
    """Return dict of datetime → timing record for all timing JSONs."""
    pattern = os.path.join(stats_dir, "timing_*.json")
    result  = {}
    for fpath in sorted(glob.glob(pattern)):
        try:
            with open(fpath) as f:
                d = json.load(f)
            ts = d.get("timestamp", "")
            dt = _parse_timing_ts(ts)
            result[dt] = {
                "timestamp":      ts,
                "training_ms":    d.get("training_time_ms", 0),
                "comm_ms":        d.get("comm_time_ms",     0),
                "agg_ms":         d.get("agg_time_ms",      0),
                "round_total_ms": d.get("round_total_ms",   0),
                "clients":        d.get("clients",          {}),
            }
        except Exception as e:
            print(f"[WARN] Could not read {fpath}: {e}", file=sys.stderr)
    return result


def match_timing(model_dt: datetime, timing_map: dict) -> dict:
    """Return the timing record whose timestamp is closest to model_dt."""
    if not timing_map:
        return {}
    closest = min(timing_map.keys(), key=lambda t: abs((t - model_dt).total_seconds()))
    diff_s  = abs((closest - model_dt).total_seconds())
    if diff_s > 7200:
        return {}
    return timing_map[closest]


# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def evaluate_model(model_path: str, config_path: str, task_type_override: str = None) -> dict:
    """Run the classic evaluator on a single .pt file and return results dict."""
    try:
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
        from FedSolPython.evaluation.Evaluator import Evaluator
    except ImportError:
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../.."))
        from FedSolPython.evaluation.Evaluator import Evaluator

    import torch

    print(f"  Loading model from {model_path} …")
    try:
        raw = torch.load(model_path, map_location="cpu", weights_only=False)
    except Exception as e:
        print(f"  [WARN] Could not load {model_path}: {e}", file=sys.stderr)
        return {}

    if isinstance(raw, dict):
        print(f"  [WARN] .pt is a state_dict — need full model. Skipping.", file=sys.stderr)
        return {}

    ev = Evaluator(config_path=config_path, model=raw)
    try:
        ev.load_config()

        # Override task_type in memory so the evaluator never calls EvaluatorYolo
        if task_type_override:
            ev.config.setdefault("evaluation", {})["task_type"] = task_type_override
            print(f"  [INFO] task_type overridden → {task_type_override}")

        ev.setup_loss()
        ev.setup_device()
        ev.validate_evaluation_config()
    except Exception as e:
        print(f"  [WARN] Config setup failed: {e}", file=sys.stderr)
        return {}

    print(f"  Running classic evaluation …")
    try:
        results = ev.evaluate()
    except Exception as e:
        print(f"  [WARN] Evaluation failed: {e}", file=sys.stderr)
        return {}

    return results


# ---------------------------------------------------------------------------
# HTML builder
# ---------------------------------------------------------------------------

def _js(val) -> str:
    return json.dumps(val, ensure_ascii=False)


def build_html(rounds: list, generated_at: str) -> str:
    """
    rounds: list of dicts with keys:
        round_num, model_stem, model_dt_str, timing_ts,
        accuracy, loss, precision_macro, recall_macro, f1_macro,
        total_samples, confusion_matrix, class_names,
        training_ms, comm_ms, agg_ms, round_total_ms, clients
    """
    n_rounds = len(rounds)
    last     = rounds[-1] if rounds else {}

    # Determine if this looks like classification (has confusion_matrix)
    has_cm  = bool(last.get("confusion_matrix"))
    cm_data = last.get("confusion_matrix", [])
    n_cls   = len(cm_data)
    class_names = last.get("class_names") or [str(i) for i in range(n_cls)]

    rounds_js = _js([{k: v for k, v in r.items() if k != "confusion_matrix"} for r in rounds])
    cm_js     = _js(cm_data)
    cnames_js = _js(class_names)

    def _fmt(v, dec=4):
        return f"{v:.{dec}f}" if isinstance(v, (int, float)) else "—"

    last_acc    = _fmt(last.get("accuracy",          0))
    last_loss   = _fmt(last.get("loss",              0))
    last_prec   = _fmt(last.get("precision_macro",   0))
    last_recall = _fmt(last.get("recall_macro",      0))
    last_f1     = _fmt(last.get("f1_macro",          0))
    last_total  = last.get("round_total_ms", 0)
    last_total_s = f"{last_total / 1000:.1f} s" if last_total else "—"
    last_samples = sum(last.get("clients", {}).values()) if last.get("clients") else last.get("total_samples", "—")

    return f"""<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8"/>
<title>FedSol — History Report (Classic)</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.min.js"></script>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Oxanium:wght@300;500;700;800&family=Fira+Code:wght@400;500&display=swap');
  *, *::before, *::after {{ box-sizing:border-box; margin:0; padding:0; }}
  :root {{
    --bg:#080a0e; --panel:#0e1117; --border:#181e2a;
    --green:#39d353; --blue:#1d8cf8; --red:#ff6b6b;
    --yellow:#ffd166; --purple:#a78bfa; --cyan:#22d3ee;
    --text:#dce8f0; --muted:#4a5568;
    --display:'Oxanium',sans-serif; --mono:'Fira Code',monospace;
  }}
  body {{ background:var(--bg); color:var(--text); font-family:var(--display); min-height:100vh; }}
  body::before {{
    content:''; position:fixed; inset:0; pointer-events:none; z-index:0;
    background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='0.03'/%3E%3C/svg%3E");
  }}
  header {{
    display:flex; align-items:center; gap:16px;
    padding:22px 36px; border-bottom:1px solid var(--border);
    position:relative; z-index:1;
  }}
  .logo-tag {{
    font-family:var(--mono); font-size:10px; letter-spacing:3px;
    color:var(--cyan); border:1px solid var(--cyan);
    padding:3px 10px; border-radius:2px; text-transform:uppercase;
  }}
  header h1 {{ font-size:20px; font-weight:700; letter-spacing:1px; }}
  .header-sub {{ font-family:var(--mono); font-size:11px; color:var(--muted); }}
  .header-ts {{ margin-left:auto; font-family:var(--mono); font-size:11px; color:var(--muted); }}
  .stat-row {{
    display:grid; grid-template-columns:repeat(4,1fr);
    gap:1px; background:var(--border);
    border-bottom:1px solid var(--border); position:relative; z-index:1;
  }}
  .stat {{
    background:var(--panel); padding:22px 28px;
    display:flex; flex-direction:column; gap:6px;
  }}
  .stat-label {{ font-family:var(--mono); font-size:9px; letter-spacing:2px; text-transform:uppercase; color:var(--muted); }}
  .stat-value {{ font-size:36px; font-weight:800; line-height:1; }}
  .stat-value.sm {{ font-size:26px; }}
  .stat-sub   {{ font-family:var(--mono); font-size:10px; color:var(--muted); }}
  .meta-bar {{
    display:flex; gap:10px; align-items:center; flex-wrap:wrap;
    padding:12px 36px; border-bottom:1px solid var(--border);
    position:relative; z-index:1;
  }}
  .pill {{
    font-family:var(--mono); font-size:10px;
    background:var(--panel); border:1px solid var(--border);
    border-radius:20px; padding:3px 12px; color:var(--muted);
  }}
  .pill span {{ color:var(--text); }}
  .section-title {{
    font-family:var(--mono); font-size:9px; letter-spacing:3px; text-transform:uppercase;
    color:var(--muted); padding:20px 36px 0; position:relative; z-index:1;
  }}
  .charts-grid {{
    display:grid; grid-template-columns:1fr 1fr;
    gap:1px; background:var(--border); position:relative; z-index:1;
    margin-top:1px;
  }}
  .chart-panel {{
    background:var(--panel); padding:28px 32px;
    display:flex; flex-direction:column; gap:14px;
  }}
  .chart-panel.full {{ grid-column:1 / -1; }}
  .chart-title {{
    font-size:11px; font-family:var(--mono); letter-spacing:2px;
    text-transform:uppercase; color:var(--muted);
    display:flex; align-items:center; gap:10px;
  }}
  .chart-title .bar {{ width:3px; height:14px; border-radius:2px; flex-shrink:0; }}
  canvas {{ width:100% !important; max-height:300px; }}
  /* confusion matrix */
  .cm-wrap {{ overflow-x:auto; }}
  .cm-table {{ border-collapse:collapse; font-family:var(--mono); font-size:11px; width:100%; }}
  .cm-table th, .cm-table td {{
    padding:6px 10px; border:1px solid var(--border); text-align:center;
  }}
  .cm-table th {{ background:var(--panel); color:var(--muted); font-weight:500; }}
  .cm-table td.diag {{ color:var(--green); font-weight:700; }}
  .cm-table td.off  {{ color:var(--red); }}
  .cm-table td.zero {{ color:var(--border); }}
  /* per-round table */
  .table-wrap {{ overflow-x:auto; position:relative; z-index:1; }}
  table {{ width:100%; border-collapse:collapse; font-family:var(--mono); font-size:11px; }}
  thead th {{
    background:var(--panel); color:var(--muted); font-weight:500;
    letter-spacing:1px; text-transform:uppercase; padding:10px 14px;
    border-bottom:1px solid var(--border); text-align:right;
  }}
  thead th:first-child {{ text-align:left; }}
  tbody tr {{ border-bottom:1px solid var(--border); }}
  tbody tr:hover {{ background:#0e1117cc; }}
  tbody td {{ padding:9px 14px; color:var(--text); text-align:right; }}
  tbody td:first-child {{ text-align:left; color:var(--muted); }}
  .good {{ color:var(--green) !important; }}
  .ok   {{ color:var(--yellow) !important; }}
  .bad  {{ color:var(--red) !important; }}
  footer {{
    text-align:center; padding:28px;
    font-family:var(--mono); font-size:10px; color:var(--muted);
    border-top:1px solid var(--border); position:relative; z-index:1;
  }}
</style>
</head>
<body>

<header>
  <div class="logo-tag">FedSol</div>
  <h1>History Report</h1>
  <div class="header-sub">&nbsp;·&nbsp;classic mode · últimas {n_rounds} rondas</div>
  <div class="header-ts">{generated_at}</div>
</header>

<!-- Summary cards — last round -->
<div class="stat-row">
  <div class="stat">
    <div class="stat-label">Accuracy</div>
    <div class="stat-value" style="color:var(--green)">{last_acc}</div>
    <div class="stat-sub">última ronda evaluada</div>
  </div>
  <div class="stat">
    <div class="stat-label">Loss</div>
    <div class="stat-value" style="color:var(--red)">{last_loss}</div>
    <div class="stat-sub">cross-entropy avg</div>
  </div>
  <div class="stat">
    <div class="stat-label">Precision / Recall</div>
    <div class="stat-value sm" style="color:var(--yellow)">{last_prec} / {last_recall}</div>
    <div class="stat-sub">macro average</div>
  </div>
  <div class="stat">
    <div class="stat-label">F1 macro · Round total</div>
    <div class="stat-value sm" style="color:var(--cyan)">{last_f1} · {last_total_s}</div>
    <div class="stat-sub">{last_samples} samples totales</div>
  </div>
</div>

<div class="meta-bar">
  <div class="pill">rondas <span>{n_rounds}</span></div>
  <div class="pill">clases <span>{n_cls if n_cls else "—"}</span></div>
  <div class="pill">F1 macro <span>{last_f1}</span></div>
  <div class="pill">samples <span>{last.get("total_samples", "—")}</span></div>
</div>

<!-- ── SECTION 1: Metrics per round ── -->
<div class="section-title">① Métricas por ronda</div>
<div class="charts-grid">

  <div class="chart-panel full">
    <div class="chart-title"><div class="bar" style="background:var(--green)"></div>Accuracy y Loss por ronda</div>
    <canvas id="accLossChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--yellow)"></div>Precision, Recall y F1 (macro) por ronda</div>
    <canvas id="prf1Chart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--cyan)"></div>Samples totales por ronda</div>
    <canvas id="samplesChart"></canvas>
  </div>

</div>

<!-- ── SECTION 2: Last round detail ── -->
<div class="section-title">② Detalle — última ronda</div>
<div class="charts-grid">

  {'<div class="chart-panel full"><div class="chart-title"><div class="bar" style="background:var(--purple)"></div>Matriz de Confusión (última ronda)</div><div class="cm-wrap"><table class="cm-table" id="cmTable"></table></div></div>' if has_cm else ''}

</div>

<!-- ── SECTION 3: Timing ── -->
<div class="section-title">③ Tiempos por ronda</div>
<div class="charts-grid">

  <div class="chart-panel full">
    <div class="chart-title"><div class="bar" style="background:var(--cyan)"></div>Desglose de tiempo por ronda (ms)</div>
    <canvas id="timingChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--blue)"></div>Samples por cliente por ronda</div>
    <canvas id="clientSamplesChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--yellow)"></div>Proporción de tiempo por fase (%)</div>
    <canvas id="stackPctChart"></canvas>
  </div>

</div>

<!-- ── SECTION 4: Per-round table ── -->
<div class="section-title">④ Tabla resumen por ronda</div>
<div class="table-wrap">
<table>
  <thead>
    <tr>
      <th>Ronda</th>
      <th>Timestamp</th>
      <th>Accuracy</th>
      <th>Loss</th>
      <th>Precision</th>
      <th>Recall</th>
      <th>F1 macro</th>
      <th>Samples</th>
      <th>Train ms</th>
      <th>Comm ms</th>
      <th>Agg ms</th>
      <th>Total ms</th>
    </tr>
  </thead>
  <tbody id="roundTableBody"></tbody>
</table>
</div>

<footer>FedSol · Classic History Report · {generated_at} · {n_rounds} rondas evaluadas</footer>

<script>
// ── Palette ──────────────────────────────────────────────────────────────────
const G='#39d353', B='#1d8cf8', R='#ff6b6b', Y='#ffd166', P='#a78bfa', C='#22d3ee';
const TICK='#4a5568', FONT={{family:'Fira Code',size:11}};
const base = {{
  responsive:true, maintainAspectRatio:true,
  plugins:{{ legend:{{display:false}},
    tooltip:{{backgroundColor:'#0e1117',borderColor:'#181e2a',borderWidth:1,
              titleColor:'#dce8f0',bodyColor:'#4a5568',titleFont:FONT,bodyFont:FONT}} }},
  scales:{{
    x:{{grid:{{color:'#181e2a'}},ticks:{{color:TICK,font:FONT,maxRotation:45,minRotation:0}}}},
    y:{{grid:{{color:'#181e2a'}},ticks:{{color:TICK,font:FONT}}}},
  }}
}};
function o(extra){{
  return Object.assign({{}},base,extra,{{
    plugins:Object.assign({{}},base.plugins,extra.plugins||{{}}),
    scales:Object.assign({{}},base.scales,extra.scales||{{}})
  }});
}}

// ── Round data ────────────────────────────────────────────────────────────────
const rounds = {rounds_js};
const rLabels = rounds.map(r=>'R'+r.round_num);

// ── 1a. Accuracy + Loss (dual axis) ──────────────────────────────────────────
new Chart(document.getElementById('accLossChart'),{{
  type:'line',
  data:{{
    labels:rLabels,
    datasets:[
      {{label:'Accuracy', data:rounds.map(r=>r.accuracy||0),
        borderColor:G,backgroundColor:'rgba(57,211,83,0.08)',
        borderWidth:2,pointRadius:5,pointBackgroundColor:G,fill:true,tension:0.3,yAxisID:'yAcc'}},
      {{label:'Loss', data:rounds.map(r=>r.loss||0),
        borderColor:R,backgroundColor:'rgba(255,107,107,0.05)',
        borderWidth:2,pointRadius:5,pointBackgroundColor:R,fill:true,tension:0.3,yAxisID:'yLoss'}},
    ]
  }},
  options:o({{
    plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
    scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
      yAcc:{{type:'linear',position:'left',grid:{{color:'#181e2a'}},ticks:{{color:TICK,font:FONT}},
             min:0,max:1,title:{{display:true,text:'Accuracy',color:TICK,font:FONT}}}},
      yLoss:{{type:'linear',position:'right',grid:{{drawOnChartArea:false}},ticks:{{color:TICK,font:FONT}},
              title:{{display:true,text:'Loss',color:TICK,font:FONT}}}},
    }}
  }})
}});

// ── 1b. Precision / Recall / F1 ──────────────────────────────────────────────
new Chart(document.getElementById('prf1Chart'),{{
  type:'line',
  data:{{
    labels:rLabels,
    datasets:[
      {{label:'Precision',data:rounds.map(r=>r.precision_macro||0),borderColor:Y,borderWidth:2,pointRadius:4,pointBackgroundColor:Y,borderDash:[],fill:false,tension:0.3}},
      {{label:'Recall',   data:rounds.map(r=>r.recall_macro||0),   borderColor:B,borderWidth:2,pointRadius:4,pointBackgroundColor:B,borderDash:[4,3],fill:false,tension:0.3}},
      {{label:'F1',       data:rounds.map(r=>r.f1_macro||0),       borderColor:P,borderWidth:2,pointRadius:4,pointBackgroundColor:P,borderDash:[2,2],fill:false,tension:0.3}},
    ]
  }},
  options:o({{
    plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
    scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,min:0,max:1,title:{{display:true,text:'Score',color:TICK,font:FONT}}}},
    }}
  }})
}});

// ── 1c. Samples ───────────────────────────────────────────────────────────────
new Chart(document.getElementById('samplesChart'),{{
  type:'bar',
  data:{{
    labels:rLabels,
    datasets:[{{
      label:'Samples',
      data:rounds.map(r=>r.total_samples||0),
      backgroundColor:C+'99',borderWidth:0,borderRadius:4
    }}]
  }},
  options:o({{
    scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,title:{{display:true,text:'Samples',color:TICK,font:FONT}}}},
    }}
  }})
}});

// ── 2. Confusion Matrix ───────────────────────────────────────────────────────
const cm = {cm_js};
const cnames = {cnames_js};
if (cm && cm.length > 0) {{
  const table = document.getElementById('cmTable');
  if (table) {{
    // Header row
    let hrow = '<tr><th>Real \\ Pred</th>';
    cnames.forEach(c => hrow += `<th>${{c}}</th>`);
    hrow += '</tr>';
    table.innerHTML = hrow;

    // Data rows
    const rowMaxes = cm.map(row => Math.max(...row));
    cm.forEach((row, i) => {{
      const total = row.reduce((a,b)=>a+b,0);
      let tr = `<tr><th>${{cnames[i]}}</th>`;
      row.forEach((v, j) => {{
        const cls = v === 0 ? 'zero' : i === j ? 'diag' : 'off';
        const pct = total > 0 ? (v/total*100).toFixed(1) : '0.0';
        tr += `<td class="${{cls}}" title="${{pct}}%">${{v}}</td>`;
      }});
      tr += '</tr>';
      table.innerHTML += tr;
    }});
  }}
}}

// ── 3a. Timing stacked bar ────────────────────────────────────────────────────
const hasT = rounds.some(r=>r.round_total_ms>0);
if (hasT) {{
  new Chart(document.getElementById('timingChart'),{{
    type:'bar',
    data:{{
      labels:rLabels,
      datasets:[
        {{label:'Entrenamiento',data:rounds.map(r=>r.training_ms||0),backgroundColor:G+'cc',borderWidth:0,borderRadius:3}},
        {{label:'Comunicación', data:rounds.map(r=>r.comm_ms||0),    backgroundColor:B+'cc',borderWidth:0,borderRadius:3}},
        {{label:'Agregación',   data:rounds.map(r=>r.agg_ms||0),     backgroundColor:Y+'cc',borderWidth:0,borderRadius:3}},
      ]
    }},
    options:o({{
      plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
      scales:{{
        x:{{...base.scales.x,stacked:true,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
        y:{{...base.scales.y,stacked:true,title:{{display:true,text:'ms',color:TICK,font:FONT}}}},
      }}
    }})
  }});

  // ── 3b. Samples per client ──────────────────────────────────────────────────
  const allCids=[...new Set(rounds.flatMap(r=>Object.keys(r.clients||{{}})))].sort();
  const cColors=[G,B,R,Y,P,C];
  new Chart(document.getElementById('clientSamplesChart'),{{
    type:'line',
    data:{{
      labels:rLabels,
      datasets:allCids.map((cid,idx)=>{{
        const col=cColors[idx%cColors.length];
        return {{label:'Client '+cid,data:rounds.map(r=>(r.clients||{{}})[cid]||0),
          borderColor:col,backgroundColor:col+'22',
          borderWidth:2,pointRadius:4,pointBackgroundColor:col,fill:false,tension:0.3}};
      }})
    }},
    options:o({{
      plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
      scales:{{
        x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
        y:{{...base.scales.y,title:{{display:true,text:'Samples',color:TICK,font:FONT}}}},
      }}
    }})
  }});

  // ── 3c. Stacked % breakdown ─────────────────────────────────────────────────
  new Chart(document.getElementById('stackPctChart'),{{
    type:'bar',
    data:{{
      labels:rLabels,
      datasets:[
        {{label:'Entrenamiento %',data:rounds.map(r=>r.round_total_ms?+(r.training_ms/r.round_total_ms*100).toFixed(1):0),backgroundColor:G+'cc',borderWidth:0,borderRadius:3}},
        {{label:'Comunicación %', data:rounds.map(r=>r.round_total_ms?+(r.comm_ms    /r.round_total_ms*100).toFixed(1):0),backgroundColor:B+'cc',borderWidth:0,borderRadius:3}},
        {{label:'Agregación %',   data:rounds.map(r=>r.round_total_ms?+(r.agg_ms     /r.round_total_ms*100).toFixed(1):0),backgroundColor:Y+'cc',borderWidth:0,borderRadius:3}},
      ]
    }},
    options:o({{
      plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
      scales:{{
        x:{{...base.scales.x,stacked:true,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
        y:{{...base.scales.y,stacked:true,min:0,max:100,title:{{display:true,text:'%',color:TICK,font:FONT}}}},
      }}
    }})
  }});
}}

// ── 4. Per-round table ────────────────────────────────────────────────────────
function cls(v, lo=0.7, hi=0.9) {{
  if(v===null||v===undefined) return '';
  return v>=hi?'good':v>=lo?'ok':'bad';
}}
function clsLoss(v) {{
  // lower is better for loss
  if(v===null||v===undefined) return '';
  return v<0.2?'good':v<0.5?'ok':'bad';
}}
const tbody = document.getElementById('roundTableBody');
rounds.forEach(r=>{{
  const samples = r.total_samples || Object.values(r.clients||{{}}).reduce((a,b)=>a+b,0) || '—';
  const tr = document.createElement('tr');
  tr.innerHTML = `
    <td>R${{r.round_num}}</td>
    <td>${{r.timing_ts||r.model_stem}}</td>
    <td class="${{cls(r.accuracy)}}">${{r.accuracy?.toFixed(4)??'—'}}</td>
    <td class="${{clsLoss(r.loss)}}">${{r.loss?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.precision_macro)}}">${{r.precision_macro?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.recall_macro)}}">${{r.recall_macro?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.f1_macro)}}">${{r.f1_macro?.toFixed(4)??'—'}}</td>
    <td>${{samples}}</td>
    <td>${{r.training_ms??'—'}}</td>
    <td>${{r.comm_ms??'—'}}</td>
    <td>${{r.agg_ms??'—'}}</td>
    <td>${{r.round_total_ms??'—'}}</td>
  `;
  tbody.appendChild(tr);
}});
</script>
</body>
</html>"""


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="FedSol per-round history report for classic (non-YOLO) models."
    )
    parser.add_argument("--rounds",     type=int,   default=10,                              help="Number of most-recent rounds (default: 10)")
    parser.add_argument("--models-dir", default="output_server/models",                      help="Directory with .pt model files")
    parser.add_argument("--stats-dir",  default="output_server/globalResults/stats",         help="Directory with timing_*.json files")
    parser.add_argument("--config",     default="config.json",                               help="Server config JSON (the same one passed to the server binary, default: config.json)")
    parser.add_argument("--base-dir",   default="output_server/globalResults",               help="Root results dir for output HTML")
    parser.add_argument("--out",        default=None,                                        help="Output HTML path (default: <base-dir>/history_report_classic.html)")
    parser.add_argument("--task-type",  default=None,                                        help="Override task_type in config (e.g. 'classification', 'regression'). Useful when the config on disk is set to object_detection but you're evaluating a classic model.")
    parser.add_argument("--no-open",    action="store_true",                                 help="Don't open the browser after generating")
    args = parser.parse_args()

    out_path = args.out or os.path.join(args.base_dir, "history_report_classic.html")

    for label, path in [("--models-dir", args.models_dir), ("--config", args.config)]:
        if not os.path.exists(path):
            print(f"[ERROR] {label} not found: {path}", file=sys.stderr)
            sys.exit(1)

    print(f"[History Report Classic] models-dir : {args.models_dir}")
    print(f"[History Report Classic] stats-dir  : {args.stats_dir}")
    print(f"[History Report Classic] server config : {args.config}")
    print(f"[History Report Classic] rounds     : {args.rounds}")

    models     = load_models(args.models_dir, args.rounds)
    timing_map = load_timing_map(args.stats_dir)

    print(f"[History Report Classic] models found   : {len(models)}")
    print(f"[History Report Classic] timing records : {len(timing_map)}")

    if not models:
        print("[ERROR] No model files found. Check --models-dir.", file=sys.stderr)
        sys.exit(1)

    rounds = []
    for i, m in enumerate(models, start=1):
        print(f"\n[Round {i}/{len(models)}] {m['stem']}  ({_fmt_ts(m['dt'])})")

        timing = match_timing(m["dt"], timing_map)
        if not timing:
            print(f"  [WARN] No matching timing found — timing fields will be empty.")

        results = evaluate_model(m["path"], args.config, task_type_override=args.task_type)
        if not results:
            continue

        rounds.append({
            "round_num":    i,
            "model_stem":   m["stem"],
            "model_dt_str": _fmt_ts(m["dt"]),
            "timing_ts":    timing.get("timestamp", ""),
            # metrics
            "accuracy":         results.get("accuracy",         None),
            "loss":             results.get("loss",             None),
            "precision_macro":  results.get("precision_macro",  None),
            "recall_macro":     results.get("recall_macro",     None),
            "f1_macro":         results.get("f1_macro",         None),
            "total_samples":    results.get("total_samples",    None),
            "confusion_matrix": results.get("confusion_matrix", []),
            "class_names":      results.get("class_names",      None),
            # timing
            "training_ms":    timing.get("training_ms",    None),
            "comm_ms":        timing.get("comm_ms",        None),
            "agg_ms":         timing.get("agg_ms",         None),
            "round_total_ms": timing.get("round_total_ms", None),
            "clients":        timing.get("clients",        {}),
        })

    if not rounds:
        print("[ERROR] No rounds could be evaluated.", file=sys.stderr)
        sys.exit(1)

    print(f"\n[History Report Classic] Generating HTML for {len(rounds)} rounds …")
    generated_at = datetime.now().strftime("%Y-%m-%d  %H:%M:%S")
    html = build_html(rounds, generated_at)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)

    print(f"[History Report Classic] HTML → {out_path}")

    if not args.no_open:
        webbrowser.open(f"file://{os.path.abspath(out_path)}")


if __name__ == "__main__":
    main()