#!/usr/bin/env python3
"""
generate_history_report.py — FedSol per-round history report CLI

For each of the last N global models saved in output_server/models/, runs
EvaluatorYolo and crosses with the matching timing JSON to produce a single
HTML report with all metrics per round — suitable for a research paper.

Usage (from repo root):
    python3 -m FedSolPython.scripts.generate_history_report [options]
  or:
    python3 code/FedSolPython/scripts/generate_history_report.py [options]

Options:
    --rounds N          Number of most-recent rounds to evaluate (default: 10)
    --models-dir PATH   Where .pt files live  (default: output_server/models)
    --stats-dir PATH    Where timing JSONs live (default: output_server/globalResults/stats)
    --val-path PATH     Val images directory   (default: input_server/dataset/val/images)
    --base-dir PATH     Root results dir for output (default: output_server/globalResults)
    --out PATH          Output HTML path (default: <base-dir>/history_report.html)
    --conf FLOAT        Confidence threshold for eval (default: 0.25)
    --imgsz INT         Image size for inference (default: 640)
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

def load_models(models_dir: str, n: int) -> list[dict]:
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
    if diff_s > 7200:   # >2 h apart → probably unrelated
        return {}
    return timing_map[closest]


# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def evaluate_model(model_path: str, val_path: str, conf: float, imgsz: int) -> dict:
    """Run EvaluatorYolo on a single .pt file and return the results dict."""
    # Import here so the script can be imported without ultralytics installed
    try:
        from FedSolPython.evaluation.EvaluatorYolo import EvaluatorYolo, load_model_robust
    except ImportError:
        # Fallback path when running as a plain script from repo root
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
        from evaluation.EvaluatorYolo import EvaluatorYolo, load_model_robust

    config = {
        "dataset": {
            "val_path": val_path,
        },
        "evaluation": {
            "prediction_config": {
                "conf": conf,
                "iou":  0.45,
            },
        },
        "training": {
            "imgsz": imgsz,
        },
    }

    print(f"  Loading model …")
    model = load_model_robust(model_path)
    if model is None:
        print(f"  [WARN] Could not load {model_path}, skipping.")
        return {}

    print(f"  Running evaluation …")
    ev      = EvaluatorYolo(model, config)
    results = ev.evaluate()
    return results


# ---------------------------------------------------------------------------
# HTML
# ---------------------------------------------------------------------------

def _js(val) -> str:
    return json.dumps(val, ensure_ascii=False)


def build_html(rounds: list[dict], generated_at: str) -> str:
    """
    rounds: list of dicts with keys:
        round_num, model_stem, model_dt_str,
        map50, map50_95, precision, recall,
        inference_ms, iou_tp_mean, ap_small, ap_medium, ap_large,
        n_images, total_gt, total_preds, tp, fp, fn, best_f1, best_f1_conf,
        training_ms, comm_ms, agg_ms, round_total_ms, clients,
        timing_ts,
        curves: {pr, f1, conf_hist},
        extras: {iou_tp_hist, dets_per_image}
    """
    n_rounds   = len(rounds)
    last       = rounds[-1] if rounds else {}

    rounds_js  = _js([{k: v for k, v in r.items() if k not in ("curves", "extras")} for r in rounds])
    # Last round curves for detail charts
    last_curves = last.get("curves", {})
    last_extras = last.get("extras", {})

    pr_data   = last_curves.get("pr",        {"recalls": [], "precisions": []})
    f1_data   = last_curves.get("f1",        {"confs":   [], "f1s": []})
    hist_data = last_curves.get("conf_hist", {"confs":   []})
    iou_hist  = last_extras.get("iou_tp_hist",    [0]*10)
    dets_img  = last_extras.get("dets_per_image", [])

    # Summary from last round
    def _fmt(v, dec=4): return f"{v:.{dec}f}" if isinstance(v, (int, float)) else "—"

    last_map50     = _fmt(last.get("map50",     0))
    last_map5095   = _fmt(last.get("map50_95",  0))
    last_prec      = _fmt(last.get("precision", 0))
    last_recall    = _fmt(last.get("recall",    0))
    last_infer     = f"{last.get('inference_ms', 0):.1f} ms"
    last_iou_mean  = _fmt(last.get("iou_tp_mean", 0))
    last_total_s   = f"{last.get('round_total_ms', 0) / 1000:.1f} s"
    last_samples   = sum(last.get("clients", {}).values()) if last.get("clients") else 0

    return f"""<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8"/>
