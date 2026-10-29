#!/usr/bin/env python3
"""Produce one illustrated, styled Calculator user manual (PDF).

The only installed output is the PDF. All schematic illustrations are drawn
directly into the document, without companion image files or HTML.
"""
from __future__ import annotations
import argparse
import re
from pathlib import Path
from xml.sax.saxutils import escape
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Flowable, PageBreak, Paragraph,
    Spacer, HRFlowable, Preformatted
)

NAVY = colors.HexColor("#142235")
BLUE = colors.HexColor("#007EA3")
DARK_BLUE = colors.HexColor("#075B75")
ICE = colors.HexColor("#EFF6F8")
GOLD = colors.HexColor("#DDAE65")
GREY = colors.HexColor("#607181")
PALE = colors.HexColor("#F3F6F9")
INK = colors.HexColor("#1D2937")
PAGE_W, PAGE_H = A4
MARGIN = 18 * mm
TEXT_W = PAGE_W - 2 * MARGIN

STYLES = {
    "title": ParagraphStyle("manual-title", fontName="Helvetica-Bold",
        fontSize=31, leading=37, textColor=NAVY, spaceAfter=10),
    "subtitle": ParagraphStyle("manual-subtitle", fontName="Helvetica",
        fontSize=14, leading=20, textColor=DARK_BLUE, spaceAfter=18),
    "part": ParagraphStyle("manual-part", fontName="Helvetica-Bold",
        fontSize=18, leading=25, textColor=NAVY, spaceBefore=20,
        spaceAfter=12, keepWithNext=True),
    "chapter": ParagraphStyle("manual-chapter", fontName="Helvetica-Bold",
        fontSize=14.5, leading=19, textColor=DARK_BLUE, spaceBefore=17,
        spaceAfter=9, keepWithNext=True),
    "sub": ParagraphStyle("manual-sub", fontName="Helvetica-Bold",
        fontSize=11.5, leading=16, textColor=NAVY, spaceBefore=13,
        spaceAfter=6, keepWithNext=True),
    "body": ParagraphStyle("manual-body", fontName="Helvetica",
        fontSize=9.5, leading=14.5, textColor=INK, spaceAfter=7),
    "bullet": ParagraphStyle("manual-bullet", fontName="Helvetica",
        fontSize=9.25, leading=13.5, textColor=INK, leftIndent=13,
        firstLineIndent=-9, spaceAfter=4),
    "pre": ParagraphStyle("manual-pre", fontName="Courier",
        fontSize=7.6, leading=10.8, textColor=INK, leftIndent=8,
        rightIndent=4, spaceAfter=7),
    "small": ParagraphStyle("manual-small", fontName="Helvetica",
        fontSize=8, leading=11.5, textColor=GREY, spaceAfter=6),
    "quote": ParagraphStyle("manual-quote", fontName="Helvetica-Oblique",
        fontSize=9.3, leading=14, textColor=DARK_BLUE, leftIndent=12,
        rightIndent=10, spaceAfter=9),
}

def rich(text: str) -> str:
    value = escape(text)
    value = re.sub(r"\x60([^\x60]+)\x60",
        r'<font face="Courier" color="#075B75">\1</font>', value)
    value = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", value)
    value = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<i>\1</i>", value)
    return value

class Note(Flowable):
    def __init__(self, text):
        super().__init__()
        self.text = text

    def wrap(self, available, height):
        self.width = available
        self.paragraph = Paragraph(rich(self.text), STYLES["quote"])
        _, inner_height = self.paragraph.wrap(available-26, 10000)
        self.height = inner_height + 14
        return available, self.height

    def draw(self):
        c = self.canv
        c.setFillColor(ICE)
        c.roundRect(0, 0, self.width, self.height, 6, stroke=0, fill=1)
        c.setFillColor(BLUE)
        c.rect(0, 0, 4, self.height, stroke=0, fill=1)
        self.paragraph.drawOn(c, 16, self.height-8-self.paragraph.height)

