#!/usr/bin/env python3
"""把 13. 英文/ 下的三个词书 JSONL 转成 Anki 可导入的 TSV。

数据源（每行一个 JSON 对象，不是标准 JSON 数组）：
  CET6_1.json      1228 词，字母序
  CET6_2.json      2078 词，真题词频序，带 realExamSentence
  IELTSluan_2.json 3427 词，雅思乱序版

输出的 TSV 带 Anki 导入指令头，直接拖进 Anki 即可，不需要装任何包。
牌型用内置的「Basic」，正面是单词+音标，背面是释义/例句/真题例句/助记。
Anki 按文件行序分配新卡位置，所以排序即学习顺序：
  1. 六级∩雅思 重叠词（学一次两边都用）
  2. 六级独有
  3. 雅思独有

用法：
  python3 vocab2anki.py                          # 生成 out/vocab.tsv
  python3 vocab2anki.py --fetch-audio            # 顺便下载有道发音 mp3 到 out/media/
  python3 vocab2anki.py --limit 50 --fetch-audio # 先拿 50 个词试跑
"""

from __future__ import annotations

import argparse
import html
import json
import re
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
BOOKS = {
    "CET6_1": BASE / "CET6_1.json",
    "CET6_2": BASE / "CET6_2.json",
    "IELTS": BASE / "IELTSluan_2.json",
}
YOUDAO = "https://dict.youdao.com/dictvoice?audio="


def load(path: Path) -> dict[str, dict]:
    """读 JSONL，返回 {小写词: 词条内容}。同词以先出现的为准。"""
    out: dict[str, dict] = {}
    with path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            d = json.loads(line)
            key = d["headWord"].lower()
            if key not in out:
                out[key] = d
    return out


def esc(s: str) -> str:
    return html.escape(s.strip())


def render_back(c: dict) -> str:
    """把一个词条的 content.word.content 渲染成 HTML 背面。"""
    parts: list[str] = []

    # 释义：按词性分行，中释在前，英释跟在后面（小字）
    trans = c.get("trans") or []
    if trans:
        rows = []
        for t in trans:
            pos = esc(t.get("pos") or "")
            cn = esc(t.get("tranCn") or "")
            en = esc(t.get("tranOther") or "")
            tag = f"<i>{pos}.</i> " if pos else ""
            row = f"{tag}{cn}"
            if en:
                row += f'<div class="en">{en}</div>'
            rows.append(f"<li>{row}</li>")
        parts.append(f'<ul class="trans">{"".join(rows)}</ul>')

    # 词根助记：对付「看着眼熟但记不住」最有效的一栏
    rem = (c.get("remMethod") or {}).get("val")
    if rem:
        parts.append(f'<div class="rem">助记 {esc(rem)}</div>')

    # 真题例句优先于普通例句——六级词书里 782 个词有，直接对着考点记
    real = (c.get("realExamSentence") or {}).get("sentences") or []
    if real:
        rows = []
        for s in real[:2]:
            src = s.get("sourceInfo") or {}
            meta = " ".join(x for x in (src.get("year"), src.get("type")) if x)
            tail = f'<span class="src">{esc(meta)}</span>' if meta else ""
            rows.append(f'<li>{esc(s.get("sContent") or "")} {tail}</li>')
        parts.append(f'<div class="label">真题</div><ul>{"".join(rows)}</ul>')

    sents = (c.get("sentence") or {}).get("sentences") or []
    if sents:
        rows = []
        for s in sents[:2]:
            cn = esc(s.get("sCn") or "")
            cn_html = f'<div class="cn">{cn}</div>' if cn else ""
            rows.append(f'<li>{esc(s.get("sContent") or "")}{cn_html}</li>')
        parts.append(f'<div class="label">例句</div><ul>{"".join(rows)}</ul>')

    # 短语：口语里最缺的就是搭配
    phrases = (c.get("phrase") or {}).get("phrases") or []
    if phrases:
        rows = [
            f'<li>{esc(p.get("pContent") or "")} — {esc(p.get("pCn") or "")}</li>'
            for p in phrases[:4]
        ]
        parts.append(f'<div class="label">搭配</div><ul>{"".join(rows)}</ul>')

    rels = (c.get("relWord") or {}).get("rels") or []
    if rels:
        words = []
        for r in rels:
            for w in (r.get("words") or [])[:3]:
                words.append(f'{esc(w.get("hwd") or "")} <i>{esc(r.get("pos") or "")}.</i>')
        if words:
            parts.append(f'<div class="rel">同根 {" · ".join(words[:8])}</div>')

    return "".join(parts)


SAFE = re.compile(r"[^a-zA-Z0-9_-]")


def media_name(word: str, kind: str) -> str:
    return f"yd_{SAFE.sub('_', word)}_{kind}.mp3"


