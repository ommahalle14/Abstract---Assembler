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
        if self._pageNumber > 1:
            self.drawString(54, 755, "Worklet 1 Workflow & Test Case Execution Analysis (Tests 001–020 & 061–070)")
            self.setStrokeColor(colors.HexColor("#CBD5E0"))
            self.setLineWidth(0.5)
            self.line(54, 750, 558, 750)
        
        self.drawString(54, 35, "MODULAR X86-32 ASSEMBLER ARCHITECTURE — WORKLET 1 EXECUTION MANUAL")
        self.drawRightString(558, 35, f"Page {self._pageNumber} of {page_count}")
        self.setStrokeColor(colors.HexColor("#CBD5E0"))
        self.setLineWidth(0.5)
        self.line(54, 45, 558, 45)
        self.restoreState()

def build_pdf(filename="Worklet1_Testcases_Workflow_Execution_Analysis.pdf"):
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
    c_dark = colors.HexColor("#2D3748")      # Charcoal body
    c_light = colors.HexColor("#F7FAFC")     # Soft light background
    c_border = colors.HexColor("#E2E8F0")
    c_code_bg = colors.HexColor("#EDF2F7")
    c_pass = colors.HexColor("#2F855A")      # Forest Green
    c_err = colors.HexColor("#C53030")       # Dark Crimson

    styles.add(ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=22,
        leading=26,
        textColor=c_primary,
        spaceAfter=6
    ))
    styles.add(ParagraphStyle(
        'DocSubtitle',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=11,
        leading=15,
        textColor=c_secondary,
        spaceAfter=12
    ))
    styles.add(ParagraphStyle(
        'MetaText',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=12,
        textColor=colors.HexColor("#4A5568")
    ))
    styles.add(ParagraphStyle(
        'SectionHeading',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=13,
        leading=17,
        textColor=c_primary,
        spaceBefore=14,
        spaceAfter=6,
        keepWithNext=True
    ))
    styles.add(ParagraphStyle(
        'SubSectionHeading',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=10.5,
        leading=14,
        textColor=c_secondary,
        spaceBefore=10,
        spaceAfter=4,
        keepWithNext=True
    ))
    styles.add(ParagraphStyle(
        'ItemTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=9.5,
        leading=13,
        textColor=c_primary,
        spaceBefore=6,
        spaceAfter=2,
        keepWithNext=True
    ))
    styles.add(ParagraphStyle(
        'CustomBody',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=12,
        textColor=c_dark,
        spaceAfter=4
    ))
    styles.add(ParagraphStyle(
        'CustomCode',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=8,
        leading=10.5,
        textColor=colors.HexColor("#1A202C")
    ))
    styles.add(ParagraphStyle(
        'CodeSnippet',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=7.5,
        leading=9.5,
        textColor=colors.HexColor("#2B6CB0")
    ))
    styles.add(ParagraphStyle(
        'TableHeader',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=8.5,
        leading=11,
        textColor=colors.white,
        alignment=1
    ))
    styles.add(ParagraphStyle(
        'TableCell',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8,
        leading=11,
        textColor=c_dark
    ))
    styles.add(ParagraphStyle(
        'TableCellCenter',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8,
        leading=11,
        textColor=c_dark,
        alignment=1
    ))
    styles.add(ParagraphStyle(
        'TableCellPass',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=8,
        leading=11,
        textColor=c_pass,
        alignment=1
    ))
    styles.add(ParagraphStyle(
        'TableCellErr',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=8,
        leading=11,
        textColor=c_err,
        alignment=1
    ))

    story = []

    # Title & Metadata
    story.append(Paragraph("WORKLET 1 EXECUTION WORKFLOW & TEST CASE ANALYSIS", styles['DocTitle']))
    story.append(Paragraph("Comprehensive Technical Evaluation of Valid Tests (001–020) and Worklet 1 Error Tests (061–070)", styles['DocSubtitle']))

    meta_data = [
        [
            Paragraph("<b>Target Subsystem:</b> Worklet 1 (Lexer, Parser, IR & Symbol Table)", styles['MetaText']),
            Paragraph("<b>Analyzed Range 1:</b> Valid Tests 001 to 020 (ISA Coverage)", styles['MetaText'])
        ],
        [
            Paragraph("<b>Pipeline Scope:</b> Phases 2 & 4 (Front-End IR & ELF Symbol Table)", styles['MetaText']),
            Paragraph("<b>Analyzed Range 2:</b> Error Tests 061 to 070 (Worklet 1 Rejections)", styles['MetaText'])
        ],
        [
            Paragraph("<b>Output Files Generated:</b> .bin, .o, IR Dumps, Relocations", styles['MetaText']),
            Paragraph("<b>Status:</b> All 30 Analyzed Cases Verified (100% Deterministic)", styles['MetaText'])
        ]
    ]
    meta_table = Table(meta_data, colWidths=[250, 254])
    meta_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), c_light),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 8),
        ('RIGHTPADDING', (0, 0), (-1, -1), 8),
    ]))
    story.append(meta_table)
    story.append(Spacer(1, 10))

    # Section 1: Worklet 1 Pipeline Architecture
    story.append(Paragraph("1. Worklet 1 Pipeline Execution Architecture", styles['SectionHeading']))
    story.append(Paragraph(
        "Worklet 1 is the front-end parser and back-half symbol resolver for the modular x86-32 assembler. "
        "When executing any assembly program, Worklet 1 executes a strictly ordered 6-step internal workflow:", styles['CustomBody']
    ))

    steps_data = [
        [Paragraph("Step", styles['TableHeader']), Paragraph("Stage Name", styles['TableHeader']), Paragraph("Core Mechanism & Data Transformation in Worklet 1", styles['TableHeader'])],
        [
            Paragraph("<b>1</b>", styles['TableCellCenter']),
            Paragraph("<b>Source Ingestion</b>", styles['TableCell']),
            Paragraph("Receives filtered <code>.text</code> lines from Worklet 3's <code>splitSource()</code>. Strips comments (<code>;</code>) and skips blank lines.", styles['TableCell'])
        ],
        [
            Paragraph("<b>2</b>", styles['TableCellCenter']),
            Paragraph("<b>Lexical Tokenization</b>", styles['TableCell']),
            Paragraph("<code>tokenizeSourceLine()</code> breaks characters into <code>Token</code> structs: MNEMONIC, REGISTER, IMMEDIATE, IDENTIFIER, LABEL, COMMA, MEMORY. Validates bracket pairing and numeric literals.", styles['TableCell'])
        ],
        [
            Paragraph("<b>3</b>", styles['TableCellCenter']),
            Paragraph("<b>Label Discovery & Dense IDs</b>", styles['TableCell']),
            Paragraph("Scans statement prefixes for <code>LABEL</code> tokens (e.g. <code>_start:</code>). Assigns contiguous dense IDs (<code>0..N</code>). Multiple labels pointing to the same instruction share the same dense ID. Detects <code>endLabel</code>.", styles['TableCell'])
        ],
        [
            Paragraph("<b>4</b>", styles['TableCellCenter']),
            Paragraph("<b>Instruction Parsing & Lowering</b>", styles['TableCell']),
            Paragraph("Maps mnemonic strings to <code>w2::Op</code> enums. Normalizes pseudo-instructions: <code>LOAD</code>/<code>STORE</code> &rarr; <code>MOV</code>, <code>PUSHA</code>/<code>POPA</code> &rarr; <code>PUSHAD</code>/<code>POPAD</code> (prefix <code>0x66</code>). Normalizes ESP in SIB base. Categorizes operands into <code>w2::Operand</code> (REG, MEM, IMM, LABEL, MEMLABEL).", styles['TableCell'])
        ],
        [
            Paragraph("<b>5</b>", styles['TableCellCenter']),
            Paragraph("<b>Symbol Reference Tracking</b>", styles['TableCell']),
            Paragraph("Records unresolved symbol targets in <code>refs</code> array: <code>REF_REL32</code> (relative branches), <code>REF_ABS_IMM</code> (immediate label references), <code>REF_ABS_DISP</code> (memory displacements).", styles['TableCell'])
        ],
        [
            Paragraph("<b>6</b>", styles['TableCellCenter']),
            Paragraph("<b>Symbol Table Construction</b>", styles['TableCell']),
            Paragraph("In Phase 4, <code>buildSymbolTable()</code> takes byte offsets from Worklet 2, validates declared globals, promotes <code>_start</code> to <code>STB_GLOBAL</code>, partitions symbols into LOCAL and GLOBAL sets, and generates binary <code>.symtab</code> and <code>.strtab</code>.", styles['TableCell'])
        ]
    ]
    steps_table = Table(steps_data, colWidths=[30, 110, 364])
    steps_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_primary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(steps_table)
    story.append(Spacer(1, 10))

    # Section 2: Detailed Workflow for Tests 001 to 020
    story.append(Paragraph("2. Detailed Workflow Analysis: Valid Tests 001 to 020", styles['SectionHeading']))
    story.append(Paragraph(
        "Each valid test case demonstrates how Worklet 1's parser transforms specific x86-32 instructions into "
        "typed IR structs (<code>w2::Instruction</code>) for consumption by Worklet 2:", styles['CustomBody']
    ))

    valid_cases = [
        ("Test 001", "MOV between registers (32-bit, 16-bit, and 8-bit)",
         "mov eax, ebx | mov ax, bx | mov al, bl | mov ah, bh | ret",
         "Worklet 1 receives 16 instruction lines. The lexer maps register strings to hardware numbers 0..7 via lookupRegister(). Differentiates 32-bit (EAX..EDI, size=32), 16-bit (AX..DI, size=16), and 8-bit registers (AL..BL size=8, AH..BH size=8). Emits 15 typed w2::Instruction IR records with operandCount=2 (REG, REG). Labels '_start' assigned dense LabelId=0. Status: 0 errors."),

        ("Test 002", "MOV register with immediate formats (decimal, hex, octal, binary)",
         "mov eax, 100 | mov eax, 0x1000 | mov eax, 0FFh | mov eax, 1010b | mov eax, 77o | mov eax, -500",
         "The lexer identifies immediate numeric tokens and invokes parseNumericLiteral(). Parses multi-radix formats: base 10 (100, -500), base 16 (0x prefix, h suffix), base 2 (b suffix), and base 8 (o suffix). Encodes values into 32-bit signed integers and tags operands as kind=IMM. Zero errors emitted."),

        ("Test 003", "Memory addressing with [base] and [base + disp]",
         "mov eax, [ebx] | mov eax, [ebx + 4] | mov eax, [ecx - 8] | mov eax, [edx + 0x1000]",
         "The lexer groups bracketed text into TokenType::MEMORY. Worklet 1's parseMemoryExpressionComponents() decomposes terms: identifies base registers (EBX, ECX, EDX) and extracts signed displacements (+4, -8, +4096). Instantiates w2::OperandKind::MEM with empty index registers."),

        ("Test 004", "SIB memory addressing [base + index*scale + disp]",
         "mov eax, [ebx + ecx*1] | mov eax, [ebx + edx*2 + 8] | mov eax, [esi + edi*4 - 16] | mov eax, [eax + ebx*8 + 100]",
         "Evaluates complex Scaled Index Byte (SIB) expressions. Validates multipliers (scale in {1, 2, 4, 8}). Verifies base and index registers. Normalizes commutative terms. Emits MemOperand structs storing base, index, scale, and displacement."),

        ("Test 005", "Index*scale + disp and absolute 32-bit displacement",
         "mov eax, [ecx*4 + 16] | mov eax, [0] | mov ebx, [0x1000]",
         "Detects addressing expressions without a base register. In [ecx*4 + 16], sets base=Reg::NONE, index=ECX, scale=4, disp=16. In [0x1000], identifies absolute displacement addressing with both base and index equal to Reg::NONE."),

        ("Test 006", "Sized memory operands (BYTE PTR, WORD PTR, DWORD PTR)",
         "mov BYTE PTR [ebx], 10 | mov WORD PTR [ebx], 20 | mov DWORD PTR [ebx], 30",
         "When parsing memory operands without a register to infer size, Worklet 1 consumes explicit size keywords ('BYTE', 'WORD', 'DWORD') followed by optional 'PTR'. Attaches explicit width bits (8, 16, 32) to the MEM operand so Worklet 2 selects the correct ModR/M and opcode."),

        ("Test 007", "XCHG across all operand combinations",
         "xchg eax, ebx | xchg ecx, edx | xchg eax, [ebx] | xchg [ebx], ecx",
         "Mnemonic 'XCHG' is looked up via lookupBaseMnemonic(). Parses two operands (REG-REG, REG-MEM, MEM-REG). Flags special 1-byte accumulator optimization forms when EAX is one of the operands. Emits w2::Op::XCHG."),

        ("Test 008", "PUSH and POP with all 32-bit and 16-bit registers",
         "push eax | push ebx | push ax | push bx | pop bx | pop ax | pop ebx | pop eax",
         "PUSH and POP are parsed as single-operand instructions (operandCount=1). Differentiates 32-bit registers (single-byte 50+r / 58+r opcodes) and 16-bit registers (requires 0x66 operand-size override prefix). Operands marked REG."),

        ("Test 009", "PUSH and POP with memory operands",
         "push dword ptr [eax] | push word ptr [ebx + 4] | pop dword ptr [eax] | pop word ptr [ebx + 4]",
         "Parses single memory operand with explicit width keywords. Emits w2::Op::PUSH and w2::Op::POP with operandCount=1 and operand[0].kind=MEM. Worklet 2 maps these to the FF /6 and 8F /0 ModR/M groups."),

        ("Test 010", "Stack state manipulation (PUSHAD, POPAD, PUSHA, POPA, PUSHFD, POPFD)",
         "pushad | popad | pusha | popa | pushfd | popfd",
         "Worklet 1 identifies 16-bit pseudo-instructions 'PUSHA' and 'POPA', lowering them to 'PUSHAD' and 'POPAD' while attaching prefix=0x66. Operands are set to operandCount=0. Handled cleanly with zero errors."),

        ("Test 011", "LEA instruction with simple, SIB, and label addressing",
         "lea eax, [ebx + 4] | lea ecx, [edx + esi*4 + 8] | lea edx, target_label",
         "Parses destination register and source memory address. For bare labels ('lea edx, target_label'), Worklet 1 sets symIsMemory=true, converts the operand to MEMLABEL, and records a REF_ABS_DISP symbol reference for link-time relocation."),

        ("Test 012", "LOAD and STORE pseudo-instructions",
         "LOAD eax, [ebx] | LOAD ecx, 100 | STORE [ebx], edx | STORE [ebp - 4], eax",
         "Worklet 1 detects custom pseudo-mnemonics: 'LOAD dst, src' is lowered to 'MOV dst, src' and 'STORE dst, src' is lowered to 'MOV dst, src'. Re-indexes operand structs to standard MOV semantics, producing valid w2::Instruction IR."),

        ("Test 013", "Accumulator conversions and flag transfers",
         "cbw | cwde | cwd | cdq | lahf | sahf",
         "Recognizes single-byte accumulator sign-extension and flag operations via lookupExtendedMnemonic(). Configures operandCount=0. Sets appropriate opcode mappings (98, 99, 9E, 9F)."),

        ("Test 014", "ADD arithmetic across 32, 16, and 8-bit operands",
         "add eax, ebx | add ecx, 100 | add byte ptr [eax], 5 | add [ebx], edx",
         "Decodes 'ADD' into w2::Op::ADD. Evaluates destination and source operand combinations (r-r, r-i, r-m, m-r, m-i). Propagates operand bit width to enable shortest immediate encoding in Worklet 2."),

        ("Test 015", "ADC (Add with Carry) across all operand shapes",
         "adc eax, ebx | adc ecx, 1 | adc edx, [ebx + 4] | adc dword ptr [eax], 10",
         "Decodes 'ADC' into w2::Op::ADC. Parses register, memory, and immediate operands with full ModR/M and SIB support. Passes 2-operand IR directly to encoder."),

        ("Test 016", "SUB arithmetic across registers, memory, and immediates",
         "sub eax, ebx | sub ecx, 500 | sub dword ptr [ebx], 100 | sub edx, [esi]",
         "Decodes 'SUB' into w2::Op::SUB. Validates two operands, sets operand kinds, and verifies numeric displacement ranges. Passes cleanly to machine code generator."),

        ("Test 017", "SBB (Subtract with Borrow) across all operand forms",
         "sbb eax, ebx | sbb edx, 1 | sbb eax, [ebx + 8] | sbb dword ptr [edi], 5",
         "Decodes 'SBB' into w2::Op::SBB. Validates 2 operands, ensuring correct register and memory operands for multi-word subtraction routines."),

        ("Test 018", "CMP comparisons across 32, 16, and 8-bit registers and memory",
         "cmp eax, ebx | cmp ax, bx | cmp al, bl | cmp ecx, 100 | cmp dword ptr [ebx], 0",
         "Decodes 'CMP' into w2::Op::CMP. Accurately preserves operand bit-width tags (8, 16, 32) across registers and sized memory references to ensure flags-only subtraction encoding."),

        ("Test 019", "AND bitwise logic across all sizes and operand types",
         "and eax, 0xFF | and ebx, ecx | and ax, 0x0F0F | and byte ptr [edx], 0x80",
         "Decodes 'AND' into w2::Op::AND. Handles bitmask immediates and register pairs, preparing operands for bitwise logical conjunction."),

        ("Test 020", "OR bitwise logic across all sizes",
         "or eax, 0x10 | or ebx, ecx | or ax, bx | or dword ptr [esi], 0x80000000",
         "Decodes 'OR' into w2::Op::OR. Handles full 32-bit bitmask immediates, sized memory operands, and register transfers.")
    ]

    for tid, title, code, flow in valid_cases:
        t_box = [
            [Paragraph(f"<b>{tid}: {title}</b>", styles['ItemTitle'])],
            [Paragraph(f"<b>Code:</b> <code>{code}</code>", styles['CustomCode'])],
            [Paragraph(f"<b>Worklet 1 Internal Workflow:</b> {flow}", styles['CustomBody'])]
        ]
        t_table = Table(t_box, colWidths=[504])
        t_table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, -1), c_light),
            ('BOX', (0, 0), (-1, -1), 0.5, c_border),
            ('TOPPADDING', (0, 0), (-1, -1), 3),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
            ('LEFTPADDING', (0, 0), (-1, -1), 6),
            ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ]))
        story.append(t_table)
        story.append(Spacer(1, 4))

    # Section 3: Detailed Workflow for Tests 061 to 070 (Error Cases)
    story.append(PageBreak())
    story.append(Paragraph("3. Detailed Workflow Analysis: Worklet 1 Error Tests 061 to 070", styles['SectionHeading']))
    story.append(Paragraph(
        "Tests 061 to 070 specifically stress Worklet 1's error detection, syntax validation, lexical rules, and "
        "symbol resolution logic. For each case, Worklet 1 halts the pipeline before machine code encoding occurs:", styles['CustomBody']
    ))

    error_cases = [
        ("Test 061", "Lexical Error: Missing Closing Bracket in Memory Operand",
         "section .text\n_start:\n    mov eax, [ebx + 4",
         "Worklet 1 Lexer (tokenizeSourceLine)", "Line 3, Column 14",
         "The lexer encounters '[' at column 14 indicating a memory operand. It loops scanning for the closing ']' delimiter. Reaching the end of the line without finding ']', it records: 'Lexical Error at line 3, column 14: missing closing \']\' for memory operand'. An incomplete bracket prevents TokenType::MEMORY creation. Worklet 1 appends an AsmError, halts IR parsing, and aborts before Phase 3."),

        ("Test 062", "Lexical Error: Invalid Immediate Literal",
         "section .text\n_start:\n    mov eax, 12345xyz",
         "Worklet 1 Lexer (parseNumericLiteral)", "Line 3, Column 14",
         "The lexer encounters digits '12345' followed immediately by alphanumeric characters 'xyz'. It invokes parseNumericLiteral() in base 10, which encounters illegal characters 'x', 'y', 'z'. Because it does not match hex (0x/h), binary (b), or octal (o), it reports: 'Lexical Error at line 3, column 14: invalid immediate \'12345xyz\''. Halts immediately."),

        ("Test 063", "Lexical Error: Reserved Register Keyword as Label",
         "section .text\neax:\n    nop",
         "Worklet 1 Lexer (tokenizeSourceLine)", "Line 2, Column 1",
         "When encountering an identifier followed by ':', Worklet 1 checks isValidRegisterName() and isValidMnemonicName(). 'eax' is a reserved x86 architecture register. To avoid lexical ambiguity where a register could be confused with a jump target, Worklet 1 emits: 'Lexical Error at line 2, column 1: label name \'eax\' is a reserved word'. Terminates lexing."),

        ("Test 064", "Syntax Error: Unsupported / Unknown Mnemonic",
         "section .text\n_start:\n    invalidop eax, ebx",
         "Worklet 1 Parser (parseAssemblySource)", "Line 3, Column 5",
         "After tokenization, Worklet 1 parses the statement's initial token as an instruction mnemonic. It queries lookupBaseMnemonic() and lookupExtendedMnemonic(). 'invalidop' is not found in the supported 114-mnemonic ISA table. Worklet 1 emits: 'Syntax Error at line 3, column 5: unsupported mnemonic \'invalidop\''. Front-end AST creation halts."),

        ("Test 065", "Syntax Error: 16-bit Register in Memory Brackets",
         "section .text\n_start:\n    mov eax, [bx]",
         "Worklet 1 Parser (parseOperand)", "Line 3, Column 14",
         "In 32-bit protected mode, memory addressing requires 32-bit registers (EAX..EDI). 16-bit addressing ([BX], [BP+SI]) is a 16-bit real mode relic. When parseMemoryExpressionComponents() extracts base register 'BX', Worklet 1 checks lookupRegister() and discovers bits==16. Emits: 'Syntax Error at line 3, column 14: 16/8-bit register \'BX\' cannot be used as an address (use 32-bit)'."),

        ("Test 066", "Syntax Error: Short Branch (LOOP) to External Symbol",
         "extern ext_func\nsection .text\n_start:\n    loop ext_func",
         "Worklet 1 Parser (parseAssemblySource)", "Line 4, Column 10",
         "The 'LOOP' mnemonic is architecturally restricted to an 8-bit signed displacement (rel8: -128..+127 bytes). External symbols declared via 'extern' reside in foreign translation units and require 32-bit link-time relocations (R_386_PC32). Because rel8 cannot fit a 32-bit relocation, Worklet 1 enforces this constraint and emits: 'Syntax Error at line 4, column 10: LOOP cannot reference an external symbol (rel8)'."),

        ("Test 067", "Syntax Error: Register Subtraction in Memory Operand",
         "section .text\n_start:\n    mov eax, [ebx-ecx]",
         "Worklet 1 Memory Validator (validateMemoryExpressionSyntax)", "Line 3, Column 14",
         "x86 SIB/ModRM hardware only supports addition: Base + Index*Scale + Disp. Subtraction of registers is not supported by CPU address generation units. Worklet 1's syntax validator parses terms and detects a minus '-' sign preceding register 'ecx'. Emits: 'Syntax Error at line 3, column 14: a register cannot be subtracted in a memory operand (\'[ebx-ecx]\')'."),

        ("Test 068", "Syntax Error: Missing Operand After Comma",
         "section .text\n_start:\n    mov eax,",
         "Worklet 1 Parser (parseAssemblySource)", "Line 3, Column 13",
         "The parser processes mnemonic 'mov' and destination register 'eax'. It consumes the comma token ',' and attempts to read the second operand. Reaching the end of line tokens, it detects a dangling comma and reports: 'Syntax Error at line 3, column 13: missing operand after comma'. Instructions cannot terminate with dangling punctuation."),

        ("Test 069", "Symbol Error: Undefined Symbol Reference",
         "section .text\n_start:\n    jmp nonexistent_target",
         "Worklet 1 Symbol Resolution (parseAssemblySource)", "Line 3, Column 9",
         "When encountering an IDENTIFIER operand for a branch instruction, Worklet 1 searches both the local label table (idByName) and the declared externs set. 'nonexistent_target' is absent from both tables. Worklet 1 reports: 'Symbol Error at line 3, column 9: undefined symbol \'nonexistent_target\''. Prevents unresolvable relocations."),

        ("Test 070", "Symbol Table Error: Declared GLOBAL Symbol Never Defined",
         "global undefined_global_func\nsection .text\n_start:\n    nop",
         "Worklet 1 Symbol Table Generator (buildSymbolTable)", "Line 1, Column 1",
         "In Phase 4, Worklet 1's buildSymbolTable() iterates through the 'globals' list passed by Worklet 3. For every global symbol, it asserts that a matching label exists in .text, .data, or equConstants. Finding no definition for 'undefined_global_func', it aborts: 'Symbol Table Error: Global symbol \'undefined_global_func\' is declared but not defined'.")
    ]

    for tid, title, code, sub, loc, flow in error_cases:
        e_box = [
            [Paragraph(f"<b>{tid}: {title}</b>", styles['ItemTitle'])],
            [Paragraph(f"<b>Code:</b> <code>{code.replace(chr(10), ' | ')}</code>", styles['CustomCode'])],
            [Paragraph(f"<b>Failing Component:</b> {sub} &nbsp;|&nbsp; <b>Location:</b> {loc}", styles['MetaText'])],
            [Paragraph(f"<b>Workflow Breakdown & Root Cause:</b> {flow}", styles['CustomBody'])]
        ]
        e_table = Table(e_box, colWidths=[504])
        e_table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, -1), colors.HexColor("#FFF5F5")),
            ('BOX', (0, 0), (-1, -1), 0.5, colors.HexColor("#FEB2B2")),
            ('TOPPADDING', (0, 0), (-1, -1), 3),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
            ('LEFTPADDING', (0, 0), (-1, -1), 6),
            ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ]))
        story.append(e_table)
        story.append(Spacer(1, 4))

    # Section 4: Master Summary Table
    story.append(PageBreak())
    story.append(Paragraph("4. Master Comparative Summary Table (30 Test Cases)", styles['SectionHeading']))
    story.append(Paragraph(
        "Summary matrix of all 30 analyzed test cases detailing the Worklet 1 subsystem involved, "
        "the instruction types, error diagnostics, and final execution status:", styles['CustomBody']
    ))

    sum_data = [
        [Paragraph("ID", styles['TableHeader']), Paragraph("Test Case Focus", styles['TableHeader']), Paragraph("Worklet 1 Subsystem", styles['TableHeader']), Paragraph("Primary Operands / Error", styles['TableHeader']), Paragraph("Result", styles['TableHeader'])],
    ]
    for tid, title, code, flow in valid_cases:
        tnum = tid.split()[1]
        sum_data.append([
            Paragraph(tnum, styles['TableCellCenter']),
            Paragraph(title[:36] + ("..." if len(title) > 36 else ""), styles['TableCell']),
            Paragraph("Phase 2 (IR Parser)", styles['TableCell']),
            Paragraph(code[:34] + ("..." if len(code) > 34 else ""), styles['CodeSnippet']),
            Paragraph("PASS", styles['TableCellPass'])
        ])

    for tid, title, code, sub, loc, flow in error_cases:
        tnum = tid.split()[1]
        short_sub = "Lexer" if "Lexer" in sub else ("Memory" if "Memory" in sub else ("Symbol" if "Symbol" in sub else "Parser"))
        short_err = title.split(":")[1].strip() if ":" in title else title
        sum_data.append([
            Paragraph(tnum, styles['TableCellCenter']),
            Paragraph(short_err[:36] + ("..." if len(short_err) > 36 else ""), styles['TableCell']),
            Paragraph(f"Phase 2 ({short_sub})", styles['TableCell']),
            Paragraph(f"Rejected at {loc}", styles['CodeSnippet']),
            Paragraph("CAUGHT", styles['TableCellErr'])
        ])

    sum_table = Table(sum_data, colWidths=[28, 170, 100, 158, 48])
    sum_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), c_primary),
        ('BOX', (0, 0), (-1, -1), 1, c_border),
        ('INNERGRID', (0, 0), (-1, -1), 0.5, c_border),
        ('TOPPADDING', (0, 0), (-1, -1), 2),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 2),
        ('LEFTPADDING', (0, 0), (-1, -1), 4),
        ('RIGHTPADDING', (0, 0), (-1, -1), 4),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, c_light])
    ]))
    story.append(sum_table)

    doc.build(story, canvasmaker=NumberedCanvas)
    print(f"Successfully generated {filename}")

if __name__ == '__main__':
    build_pdf()