<title>FedSol — History Report</title>
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
  .stat-sub   {{ font-family:var(--mono); font-size:10px; color:var(--muted); }}
  .stat-value.sm {{ font-size:26px; }}
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
  canvas {{ width:100% !important; max-height:280px; }}
  /* round table */
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
  <div class="header-sub">&nbsp;·&nbsp;últimas {n_rounds} rondas</div>
  <div class="header-ts">{generated_at}</div>
</header>

<!-- Summary cards — last round -->
<div class="stat-row">
  <div class="stat">
    <div class="stat-label">mAP @ 0.50</div>
    <div class="stat-value" style="color:var(--green)">{last_map50}</div>
    <div class="stat-sub">última ronda evaluada</div>
  </div>
  <div class="stat">
    <div class="stat-label">mAP @ 0.50:0.95</div>
    <div class="stat-value" style="color:var(--blue)">{last_map5095}</div>
    <div class="stat-sub">COCO standard</div>
  </div>
  <div class="stat">
    <div class="stat-label">Precision / Recall</div>
    <div class="stat-value sm" style="color:var(--yellow)">{last_prec} / {last_recall}</div>
    <div class="stat-sub">conf ≥ {last.get('best_f1_conf', 0.25):.2f}</div>
  </div>
  <div class="stat">
    <div class="stat-label">Inference · Round total</div>
    <div class="stat-value sm" style="color:var(--cyan)">{last_infer} · {last_total_s}</div>
    <div class="stat-sub">{last_samples} samples · IoU TP medio {last_iou_mean}</div>
  </div>
</div>

<!-- Meta pills -->
<div class="meta-bar">
  <div class="pill">rondas <span>{n_rounds}</span></div>
  <div class="pill">imágenes val <span>{last.get('n_images', '—')}</span></div>
  <div class="pill">GT boxes <span>{last.get('total_gt', '—')}</span></div>
  <div class="pill">TP <span>{last.get('tp', '—')}</span></div>
  <div class="pill">FP <span>{last.get('fp', '—')}</span></div>
  <div class="pill">FN <span>{last.get('fn', '—')}</span></div>
  <div class="pill">best F1 <span>{_fmt(last.get('best_f1', 0), 3)}</span> @ conf <span>{_fmt(last.get('best_f1_conf', 0), 2)}</span></div>
</div>

<!-- ── SECTION 1: Metrics per round ── -->
<div class="section-title">① Métricas por ronda</div>
<div class="charts-grid">

  <div class="chart-panel full">
    <div class="chart-title"><div class="bar" style="background:var(--green)"></div>Evolución mAP, Precision y Recall</div>
    <canvas id="mapChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--purple)"></div>AP por tamaño de objeto</div>
    <canvas id="apSizeChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--cyan)"></div>Inference ms/imagen por ronda</div>
    <canvas id="inferChart"></canvas>
  </div>

</div>

<!-- ── SECTION 2: Detection quality (last round) ── -->
<div class="section-title">② Calidad de detección — última ronda</div>
<div class="charts-grid">

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--green)"></div>Curva Precision-Recall (IoU=0.50)</div>
    <canvas id="prChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--blue)"></div>F1 vs Confidence Threshold</div>
    <canvas id="f1Chart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--red)"></div>Histograma de Confianzas</div>
    <canvas id="confHistChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--purple)"></div>Histograma IoU de TPs</div>
    <canvas id="iouHistChart"></canvas>
  </div>

  <div class="chart-panel full">
    <div class="chart-title"><div class="bar" style="background:var(--yellow)"></div>Detecciones por imagen</div>
    <canvas id="detsImgChart"></canvas>
  </div>

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
    <canvas id="samplesChart"></canvas>
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
      <th>mAP50</th>
      <th>mAP50-95</th>
      <th>Precision</th>
      <th>Recall</th>
      <th>Best F1</th>
      <th>Infer ms/img</th>
      <th>IoU TP medio</th>
      <th>AP Small</th>
      <th>AP Med</th>
      <th>TP</th><th>FP</th><th>FN</th>
      <th>Train ms</th>
      <th>Comm ms</th>
      <th>Agg ms</th>
      <th>Total ms</th>
      <th>Samples</th>
    </tr>
  </thead>
  <tbody id="roundTableBody"></tbody>
</table>
</div>

<footer>FedSol · History Report · {generated_at} · {n_rounds} rondas evaluadas</footer>

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