def fetch_one(job: tuple[str, Path]) -> str | None:
    url, dest = job
    if dest.exists() and dest.stat().st_size > 512:
        return None
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=20) as r:
            data = r.read()
        if len(data) < 512:
            return f"{dest.name}: 响应过短 ({len(data)} 字节)"
        dest.write_bytes(data)
    except Exception as e:  # 网络问题不该中断整批
        return f"{dest.name}: {e}"
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(BASE / "工具" / "out"))
    ap.add_argument("--limit", type=int, default=0, help="只处理前 N 个词，用于试跑")
    ap.add_argument("--fetch-audio", action="store_true", help="下载有道发音 mp3")
    ap.add_argument("--workers", type=int, default=8)
    args = ap.parse_args()

    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    mediadir = outdir / "media"

    books = {name: load(p) for name, p in BOOKS.items()}
    cet6 = {**books["CET6_1"], **books["CET6_2"]}  # CET6_2 带真题例句，覆盖优先
    ielts = books["IELTS"]
    overlap = set(cet6) & set(ielts)

    # 六级内部排序：CET6_2 有真题词频，用它的 rank；只在 CET6_1 里的排在后面
    freq = {w: d["wordRank"] for w, d in books["CET6_2"].items()}

    def cet6_key(w: str) -> tuple[int, int, str]:
        return (0, freq[w], w) if w in freq else (1, 0, w)

    groups = [
        ("英语::1-六级雅思共有", sorted(overlap, key=cet6_key), cet6, "共有"),
        ("英语::2-六级独有", sorted(set(cet6) - overlap, key=cet6_key), cet6, "六级"),
        ("英语::3-雅思独有", sorted(set(ielts) - set(cet6),
                                key=lambda w: ielts[w]["wordRank"]), ielts, "雅思"),
    ]

    rows: list[tuple[str, str, str, str]] = []
    audio_jobs: list[tuple[str, Path]] = []
    n = 0
    for deck, words, src, tag in groups:
        for w in words:
            if args.limit and n >= args.limit:
                break
            entry = src[w]
            c = entry["content"]["word"]["content"]
            head = entry["headWord"]

            phon = []
            if c.get("ukphone"):
                phon.append(f'英 /{esc(c["ukphone"])}/')
            if c.get("usphone"):
                phon.append(f'美 /{esc(c["usphone"])}/')

            # 82 个词没有英音数据，回退到美音
            speech, kind = (c.get("ukspeech"), "uk")
            if not speech:
                speech, kind = (c.get("usspeech"), "us")
            audio = ""
            if speech:
                if args.fetch_audio:
                    fn = media_name(head, kind)
                    audio_jobs.append((YOUDAO + speech, mediadir / fn))
                    audio = f"[sound:{fn}]"
                else:
                    audio = (f'<audio controls src="{YOUDAO}'
                             f'{html.escape(speech, quote=True)}"></audio>')

            front = (f'<div class="word">{esc(head)}</div>'
                     f'<div class="phon">{" ".join(phon)}</div>{audio}')
            tags = [tag]
            if c.get("realExamSentence"):
                tags.append("有真题例句")
            rows.append((front, render_back(c), " ".join(tags), deck))
            n += 1

    print(f"六级去重 {len(cet6)} · 雅思 {len(ielts)} · 重叠 {len(overlap)}")

    if audio_jobs:
        mediadir.mkdir(parents=True, exist_ok=True)
        print(f"下载发音 {len(audio_jobs)} 个 → {mediadir}")
        with ThreadPoolExecutor(max_workers=args.workers) as ex:
            errs = [e for e in ex.map(fetch_one, audio_jobs) if e]
        print(f"媒体目录现有 {len(list(mediadir.glob('*.mp3')))} 个 mp3，失败 {len(errs)} 个")
        for e in errs[:5]:
            print("  " + e, file=sys.stderr)

        # 有道对少数词返回 500，把下不到的 [sound:] 引用去掉，
        # 否则 Anki 每次都报缺失媒体
        missing = {j[1].name for j in audio_jobs if not j[1].exists()}
        if missing:
            pat = re.compile(r"\[sound:(" + "|".join(map(re.escape, missing)) + r")\]")
            rows = [(pat.sub("", front), b, t, d) for front, b, t, d in rows]
            print(f"剔除 {len(missing)} 个下载失败的发音引用")

    tsv = outdir / "vocab.tsv"
    with tsv.open("w", encoding="utf-8") as f:
        f.write("#separator:tab\n#html:true\n#notetype:Basic\n"
                "#tags column:3\n#deck column:4\n")
        for front, back, tags, deck in rows:
            # 字段内不能出现制表符或裸换行，否则 Anki 会错位切列
            cells = [x.replace("\t", " ").replace("\n", " ") for x in (front, back, tags, deck)]
            f.write("\t".join(cells) + "\n")

    print(f"写入 {len(rows)} 张卡 → {tsv}")
    if audio_jobs:
        print(f"导入前把 {mediadir} 里的 mp3 全部复制进 Anki 的 collection.media 目录")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
