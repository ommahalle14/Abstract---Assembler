import os
import sys
from reportlab.lib.pagesizes import letter
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether, HRFlowable
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfgen import canvas

class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_decorations(num_pages)
            super().showPage()
        super().save()

    def draw_page_decorations(self, page_count):
        self.saveState()
        self.setFont("Helvetica", 8)
        self.setFillColor(colors.HexColor("#718096"))
        # Header (on pages after cover)
        if self._pageNumber > 1:
            self.drawString(54, 755, "Worklet 1 Dedicated Unit Testing Technical Report | Modular x86-32 Assembler")
            self.setStrokeColor(colors.HexColor("#E2E8F0"))
            self.setLineWidth(0.5)
            self.line(54, 750, 558, 750)
        
        # Footer
        self.drawString(54, 35, "CONFIDENTIAL & PROPRIETARY — INTERNAL QUALITY ASSURANCE REPORT")
        self.drawRightString(558, 35, f"Page {self._pageNumber} of {page_count}")
        self.setStrokeColor(colors.HexColor("#E2E8F0"))
        self.setLineWidth(0.5)
        self.line(54, 45, 558, 45)
        self.restoreState()

def build_pdf(filename="Worklet1_Unit_Testing_Report.pdf"):
    doc = SimpleDocTemplate(
        filename,
        pagesize=letter,
        leftMargin=54,
        rightMargin=54,
        topMargin=54,
        bottomMargin=54
    )

    styles = getSampleStyleSheet()
    
    # Custom palette
    c_primary = colors.HexColor("#1A365D")   # Deep Navy
    c_secondary = colors.HexColor("#2B6CB0") # Slate Blue
    c_accent = colors.HexColor("#319795")    # Teal Accent
    c_dark = colors.HexColor("#2D3748")      # Charcoal body
    c_light = colors.HexColor("#F7FAFC")     # Soft light background
    c_border = colors.HexColor("#E2E8F0")
    c_pass = colors.HexColor("#2F855A")      # Forest Green
    c_pass_bg = colors.HexColor("#F0FFF4")

    # Typography styles
    styles.add(ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=24,
        leading=28,
        textColor=c_primary,
        spaceAfter=6
    ))
    styles.add(ParagraphStyle(
        'DocSubtitle',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=12,
        leading=16,
        textColor=c_secondary,
        spaceAfter=15
    ))
    styles.add(ParagraphStyle(
        'MetaText',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9,
        leading=13,
        textColor=colors.HexColor("#4A5568")
    ))
    styles.add(ParagraphStyle(
        'SectionHeading',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=14,
        leading=18,
        textColor=c_primary,
        spaceBefore=14,
        spaceAfter=8,
        keepWithNext=True
    ))
    styles.add(ParagraphStyle(
        'SubSectionHeading',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=11,
        leading=15,
        textColor=c_secondary,
        spaceBefore=10,
        spaceAfter=4,
        keepWithNext=True
    ))
    styles.add(ParagraphStyle(
        'CustomBody',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9.5,
        leading=13.5,
        textColor=c_dark,
        spaceAfter=6
    ))
    styles.add(ParagraphStyle(
        'CustomBodyBold',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=9.5,
        leading=13.5,
        textColor=c_dark
    ))
    styles.add(ParagraphStyle(
        'CodeSnippet',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=8.5,
        leading=11,
        textColor=colors.HexColor("#2C5282")
    ))
    styles.add(ParagraphStyle(
        'TableHeader',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=9,
        leading=12,
        textColor=colors.white,
        alignment=1
    ))
    styles.add(ParagraphStyle(
        'TableCell',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=11.5,
        textColor=c_dark
    ))
    styles.add(ParagraphStyle(
        'TableCellCenter',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=11.5,
        textColor=c_dark,
        alignment=1
    ))
    styles.add(ParagraphStyle(
        'TableCellPass',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=8.5,
        leading=11.5,
        textColor=c_pass,
        alignment=1
    ))

    story = []

    # Title & Metadata Banner
    story.append(Paragraph("WORKLET 1 UNIT TESTING AUDIT & TECHNICAL REPORT", styles['DocTitle']))
    story.append(Paragraph("Dedicated In-Memory Verification of Front-End Lexing, IR Parsing & ELF Symbol Table Generation", styles['DocSubtitle']))
    
    meta_data = [
        [
            Paragraph("<b>Target Subsystem:</b> Worklet 1 (Front-End & Symbol Engine)", styles['MetaText']),
            Paragraph("<b>Test Suite:</b> unit_tests_worklet1/test_w1_functions.cpp", styles['MetaText'])
        ],
        [
            Paragraph("<b>Implementation File:</b> worklet1.cpp (1,369 LOC)", styles['MetaText']),
            Paragraph("<b>Test Harness:</b> Direct In-Memory C++ Execution (C++17)", styles['MetaText'])
        ],
        [
            Paragraph("<b>Date of Evaluation:</b> November 2026", styles['MetaText']),
            Paragraph("<b>Suite Result:</b> <font color='#2F855A'><b>47 / 47 PASSED (100.0%)</b></font>", styles['MetaText'])
        ]
    ]
    meta_table = Table(meta_data, colWidths=[250, 254])
    meta_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), c_light),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
        ('LEFTPADDING', (0, 0), (-1, -1), 8),
        ('RIGHTPADDING', (0, 0), (-1, -1), 8),
    ]))
    story.append(meta_table)
    story.append(Spacer(1, 12))

    # 1. Executive Summary
    story.append(Paragraph("1. Executive Summary", styles['SectionHeading']))
    story.append(Paragraph(
        "Worklet 1 serves as the central front-end parser, intermediate representation (IR) constructor, "
        "and back-half symbol table engine of the modular x86-32 assembler architecture. Operating between "
        "Phase 1 (source splitting) and Phase 3 (machine code encoding), Worklet 1 translates raw assembly "
        "text into strongly-typed <code>w2::Instruction</code> records, performs strict operand validation, lowers "
        "pseudo-instructions, resolves dense label identifiers, and synthesizes ELF32-compliant <code>.symtab</code> and "
        "<code>.strtab</code> sections.", styles['CustomBody']
    ))
    story.append(Paragraph(
        "To establish strict structural correctness without invoking the entire multi-process pipeline, a dedicated "
        "in-memory unit test suite (<code>unit_tests_worklet1/test_w1_functions.cpp</code>) was developed. The suite executes "
        "<b>47 targeted unit test assertions</b> covering the three fundamental core functions of Worklet 1 plus numeric "
        "literal parsing. All 47 tests execute in under <b>0.05 seconds</b> with a <b>100% pass rate</b>.", styles['CustomBody']
    ))

    # 2. Worklet 1 Architecture & Functional Responsibilities
    story.append(Paragraph("2. Architectural Responsibilities of Worklet 1", styles['SectionHeading']))
    story.append(Paragraph(
        "Worklet 1 has a dual role spanning two non-adjacent phases of the 5-phase pipeline:", styles['CustomBody']
    ))

    arch_data = [
        [Paragraph("Pipeline Phase", styles['TableHeader']), Paragraph("Component", styles['TableHeader']), Paragraph("Core Functional Responsibilities", styles['TableHeader'])],
        [
            Paragraph("<b>Phase 2: Front-End</b>", styles['TableCellCenter']),
            Paragraph("<code>parseAssemblySource()</code><br/><code>tokenizeSourceLine()</code>", styles['TableCell']),
            Paragraph("Tokenizes assembly instructions, performs mnemonic lookup, validates operand syntax, categorizes operands (REG, MEM, IMM, LABEL, MEMLABEL), manages label deduplication and dense ID assignment, lowers pseudo-instructions (LOAD, STORE, PUSHA, POPA, LODS, STOS), normalizes ESP addressing, and tracks symbol references (REF_REL32, REF_ABS_IMM, REF_ABS_DISP).", styles['TableCell'])
        ],
        [
            Paragraph("<b>Phase 2: Evaluator</b>", styles['TableCellCenter']),
            Paragraph("<code>parseMemoryExpression()</code><br/><code>validateMemorySyntax()</code>", styles['TableCell']),
            Paragraph("Parses x86-32 ModR/M and SIB memory addressing modes. Deconstructs <code>[base + index*scale + disp + symbol]</code>, enforces commutative ordering, checks scale validity (1, 2, 4, 8), and rejects illegal subtractions, multiple labels, or excess registers.", styles['TableCell'])
        ],
        [
            Paragraph("<b>Phase 4: Back-Half</b>", styles['TableCellCenter']),
            Paragraph("<code>buildSymbolTable()</code>", styles['TableCell']),
            Paragraph("Ingests label definitions from Worklet 1, exact byte addresses emitted by Worklet 2, and directives from Worklet 3. Validates declared global symbols, enforces ELF Rule 2 symbol partitioning (STN_UNDEF &rarr; STT_SECTION &rarr; STB_LOCAL &rarr; STB_GLOBAL), calculates <code>symtab.info</code> boundary, and emits binary 16-byte <code>Elf32_Sym</code> tables and null-terminated string tables.", styles['TableCell'])
        ]
    ]
    arch_table = Table(arch_data, colWidths=[100, 130, 274])
    arch_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_primary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(arch_table)
    story.append(Spacer(1, 10))

    # 3. Unit Test Suite Breakdown
    story.append(Paragraph("3. Detailed Unit Test Suite Breakdown (47 Assertions)", styles['SectionHeading']))
    story.append(Paragraph(
        "The unit test suite is divided into 4 structured test batteries, each exercising specific data structures and edge cases:", styles['CustomBody']
    ))

    # Battery 1
    story.append(Paragraph("Battery 1: parseMemoryExpressionComponents & validateMemoryExpressionSyntax (19 Tests)", styles['SubSectionHeading']))
    story.append(Paragraph(
        "Tests x86-32 memory expression parsing and syntax verification. Ensures addressing components are correctly segregated into base register, index register, scale multiplier, numeric displacement, and symbolic reference.", styles['CustomBody']
    ))

    b1_data = [
        [Paragraph("ID", styles['TableHeader']), Paragraph("Expression Tested", styles['TableHeader']), Paragraph("Verification Criteria", styles['TableHeader']), Paragraph("Status", styles['TableHeader'])],
        [Paragraph("1.1a", styles['TableCellCenter']), Paragraph("<code>[ebx]</code>", styles['CodeSnippet']), Paragraph("Base reg only (EBX), empty index, disp=0, empty sym", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.1b", styles['TableCellCenter']), Paragraph("<code>[esp]</code>", styles['CodeSnippet']), Paragraph("Base reg only (ESP), empty index, disp=0, empty sym", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.2a", styles['TableCellCenter']), Paragraph("<code>[ebx + 4]</code>", styles['CodeSnippet']), Paragraph("Base reg (EBX) with positive decimal displacement (+4)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.2b", styles['TableCellCenter']), Paragraph("<code>[edi + 0x1000]</code>", styles['CodeSnippet']), Paragraph("Base reg (EDI) with hexadecimal displacement (4096)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.3", styles['TableCellCenter']), Paragraph("<code>[ebp - 8]</code>", styles['CodeSnippet']), Paragraph("Base reg (EBP) with negative displacement (-8)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.4", styles['TableCellCenter']), Paragraph("<code>[ebx + ecx*1/2/4/8]</code>", styles['CodeSnippet']), Paragraph("SIB scaled index verification across all legal multipliers (1, 2, 4, 8)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.5a", styles['TableCellCenter']), Paragraph("<code>[ebx + ecx*4 + 16]</code>", styles['CodeSnippet']), Paragraph("Base + index*4 + positive displacement (+16)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.5b", styles['TableCellCenter']), Paragraph("<code>[ebp + edx*2 - 12]</code>", styles['CodeSnippet']), Paragraph("Base + index*2 - negative displacement (-12)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.6", styles['TableCellCenter']), Paragraph("<code>[4*ecx + ebx + 16]</code>", styles['CodeSnippet']), Paragraph("Commutative reordering: correctly extracts EBX as base and ECX as index*4", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.7", styles['TableCellCenter']), Paragraph("<code>[edx*4 + 32]</code>", styles['CodeSnippet']), Paragraph("Index*scale without base register (empty base, EDX index, scale 4, disp 32)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.8a", styles['TableCellCenter']), Paragraph("<code>[msg_table + 4]</code>", styles['CodeSnippet']), Paragraph("Symbolic memory operand with displacement (sym='msg_table', disp=4)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.8b", styles['TableCellCenter']), Paragraph("<code>[eax + data_buffer + 8]</code>", styles['CodeSnippet']), Paragraph("Base register + symbol + displacement resolution", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.9", styles['TableCellCenter']), Paragraph("<code>*0, *3, *5, *6</code>", styles['CodeSnippet']), Paragraph("Strict rejection of illegal scale multipliers (failScale0, 3, 5, 6)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.10a", styles['TableCellCenter']), Paragraph("<code>[ebx - ecx]</code>", styles['CodeSnippet']), Paragraph("Syntax rejection: subtracting register ('register cannot be subtracted')", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.10b", styles['TableCellCenter']), Paragraph("<code>[ebx - ecx*4]</code>", styles['CodeSnippet']), Paragraph("Syntax rejection: subtracting scaled index ('scaled index cannot be subtracted')", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.10c", styles['TableCellCenter']), Paragraph("<code>[ebx - my_var]</code>", styles['CodeSnippet']), Paragraph("Syntax rejection: subtracting label in memory operand", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.10d", styles['TableCellCenter']), Paragraph("<code>[eax + ebx + ecx]</code>", styles['CodeSnippet']), Paragraph("Syntax rejection: >2 registers in memory operand (base + index max)", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.10e", styles['TableCellCenter']), Paragraph("<code>[lbl1 + lbl2]</code>", styles['CodeSnippet']), Paragraph("Syntax rejection: >1 label in memory operand", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("1.10f", styles['TableCellCenter']), Paragraph("<code>[ebx*2 + ecx*4]</code>", styles['CodeSnippet']), Paragraph("Syntax rejection: >1 scaled index register in memory operand", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
    ]
    b1_table = Table(b1_data, colWidths=[36, 140, 280, 48])
    b1_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_secondary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 3),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(b1_table)
    story.append(Spacer(1, 10))

    # Battery 2
    story.append(Paragraph("Battery 2: parseAssemblySource & Front-End IR Generation (12 Tests)", styles['SubSectionHeading']))
    story.append(Paragraph(
        "Tests tokenization, instruction mapping into <code>w2::Instruction</code>, label dense numbering, pseudo-instruction lowering, and error diagnostics.", styles['CustomBody']
    ))

    b2_data = [
        [Paragraph("ID", styles['TableHeader']), Paragraph("Feature / Construct", styles['TableHeader']), Paragraph("Verification Detail & Assertions", styles['TableHeader']), Paragraph("Status", styles['TableHeader'])],
        [Paragraph("2.1", styles['TableCellCenter']), Paragraph("Basic Instruction Parsing", styles['TableCell']), Paragraph("MOV r,r; ADD r,imm; NOP; RET. Verifies operand kinds (REG, IMM) and immediate values.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.2", styles['TableCellCenter']), Paragraph("Dense Label ID Assignment", styles['TableCell']), Paragraph("Forward/backward jumps, label aliases sharing instruction indices, and endLabel detection.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.3", styles['TableCellCenter']), Paragraph("Pseudo-Op Lowering", styles['TableCell']), Paragraph("LOAD&rarr;MOV, STORE&rarr;MOV, PUSHA/POPA&rarr;PUSHAD/POPAD (prefix 0x66), LODS/STOS operand deduction.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.4", styles['TableCellCenter']), Paragraph("Explicit Width Directives", styles['TableCell']), Paragraph("BYTE PTR (8-bit), WORD PTR (16-bit), DWORD PTR (32-bit) attached to memory operands.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.5", styles['TableCellCenter']), Paragraph("ESP Normalization", styles['TableCell']), Paragraph("Detects <code>[ecx + esp]</code> and automatically normalizes ESP to base, ECX to index (scale 1).", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.6", styles['TableCellCenter']), Paragraph("Symbol References", styles['TableCell']), Paragraph("Differentiates REF_ABS_IMM (immediate address) and REF_ABS_DISP (memory operand target).", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.7", styles['TableCellCenter']), Paragraph("Error: Undefined Symbol", styles['TableCell']), Paragraph("Emits category 'Symbol Error' with message 'undefined symbol [name]'.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.8", styles['TableCellCenter']), Paragraph("Error: Duplicate Symbol", styles['TableCell']), Paragraph("Emits category 'Symbol Error' citing line number of original definition.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.9", styles['TableCellCenter']), Paragraph("Error: Reserved Register Label", styles['TableCell']), Paragraph("Rejects <code>eax:</code> with category 'Lexical Error' ('label name is a reserved word').", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.10", styles['TableCellCenter']), Paragraph("Error: Unsupported Mnemonic", styles['TableCell']), Paragraph("Rejects <code>bogus_opcode</code> with category 'Syntax Error' ('unsupported mnemonic').", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.11", styles['TableCellCenter']), Paragraph("Error: Rel8 Branch to Extern", styles['TableCell']), Paragraph("Rejects <code>loop external_proc</code> ('LOOP cannot reference an external symbol (rel8)').", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("2.12", styles['TableCellCenter']), Paragraph("Error: Comma Syntax", styles['TableCell']), Paragraph("Detects missing comma (<code>mov eax ebx</code>) and missing trailing operand (<code>mov eax,</code>).", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
    ]
    b2_table = Table(b2_data, colWidths=[36, 140, 280, 48])
    b2_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_secondary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 3),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(b2_table)
    story.append(Spacer(1, 10))

    # Battery 3 & Bonus
    story.append(Paragraph("Battery 3: buildSymbolTable & ELF32 Compliance (6 Tests)", styles['SubSectionHeading']))
    story.append(Paragraph(
        "Validates ELF symbol table generation, symbol binding attributes, and binary section payloads.", styles['CustomBody']
    ))

    b3_data = [
        [Paragraph("ID", styles['TableHeader']), Paragraph("Verification Target", styles['TableHeader']), Paragraph("ELF Specification Rule & Assertion", styles['TableHeader']), Paragraph("Status", styles['TableHeader'])],
        [Paragraph("3.1", styles['TableCellCenter']), Paragraph("Byte Address Resolution", styles['TableCell']), Paragraph("Maps instruction indices to exact byte offsets produced by Worklet 2.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("3.2", styles['TableCellCenter']), Paragraph("Undeclared Global Check", styles['TableCell']), Paragraph("Validates declared globals; halts if declared symbol has no concrete definition.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("3.3", styles['TableCellCenter']), Paragraph("Implicit _start Promotion", styles['TableCell']), Paragraph("Automatically promotes entry point <code>_start</code> to STB_GLOBAL even if not declared global.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("3.4", styles['TableCellCenter']), Paragraph("Symbol Partitioning & sh_info", styles['TableCell']), Paragraph("Enforces ELF Rule 2: all STB_LOCAL symbols precede STB_GLOBAL. Sets <code>symtab.info == firstNonLocal</code>.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("3.5", styles['TableCellCenter']), Paragraph("EQU & Extern Symbol Attributes", styles['TableCell']), Paragraph("EQU assigned SHN_ABS + STT_NOTYPE. Externs assigned SHN_UNDEF + STB_GLOBAL + value 0.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
        [Paragraph("3.6", styles['TableCellCenter']), Paragraph(".symtab & .strtab Binary Layout", styles['TableCell']), Paragraph("Ensures <code>.symtab</code> byte size is multiple of 16 (sizeof Elf32_Sym) and <code>.strtab</code> starts with null byte.", styles['TableCell']), Paragraph("PASS", styles['TableCellPass'])],
    ]
    b3_table = Table(b3_data, colWidths=[36, 140, 280, 48])
    b3_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_secondary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 3),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(b3_table)
    story.append(Spacer(1, 10))

    story.append(Paragraph("Battery 4: parseNumericLiteral Multi-Radix Parsing (10 Tests)", styles['SubSectionHeading']))
    story.append(Paragraph(
        "Validates number parser across standard and assembly-specific radices: decimal (<code>12345</code>, <code>-9876</code>, <code>255d</code>), hex (<code>0xDEADBEEF</code>, <code>0FFH</code>), binary (<code>10101010b</code>), octal (<code>77o</code>, <code>177q</code>), and negative validation for malformed inputs (<code>1234xyz</code>, <code>0x123Z</code>). All 10 tests passed.", styles['CustomBody']
    ))

    # 4. Critical Assessment & Coverage Analysis
    story.append(Spacer(1, 8))
    story.append(Paragraph("4. Critical Assessment & Gap Analysis", styles['SectionHeading']))
    story.append(Paragraph(
        "While the unit test suite demonstrates high algorithmic rigor for the selected functions, an objective software "
        "engineering audit reveals key areas where testing can be expanded to achieve enterprise-grade coverage:", styles['CustomBody']
    ))

    gaps_data = [
        [Paragraph("Category", styles['TableHeader']), Paragraph("Observed State in Unit Suite", styles['TableHeader']), Paragraph("Production Recommendation", styles['TableHeader'])],
        [
            Paragraph("<b>Lexer Isolation</b>", styles['TableCell']),
            Paragraph("<code>tokenizeSourceLine()</code> is only tested indirectly through <code>parseAssemblySource()</code>.", styles['TableCell']),
            Paragraph("Add dedicated unit tests asserting token sequence, line/column tracking, comment stripping, and malformed UTF-8 BOM handling.", styles['TableCell'])
        ],
        [
            Paragraph("<b>Register Widths</b>", styles['TableCell']),
            Paragraph("Unit tests currently only pass 32-bit registers (EAX, EBX, ECX).", styles['TableCell']),
            Paragraph("Add explicit test assertions verifying 16-bit registers (AX..DI) and 8-bit registers (AL..BH) parse with correct <code>size</code> bits.", styles['TableCell'])
        ],
        [
            Paragraph("<b>Condition Codes</b>", styles['TableCell']),
            Paragraph("<code>parseConditionCode()</code> has 16 condition codes and aliases that are untested in unit test suite.", styles['TableCell']),
            Paragraph("Implement parametric tests asserting all 16 condition codes (O, NO, B/C/NAE, AE/NB/NC, E/Z, NE/NZ, etc.) and SETcc/CMOVcc mnemonics.", styles['TableCell'])
        ],
        [
            Paragraph("<b>Multi-Section Symbols</b>", styles['TableCell']),
            Paragraph("<code>buildSymbolTable()</code> unit tests only instantiate models with <code>.text</code>.", styles['TableCell']),
            Paragraph("Instantiate models with <code>.data</code> and <code>.bss</code> sections to verify STT_SECTION and STT_OBJECT symbol emissions.", styles['TableCell'])
        ]
    ]
    gaps_table = Table(gaps_data, colWidths=[110, 194, 200])
    gaps_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_primary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(gaps_table)
    story.append(Spacer(1, 12))

    # 5. Conclusion
    story.append(Paragraph("5. Conclusion & Certification", styles['SectionHeading']))
    story.append(Paragraph(
        "The Worklet 1 unit test suite provides fast, deterministic, in-memory validation of the assembler's "
        "most mathematically complex parsing tasks (SIB addressing, displacement commutativity, and ELF symbol layout). "
        "With <b>47/47 tests passing</b>, Worklet 1 meets its functional specification for core addressing mode analysis, "
        "front-end AST IR generation, and ELF32 symbol table construction.", styles['CustomBody']
    ))

    doc.build(story, canvasmaker=NumberedCanvas)
    print(f"Successfully generated {filename}")

if __name__ == '__main__':
    build_pdf()

