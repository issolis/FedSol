import json
import os
import glob
from datetime import datetime


class DefaultReport:

    @staticmethod
    def generate(results, params):
        # ── Console output ────────────────────────────────────────────────────
        if params.get("print_to_console", True):
            print("[EVALUATION RESULTS]")
            scalar_keys = ["map50", "map50_95", "precision", "recall"]
            for key in scalar_keys:
                if key in results:
                    print(f"{key}: {results[key]}")

        if not params.get("save_results", False):
            return

        # ── Paths ─────────────────────────────────────────────────────────────
        base_dir           = params.get("base_dir", "output_server/globalResults")
        use_timestamp_path = params.get("use_timestamp_path", True)

        if use_timestamp_path:
            now        = datetime.now()
            day_folder = now.strftime("%Y-%m-%d")
            stem       = now.strftime("%Y-%m-%d_%H-%M-%S")
            json_path  = os.path.join(base_dir, day_folder, stem + ".json")
            html_path  = os.path.join(base_dir, day_folder, stem + "_report.html")
        else:
            json_path = params.get("results_path", "results/evaluation.json")
            html_path = json_path.replace(".json", "_report.html")

        os.makedirs(os.path.dirname(json_path), exist_ok=True)

        # ── Save JSON (scalar metrics only, no curve blobs) ───────────────────
        scalar_results = {
            k: v for k, v in results.items()
            if k not in ("curves", "_meta")
        }
        scalar_results["saved_at"] = json_path
        with open(json_path, "w") as f:
            json.dump(scalar_results, f, indent=4)

        # ── Generate HTML report ──────────────────────────────────────────────
        curves = results.get("curves", {})
        meta   = results.get("_meta", {})
        html   = DefaultReport._build_html(scalar_results, curves, meta)

        with open(html_path, "w", encoding="utf-8") as f:
            f.write(html)

        print(f"[Report] JSON  → {json_path}")
        print(f"[Report] HTML  → {html_path}")

    # ── HTML builder ─────────────────────────────────────────────────────────

    @staticmethod
    def _build_html(results, curves, meta):
        ts         = datetime.now().strftime("%Y-%m-%d  %H:%M:%S")
        map50      = results.get("map50", 0)
        map50_95   = results.get("map50_95", 0)
        precision  = results.get("precision", 0)
        recall     = results.get("recall", 0)

        pr_data   = curves.get("pr",        {"recalls": [], "precisions": []})
        f1_data   = curves.get("f1",        {"confs": [], "f1s": []})
        hist_data = curves.get("conf_hist", {"confs": []})

        n_images  = meta.get("images", "—")
        total_gt  = meta.get("total_gt", "—")
        total_pred= meta.get("total_preds", "—")
        tp50      = meta.get("tp50", "—")
        conf_thr  = meta.get("conf_thresh", 0.25)

        # Best F1 point
        best_f1 = 0.0
        best_f1_conf = conf_thr
        if f1_data["f1s"]:
            idx = int(max(range(len(f1_data["f1s"])), key=lambda i: f1_data["f1s"][i]))
            best_f1      = f1_data["f1s"][idx]
            best_f1_conf = f1_data["confs"][idx]

        # Histogram buckets (20 bins 0..1)
        raw_confs = hist_data["confs"]
        bins      = [0.0] * 20
        for c in raw_confs:
            idx = min(int(c * 20), 19)
            bins[idx] += 1

        pr_json  = json.dumps(pr_data)
        f1_json  = json.dumps(f1_data)
        bins_json= json.dumps(bins)

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
    --bg:#080a0e; --panel:#0e1117; --border:#181e2a;
    --green:#39d353; --blue:#1d8cf8; --red:#ff6b6b; --yellow:#ffd166;
    --text:#dce8f0; --muted:#4a5568;
    --display:'Oxanium',sans-serif; --mono:'Fira Code',monospace;
  }}
  body {{ background:var(--bg); color:var(--text); font-family:var(--display); min-height:100vh; }}
  body::before {{
    content:''; position:fixed; inset:0; pointer-events:none; z-index:0;
    background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='0.035'/%3E%3C/svg%3E");
  }}
  header {{ display:flex; align-items:center; gap:16px; padding:22px 36px; border-bottom:1px solid var(--border); position:relative; z-index:1; }}
  .logo-tag {{ font-family:var(--mono); font-size:10px; letter-spacing:3px; color:var(--green); border:1px solid var(--green); padding:3px 10px; border-radius:2px; text-transform:uppercase; }}
  header h1 {{ font-size:20px; font-weight:700; letter-spacing:1px; }}
  .header-ts {{ margin-left:auto; font-family:var(--mono); font-size:11px; color:var(--muted); }}
  .stat-row {{ display:grid; grid-template-columns:repeat(4,1fr); gap:1px; background:var(--border); border-bottom:1px solid var(--border); position:relative; z-index:1; }}
  .stat {{ background:var(--panel); padding:22px 28px; display:flex; flex-direction:column; gap:6px; }}
  .stat-label {{ font-family:var(--mono); font-size:9px; letter-spacing:2px; text-transform:uppercase; color:var(--muted); }}
  .stat-value {{ font-size:36px; font-weight:800; line-height:1; }}
  .stat-sub {{ font-family:var(--mono); font-size:10px; color:var(--muted); }}
  .meta-bar {{ display:flex; gap:10px; align-items:center; flex-wrap:wrap; padding:12px 36px; border-bottom:1px solid var(--border); position:relative; z-index:1; }}
  .pill {{ font-family:var(--mono); font-size:10px; background:var(--panel); border:1px solid var(--border); border-radius:20px; padding:3px 12px; color:var(--muted); }}
  .pill span {{ color:var(--text); }}
  .charts-grid {{ display:grid; grid-template-columns:1fr 1fr; gap:1px; background:var(--border); position:relative; z-index:1; }}
  .chart-panel {{ background:var(--panel); padding:28px 32px; display:flex; flex-direction:column; gap:14px; }}
  .chart-title {{ font-size:11px; font-family:var(--mono); letter-spacing:2px; text-transform:uppercase; color:var(--muted); display:flex; align-items:center; gap:10px; }}
  .chart-title::before {{ content:''; display:block; width:3px; height:14px; border-radius:2px; background:var(--green); }}
  .chart-panel:nth-child(2) .chart-title::before {{ background:var(--blue); }}
  .chart-panel:nth-child(3) .chart-title::before {{ background:var(--red); }}
  canvas {{ width:100% !important; max-height:300px; }}
  footer {{ text-align:center; padding:28px; font-family:var(--mono); font-size:10px; color:var(--muted); border-top:1px solid var(--border); position:relative; z-index:1; }}
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

