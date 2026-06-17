import json
import os
import glob
from datetime import datetime


class DefaultReportYolo:
    """
    Generates an interactive HTML evaluation report for YOLO models.
    Called by DefaultReport when the results dict contains curve data.

    Panels:
      Row 1 — stat cards: mAP50, mAP50-95, Precision, Recall
      Row 2 — extra cards: Inference ms/img, IoU TP mean, AP small, AP medium/large
      Charts:
        1. Precision-Recall curve
        2. F1 vs Confidence
        3. Confidence histogram
        4. TP/FP/FN bars  (full width)
        5. IoU of TP histogram
        6. AP small / medium / large bars
        7. Detections per image distribution
      Footer timing pills: training ms, comm ms, agg ms, total ms, samples per client
    """

    @staticmethod
    def generate(results: dict, params: dict, html_path: str):
        curves   = results.get("curves", {})
        extras   = results.get("extras", {})
        meta     = results.get("_meta", {})
        base_dir = params.get("base_dir", "output_server/globalResults")

        last_timing = DefaultReportYolo._load_last_timing(base_dir)
        html = DefaultReportYolo._build_html(results, curves, extras, meta, last_timing)

        os.makedirs(os.path.dirname(html_path), exist_ok=True)
        with open(html_path, "w", encoding="utf-8") as f:
            f.write(html)

        print(f"[Report] HTML  → {html_path}")

    # ── Last timing only ──────────────────────────────────────────────────────

    @staticmethod
    def _load_last_timing(base_dir: str) -> dict:
        """Devuelve solo el timing JSON más reciente."""
        pattern = os.path.join(base_dir, "stats", "timing_*.json")
        files   = sorted(glob.glob(pattern))
        if not files:
            return {}
        try:
            with open(files[-1]) as f:
                d = json.load(f)
            return {
                "timestamp":      d.get("timestamp", ""),
                "training_ms":    d.get("training_time_ms", 0),
                "comm_ms":        d.get("comm_time_ms", 0),
                "agg_ms":         d.get("agg_time_ms", 0),
                "round_total_ms": d.get("round_total_ms", 0),
                "clients":        d.get("clients", {}),
            }
        except Exception:
            return {}

    # ── HTML ──────────────────────────────────────────────────────────────────

    @staticmethod
    def _build_html(results, curves, extras, meta, timing=None) -> str:
        ts        = datetime.now().strftime("%Y-%m-%d  %H:%M:%S")
        map50     = results.get("map50",     0)
        map50_95  = results.get("map50_95",  0)
        precision = results.get("precision", 0)
        recall    = results.get("recall",    0)

        pr_data   = curves.get("pr",        {"recalls": [], "precisions": []})
        f1_data   = curves.get("f1",        {"confs": [], "f1s": []})
        hist_data = curves.get("conf_hist", {"confs": []})

        infer_ms  = extras.get("inference_ms",   0.0)
        iou_mean  = extras.get("iou_tp_mean",    0.0)
        iou_hist  = extras.get("iou_tp_hist",    [0]*10)
        ap_small  = extras.get("ap_small",       0.0)
        ap_medium = extras.get("ap_medium",      0.0)
        ap_large  = extras.get("ap_large",       0.0)
        dets_img  = extras.get("dets_per_image", [])

        n_images   = meta.get("images",      "—")
        total_gt   = meta.get("total_gt",    "—")
        total_pred = meta.get("op_tp", 0) + meta.get("op_fp", 0)
        tp50       = meta.get("op_tp",       0)
        conf_thr   = meta.get("conf_thresh", 0.25)

        fp   = meta.get("op_fp", max((meta.get("total_preds", 0) - tp50) if isinstance(meta.get("total_preds"), int) else 0, 0))
        fn   = meta.get("op_fn", max((meta.get("total_gt",    0) - tp50) if isinstance(meta.get("total_gt"),    int) else 0, 0))
        tp50 = meta.get("op_tp", tp50)

        best_f1 = 0.0
        best_f1_conf = conf_thr
        if f1_data["f1s"]:
            idx          = int(max(range(len(f1_data["f1s"])), key=lambda i: f1_data["f1s"][i]))
            best_f1      = f1_data["f1s"][idx]
            best_f1_conf = f1_data["confs"][idx]

        raw_confs = hist_data["confs"]
        conf_bins = [0] * 20
        for c in raw_confs:
            conf_bins[min(int(c * 20), 19)] += 1

        dets_labels = [str(i) for i in range(len(dets_img))]

        # Timing pills
        t = timing or {}
        t_train   = t.get("training_ms",    "—")
        t_comm    = t.get("comm_ms",        "—")
        t_agg     = t.get("agg_ms",         "—")
        t_total   = t.get("round_total_ms", "—")
        t_ts      = t.get("timestamp",      "—")
        clients   = t.get("clients", {})
        samples_pills = "".join(
            f'<div class="pill">client {cid} <span>{n}</span></div>'
            for cid, n in sorted(clients.items())
        )

        pr_json       = json.dumps(pr_data)
        f1_json       = json.dumps(f1_data)
        conf_bins_json= json.dumps(conf_bins)
        cm_json       = json.dumps({"labels": ["TP", "FP", "FN"], "values": [tp50, fp, fn]})
        iou_hist_json = json.dumps(iou_hist)
        ap_size_json  = json.dumps({
            "labels": ["Small\n(<32²px)", "Medium\n(32²–96²px)", "Large\n(>96²px)"],
            "values": [ap_small, ap_medium, ap_large]
        })
        dets_json     = json.dumps({"labels": dets_labels, "values": dets_img})

        return f"""<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8"/>
<title>FedSol — Reporte de Evaluación</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.min.js"></script>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Oxanium:wght@300;500;700;800&family=Fira+Code:wght@400;500&display=swap');
  *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
  :root {{
    --bg:      #080a0e;
    --panel:   #0e1117;
    --border:  #181e2a;
    --green:   #39d353;
    --blue:    #1d8cf8;
    --red:     #ff6b6b;
    --yellow:  #ffd166;
    --purple:  #a78bfa;
    --cyan:    #22d3ee;
    --text:    #dce8f0;
    --muted:   #4a5568;
    --display: 'Oxanium', sans-serif;
    --mono:    'Fira Code', monospace;
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
    color:var(--green); border:1px solid var(--green);
    padding:3px 10px; border-radius:2px; text-transform:uppercase;
  }}
  header h1 {{ font-size:20px; font-weight:700; letter-spacing:1px; }}
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
  .stat-value.sm {{ font-size:28px; }}
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
  .charts-grid {{
    display:grid; grid-template-columns:1fr 1fr;
    gap:1px; background:var(--border); position:relative; z-index:1;
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
  canvas {{ width:100% !important; max-height:260px; }}
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
  <h1>Reporte de Evaluación</h1>
  <div class="header-ts">{ts}</div>
</header>

<div class="stat-row">
  <div class="stat">
    <div class="stat-label">mAP @ 0.50</div>
    <div class="stat-value" style="color:var(--green)">{map50:.4f}</div>
    <div class="stat-sub">IoU threshold 0.50</div>
  </div>
  <div class="stat">
    <div class="stat-label">mAP @ 0.50:0.95</div>
    <div class="stat-value" style="color:var(--blue)">{map50_95:.4f}</div>
    <div class="stat-sub">COCO standard</div>
  </div>
  <div class="stat">
    <div class="stat-label">Precision</div>
    <div class="stat-value" style="color:var(--yellow)">{precision:.4f}</div>
    <div class="stat-sub">conf ≥ {conf_thr}</div>
  </div>
  <div class="stat">
    <div class="stat-label">Recall</div>
    <div class="stat-value" style="color:var(--red)">{recall:.4f}</div>
    <div class="stat-sub">conf ≥ {conf_thr}</div>
  </div>
</div>

<div class="stat-row">
  <div class="stat">
    <div class="stat-label">Inference</div>
    <div class="stat-value sm" style="color:var(--cyan)">{infer_ms:.1f} ms</div>
    <div class="stat-sub">por imagen</div>
  </div>
  <div class="stat">
    <div class="stat-label">IoU medio TP</div>
    <div class="stat-value sm" style="color:var(--purple)">{iou_mean:.4f}</div>
    <div class="stat-sub">precisión de cajas</div>
  </div>
  <div class="stat">
    <div class="stat-label">AP Small</div>
    <div class="stat-value sm" style="color:var(--green)">{ap_small:.4f}</div>
    <div class="stat-sub">área &lt; 32² px</div>
  </div>
  <div class="stat">
    <div class="stat-label">AP Med / Large</div>
    <div class="stat-value sm" style="color:var(--blue)">{ap_medium:.4f} / {ap_large:.4f}</div>
    <div class="stat-sub">32²–96² / &gt;96² px</div>
  </div>
</div>

<div class="meta-bar">
  <div class="pill">imágenes <span>{n_images}</span></div>
  <div class="pill">GT boxes <span>{total_gt}</span></div>
  <div class="pill">predicciones <span>{total_pred}</span></div>
  <div class="pill">TP@0.50 <span>{tp50}</span></div>
  <div class="pill">FP <span>{fp}</span></div>
  <div class="pill">FN <span>{fn}</span></div>
  <div class="pill">best F1 <span>{best_f1:.3f}</span> @ conf <span>{best_f1_conf:.2f}</span></div>
</div>

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
    <canvas id="histChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--purple)"></div>Distribución IoU de TP</div>
    <canvas id="iouHistChart"></canvas>
  </div>

  <div class="chart-panel full">
    <div class="chart-title"><div class="bar" style="background:var(--purple)"></div>Detecciones @ IoU=0.50 — TP / FP / FN</div>
    <canvas id="cmChart" style="max-height:160px"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--cyan)"></div>AP por Tamaño de Objeto</div>
    <canvas id="apSizeChart"></canvas>
  </div>

  <div class="chart-panel">
    <div class="chart-title"><div class="bar" style="background:var(--yellow)"></div>Detecciones por Imagen (conf ≥ {conf_thr})</div>
    <canvas id="detsImgChart"></canvas>
  </div>

</div>

<!-- Timing de la ronda -->
<div class="meta-bar" style="border-top:1px solid var(--border); border-bottom:none;">
  <div class="pill">ronda <span>{t_ts}</span></div>
  <div class="pill">entrenamiento <span>{t_train} ms</span></div>
  <div class="pill">comunicación <span>{t_comm} ms</span></div>
  <div class="pill">agregación <span>{t_agg} ms</span></div>
  <div class="pill">total <span>{t_total} ms</span></div>
  {samples_pills}
</div>

<footer>FedSol · Federated Learning for YOLOv8 · {ts}</footer>

<script>
const G='#39d353', B='#1d8cf8', R='#ff6b6b', Y='#ffd166', P='#a78bfa', C='#22d3ee';
const GRID='rgba(255,255,255,0.05)', TICK='#4a5568';
const FONT={{ family:'Fira Code', size:10 }};
const base = {{
  responsive:true,
  animation:{{ duration:500, easing:'easeOutQuart' }},
  plugins:{{
    legend:{{ display:false }},
    tooltip:{{ backgroundColor:'#0e1117', borderColor:'#181e2a', borderWidth:1,
      titleFont:{{ family:'Fira Code', size:11 }}, bodyFont:{{ family:'Fira Code', size:11 }} }}
  }},
  scales:{{
    x:{{ grid:{{ color:GRID }}, ticks:{{ color:TICK, font:FONT }} }},
    y:{{ grid:{{ color:GRID }}, ticks:{{ color:TICK, font:FONT }} }},
  }}
}};
function o(extra) {{
  const r = JSON.parse(JSON.stringify(base));
  if (extra.scales)   Object.assign(r.scales, extra.scales);
  if (extra.plugins)  Object.assign(r.plugins, extra.plugins);
  if (extra.indexAxis) r.indexAxis = extra.indexAxis;
  return r;
}}

// 1. PR Curve
const pr = {pr_json};
new Chart(document.getElementById('prChart'), {{
  type:'line',
  data:{{ labels:pr.recalls.map(v=>v.toFixed(2)),
    datasets:[{{ label:'Precision', data:pr.precisions,
      borderColor:G, backgroundColor:'rgba(57,211,83,0.08)',
      borderWidth:2, pointRadius:0, fill:true, tension:0.3 }}] }},
  options:o({{ scales:{{
    x:{{ ...base.scales.x, title:{{ display:true, text:'Recall', color:TICK, font:FONT }} }},
    y:{{ ...base.scales.y, min:0, max:1, title:{{ display:true, text:'Precision', color:TICK, font:FONT }} }}
  }} }})
}});

// 2. F1 Curve
const f1 = {f1_json};
new Chart(document.getElementById('f1Chart'), {{
  type:'line',
  data:{{ labels:f1.confs.map(v=>v.toFixed(2)),
    datasets:[{{ label:'F1', data:f1.f1s,
      borderColor:B, backgroundColor:'rgba(29,140,248,0.08)',
      borderWidth:2, pointRadius:0, fill:true, tension:0.3 }}] }},
  options:o({{ scales:{{
    x:{{ ...base.scales.x, title:{{ display:true, text:'Confidence', color:TICK, font:FONT }} }},
    y:{{ ...base.scales.y, min:0, max:1, title:{{ display:true, text:'F1', color:TICK, font:FONT }} }}
  }} }})
}});

// 3. Confidence histogram
const cb = {conf_bins_json};
new Chart(document.getElementById('histChart'), {{
  type:'bar',
  data:{{ labels:cb.map((_,i)=>(i*0.05).toFixed(2)),
    datasets:[{{ data:cb,
      backgroundColor:cb.map((_,i)=>`rgba(${{Math.round(255*(1-i/20))}},${{Math.round(211*i/20)}},${{Math.round(83*i/20)}},0.85)`),
      borderWidth:0, borderRadius:2 }}] }},
  options:o({{ scales:{{
    x:{{ ...base.scales.x, title:{{ display:true, text:'Confidence', color:TICK, font:FONT }} }},
    y:{{ ...base.scales.y, title:{{ display:true, text:'Dets', color:TICK, font:FONT }} }}
  }} }})
}});

// 4. IoU TP histogram
const iouH = {iou_hist_json};
new Chart(document.getElementById('iouHistChart'), {{
  type:'bar',
  data:{{ labels:iouH.map((_,i)=>((0.50+i*0.05).toFixed(2))),
    datasets:[{{ data:iouH,
      backgroundColor:iouH.map((_,i)=>`rgba(${{Math.round(167-i*10)}},${{Math.round(139+i*10)}},250,0.85)`),
      borderWidth:0, borderRadius:2 }}] }},
  options:o({{ scales:{{
    x:{{ ...base.scales.x, title:{{ display:true, text:'IoU', color:TICK, font:FONT }} }},
    y:{{ ...base.scales.y, title:{{ display:true, text:'TP count', color:TICK, font:FONT }} }}
  }} }})
}});

// 5. TP/FP/FN
const cm = {cm_json};
new Chart(document.getElementById('cmChart'), {{
  type:'bar',
  data:{{ labels:cm.labels,
    datasets:[{{ data:cm.values,
      backgroundColor:['rgba(57,211,83,0.85)','rgba(255,107,107,0.85)','rgba(255,209,102,0.85)'],
      borderWidth:0, borderRadius:4 }}] }},
  options:o({{
    indexAxis:'y',
    plugins:{{ tooltip:{{ callbacks:{{ label:ctx=>' '+ctx.parsed.x+' detecciones' }} }} }},
    scales:{{
      x:{{ ...base.scales.x, title:{{ display:true, text:'Detecciones', color:TICK, font:FONT }} }},
      y:{{ ...base.scales.y, ticks:{{ color:'#dce8f0', font:{{ family:'Fira Code', size:13 }}, padding:8 }}, grid:{{ display:false }} }}
    }}
  }})
}});

// 6. AP by size
const apS = {ap_size_json};
new Chart(document.getElementById('apSizeChart'), {{
  type:'bar',
  data:{{ labels:apS.labels,
    datasets:[{{ data:apS.values,
      backgroundColor:[C,'rgba(29,140,248,0.85)','rgba(255,209,102,0.85)'],
      borderWidth:0, borderRadius:4 }}] }},
  options:o({{ scales:{{
    x:{{ ...base.scales.x }},
    y:{{ ...base.scales.y, min:0, max:1, title:{{ display:true, text:'AP', color:TICK, font:FONT }} }}
  }} }})
}});

// 7. Dets per image
const di = {dets_json};
new Chart(document.getElementById('detsImgChart'), {{
  type:'bar',
  data:{{ labels:di.labels.map((_,i)=>'img '+(i+1)),
    datasets:[{{ data:di.values,
      backgroundColor:'rgba(255,209,102,0.75)',
      borderWidth:0, borderRadius:2 }}] }},
  options:o({{ scales:{{
    x:{{ ...base.scales.x, title:{{ display:true, text:'Imagen', color:TICK, font:FONT }} }},
    y:{{ ...base.scales.y, title:{{ display:true, text:'Detecciones', color:TICK, font:FONT }} }}
  }} }})
}});
</script>
</body>
</html>"""