// ── 1a. mAP + Precision + Recall evolution ───────────────────────────────────
new Chart(document.getElementById('mapChart'),{{
  type:'line',
  data:{{
    labels:rLabels,
    datasets:[
      {{label:'mAP50',    data:rounds.map(r=>r.map50),    borderColor:G,backgroundColor:'rgba(57,211,83,0.08)',  borderWidth:2,pointRadius:5,pointBackgroundColor:G,fill:true, tension:0.3}},
      {{label:'mAP50-95', data:rounds.map(r=>r.map50_95), borderColor:B,backgroundColor:'rgba(29,140,248,0.05)',borderWidth:2,pointRadius:5,pointBackgroundColor:B,fill:true, tension:0.3}},
      {{label:'Precision',data:rounds.map(r=>r.precision),borderColor:Y,borderWidth:1.5,pointRadius:3,pointBackgroundColor:Y,borderDash:[4,3],fill:false,tension:0.3}},
      {{label:'Recall',   data:rounds.map(r=>r.recall),   borderColor:R,borderWidth:1.5,pointRadius:3,pointBackgroundColor:R,borderDash:[4,3],fill:false,tension:0.3}},
    ]
  }},
  options:o({{
    plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
    scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,min:0,max:1,title:{{display:true,text:'Score',color:TICK,font:FONT}}}}
    }}
  }})
}});

// ── 1b. AP by size per round ─────────────────────────────────────────────────
new Chart(document.getElementById('apSizeChart'),{{
  type:'bar',
  data:{{
    labels:rLabels,
    datasets:[
      {{label:'AP Small',  data:rounds.map(r=>r.ap_small),  backgroundColor:G+'bb',borderWidth:0,borderRadius:3}},
      {{label:'AP Medium', data:rounds.map(r=>r.ap_medium), backgroundColor:B+'bb',borderWidth:0,borderRadius:3}},
      {{label:'AP Large',  data:rounds.map(r=>r.ap_large),  backgroundColor:Y+'bb',borderWidth:0,borderRadius:3}},
    ]
  }},
  options:o({{
    plugins:{{legend:{{display:true,labels:{{color:TICK,font:FONT,boxWidth:12}}}}}},
    scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,min:0,max:1,title:{{display:true,text:'AP',color:TICK,font:FONT}}}}
    }}
  }})
}});

// ── 1c. Inference ms per round ───────────────────────────────────────────────
new Chart(document.getElementById('inferChart'),{{
  type:'line',
  data:{{
    labels:rLabels,
    datasets:[{{
      label:'Inference ms/img',
      data:rounds.map(r=>r.inference_ms),
      borderColor:C,backgroundColor:'rgba(34,211,238,0.06)',
      borderWidth:2,pointRadius:4,pointBackgroundColor:C,fill:true,tension:0.3
    }}]
  }},
  options:o({{
    scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Ronda',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,title:{{display:true,text:'ms',color:TICK,font:FONT}}}}
    }}
  }})
}});

// ── 2a. PR curve (last round) ─────────────────────────────────────────────────
const pr = {_js(pr_data)};
if(pr.recalls.length>0){{
  new Chart(document.getElementById('prChart'),{{
    type:'line',
    data:{{labels:pr.recalls.map(v=>v.toFixed(2)),
      datasets:[{{label:'Precision',data:pr.precisions,
        borderColor:G,backgroundColor:'rgba(57,211,83,0.08)',
        borderWidth:2,pointRadius:0,fill:true,tension:0.3}}]}},
    options:o({{scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Recall',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,min:0,max:1,title:{{display:true,text:'Precision',color:TICK,font:FONT}}}}
    }}}})
  }});
}}

// ── 2b. F1 curve (last round) ─────────────────────────────────────────────────
const f1 = {_js(f1_data)};
if(f1.confs.length>0){{
  new Chart(document.getElementById('f1Chart'),{{
    type:'line',
    data:{{labels:f1.confs.map(v=>v.toFixed(2)),
      datasets:[{{label:'F1',data:f1.f1s,
        borderColor:B,backgroundColor:'rgba(29,140,248,0.08)',
        borderWidth:2,pointRadius:0,fill:true,tension:0.3}}]}},
    options:o({{scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Confidence',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,min:0,max:1,title:{{display:true,text:'F1',color:TICK,font:FONT}}}}
    }}}})
  }});
}}

// ── 2c. Confidence histogram (last round) ────────────────────────────────────
const cb = {_js(hist_data.get('confs', []) if isinstance(hist_data, dict) else [])};
if(cb.length>0){{
  new Chart(document.getElementById('confHistChart'),{{
    type:'bar',
    data:{{labels:cb.map((_,i)=>(i*0.05).toFixed(2)),
      datasets:[{{data:cb,
        backgroundColor:cb.map((_,i)=>`rgba(${{Math.round(255*(1-i/20))}},${{Math.round(211*i/20)}},${{Math.round(83*i/20)}},0.85)`),
        borderWidth:0,borderRadius:2}}]}},
    options:o({{scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Confidence',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,title:{{display:true,text:'Dets',color:TICK,font:FONT}}}}
    }}}})
  }});
}}