class ModePicture(Flowable):
    """A faithful concept illustration, not a mislabelled screenshot."""
    def __init__(self, mode):
        super().__init__()
        self.mode = mode

    def wrap(self, available, height):
        self.width = min(available, TEXT_W)
        self.height = 216
        return self.width, self.height

    def draw(self):
        c = self.canv
        c.setFillColor(PALE)
        c.roundRect(0, 0, self.width, 216, 8, fill=1, stroke=0)
        x, y, w, h = 14, 24, 225, 176
        c.setFillColor(NAVY)
        c.roundRect(x, y, w, h, 10, fill=1, stroke=0)
        c.setFillColor(colors.HexColor("#223B52"))
        c.roundRect(x+9, y+h-38, w-18, 27, 4, fill=1, stroke=0)
        c.setFillColor(colors.white)
        c.setFont("Helvetica-Bold", 10)
        c.drawString(x+15, y+h-26, self.mode.upper())
        answer = {"Standard":"14", "Scientific":"1.41421356237",
                  "Programmer":"0000 00FF"}[self.mode]
        c.setFont("Courier-Bold", 15 if self.mode != "Scientific" else 11)
        c.drawRightString(x+w-18, y+h-62, answer)
        keys = {
            "Standard": ["%","CE","C","/","1/x","x2","sqrt","*","7","8","9","-",
                         "4","5","6","+","1","2","3","=","+/-","0",".","="],
            "Scientific": ["DEG","pi","e","^","sin","cos","tan","2nd",
                           "ln","log","exp","!","7","8","9","/","4","5","6","*",
                           "1","2","3","=","(","0",")","+"],
            "Programmer": ["BIN","OCT","DEC","HEX","W8","W16","W32","W64",
                           "AND","OR","XOR","NOT","7","8","9","A","4","5","6","B",
                           "1","2","3","C","0","D","E","F"]
        }[self.mode]
        cw = (w-22-9)/4
        for i, key in enumerate(keys):
            row, col = divmod(i, 4)
            if row >= 7:
                break
            bx = x+11+col*(cw+3)
            by = y+10+(6-row)*13
            c.setFillColor(BLUE if key in ("=", "DEC", "HEX") else colors.HexColor("#334A60"))
            c.roundRect(bx, by, cw, 11, 2, fill=1, stroke=0)
            c.setFillColor(colors.white)
            c.setFont("Helvetica", 6.5)
            c.drawCentredString(bx+cw/2, by+3.2, key)
        rx = x+w+18
        descriptions = {
            "Standard":["Everyday arithmetic", "Operator precedence",
                        "Finite binary64 values", "Memory and history"],
            "Scientific":["Functions and variables", "50 digits by default",
                          "16 to 1,000 digits", "Real and complex values"],
            "Programmer":["Binary, octal, decimal, hex", "8/16/32/64-bit widths",
                          "Signed/unsigned views", "Bitwise and shift operations"]
        }[self.mode]
        c.setFillColor(DARK_BLUE)
        c.setFont("Helvetica-Bold", 14)
        c.drawString(rx, 165, self.mode+" mode")
        for i, line in enumerate(descriptions):
            c.setFillColor(GOLD)
            c.circle(rx+3, 138-i*25, 2, fill=1, stroke=0)
            c.setFillColor(INK)
            c.setFont("Helvetica", 9)
            c.drawString(rx+12, 135-i*25, line)
        c.setFillColor(GREY)
        c.setFont("Helvetica-Oblique", 7)
        c.drawString(14, 7,
            "Illustrative schematic; actual controls and layout vary by platform.")

