import time
import pathlib
import torch
import numpy as np


def load_model_robust(path, map_location="cpu"):
    """
    Loads a .pt file handling two formats:
    1. torch.save(yolo_obj, path) — build_and_save.py format.
    2. yolo_obj.save(path)        — Ultralytics-standard dict checkpoint.
    """
    from ultralytics import YOLO as _YOLO

    raw = None
    try:
        raw = torch.load(path, map_location=map_location, weights_only=False)
    except TypeError:
        try:
            raw = torch.load(path, map_location=map_location)
        except Exception:
            pass
    except Exception:
        pass

    if raw is not None:
        if isinstance(raw, _YOLO) and hasattr(raw, "predict"):
            return raw
        if isinstance(raw, torch.nn.Module):
            return raw

    try:
        m = _YOLO(path)
        if hasattr(m, "model") and hasattr(m.model, "parameters"):
            return m
    except Exception:
        pass

    return raw


# COCO small/medium/large area thresholds (in pixels²)
_SMALL_MAX  = 32 ** 2       # < 1024 px²
_MEDIUM_MAX = 96 ** 2       # < 9216 px²
# large = everything above _MEDIUM_MAX


class EvaluatorYolo:
    """
    Standalone YOLO evaluator using model.predict() image by image.
    Does NOT use model.val() (broken in Ultralytics 8.4.x).

    Returns both scalar metrics and raw curve/extra data for plotting:
        {
            "map50":       float,
            "map50_95":    float,
            "precision":   float,
            "recall":      float,
            "curves": {
                "pr":        {"recalls": [...], "precisions": [...]},
                "f1":        {"confs": [...], "f1s": [...]},
                "conf_hist": {"confs": [...]},
            },
            "extras": {
                "inference_ms":    float,   # mean ms per image
                "iou_tp_mean":     float,   # mean IoU of matched TP boxes
                "iou_tp_hist":     [...],   # 10-bucket histogram of TP IoUs
                "ap_small":        float,   # AP for GT area < 32²
                "ap_medium":       float,   # AP for GT area 32²..96²
                "ap_large":        float,   # AP for GT area > 96²
                "gt_size_hist":    [...],   # 10-bucket histogram of GT box areas (px²)
                "dets_per_image":  [...],   # detections per image (above conf_thresh)
            },
            "_meta": { ... }
        }
    """

    def __init__(self, model, config: dict):
        self.model  = model
        self.config = config

    # ── Public entry point ────────────────────────────────────────────────────

    def evaluate(self) -> dict:
        evaluation_cfg = self.config["evaluation"]
        pred_cfg       = evaluation_cfg.get("prediction_config", {})
        training_cfg   = self.config.get("training", {})
        dataset_cfg    = self.config["dataset"]

        conf_thresh = pred_cfg.get("conf", 0.25)
        nms_iou     = pred_cfg.get("iou", 0.45)
        imgsz       = training_cfg.get("imgsz", 640)
        device      = "cuda" if torch.cuda.is_available() else "cpu"

        # ── Locate val images ─────────────────────────────────────────────────
        val_images_path = dataset_cfg.get("val_path", dataset_cfg.get("train_path"))
        images_dir = pathlib.Path(val_images_path)

        if not images_dir.is_dir():
            raise FileNotFoundError(
                f"[EvaluatorYolo] val images directory not found: {images_dir}"
            )

        image_files = sorted(
            p for p in images_dir.rglob("*")
            if p.suffix.lower() in {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".webp"}
        )

        if not image_files:
            print(f"[EvaluatorYolo] No images found in: {images_dir}")
            return self._zero_results()

        labels_dir = images_dir.parent / "labels"
        if not labels_dir.is_dir():
            labels_dir = images_dir.parent.parent / "labels" / images_dir.name
        if not labels_dir.is_dir():
            labels_dir = images_dir

        print(
            f"[EvaluatorYolo] {len(image_files)} images | "
            f"labels={labels_dir} | conf={conf_thresh} iou={nms_iou} "
            f"imgsz={imgsz} device={device}"
        )

        iou_thresholds     = np.round(np.linspace(0.50, 0.95, 10), 2)
        per_thresh_entries = [[] for _ in iou_thresholds]
        per_thresh_gt      = np.zeros(len(iou_thresholds), dtype=int)

        # Per-size accumulators for AP_small/medium/large
        # Each entry: (conf, tp_flag) same as per_thresh_entries but only for
        # predictions whose best-matched GT falls in that size bucket.
        size_entries = {"small": [], "medium": [], "large": []}
        size_gt      = {"small": 0,  "medium": 0,  "large": 0}

        total_gt       = 0
        all_pred_confs = []   # for confidence histogram
        tp_ious        = []   # IoU of every TP match (at IoU=0.50)
        dets_per_image = []   # above-conf_thresh detections per image
        gt_areas       = []   # GT box areas in px²
        inference_ms   = []   # per-image inference time

        # TP/FP/FN at configured conf_thresh (operating point, IoU=0.50)
        op_tp = 0
        op_fp = 0
        op_fn = 0

        # ── Main loop ─────────────────────────────────────────────────────────
        for img_path in image_files:
            label_path = labels_dir / (img_path.stem + ".txt")

            t0 = time.perf_counter()
            try:
                results = self.model.predict(
                    source=str(img_path),
                    conf=0.01,
                    iou=nms_iou,
                    imgsz=imgsz,
                    device=device,
                    verbose=False,
                    save=False,
                )
                r = results[0]
                img_h, img_w = r.orig_shape
            except Exception as e:
                print(f"[EvaluatorYolo] predict failed for {img_path.name}: {e}")
                img_w = img_h = imgsz
                gt = self._read_labels(label_path, img_w, img_h)
                total_gt += len(gt)
                continue
            inference_ms.append((time.perf_counter() - t0) * 1000)

            gt   = self._read_labels(label_path, img_w, img_h)
            n_gt = len(gt)
            total_gt += n_gt
            for ti in range(len(iou_thresholds)):
                per_thresh_gt[ti] += n_gt

            # GT areas for size histogram + size-bucket GT counts
            if n_gt > 0:
                gt_boxes_all = gt[:, 1:]
                areas = ((gt_boxes_all[:, 2] - gt_boxes_all[:, 0]) *
                         (gt_boxes_all[:, 3] - gt_boxes_all[:, 1]))
                gt_areas.extend(areas.tolist())
                size_gt["small"]  += int((areas <  _SMALL_MAX).sum())
                size_gt["medium"] += int(((areas >= _SMALL_MAX) & (areas < _MEDIUM_MAX)).sum())
                size_gt["large"]  += int((areas >= _MEDIUM_MAX).sum())

            if r.boxes is None or len(r.boxes) == 0:
                dets_per_image.append(0)
                continue

            pred_boxes = r.boxes.xyxy.cpu().numpy()
            pred_confs = r.boxes.conf.cpu().numpy()
            pred_cls   = r.boxes.cls.cpu().numpy().astype(int)

            above_thresh = int((pred_confs >= conf_thresh).sum())
            dets_per_image.append(above_thresh)
            all_pred_confs.extend(pred_confs[pred_confs >= conf_thresh].tolist())

            if n_gt == 0:
                for conf in pred_confs:
                    for te in per_thresh_entries:
                        te.append((float(conf), 0))
                    for sz in size_entries.values():
                        sz.append((float(conf), 0))
                continue

            gt_boxes = gt[:, 1:]
            gt_cls   = gt[:, 0].astype(int)
            iou_mat  = self._box_iou_matrix(pred_boxes, gt_boxes)
            order    = np.argsort(-pred_confs)

            # GT areas for size-bucket matching
            gt_areas_img = ((gt_boxes[:, 2] - gt_boxes[:, 0]) *
                            (gt_boxes[:, 3] - gt_boxes[:, 1]))

            # Match at IoU=0.50 first to collect TP IoUs and size-bucket entries
            gt_matched_50 = np.zeros(n_gt, dtype=bool)
            for idx in order:
                cls_match = (pred_cls[idx] == gt_cls)
                ious      = iou_mat[idx] * cls_match
                best_gt   = int(np.argmax(ious))
                best_iou  = ious[best_gt]

                if best_iou >= 0.50 and not gt_matched_50[best_gt]:
                    tp_ious.append(float(best_iou))
                    # Size bucket of the matched GT
                    area = gt_areas_img[best_gt]
                    if area < _SMALL_MAX:
                        size_entries["small"].append((float(pred_confs[idx]), 1))
                    elif area < _MEDIUM_MAX:
                        size_entries["medium"].append((float(pred_confs[idx]), 1))
                    else:
                        size_entries["large"].append((float(pred_confs[idx]), 1))
                    gt_matched_50[best_gt] = True
                else:
                    # FP — assign to size bucket of closest GT (for denominator)
                    area = gt_areas_img[best_gt] if best_gt >= 0 else 0
                    if area < _SMALL_MAX:
                        size_entries["small"].append((float(pred_confs[idx]), 0))
                    elif area < _MEDIUM_MAX:
                        size_entries["medium"].append((float(pred_confs[idx]), 0))
                    else:
                        size_entries["large"].append((float(pred_confs[idx]), 0))

            # ── Operating-point TP/FP/FN (conf >= conf_thresh, IoU=0.50) ──────
            op_mask      = pred_confs >= conf_thresh
            op_boxes     = pred_boxes[op_mask]
            op_confs_img = pred_confs[op_mask]
            op_cls_img   = pred_cls[op_mask]

            if len(op_boxes) == 0:
                op_fn += n_gt
            elif n_gt == 0:
                op_fp += len(op_boxes)
            else:
                op_iou_mat  = self._box_iou_matrix(op_boxes, gt_boxes)
                op_order    = np.argsort(-op_confs_img)
                op_matched  = np.zeros(n_gt, dtype=bool)
                for idx in op_order:
                    cls_m    = (op_cls_img[idx] == gt_cls)
                    ious     = op_iou_mat[idx] * cls_m
                    best_gt  = int(np.argmax(ious))
                    best_iou = ious[best_gt]
                    if best_iou >= 0.50 and not op_matched[best_gt]:
                        op_tp += 1
                        op_matched[best_gt] = True
                    else:
                        op_fp += 1
                op_fn += int((~op_matched).sum())

            # Match at all thresholds
            for ti, thresh in enumerate(iou_thresholds):
                gt_matched = np.zeros(n_gt, dtype=bool)
                for idx in order:
                    cls_match = (pred_cls[idx] == gt_cls)
                    ious      = iou_mat[idx] * cls_match
                    best_gt   = int(np.argmax(ious))
                    best_iou  = ious[best_gt]

                    if best_iou >= thresh and not gt_matched[best_gt]:
                        per_thresh_entries[ti].append((float(pred_confs[idx]), 1))
                        gt_matched[best_gt] = True
                    else:
                        per_thresh_entries[ti].append((float(pred_confs[idx]), 0))

        # ── AP per threshold ──────────────────────────────────────────────────
        ap_values = np.zeros(len(iou_thresholds))
        for ti in range(len(iou_thresholds)):
            entries = per_thresh_entries[ti]
            n_gt_t  = per_thresh_gt[ti]
            if not entries or n_gt_t == 0:
                continue
            entries.sort(key=lambda x: -x[0])
            tp_arr     = np.array([e[1] for e in entries], dtype=float)
            fp_arr     = 1.0 - tp_arr
            tp_cum     = np.cumsum(tp_arr)
            fp_cum     = np.cumsum(fp_arr)
            recalls    = np.concatenate([[0.0], tp_cum / (n_gt_t + 1e-9)])
            precisions = np.concatenate([[1.0], tp_cum / (tp_cum + fp_cum + 1e-9)])
            ap_values[ti] = self._compute_ap_101(recalls, precisions)

        map50    = float(ap_values[0])
        map50_95 = float(ap_values.mean())

        # ── AP small / medium / large (at IoU=0.50) ───────────────────────────
        def _ap_for_size(bucket_name):
            entries = size_entries[bucket_name]
            n_gt_s  = size_gt[bucket_name]
            if not entries or n_gt_s == 0:
                return 0.0
            entries.sort(key=lambda x: -x[0])
            tp_arr     = np.array([e[1] for e in entries], dtype=float)
            fp_arr     = 1.0 - tp_arr
            tp_cum     = np.cumsum(tp_arr)
            fp_cum     = np.cumsum(fp_arr)
            recalls    = np.concatenate([[0.0], tp_cum / (n_gt_s + 1e-9)])
            precisions = np.concatenate([[1.0], tp_cum / (tp_cum + fp_cum + 1e-9)])
            return self._compute_ap_101(recalls, precisions)

        ap_small  = _ap_for_size("small")
        ap_medium = _ap_for_size("medium")
        ap_large  = _ap_for_size("large")

        # ── PR curve at IoU=0.50 ──────────────────────────────────────────────
        entries_50 = per_thresh_entries[0]
        pr_recalls    = [0.0]
        pr_precisions = [1.0]
        f1_confs      = []
        f1_values     = []
        precision = recall = 0.0

        if entries_50 and total_gt > 0:
            entries_50.sort(key=lambda x: -x[0])
            confs_arr = np.array([e[0] for e in entries_50])
            tp_arr    = np.cumsum([e[1] for e in entries_50], dtype=float)
            fp_arr    = np.arange(1, len(entries_50) + 1, dtype=float) - tp_arr
            prec_arr  = tp_arr / (tp_arr + fp_arr + 1e-9)
            rec_arr   = tp_arr / (total_gt + 1e-9)

            pr_recalls    = rec_arr.tolist()
            pr_precisions = prec_arr.tolist()

            conf_steps = np.linspace(confs_arr.max(), max(confs_arr.min(), 0.01), 100)
            for c in conf_steps:
                mask = confs_arr >= c
                if not mask.any():
                    continue
                # Use [-1] not [0]: at threshold c, all predictions with conf>=c
                # are "active". The last one has the highest cumulative TP/FP count
                # (lowest conf within the active set) giving the correct P/R pair.
                p  = float(prec_arr[mask][-1])
                rv = float(rec_arr[mask][-1])
                f1 = 2 * p * rv / (p + rv + 1e-9)
                f1_confs.append(round(float(c), 4))
                f1_values.append(round(f1, 4))

            mask = confs_arr >= conf_thresh
            if mask.any():
                precision = float(prec_arr[mask][-1])
                recall    = float(rec_arr[mask][-1])
            else:
                precision = float(prec_arr[-1])
                recall    = float(rec_arr[-1])

        total_preds = len(entries_50) if entries_50 else 0
        total_tp50  = int(sum(e[1] for e in entries_50)) if entries_50 else 0

        # ── Extras ────────────────────────────────────────────────────────────
        mean_infer_ms = round(float(np.mean(inference_ms)), 2) if inference_ms else 0.0
        iou_tp_mean   = round(float(np.mean(tp_ious)), 4) if tp_ious else 0.0

        # TP IoU histogram (10 buckets: 0.50..1.00)
        iou_tp_hist = [0] * 10
        for v in tp_ious:
            idx = min(int((v - 0.50) / 0.05), 9)
            iou_tp_hist[idx] += 1

        # GT area histogram (10 log-spaced buckets 1..max_area)
        gt_size_hist = [0] * 10
        if gt_areas:
            max_area = max(gt_areas)
            for a in gt_areas:
                idx = min(int(a / max_area * 10), 9)
                gt_size_hist[idx] += 1

        print(
            f"[EvaluatorYolo] Images={len(image_files)}  GT={total_gt}  "
            f"Preds={total_preds}  TP@0.50={total_tp50}"
        )
        print(
            f"[EvaluatorYolo] P={precision:.4f}  R={recall:.4f}  "
            f"mAP50={map50:.4f}  mAP50-95={map50_95:.4f}  "
            f"IoU_TP={iou_tp_mean:.4f}  ms/img={mean_infer_ms:.1f}"
        )
        print(
            f"[EvaluatorYolo] AP_small={ap_small:.4f}  "
            f"AP_medium={ap_medium:.4f}  AP_large={ap_large:.4f}"
        )

        return {
            "map50":     round(map50,     4),
            "map50_95":  round(map50_95,  4),
            "precision": round(precision, 4),
            "recall":    round(recall,    4),
            "curves": {
                "pr": {
                    "recalls":    [round(v, 4) for v in pr_recalls],
                    "precisions": [round(v, 4) for v in pr_precisions],
                },
                "f1": {
                    "confs": f1_confs,
                    "f1s":   f1_values,
                },
                "conf_hist": {
                    "confs": [round(v, 4) for v in sorted(all_pred_confs)],
                },
            },
            "extras": {
                "inference_ms":   mean_infer_ms,
                "iou_tp_mean":    iou_tp_mean,
                "iou_tp_hist":    iou_tp_hist,
                "ap_small":       round(ap_small,  4),
                "ap_medium":      round(ap_medium, 4),
                "ap_large":       round(ap_large,  4),
                "gt_size_hist":   gt_size_hist,
                "dets_per_image": dets_per_image,
            },
            "_meta": {
                "images":      len(image_files),
                "total_gt":    total_gt,
                "total_preds": total_preds,
                "tp50":        total_tp50,
                "conf_thresh": conf_thresh,
                "op_tp":       op_tp,
                "op_fp":       op_fp,
                "op_fn":       op_fn,
            },
        }

    # ── Private helpers ───────────────────────────────────────────────────────

    @staticmethod
    def _zero_results() -> dict:
        return {
            "map50": 0.0, "map50_95": 0.0,
            "precision": 0.0, "recall": 0.0,
            "curves": {
                "pr":        {"recalls": [], "precisions": []},
                "f1":        {"confs": [], "f1s": []},
                "conf_hist": {"confs": []},
            },
            "extras": {
                "inference_ms": 0.0, "iou_tp_mean": 0.0, "iou_tp_hist": [0]*10,
                "ap_small": 0.0, "ap_medium": 0.0, "ap_large": 0.0,
                "gt_size_hist": [0]*10, "dets_per_image": [],
            },
            "_meta": {"images": 0, "total_gt": 0, "total_preds": 0, "tp50": 0, "conf_thresh": 0.25},
        }

    @staticmethod
    def _read_labels(label_path: pathlib.Path, img_w: int, img_h: int) -> np.ndarray:
        if not label_path.exists():
            return np.zeros((0, 5), dtype=np.float32)
        boxes = []
        for line in label_path.read_text().strip().splitlines():
            parts = line.strip().split()
            if len(parts) < 5:
                continue
            cls, cx, cy, bw, bh = map(float, parts[:5])
            x1 = (cx - bw / 2) * img_w
            y1 = (cy - bh / 2) * img_h
            x2 = (cx + bw / 2) * img_w
            y2 = (cy + bh / 2) * img_h
            boxes.append([cls, x1, y1, x2, y2])
        return np.array(boxes, dtype=np.float32) if boxes else np.zeros((0, 5), dtype=np.float32)

    @staticmethod
    def _box_iou_matrix(pred_boxes: np.ndarray, gt_boxes: np.ndarray) -> np.ndarray:
        x1 = np.maximum(pred_boxes[:, None, 0], gt_boxes[None, :, 0])
        y1 = np.maximum(pred_boxes[:, None, 1], gt_boxes[None, :, 1])
        x2 = np.minimum(pred_boxes[:, None, 2], gt_boxes[None, :, 2])
        y2 = np.minimum(pred_boxes[:, None, 3], gt_boxes[None, :, 3])
        inter  = np.maximum(x2 - x1, 0.0) * np.maximum(y2 - y1, 0.0)
        area_p = (pred_boxes[:, 2] - pred_boxes[:, 0]) * (pred_boxes[:, 3] - pred_boxes[:, 1])
        area_g = (gt_boxes[:, 2] - gt_boxes[:, 0]) * (gt_boxes[:, 3] - gt_boxes[:, 1])
        union  = area_p[:, None] + area_g[None, :] - inter
        return inter / (union + 1e-9)

    @staticmethod
    def _compute_ap_101(recalls: np.ndarray, precisions: np.ndarray) -> float:
        ap = 0.0
        for thr in np.linspace(0.0, 1.0, 101):
            mask = recalls >= thr
            ap  += precisions[mask].max() if mask.any() else 0.0
        return ap / 101.0