// ── 2d. IoU TP histogram (last round) ────────────────────────────────────────
const iouH = {_js(iou_hist)};
const iouLabels = iouH.map((_,i)=>((0.50+i*0.05).toFixed(2)));
new Chart(document.getElementById('iouHistChart'),{{
  type:'bar',
  data:{{labels:iouLabels,
    datasets:[{{data:iouH,
      backgroundColor:iouH.map((_,i)=>`rgba(${{Math.round(167-i*10)}},${{Math.round(139+i*10)}},250,0.85)`),
      borderWidth:0,borderRadius:2}}]}},
  options:o({{scales:{{
    x:{{...base.scales.x,title:{{display:true,text:'IoU',color:TICK,font:FONT}}}},
    y:{{...base.scales.y,title:{{display:true,text:'TP count',color:TICK,font:FONT}}}}
  }}}})
}});

// ── 2e. Detections per image (last round) ────────────────────────────────────
const dets = {_js(dets_img)};
if(dets.length>0){{
  new Chart(document.getElementById('detsImgChart'),{{
    type:'bar',
    data:{{labels:dets.map((_,i)=>'img '+(i+1)),
      datasets:[{{data:dets,backgroundColor:'rgba(255,209,102,0.75)',borderWidth:0,borderRadius:2}}]}},
    options:o({{scales:{{
      x:{{...base.scales.x,title:{{display:true,text:'Imagen',color:TICK,font:FONT}}}},
      y:{{...base.scales.y,title:{{display:true,text:'Detecciones',color:TICK,font:FONT}}}}
    }}}})
  }});
}}