class Manual(BaseDocTemplate):
    def __init__(self, output):
        super().__init__(str(output), pagesize=A4,
            leftMargin=MARGIN, rightMargin=MARGIN, topMargin=23*mm,
            bottomMargin=20*mm,
            title="Calculator - User Manual and Operating Principles",
            author="Shannon Smith",
            subject="Illustrated user guide and operating assumptions",
            pageCompression=1)
        frame = Frame(MARGIN, 19*mm, TEXT_W, PAGE_H-42*mm,
            leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates(PageTemplate(id="manual", frames=[frame],
                                           onPage=self.decorate))

    def decorate(self, c, doc):
        c.saveState()
        if doc.page > 1:
            c.setStrokeColor(colors.HexColor("#CBD7DF"))
            c.line(MARGIN, PAGE_H-16*mm, PAGE_W-MARGIN, PAGE_H-16*mm)
            c.setFillColor(DARK_BLUE)
            c.setFont("Helvetica-Bold", 8)
            c.drawString(MARGIN, PAGE_H-12.5*mm, "CALCULATOR  /  USER MANUAL")
        c.setFont("Helvetica", 8)
        c.setFillColor(GREY)
        c.drawString(MARGIN, 12*mm, "INFILTRATOROS  |  Calculator 0.2.64")
        c.drawRightString(PAGE_W-MARGIN, 12*mm, f"{doc.page:02d}")
        c.restoreState()

def blocks(source):
    lines = source.splitlines()
    result = []
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        i += 1
        stripped = line.strip()
        if not stripped:
            result.append(("blank",""))
        elif stripped.startswith(chr(96)*3):
            code = []
            while i < len(lines) and not lines[i].startswith(chr(96)*3):
                code.append(lines[i])
                i += 1
            i += 1
            result.append(("code", "\n".join(code)))
        elif line.startswith("### "):
            result.append(("sub", line[4:]))
        elif line.startswith("## "):
            result.append(("chapter", line[3:]))
        elif line.startswith("# "):
            result.append(("part", line[2:]))
        elif line.startswith("> "):
            result.append(("note", line[2:]))
        elif re.match(r"^\s*[-*] ", line):
            result.append(("bullet", re.sub(r"^\s*[-*] ", "", line)))
        elif re.match(r"^\s{2,}\S", line):
            result.append(("indented", stripped))
        elif stripped in ("---","***"):
            result.append(("rule",""))
        else:
            result.append(("text",stripped))
    return result

def generate(source: Path, output: Path):
    manuscript = source.read_text(encoding="utf-8")
    for needed in ("Standard","Programmer","Operating assumptions"):
        if needed.casefold() not in manuscript.casefold():
            raise ValueError(f"Manual manuscript is missing {needed}")
    output.parent.mkdir(parents=True, exist_ok=True)
    story = [
        Spacer(1, 26*mm),
        Paragraph("CALCULATOR", STYLES["title"]),
        Paragraph("User Manual &amp; Operating Principles", STYLES["subtitle"]),
        HRFlowable(width="100%", thickness=2, color=BLUE, spaceAfter=22),
        Paragraph("A practical guide to everyday calculations, scientific work, "
                  "programmer operations and the assumptions behind the results.",
                  STYLES["body"]),
        Spacer(1, 12*mm),
        ModePicture("Standard"), Spacer(1, 7*mm),
        Paragraph("InfiltratorOS software family  |  October 2026",STYLES["small"]),
        Paragraph("Copyright &copy; 1993-2026 Shannon Smith",STYLES["small"]),
        PageBreak(),
    ]
    seen = set()
    paragraph = []
    def flush():
        if paragraph:
            story.append(Paragraph(rich(" ".join(paragraph)), STYLES["body"]))
            paragraph.clear()
    for kind, content in blocks(manuscript):
        if kind == "text":
            paragraph.append(content)
            continue
        flush()
        if kind == "blank":
            continue
        if kind == "part":
            if "Part II" in content:
                story.append(PageBreak())
            story.append(Paragraph(rich(content),STYLES["part"]))
        elif kind == "chapter":
            story.append(Paragraph(rich(content),STYLES["chapter"]))
            if re.match(r"^\d+\. ", content):
                for mode in ("Standard","Scientific","Programmer"):
                    if mode.casefold() in content.casefold() and mode not in seen:
                        seen.add(mode)
                        story.append(ModePicture(mode))
                        break
        elif kind == "sub":
            story.append(Paragraph(rich(content), STYLES["sub"]))
        elif kind == "note":
            story.append(Note(content))
        elif kind == "bullet":
            story.append(Paragraph("&bull;  "+rich(content),STYLES["bullet"]))
        elif kind == "indented":
            story.append(Paragraph(
                '<font face="Courier">'+escape(content)+"</font>", STYLES["pre"]))
        elif kind == "code":
            story.append(Preformatted(content, STYLES["pre"], maxLineLength=91))
        elif kind == "rule":
            story.append(HRFlowable(width="100%",
                color=colors.HexColor("#CBD7DF")))
    flush()
    Manual(output).build(story)
    data=output.read_bytes()
    if not data.startswith(b"%PDF-") or len(data) < 10000:
        raise RuntimeError("PDF manual generation failed")
    print(f"Illustrated Calculator PDF: {output} ({len(data)} bytes)")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args=parser.parse_args()
    generate(args.source, args.output)

if __name__=="__main__":
    main()