<div class="meta-bar">
  <div class="pill">imágenes <span>{n_images}</span></div>
  <div class="pill">GT boxes <span>{total_gt}</span></div>
  <div class="pill">predicciones <span>{total_pred}</span></div>
  <div class="pill">TP@0.50 <span>{tp50}</span></div>
  <div class="pill">best F1 <span>{best_f1:.3f}</span> @ conf <span>{best_f1_conf:.2f}</span></div>
</div>

<div class="charts-grid">
  <div class="chart-panel">
    <div class="chart-title">Curva Precision-Recall (IoU=0.50)</div>
    <canvas id="prChart"></canvas>
  </div>
  <div class="chart-panel">
    <div class="chart-title">F1 vs Confidence Threshold</div>
    <canvas id="f1Chart"></canvas>
  </div>
  <div class="chart-panel">
    <div class="chart-title">Histograma de Confianzas</div>
    <canvas id="histChart"></canvas>
  </div>
</div>

<footer>FedSol · Federated Learning for YOLOv8 · {ts}</footer>

<script>
const ACCENT='#39d353', ACCENT2='#1d8cf8', ACCENT3='#ff6b6b';
const GRID='rgba(255,255,255,0.05)', TEXT='#4a5568';
const FONT={{ family:'Fira Code', size:10 }};
const base = {{
  responsive:true, animation:{{ duration:600, easing:'easeOutQuart' }},
  plugins:{{ legend:{{ display:false }}, tooltip:{{ backgroundColor:'#0e1117', borderColor:'#181e2a', borderWidth:1, titleFont:FONT, bodyFont:FONT }} }},
  scales:{{ x:{{ grid:{{ color:GRID }}, ticks:{{ color:TEXT, font:FONT }} }}, y:{{ grid:{{ color:GRID }}, ticks:{{ color:TEXT, font:FONT }} }} }}
}};
function m(a,b) {{ const o=Object.assign({{}},a); for(const k in b) o[k]=(b[k]&&typeof b[k]==='object'&&!Array.isArray(b[k]))?m(a[k]||{{}},b[k]):b[k]; return o; }}

const pr={pr_json};
new Chart(document.getElementById('prChart'),{{type:'line',data:{{labels:pr.recalls.map(v=>v.toFixed(2)),datasets:[{{label:'Precision',data:pr.precisions,borderColor:ACCENT,backgroundColor:'rgba(57,211,83,0.08)',borderWidth:2,pointRadius:0,fill:true,tension:0.3}}]}},options:m(base,{{scales:{{x:{{title:{{display:true,text:'Recall',color:TEXT,font:FONT}}}},y:{{min:0,max:1,title:{{display:true,text:'Precision',color:TEXT,font:FONT}}}}}}}})}});

const f1={f1_json};
new Chart(document.getElementById('f1Chart'),{{type:'line',data:{{labels:f1.confs.map(v=>v.toFixed(2)),datasets:[{{label:'F1',data:f1.f1s,borderColor:ACCENT2,backgroundColor:'rgba(29,140,248,0.08)',borderWidth:2,pointRadius:0,fill:true,tension:0.3}}]}},options:m(base,{{scales:{{x:{{title:{{display:true,text:'Confidence',color:TEXT,font:FONT}}}},y:{{min:0,max:1,title:{{display:true,text:'F1',color:TEXT,font:FONT}}}}}}}})}});

const bins={bins_json};
new Chart(document.getElementById('histChart'),{{type:'bar',data:{{labels:bins.map((_,i)=>(i*0.05).toFixed(2)),datasets:[{{data:bins,backgroundColor:bins.map((_,i)=>{{const v=i/20;return `rgba(${{Math.round(255*(1-v))}},${{Math.round(211*v)}},${{Math.round(83*v)}},0.8)`;}}),borderWidth:0,borderRadius:2}}]}},options:m(base,{{scales:{{x:{{title:{{display:true,text:'Confidence',color:TEXT,font:FONT}}}},y:{{title:{{display:true,text:'Detecciones',color:TEXT,font:FONT}}}}}}}})}});
</script>
</body>
</html>"""