// ── 3a. Timing stacked bar ───────────────────────────────────────────────────
const hasT = rounds.some(r=>r.round_total_ms>0);
if(hasT){{
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

  // ── 3b. Samples per client ───────────────────────────────────────────────
  const allCids=[...new Set(rounds.flatMap(r=>Object.keys(r.clients||{{}})))].sort();
  const cColors=[G,B,R,Y,P,C];
  new Chart(document.getElementById('samplesChart'),{{
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

  // ── 3c. Stacked % breakdown ───────────────────────────────────────────────
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
function cls(v, lo=0.6, hi=0.8) {{
  if(v===null||v===undefined) return '';
  return v>=hi?'good':v>=lo?'ok':'bad';
}}
const tbody = document.getElementById('roundTableBody');
rounds.forEach(r=>{{
  const samples = Object.values(r.clients||{{}}).reduce((a,b)=>a+b,0);
  const tr = document.createElement('tr');
  tr.innerHTML = `
    <td>R${{r.round_num}}</td>
    <td>${{r.timing_ts||r.model_stem}}</td>
    <td class="${{cls(r.map50)}}">${{r.map50?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.map50_95,0.3,0.5)}}">${{r.map50_95?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.precision)}}">${{r.precision?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.recall)}}">${{r.recall?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.best_f1)}}">${{r.best_f1?.toFixed(3)??'—'}}</td>
    <td>${{r.inference_ms?.toFixed(1)??'—'}}</td>
    <td class="${{cls(r.iou_tp_mean)}}">${{r.iou_tp_mean?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.ap_small)}}">${{r.ap_small?.toFixed(4)??'—'}}</td>
    <td class="${{cls(r.ap_medium)}}">${{r.ap_medium?.toFixed(4)??'—'}}</td>
    <td>${{r.tp??'—'}}</td><td>${{r.fp??'—'}}</td><td>${{r.fn??'—'}}</td>
    <td>${{r.training_ms??'—'}}</td>
    <td>${{r.comm_ms??'—'}}</td>
    <td>${{r.agg_ms??'—'}}</td>
    <td>${{r.round_total_ms??'—'}}</td>
    <td>${{samples||'—'}}</td>
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
        description="FedSol per-round history report — evaluates last N models and crosses with timing."
    )
    parser.add_argument("--rounds",     type=int,   default=10,       metavar="N",    help="Number of most-recent rounds (default: 10)")
    parser.add_argument("--models-dir", default="output_server/models",               help="Directory with .pt model files")
    parser.add_argument("--stats-dir",  default="output_server/globalResults/stats",  help="Directory with timing_*.json files")
    parser.add_argument("--val-path",   default="input_server/dataset/val/images",    help="Validation images directory")
    parser.add_argument("--base-dir",   default="output_server/globalResults",        help="Root results dir for output HTML")
    parser.add_argument("--out",        default=None,                                 help="Output HTML path (default: <base-dir>/history_report.html)")
    parser.add_argument("--conf",       type=float, default=0.25,                     help="Confidence threshold (default: 0.25)")
    parser.add_argument("--imgsz",      type=int,   default=640,                      help="Image size for inference (default: 640)")
    parser.add_argument("--no-open",    action="store_true",                          help="Don't open the browser after generating")
    args = parser.parse_args()

    out_path = args.out or os.path.join(args.base_dir, "history_report.html")

    # Validate dirs
    for label, path in [("--models-dir", args.models_dir), ("--val-path", args.val_path)]:
        if not os.path.exists(path):
            print(f"[ERROR] {label} not found: {path}", file=sys.stderr)
            sys.exit(1)

    print(f"[History Report] models-dir : {args.models_dir}")
    print(f"[History Report] stats-dir  : {args.stats_dir}")
    print(f"[History Report] val-path   : {args.val_path}")
    print(f"[History Report] rounds     : {args.rounds}")

    models     = load_models(args.models_dir, args.rounds)
    timing_map = load_timing_map(args.stats_dir)

    print(f"[History Report] models found       : {len(models)}")
    print(f"[History Report] timing records     : {len(timing_map)}")

    if not models:
        print("[ERROR] No model files found. Check --models-dir.", file=sys.stderr)
        sys.exit(1)

    rounds = []
    for i, m in enumerate(models, start=1):
        print(f"\n[Round {i}/{len(models)}] {m['stem']}  ({_fmt_ts(m['dt'])})")

        timing = match_timing(m["dt"], timing_map)
        if not timing:
            print(f"  [WARN] No matching timing found — timing fields will be empty.")

        results = evaluate_model(m["path"], args.val_path, args.conf, args.imgsz)
        if not results:
            continue

        meta   = results.get("_meta",   {})
        extras = results.get("extras",  {})
        curves = results.get("curves",  {})
        f1d    = curves.get("f1",       {"confs": [], "f1s": []})

        best_f1, best_f1_conf = 0.0, args.conf
        if f1d["f1s"]:
            idx          = max(range(len(f1d["f1s"])), key=lambda k: f1d["f1s"][k])
            best_f1      = f1d["f1s"][idx]
            best_f1_conf = f1d["confs"][idx]

        tp = meta.get("op_tp", meta.get("tp50", 0))
        fp = meta.get("op_fp", 0)
        fn = meta.get("op_fn", 0)

        rounds.append({
            "round_num":    i,
            "model_stem":   m["stem"],
            "model_dt_str": _fmt_ts(m["dt"]),
            "timing_ts":    timing.get("timestamp", ""),
            # metrics
            "map50":        results.get("map50",     0),
            "map50_95":     results.get("map50_95",  0),
            "precision":    results.get("precision", 0),
            "recall":       results.get("recall",    0),
            "best_f1":      best_f1,
            "best_f1_conf": best_f1_conf,
            "inference_ms": extras.get("inference_ms",  0.0),
            "iou_tp_mean":  extras.get("iou_tp_mean",   0.0),
            "ap_small":     extras.get("ap_small",      0.0),
            "ap_medium":    extras.get("ap_medium",     0.0),
            "ap_large":     extras.get("ap_large",      0.0),
            "n_images":     meta.get("images",      0),
            "total_gt":     meta.get("total_gt",    0),
            "total_preds":  meta.get("total_preds", 0),
            "tp": tp, "fp": fp, "fn": fn,
            # timing
            "training_ms":    timing.get("training_ms",    None),
            "comm_ms":        timing.get("comm_ms",        None),
            "agg_ms":         timing.get("agg_ms",         None),
            "round_total_ms": timing.get("round_total_ms", None),
            "clients":        timing.get("clients",        {}),
            # curves + extras (for last-round detail charts)
            "curves":  curves,
            "extras":  extras,
        })

    if not rounds:
        print("[ERROR] No rounds could be evaluated.", file=sys.stderr)
        sys.exit(1)

    print(f"\n[History Report] Generating HTML for {len(rounds)} rounds …")
    generated_at = datetime.now().strftime("%Y-%m-%d  %H:%M:%S")
    html = build_html(rounds, generated_at)

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)

    print(f"[History Report] HTML → {out_path}")

    if not args.no_open:
        webbrowser.open(f"file://{os.path.abspath(out_path)}")


if __name__ == "__main__":
    main()